#!/usr/bin/env python3
"""Assemble a flight candidate from the explicitly pinned REM tuple."""
import argparse
import hashlib
import json
import os
from pathlib import Path, PurePosixPath
import re
import shlex
import shutil
import subprocess

ROOT = Path(__file__).resolve().parents[2]
TUPLE = ROOT / "tools/rem-flight-image/tuples/flight-20260925.json"


def run(args, **kwargs):
    subprocess.run(args, check=True, **kwargs)


def sha(path):
    h = hashlib.sha256()
    with path.open("rb") as f:
        for block in iter(lambda: f.read(1024 * 1024), b""):
            h.update(block)
    return h.hexdigest()


def elf_machine(path):
    try:
        out = subprocess.check_output(["readelf", "-h", str(path)], text=True, stderr=subprocess.DEVNULL)
    except (subprocess.CalledProcessError, FileNotFoundError):
        return None
    match = re.search(r"^\s*Machine:\s*(.*?)\s*$", out, re.M)
    return match.group(1) if match else None


def safe_guest_path(value):
    p = PurePosixPath(value)
    if not p.is_absolute() or ".." in p.parts:
        raise ValueError(f"guest path must be absolute and stay inside root: {value}")
    return Path(*p.parts[1:])


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--output", required=True, type=Path, help="new, nonexistent candidate directory")
    ap.add_argument("--tuple", type=Path, default=TUPLE)
    ap.add_argument("--artifacts", type=Path, help="optional selected and accepted overlay JSON")
    args = ap.parse_args()
    out = args.output.resolve()
    if out.exists():
        ap.error(f"output already exists; refusing to overwrite: {out}")
    spec = json.loads(args.tuple.read_text())
    out.mkdir(parents=True)
    tree = out / "rootfs-tree"
    tree.mkdir()
    inputs = {}
    for key, item in spec["inputs"].items():
        p = (ROOT / item["path"]).resolve()
        if not p.is_file() or sha(p) != item["sha256"]:
            raise SystemExit(f"pinned input check failed for {key}: {p}")
        expected_machine = spec.get("expected_elf_machines", {}).get(key)
        if expected_machine and elf_machine(p) != expected_machine:
            raise SystemExit(f"ELF architecture mismatch for {key}: {p}")
        inputs[key] = p

    image = out / "rem-rootfs.ext4"
    run(["cp", "--sparse=always", str(inputs["rootfs"]), str(image)])
    # Extract to a normal host tree. All additions below use install/copy and
    # mkfs.ext4 -d; debugfs is used only for read-only verification.
    extraction = subprocess.run(["debugfs", "-R", f"rdump / {tree}", str(image)],
                                text=True, capture_output=True)
    if extraction.returncode:
        raise SystemExit(f"rootfs extraction failed: {extraction.stderr.strip()}")
    if not (tree / "sbin/init").is_file() or not (tree / "bin/busybox").is_file():
        raise SystemExit("base rootfs extraction is incomplete (init or BusyBox missing)")

    selected = []
    dependency_checks = []
    test_only_selected = False
    removed_guest_files = []
    if args.artifacts:
        overlay = json.loads(args.artifacts.read_text())
        for entry in overlay.get("artifacts", []):
            candidate_test_only = (entry.get("accepted") is False and
                                   entry.get("candidate_test_only") is True)
            if entry.get("accepted") is not True and not candidate_test_only:
                raise SystemExit(f"refusing unaccepted artifact: {entry.get('name', entry)}")
            test_only_selected = test_only_selected or candidate_test_only
            src = Path(entry["source"]).resolve(strict=True)
            expected = entry.get("sha256")
            if not expected or sha(src) != expected:
                raise SystemExit(f"missing/wrong pinned hash for {entry.get('name', src)}")
            if "size" not in entry or src.stat().st_size != int(entry["size"]):
                raise SystemExit(f"missing/wrong pinned size for {entry.get('name', src)}")
            dest_rel = safe_guest_path(entry["destination"])
            dest = tree / dest_rel
            dest.parent.mkdir(parents=True, exist_ok=True)
            mode = int(entry["mode"], 8)
            if entry.get("executable") and not mode & 0o111:
                raise SystemExit(f"artifact marked executable but mode is not: {entry['name']}")
            if entry.get("executable") and not (src.stat().st_mode & 0o111):
                raise SystemExit(f"artifact source is not executable: {entry['name']}")
            if entry.get("executable") and not entry.get("elf_machine"):
                raise SystemExit(f"executable artifact needs a pinned ELF machine: {entry['name']}")
            if entry.get("elf_machine"):
                actual = elf_machine(src)
                if actual != entry["elf_machine"]:
                    raise SystemExit(f"ELF architecture mismatch for {entry['name']}: {actual}")
            for dep_path, dep_hash in entry.get("dependency_hashes", {}).items():
                dependency_checks.append((entry.get("name", str(src)), dep_path, dep_hash))
            shutil.copyfile(src, dest)
            os.chmod(dest, mode)
            selected.append({"name": entry.get("name", dest_rel.as_posix()),
                             "owner": entry.get("owner"),
                             "source": entry.get("source", str(src)),
                             "provenance": entry.get("provenance"),
                             "dependencies": entry.get("dependencies", []),
                             "dependency_hashes": entry.get("dependency_hashes", {}),
                             "accepted": entry.get("accepted") is True,
                             "candidate_test_only": candidate_test_only,
                             "path": "/" + dest_rel.as_posix(), "size": dest.stat().st_size,
                             "mode": f"{mode:04o}", "sha256": sha(dest),
                             "elf_machine": elf_machine(dest),
                             "executable": bool(entry.get("executable"))})

        selected_paths = {item["path"] for item in selected}
        for removal in overlay.get("remove_guest_files", []):
            guest_path = removal["path"]
            rel = safe_guest_path(guest_path)
            if guest_path in selected_paths:
                raise SystemExit(f"overlay both selects and removes guest file: {guest_path}")
            target = tree / rel
            try:
                resolved = target.resolve(strict=True)
            except FileNotFoundError:
                raise SystemExit(f"required stale guest file is missing: {guest_path}")
            if not resolved.is_relative_to(tree.resolve()) or target.is_symlink() or not resolved.is_file():
                raise SystemExit(f"refusing unsafe/non-regular guest file removal: {guest_path}")
            expected_hash = removal.get("sha256")
            actual_hash = sha(resolved)
            if not expected_hash or actual_hash != expected_hash:
                raise SystemExit(f"stale guest file hash mismatch: {guest_path}")
            resolved.unlink()
            removed_guest_files.append({"path": guest_path, "sha256": actual_hash,
                                        "reason": removal.get("reason", "reviewed overlay removal")})

    for artifact_name, dep_path, expected_hash in dependency_checks:
        dep = tree / safe_guest_path(dep_path)
        if not dep.is_file() or sha(dep) != expected_hash:
            raise SystemExit(f"dependency check failed for {artifact_name}: {dep_path}")

    # These are required for the package's existing init redirections and
    # devtmpfs setup. Create them while fakeroot and mkfs share one process.
    qimage = shlex.quote(str(image))
    qtree = shlex.quote(str(tree))
    qconsole = shlex.quote(str(tree / "dev/console"))
    qnull = shlex.quote(str(tree / "dev/null"))
    sh = (
        'set -eu; '
        f'rm -f {qconsole} {qnull}; '
        f'mknod -m 600 {qconsole} c 5 1; '
        f'mknod -m 666 {qnull} c 1 3; '
        f'truncate -s {int(spec["image_size_bytes"])} {qimage}; '
        f'mkfs.ext4 -q -F -L REM-FLIGHT -d {qtree} {qimage}'
    )
    run(["fakeroot", "sh", "-c", sh])
    run(["sync", str(image)])
    run(["e2fsck", "-fn", str(image)])

    required = spec["required_guest_files"]
    manifest_items = []
    for guest_path in required:
        rel = safe_guest_path(guest_path)
        p = tree / rel
        if not p.is_file() or p.stat().st_size == 0:
            raise SystemExit(f"required guest file missing/empty: {guest_path}")
        item = {"path": guest_path, "size": p.stat().st_size,
                "mode": f"{p.stat().st_mode & 0o7777:04o}", "sha256": sha(p),
                "elf_machine": elf_machine(p),
                "executable": guest_path in spec.get("required_executables", [])}
        if item["executable"] and not (p.stat().st_mode & 0o111):
            raise SystemExit(f"required executable lacks execute permission: {guest_path}")
        expected_machine = spec.get("required_guest_elf_machines", {}).get(guest_path)
        if expected_machine and item["elf_machine"] != expected_machine:
            raise SystemExit(f"ELF architecture mismatch for guest file: {guest_path}")
        manifest_items.append(item)
    for item in selected:
        if item["size"] == 0:
            raise SystemExit(f"selected artifact is empty: {item['name']}")
        manifest_items.append(item)

    # Read-only checks against the final ext4 image catch omissions during mkfs.
    for guest_path in required:
        result = subprocess.check_output(["debugfs", "-R", f"stat {guest_path}", str(image)],
                                         text=True, stderr=subprocess.DEVNULL)
        size_match = re.search(r"\bSize:\s*(\d+)", result)
        if "Type: regular" not in result or not size_match or int(size_match.group(1)) == 0:
            raise SystemExit(f"final image verification failed for {guest_path}")
    device_records = {}
    for guest_path, expected in spec["required_guest_devices"].items():
        result = subprocess.check_output(["debugfs", "-R", f"stat {guest_path}", str(image)],
                                         text=True, stderr=subprocess.DEVNULL)
        mode = re.search(r"Mode:\s+([0-7]{3,6})", result)
        dev = re.search(r"Device major/minor number:\s*([0-9a-f]+):([0-9a-f]+)", result, re.I)
        if expected["type"] not in result or not mode or int(mode.group(1), 8) & 0o7777 != int(expected["mode"], 8):
            raise SystemExit(f"required device node type/mode mismatch: {guest_path}")
        if not dev or int(dev.group(1), 16) != expected["major"] or int(dev.group(2), 16) != expected["minor"]:
            raise SystemExit(f"required device node major/minor mismatch: {guest_path}")
        device_records[guest_path] = expected

    shutil.copy2(inputs["qemu"], out / "qemu-system-myemulator32")
    shutil.copy2(inputs["kernel"], out / "vmlinux")
    os.chmod(out / "qemu-system-myemulator32", 0o755)
    tuple_record = {
        "tuple": spec["name"], "machine": spec["machine"], "memory": spec["memory"],
        "integration_status": ("TEST ONLY: contains unaccepted candidate-test artifacts"
                               if test_only_selected else "candidate built from accepted selections"),
        "icount": spec["icount"], "root_device": spec["root_device"],
        "shell": spec["shell"], "editor": spec["editor"],
        "compiler": spec["compiler"], "toolchain_runtime": spec["toolchain_runtime"],
        "tool_search_path": spec.get("tool_search_path", []),
        "build_executor": spec["build_executor"],
        "compiler_runtime": spec["compiler_runtime"],
        "inputs": {k: {"path": spec["inputs"][k]["path"], "size": inputs[k].stat().st_size,
                       "sha256": sha(inputs[k]), "mode": f"{inputs[k].stat().st_mode & 0o7777:04o}",
                       "elf_machine": elf_machine(inputs[k])} for k in inputs},
        "outputs": {"rootfs": {"size": image.stat().st_size, "sha256": sha(image),
                                 "mode": f"{image.stat().st_mode & 0o7777:04o}"}},
        "guest_artifacts": manifest_items,
        "removed_guest_files": removed_guest_files,
        "guest_devices": device_records,
    }
    if "acceptance_gate" in spec:
        tuple_record["acceptance_gate"] = spec["acceptance_gate"]
    (out / "manifest.json").write_text(json.dumps(tuple_record, indent=2) + "\n")
    tool_dir = Path(__file__).resolve().parent
    shutil.copy2(tool_dir / "verify.py", out / "verify.py")
    shutil.copy2(tool_dir / "boot.sh", out / "boot.sh")
    shutil.rmtree(tree)
    print(out)


if __name__ == "__main__":
    main()
