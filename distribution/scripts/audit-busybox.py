#!/usr/bin/env python3
"""Fail if a staged REM root contains BusyBox, aliases, or boot references."""
import hashlib
import os
from pathlib import Path
import sys

ROOT = Path(sys.argv[1]).resolve(strict=True)
KNOWN = {
    "9e7acc1db28a5506215293e3d0af19ab734bcb527d5145b45372c2963ba9e31d",
    "1d8ae8c351cedffdea2473c3d8108725be39c4f6899751d92af931044ad4d1b5",
}
issues = []

def digest(path):
    h = hashlib.sha256()
    with path.open("rb") as stream:
        for block in iter(lambda: stream.read(1024 * 1024), b""):
            h.update(block)
    return h.hexdigest()

for path in ROOT.rglob("*"):
    relative = "/" + path.relative_to(ROOT).as_posix()
    if path.is_symlink():
        try:
            target = (path.parent / os.readlink(path)).resolve(strict=False)
            if target == ROOT / "bin/busybox" or "busybox" in os.readlink(path).lower():
                issues.append(f"BusyBox symlink: {relative} -> {os.readlink(path)}")
        except OSError as error:
            issues.append(f"unreadable symlink {relative}: {error}")
        continue
    if not path.is_file():
        continue
    if "busybox" in path.name.lower() or digest(path) in KNOWN:
        issues.append(f"BusyBox file: {relative}")
    if relative.startswith(("/etc/init.d/", "/etc/rc")) and path.suffix in (".sh", ""):
        try:
            data = path.read_text(errors="replace").lower()
        except OSError:
            continue
        if "busybox" in data or "/bin/busybox" in data:
            issues.append(f"BusyBox boot reference: {relative}")

if issues:
    print("REM_BUSYBOX_AUDIT|FAIL")
    for issue in issues:
        print(issue, file=sys.stderr)
    raise SystemExit(1)
print("REM_BUSYBOX_AUDIT|PASS|physical-files=0|aliases=0|boot-references=0")
