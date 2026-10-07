#!/usr/bin/env python3
"""Host-only source archive creation and read-back verification."""
import hashlib
import os
from pathlib import Path
import sys
import tarfile

root = Path(os.environ.get("REM_NATIVE_WORK", "/tmp/rem-native-userland-work"))
kit = root / "kit"
output = Path(sys.argv[1])
excluded = {".git", "native", "native-build", "native-deps", "native-tests"}
top = {
    "bootstrap", "packages", "patches", "tests", "docs", "certificates",
    "README-REM.txt", "build-all-native.sh", "install-native.sh",
    "rollback-native.sh", "acceptance-native.sh", "native-env.sh",
}
members = []
for name in sorted(top):
    path = kit / name
    if not path.exists():
        raise SystemExit(f"Missing source kit component: {path}")
    members.append(path)
    if path.is_dir():
        for directory, directories, files in os.walk(path):
            directories[:] = sorted(d for d in directories if d not in excluded)
            members.extend(Path(directory) / d for d in directories)
            members.extend(Path(directory) / f for f in sorted(files))
if output.exists():
    raise SystemExit(f"Refusing to replace preserved archive: {output}")
with tarfile.open(output, "w", format=tarfile.USTAR_FORMAT) as archive:
    for path in members:
        if path.is_file():
            with path.open("rb") as source:
                if source.read(4) == b"\x7fELF":
                    raise SystemExit(f"Generated ELF is not source: {path}")
        archive.add(path, arcname="rem-native-userland/" + str(path.relative_to(kit)),
                    recursive=False)
with tarfile.open(output, "r:") as archive:
    names = set()
    for member in archive:
        names.add(member.name)
        path = Path(member.name)
        if path.is_absolute() or ".." in path.parts:
            raise SystemExit(f"Unsafe archive path: {member.name}")
        if member.issym():
            target = Path(member.linkname)
            if target.is_absolute() or ".." in target.parts:
                raise SystemExit(f"Unsafe archive symlink: {member.name}")
        if member.isfile():
            source = archive.extractfile(member)
            if source.read(4) == b"\x7fELF":
                raise SystemExit(f"ELF in archive: {member.name}")
    for script in sorted(kit.glob("packages/*/build-native.sh")):
        name = "rem-native-userland/" + str(script.relative_to(kit))
        if name not in names:
            raise SystemExit(f"Missing package builder: {name}")
    for script in ("build-all-native.sh", "install-native.sh", "rollback-native.sh"):
        if not archive.getmember("rem-native-userland/" + script).mode & 0o111:
            raise SystemExit(f"Nonexecutable script: {script}")
digest = hashlib.sha256()
with output.open("rb") as source:
    for block in iter(lambda: source.read(1024 * 1024), b""):
        digest.update(block)
checksum = digest.hexdigest()
output.with_suffix(output.suffix + ".sha256").write_text(
    f"{checksum}  {output.name}\n")
print(f"{output}\nSIZE={output.stat().st_size}\nSHA256={checksum}\nFILES={len(names)}")
