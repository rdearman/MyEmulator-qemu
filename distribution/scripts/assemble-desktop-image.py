#!/usr/bin/env python3
"""Assemble the pinned desktop seed plus shared REM packages into an ext4 image."""
import hashlib
import json
import os
from pathlib import Path
import shutil
import stat
import subprocess
import sys

ROOT = Path(__file__).resolve().parents[2]
SEED = ROOT / "REM-FLIGHT-HUMAN-UAT-FULL/rootfs.ext4"
SEED_SHA = "9e3f2c25f28205fab79722b9c546399f28cb30e065fc583d1612d6521126f20e"
BB_HASHES = {
    "9e7acc1db28a5506215293e3d0af19ab734bcb527d5145b45372c2963ba9e31d",
    "1d8ae8c351cedffdea2473c3d8108725be39c4f6899751d92af931044ad4d1b5",
}

def sha(path):
    h = hashlib.sha256()
    with path.open("rb") as stream:
        for block in iter(lambda: stream.read(1024 * 1024), b""):
            h.update(block)
    return h.hexdigest()

def run(*args, **kwargs):
    print("REM_ASSEMBLE|RUN|" + " ".join(map(str, args)), flush=True)
    return subprocess.run(args, check=True, **kwargs)

def copy_package(stage, package_dir):
    manifest_path = package_dir / "manifest.json"
    manifest = json.loads(manifest_path.read_text())
    if manifest.get("status") == "pending":
        print(f"REM_ASSEMBLE|PACKAGE_PENDING|{manifest['name']}", flush=True)
        return
    if manifest.get("status") == "source-build":
        return
    if manifest.get("status") == "seed-provided":
        for entry in manifest.get("files", []):
            target = stage / entry["path"].lstrip("/")
            if not target.is_file() or sha(target) != entry["sha256"]:
                raise SystemExit(f"pinned seed package file missing/hash mismatch: {entry['path']}")
        return
    for entry in manifest.get("files", []):
        rel = Path(entry["source"])
        source = package_dir / rel
        guest = Path(entry["path"].lstrip("/"))
        if not source.is_file() or sha(source) != entry["sha256"]:
            raise SystemExit(f"package payload missing/hash mismatch: {source}")
        target = stage / guest
        target.parent.mkdir(parents=True, exist_ok=True)
        if target.is_symlink():
            target.unlink()
        shutil.copyfile(source, target)
        target.chmod(int(entry.get("mode", "0755"), 8))

def remove_busybox(stage):
    for path in sorted(stage.rglob("*"), reverse=True):
        if path.is_symlink():
            if "busybox" in os.readlink(path).lower():
                path.unlink()
            continue
        if not path.is_file():
            continue
        if "busybox" in path.name.lower() or sha(path) in BB_HASHES:
            path.unlink()

def set_link(path, target):
    path.parent.mkdir(parents=True, exist_ok=True)
    if path.is_symlink() or path.is_file():
        path.unlink()
    elif path.exists():
        shutil.rmtree(path)
    path.symlink_to(target)

def rootfs_records(stage):
    result = []
    for path in sorted(stage.rglob("*")):
        rel = "/" + path.relative_to(stage).as_posix()
        st = path.lstat()
        record = {"path": rel, "mode": format(stat.S_IMODE(st.st_mode), "04o"),
                  "uid": st.st_uid, "gid": st.st_gid}
        if rel == "/home/dev" or rel.startswith("/home/dev/"):
            record["uid"], record["gid"] = 1000, 100
        if path.is_symlink():
            record.update(type="symlink", target=os.readlink(path))
        elif path.is_file():
            record.update(type="file", size=st.st_size, sha256=sha(path))
        elif path.is_dir():
            record["type"] = "directory"
        else:
            record["type"] = "special"
        result.append(record)
    return result

def main():
    if len(sys.argv) != 3:
        raise SystemExit("usage: assemble-desktop-image.py OUTPUT_DIR SIZE_MIB")
    out = Path(sys.argv[1]).resolve()
    if not str(out).startswith("/tmp/"):
        raise SystemExit("output must be under /tmp")
    size_mib = int(sys.argv[2])
    if size_mib < 768:
        raise SystemExit("desktop rootfs image must be at least 768 MiB")
    if not SEED.is_file() or sha(SEED) != SEED_SHA:
        raise SystemExit(f"pinned read-only seed missing or hash mismatch: {SEED}")
    if out.exists() and any(out.iterdir()):
        raise SystemExit(f"refusing to overwrite non-empty output directory: {out}")
    out.mkdir(parents=True, exist_ok=True)
    stage = out / "rootfs-stage"
    stage.mkdir(exist_ok=True)
    log = out / "seed-extract.log"
    with log.open("w") as stream:
        run("debugfs", "-R", f"rdump / {stage}", str(SEED), stdout=stream,
            stderr=subprocess.STDOUT)
    if not (stage / "bin/busybox").is_file():
        raise SystemExit("seed extraction did not produce expected tree")

    print("REM_ASSEMBLE|SEED|PASS|read-only pinned image", flush=True)
    # Remove aliases before overlaying package files: copyfile over an existing
    # symlink would otherwise overwrite its BusyBox target instead of the path.
    remove_busybox(stage)
    for path in (stage / "usr/include").rglob("*"):
        if not path.is_symlink():
            continue
        link = Path(os.readlink(path))
        resolved = (stage / link.as_posix().lstrip("/")) if link.is_absolute() else path.parent / link
        if (not resolved.resolve(strict=False).is_relative_to(stage) or
                not resolved.exists()):
            path.unlink()
    packages = sorted((ROOT / "distribution/packages").glob("*/manifest.json"))
    for manifest in packages:
        package_dir = manifest.parent
        copy_package(stage, package_dir)
    system = out / "build/rem-system"
    network = out / "build/rem-network"
    run("sh", str(ROOT / "distribution/scripts/build-system-package.sh"),
        str(system), str(stage / "usr"))
    run("sh", str(ROOT / "distribution/scripts/build-network-package.sh"),
        str(network), str(stage / "usr"))
    shutil.copytree(system, stage, dirs_exist_ok=True, symlinks=True)
    shutil.copytree(network, stage, dirs_exist_ok=True, symlinks=True)

    def built_manifest(name, output, guest_prefix, version):
        files = []
        for path in sorted(output.rglob("*")):
            if not path.is_file() or path.is_symlink():
                continue
            rel = path.relative_to(output)
            files.append({"path": "/" + (Path(guest_prefix) / rel).as_posix(),
                          "type": "file", "mode": format(stat.S_IMODE(path.stat().st_mode), "04o"),
                          "size": path.stat().st_size, "sha256": sha(path),
                          "source": str(Path("build") / name / rel),
                          "provenance": "Generated from the distribution source and pinned seed sysroot."})
        return {"name": name, "version": version, "architecture": "myemulator32",
                "dependencies": [], "status": "installed",
                "seed_sha256": SEED_SHA,
                "compiler_sha256": sha(ROOT / ".toolchain-install/bin/myemulator2-elf-gcc"),
                "files": files}

    generated_packages = {
        "rem-system": built_manifest("rem-system", system, "", "0.2-source1"),
        "rem-network": built_manifest("rem-network", network, "", "0.2-source1"),
    }

    config = ROOT / "distribution/config"
    for name in ("hostname", "inittab", "passwd", "group", "shadow", "resolv.conf"):
        target = stage / "etc" / name
        target.parent.mkdir(parents=True, exist_ok=True)
        shutil.copyfile(config / name, target)
        target.chmod(0o644)
    initd = stage / "etc/init.d"
    if initd.exists():
        shutil.rmtree(initd)
    initd.mkdir(parents=True)
    for name, destination in (("rc", "rc"), ("rcS", "rcS"),
                              ("local", "local"), ("rc.local", "rc.local"),
                              ("S40network", "network")):
        target = initd / destination
        shutil.copyfile(config / name, target)
        target.chmod(0o755)
    for runlevel, prefix, service in ((3, "S40", "network"),
                                      (3, "S99", "local"),
                                      (0, "K40", "network"),
                                      (0, "K00", "local"),
                                      (6, "K40", "network"),
                                      (6, "K00", "local")):
        directory = stage / f"etc/rc{runlevel}.d"
        directory.mkdir(parents=True, exist_ok=True)
        name = prefix + service
        link = directory / name
        if link.exists() or link.is_symlink():
            link.unlink()
        link.symlink_to("../init.d/" + ("network" if service == "network" else "local"))
    for legacy in ("S99local", "rcS.local"):
        candidate = initd / legacy
        if candidate.exists() or candidate.is_symlink():
            candidate.unlink()
    (stage / "home/dev").mkdir(parents=True, exist_ok=True)
    (stage / "root").mkdir(parents=True, exist_ok=True)
    (stage / "tmp").mkdir(parents=True, exist_ok=True)
    (stage / "var/run").mkdir(parents=True, exist_ok=True)
    (stage / "var/log").mkdir(parents=True, exist_ok=True)
    (stage / "dev").mkdir(parents=True, exist_ok=True)
    (stage / "dev/pts").mkdir(parents=True, exist_ok=True)
    # Replace the seed's /dev/tty -> console alias with an explicit tty node.
    old_tty = stage / "dev/tty"
    if old_tty.is_symlink() or old_tty.is_file():
        old_tty.unlink()

    set_link(stage / "bin/sh", "../usr/bin/bash")
    set_link(stage / "bin/bash", "../usr/bin/bash")
    set_link(stage / "bin/vi", "../usr/bin/vim")
    set_link(stage / "bin/mount", "../usr/bin/mount")
    set_link(stage / "bin/umount", "../usr/bin/umount")
    aliases = ("cat", "chgrp", "chmod", "chown", "cp", "cut", "date", "df",
               "du", "echo", "env", "false", "find", "grep", "groups", "gzip",
               "head", "id", "ln", "ls", "mkdir", "mv", "od", "pwd", "printf",
               "rm", "rmdir", "sed", "sha256sum", "sleep", "sort", "stty", "sync",
               "tail", "tar", "tee", "test", "touch", "tr", "true", "uname",
               "uniq", "wc", "whoami", "xargs", "bzip2", "bunzip2", "xz", "unxz",
               "diff", "cmp", "vim", "chibicc", "samu", "curl", "as", "ld", "ar",
               "ranlib", "readelf", "nm", "objdump", "objcopy", "strip", "make")
    for name in aliases:
        if (stage / "usr/bin" / name).is_file():
            set_link(stage / "bin" / name, "../usr/bin/" + name)
    if (stage / "usr/bin/gawk").is_file():
        set_link(stage / "usr/bin/awk", "gawk")
    for name in ("mount", "umount"):
        if not (stage / f"usr/bin/{name}").exists():
            set_link(stage / f"usr/bin/{name}", "rem-mount")
    for path in (stage / "bin/busybox", stage / "usr/bin/busybox"):
        if path.exists() or path.is_symlink():
            path.unlink()

    run(sys.executable, str(ROOT / "distribution/scripts/audit-busybox.py"), str(stage))
    os.chmod(stage / "tmp", 0o1777)
    os.chmod(stage / "root", 0o700)
    os.chmod(stage / "etc/shadow", 0o600)
    for path in (stage / "sbin/poweroff", stage / "sbin/reboot", stage / "sbin/halt"):
        if path.exists():
            os.chmod(path, 0o4755)
    package_db = stage / "var/lib/rempkg/installed"
    package_db.mkdir(parents=True, exist_ok=True)
    for manifest in packages:
        data = json.loads(manifest.read_text())
        installed = generated_packages.get(data.get("name"))
        if installed is None:
            target = package_db / (data.get("name", manifest.stem) + ".json")
            shutil.copyfile(manifest, target)
        else:
            target = package_db / (data["name"] + ".json")
            target.write_text(json.dumps(installed, indent=2, sort_keys=True) + "\n")
        target.chmod(0o644)
    for name, data in generated_packages.items():
        output = out / "manifests/packages"
        output.mkdir(parents=True, exist_ok=True)
        (output / f"{name}.json").write_text(json.dumps(data, indent=2, sort_keys=True) + "\n")
    source_epoch = 1791504000
    for path in sorted(stage.rglob("*"), key=lambda item: len(item.parts), reverse=True):
        try:
            os.utime(path, (source_epoch, source_epoch), follow_symlinks=False)
        except (NotImplementedError, OSError):
            pass
    os.utime(stage, (source_epoch, source_epoch))
    records = rootfs_records(stage)
    for name, mode, major, minor in (("console", "0600", 5, 1), ("null", "0666", 1, 3),
                                    ("tty", "0666", 5, 0), ("ttyMY0", "0620", 240, 0),
                                    ("random", "0666", 1, 8), ("urandom", "0666", 1, 9)):
        records.append({"path": "/dev/" + name, "type": "character-device",
                        "mode": mode, "uid": 0, "gid": 0,
                        "major": major, "minor": minor})
    (out / "manifests").mkdir(exist_ok=True)
    (out / "manifests/rootfs-files.json").write_text(json.dumps({
        "name": "rem-linux-desktop-reference-rootfs", "version": "0.2",
        "architecture": "myemulator32", "seed_sha256": SEED_SHA,
        "files": records,
    }, indent=2, sort_keys=True) + "\n")
    image = out / "rootfs.ext4"
    env = os.environ.copy()
    env["E2FSPROGS_FAKE_TIME"] = "1791504000"
    env["SOURCE_DATE_EPOCH"] = "1791504000"
    run("mkfs.ext4", "-F", "-q", "-b", "4096", "-U",
        "b0b00200-2026-4000-8000-000000000002", "-E",
        "lazy_itable_init=0,lazy_journal_init=0,hash_seed=b0b00200-2026-4000-8000-000000000002",
        "-d", str(stage),
        str(image), f"{size_mib}M", env=env)
    debugfs_commands = out / "manifests/ext4-metadata.commands"
    metadata_commands = [
        f"set_inode_field / atime @{source_epoch}",
        f"set_inode_field / ctime @{source_epoch}",
        f"set_inode_field / crtime @{source_epoch}",
        "set_inode_field /home/dev uid 1000", "set_inode_field /home/dev gid 100",
        "set_inode_field /home/dev mode 040755",
    ]
    for path in sorted(stage.rglob("*")):
        guest = "/" + path.relative_to(stage).as_posix()
        metadata_commands.extend((f"set_inode_field {guest} atime @{source_epoch}",
                                  f"set_inode_field {guest} ctime @{source_epoch}",
                                  f"set_inode_field {guest} crtime @{source_epoch}"))
    for path in sorted((stage / "home/dev").rglob("*")):
        relative = "/home/dev/" + path.relative_to(stage / "home/dev").as_posix()
        metadata_commands.extend((f"set_inode_field {relative} uid 1000",
                                  f"set_inode_field {relative} gid 100"))
    metadata_commands.extend((
        "cd /dev",
        "mknod console c 5 1", "set_inode_field console mode 020600",
        "mknod null c 1 3", "set_inode_field null mode 020666",
        "mknod tty c 5 0", "set_inode_field tty mode 020666",
        "mknod ttyMY0 c 240 0", "set_inode_field ttyMY0 mode 020620",
        "mknod random c 1 8", "set_inode_field random mode 020666",
        "mknod urandom c 1 9", "set_inode_field urandom mode 020666",
    ))
    for name in ("console", "null", "tty", "ttyMY0", "random", "urandom"):
        metadata_commands.extend((f"set_inode_field {name} atime @{source_epoch}",
                                  f"set_inode_field {name} ctime @{source_epoch}",
                                  f"set_inode_field {name} crtime @{source_epoch}"))
    metadata_commands.append("quit")
    debugfs_commands.write_text("\n".join(metadata_commands) + "\n")
    run("debugfs", "-w", "-f", str(debugfs_commands), str(image), env=env)
    run(sys.executable, str(ROOT / "distribution/scripts/verify-image.py"),
        str(stage), str(image), str(out / "image-readback"))
    print(f"REM_ASSEMBLE|IMAGE|{image}|{image.stat().st_size}", flush=True)

if __name__ == "__main__":
    main()
