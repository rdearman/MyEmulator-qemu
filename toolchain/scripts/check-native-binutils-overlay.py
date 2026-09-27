#!/usr/bin/env python3
"""Check hashes, guest paths, and packaged contents of the REM binutils overlay."""
from __future__ import annotations

import hashlib
import stat
import sys
import tarfile
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
OVERLAY = ROOT / "toolchain/native-binutils-flight-overlay"
ARCHIVE = ROOT / "toolchain/native-binutils-flight-overlay.tar"
EXPECTED = {
    "as": "c56f6b236f15200426c9fe5a81caa8251df71b401c0bddd93827c1a4f68e7d93",
    "ld": "edd83609045df123c8a261cf615f70de1bd3e249082a9fe39f392ef0f6ee3b65",
    "ar": "31585d2cfa5dcdf702e4655b2ac386b7580641aaa378d2b39bde106bb4543ba7",
    "ranlib": "b2f8ac9eba8000c34c1f609d1955bc8e3495532b3bf0739ad714554e36d00bd3",
    "objdump": "57f7062b4f14ea3edd6aa2f74f7d6ffab21dcce8cadcdc0d7730c9b05140fa67",
    "readelf": "778d9e788068b2d8b78e85bfa2cf99c1c51f78873c20939a2a8bd91eb01b2eaa",
    "nm": "da0c0a6ecc28f79aaf6086778f1fe4083b780762fa134b99e5f72d04858d7d71",
    "objcopy": "99e2f48012f84f36470cd97aa9b267563b942750687b9b162a55238e3d80637c",
    "strip": "7f7648eb96acd09584d336f3c751a8a9dc17a33f26db37edd8ac1e024d8fced9",
    "size": "8f39adfc4aa7762e0a14eb6c63a7530cebcfdb47580f36d4bf6783f534b62690",
    "strings": "c0de7c8e36a7f4e0db77f5047371d80bab4a76f4b376a0e866a69e2f150358da",
}
ALIASES = ("usr/bin", "usr/myemulator2-linux-musl/bin")
TOOLS = (*EXPECTED, "ld.bfd")

def check(condition: bool, message: str) -> None:
    if not condition:
        raise SystemExit(f"FAIL {message}")
    print(f"PASS {message}")

def sha256(path: Path) -> str:
    return hashlib.sha256(path.read_bytes()).hexdigest()

for name, expected in EXPECTED.items():
    path = OVERLAY / "root/native/bin" / name
    data = path.read_bytes()
    mode = path.stat().st_mode
    check(sha256(path) == expected, f"{name} hash")
    check(data[:5] == b"\x7fELF\x01" and data[5] == 1,
          f"{name} is ELF32 little-endian")
    check(mode & (stat.S_IXUSR | stat.S_IXGRP | stat.S_IXOTH),
          f"{name} executable mode")

check((OVERLAY / "root/native/bin/ld.bfd").is_symlink()
      and (OVERLAY / "root/native/bin/ld.bfd").readlink().as_posix()
      == "/root/native/bin/ld", "canonical ld.bfd alias")
for directory in ALIASES:
    for name in TOOLS:
        alias = OVERLAY / directory / name
        check(alias.is_symlink() and alias.readlink().as_posix()
              == f"/root/native/bin/{name}", f"{directory}/{name} canonical alias")

with tarfile.open(ARCHIVE, "r") as archive:
    members = {item.name: item for item in archive.getmembers()}
    expected_paths = {p.relative_to(OVERLAY).as_posix() for p in OVERLAY.rglob("*")}
    check(set(members) == expected_paths, "archive contains exactly the overlay tree")
    for name, digest in EXPECTED.items():
        member = members[f"root/native/bin/{name}"]
        check(hashlib.sha256(archive.extractfile(member).read()).hexdigest() == digest,
              f"packaged {name} hash")
    for directory in ALIASES:
        for name in TOOLS:
            member = members[f"{directory}/{name}"]
            check(member.issym() and member.linkname == f"/root/native/bin/{name}",
                  f"packaged {directory}/{name} symlink")

print("PASS REM native binutils overlay is integration-ready")
