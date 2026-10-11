#!/usr/bin/env python3
"""Verify candidate host inputs and guest file contents against manifest.json."""
import argparse
import hashlib
import json
from pathlib import Path, PurePosixPath
import re
import subprocess
import tempfile


def sha(path):
    h = hashlib.sha256()
    with path.open("rb") as f:
        for b in iter(lambda: f.read(1024 * 1024), b""):
            h.update(b)
    return h.hexdigest()


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("candidate", type=Path)
    args = ap.parse_args()
    root = args.candidate.resolve()
    m = json.loads((root / "manifest.json").read_text())
    failures = []
    for key, item in m["inputs"].items():
        if key == "rootfs":
            # The input is the pinned seed image; the candidate rootfs is a
            # rebuilt image whose identity is recorded under outputs below.
            continue
        p = root / {"qemu": "qemu-system-myemulator32", "kernel": "vmlinux", "rootfs": "rem-rootfs.ext4"}[key]
        if not p.is_file() or p.stat().st_size != item["size"] or sha(p) != item["sha256"]:
            failures.append(f"{key}: missing, wrong size, or SHA-256 mismatch")
        elif (p.stat().st_mode & 0o7777) != int(item["mode"], 8):
            failures.append(f"{key}: mode mismatch")
        if key == "qemu" and p.exists() and not (p.stat().st_mode & 0o111):
            failures.append("qemu: required execute permission is missing")
        if key in ("qemu", "kernel") and p.exists() and p.stat().st_size:
            out = subprocess.run(["readelf", "-h", str(p)], text=True, capture_output=True)
            actual = re.search(r"^\s*Machine:\s*(.*?)\s*$", out.stdout, re.M)
            if item["elf_machine"] and (not actual or actual.group(1) != item["elf_machine"]):
                failures.append(f"{key}: ELF architecture mismatch")
    image = root / "rem-rootfs.ext4"
    if image.exists():
        expected = m["outputs"]["rootfs"]
        if image.stat().st_size != expected["size"] or sha(image) != expected["sha256"]:
            failures.append("rootfs: candidate size or SHA-256 mismatch")
        if (image.stat().st_mode & 0o7777) != int(expected["mode"], 8):
            failures.append("rootfs: mode mismatch")
        ck = subprocess.run(["e2fsck", "-fn", str(image)], text=True, capture_output=True)
        if ck.returncode not in (0,):
            failures.append("rootfs: e2fsck failed")
        for item in m["guest_artifacts"]:
            with tempfile.TemporaryDirectory(prefix="rem-flight-verify-") as td:
                extracted = Path(td) / "artifact"
                p = subprocess.run(["debugfs", "-R", f"dump {item['path']} {extracted}", str(image)],
                                   text=True, capture_output=True)
                if p.returncode or not extracted.is_file():
                    failures.append(f"guest file missing: {item['path']}")
                    continue
                if extracted.stat().st_size != item["size"] or sha(extracted) != item["sha256"]:
                    failures.append(f"guest file size/hash mismatch: {item['path']}")
                stat = subprocess.run(["debugfs", "-R", f"stat {item['path']}", str(image)],
                                      text=True, capture_output=True).stdout
                mode = re.search(r"Mode:\s+([0-7]{3,6})", stat)
                actual_mode = int(mode.group(1), 8) & 0o7777 if mode else None
                if actual_mode != int(item["mode"], 8):
                    failures.append(f"guest mode mismatch: {item['path']}")
                if item.get("executable") and not actual_mode & 0o111:
                    failures.append(f"guest executable lost execute permission: {item['path']}")
                if item.get("elf_machine"):
                    eh = subprocess.run(["readelf", "-h", str(extracted)], text=True, capture_output=True)
                    em = re.search(r"^\s*Machine:\s*(.*?)\s*$", eh.stdout, re.M)
                    if not em or em.group(1) != item["elf_machine"]:
                        failures.append(f"guest architecture mismatch: {item['path']}")
            for dep_path, dep_hash in item.get("dependency_hashes", {}).items():
                guest = PurePosixPath(dep_path)
                if not guest.is_absolute() or ".." in guest.parts or '"' in dep_path:
                    failures.append(f"unsafe dependency path in manifest: {dep_path}")
                    continue
                with tempfile.TemporaryDirectory(prefix="rem-flight-dependency-") as td:
                    extracted_dep = Path(td) / "dependency"
                    dep_dump = subprocess.run(
                        ["debugfs", "-R", f"dump {dep_path} {extracted_dep}", str(image)],
                        text=True, capture_output=True)
                    if (dep_dump.returncode or not extracted_dep.is_file() or
                            sha(extracted_dep) != dep_hash):
                        failures.append(f"guest dependency missing or hash mismatch: {dep_path}")
        for item in m.get("removed_guest_files", []):
            guest = PurePosixPath(item.get("path", ""))
            if not guest.is_absolute() or ".." in guest.parts or '"' in str(guest):
                failures.append(f"unsafe removed guest path in manifest: {guest}")
                continue
            stat = subprocess.run(["debugfs", "-R", f"stat {guest}", str(image)],
                                  text=True, capture_output=True)
            if "Type:" in stat.stdout:
                failures.append(f"removed guest file is still present: {guest}")
        for guest_path, expected in m.get("guest_devices", {}).items():
            stat = subprocess.run(["debugfs", "-R", f"stat {guest_path}", str(image)],
                                  text=True, capture_output=True).stdout
            mode = re.search(r"Mode:\s+([0-7]{3,6})", stat)
            dev = re.search(r"Device major/minor number:\s*[0-9a-f]+:[0-9a-f]+\s*\(hex\s+([0-9a-f]+):([0-9a-f]+)\)",
                            stat, re.I)
            if not dev:
                dev = re.search(r"Device major/minor number:\s*([0-9a-f]+):([0-9a-f]+)", stat, re.I)
            if expected["type"] not in stat or not mode or int(mode.group(1), 8) & 0o7777 != int(expected["mode"], 8):
                failures.append(f"guest device type/mode mismatch: {guest_path}")
            if not dev or int(dev.group(1), 16) != expected["major"] or int(dev.group(2), 16) != expected["minor"]:
                failures.append(f"guest device number mismatch: {guest_path}")
    else:
        failures.append("rootfs: missing")
    if failures:
        raise SystemExit("VERIFICATION FAILED:\n" + "\n".join(f"- {x}" for x in failures))
    print(f"verified {m['tuple']}: QEMU, kernel, ext4, and {len(m['guest_artifacts'])} guest files")


if __name__ == "__main__":
    main()
