#!/usr/bin/env python3
"""Read an ext4 image back with debugfs and compare its payload to staging."""
import argparse
import hashlib
from pathlib import Path
import subprocess
import sys

ROOT = Path(__file__).resolve().parents[2]

def sha(path):
    h = hashlib.sha256()
    with path.open("rb") as stream:
        for block in iter(lambda: stream.read(1024 * 1024), b""):
            h.update(block)
    return h.hexdigest()

def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("stage", type=Path)
    parser.add_argument("image", type=Path)
    parser.add_argument("readback", type=Path)
    args = parser.parse_args()
    fsck = subprocess.run(["e2fsck", "-fn", str(args.image)], stdout=subprocess.PIPE,
                          stderr=subprocess.STDOUT, text=True)
    if fsck.returncode:
        print(fsck.stdout, file=sys.stderr)
        raise SystemExit("ext4 candidate is not clean")
    args.readback.mkdir(parents=True, exist_ok=False)
    with (args.readback.parent / "readback.log").open("w") as log:
        subprocess.run(["debugfs", "-R", f"rdump / {args.readback}", str(args.image)],
                       stdout=log, stderr=subprocess.STDOUT, check=True)
    for source in args.stage.rglob("*"):
        rel = source.relative_to(args.stage)
        target = args.readback / rel
        if source.is_symlink():
            if not target.is_symlink() or source.readlink() != target.readlink():
                raise SystemExit(f"ext4 symlink mismatch: /{rel}")
        elif source.is_dir():
            if not target.is_dir():
                raise SystemExit(f"ext4 directory missing: /{rel}")
        elif source.is_file():
            if not target.is_file() or source.stat().st_size != target.stat().st_size or sha(source) != sha(target):
                raise SystemExit(f"ext4 file readback mismatch: /{rel}")
    audit = subprocess.run([sys.executable, str(ROOT / "distribution/scripts/audit-busybox.py"),
                            str(args.readback)], text=True)
    if audit.returncode:
        raise SystemExit("post-ext4 BusyBox audit failed")
    print("REM_IMAGE_VERIFY|EXT4_CLEAN|PASS")
    print("REM_IMAGE_VERIFY|PAYLOAD_READBACK|PASS")

if __name__ == "__main__":
    main()
