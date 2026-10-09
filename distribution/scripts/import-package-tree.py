#!/usr/bin/env python3
"""Import an install tree and write a hash-checked REM package manifest."""
import argparse
import hashlib
import json
from pathlib import Path, PurePosixPath
import re
import shutil
import subprocess


def sha(path: Path) -> str:
    h = hashlib.sha256()
    with path.open("rb") as stream:
        for block in iter(lambda: stream.read(1024 * 1024), b""):
            h.update(block)
    return h.hexdigest()


def elf_machine(path: Path):
    try:
        out = subprocess.check_output(["readelf", "-h", str(path)], text=True,
                                      stderr=subprocess.DEVNULL)
    except (OSError, subprocess.CalledProcessError):
        return None
    match = re.search(r"^\s*Machine:\s*(.*?)\s*$", out, re.M)
    return match.group(1) if match else None


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--package", required=True, type=Path)
    parser.add_argument("--source-root", required=True, type=Path)
    parser.add_argument("--name", required=True)
    parser.add_argument("--version", required=True)
    parser.add_argument("--provenance", required=True)
    parser.add_argument("--origin-label", default=None,
                        help="stable source label; avoids machine-local source paths")
    parser.add_argument("--dependency", action="append", default=[])
    parser.add_argument("--file", action="append", default=[], metavar="GUEST=SOURCE")
    args = parser.parse_args()
    package = args.package.resolve()
    source_root = args.source_root.resolve(strict=True)
    payload = package / "payload"
    payload.mkdir(parents=True, exist_ok=True)
    files = []
    seen = set()
    for spec in args.file:
        if "=" not in spec:
            parser.error(f"invalid --file mapping: {spec}")
        guest, source_rel = spec.split("=", 1)
        guest_path = PurePosixPath(guest)
        if not guest_path.is_absolute() or ".." in guest_path.parts:
            parser.error(f"guest path must be absolute and safe: {guest}")
        if guest in seen:
            parser.error(f"duplicate guest path: {guest}")
        seen.add(guest)
        source = (source_root / source_rel).resolve(strict=True)
        if not source.is_file():
            parser.error(f"package input is not a regular file: {source}")
        rel_payload = Path(*guest_path.parts[1:])
        destination = payload / rel_payload
        destination.parent.mkdir(parents=True, exist_ok=True)
        shutil.copyfile(source, destination)
        destination.chmod(0o755)
        machine = elf_machine(destination)
        if machine and machine != "<unknown>: 0xf2e2":
            parser.error(f"wrong REM ELF machine for {guest}: {machine}")
        files.append({
            "path": guest,
            "type": "file",
            "mode": "0755",
            "size": destination.stat().st_size,
            "sha256": sha(destination),
            "source": "payload/" + rel_payload.as_posix(),
            "origin": args.origin_label or str(source),
            "provenance": args.provenance,
            "elf_machine": machine,
        })
    manifest = {
        "name": args.name,
        "version": args.version,
        "architecture": "myemulator32",
        "dependencies": sorted(set(args.dependency)),
        "provenance": args.provenance,
        "files": sorted(files, key=lambda entry: entry["path"]),
    }
    manifest_path = package / "manifest.json"
    if manifest_path.is_file():
        previous = json.loads(manifest_path.read_text())
        if (previous.get("name"), previous.get("version"), previous.get("architecture")) != (
                manifest["name"], manifest["version"], manifest["architecture"]):
            parser.error("existing package identity differs; refusing to replace manifest")
        merged = {entry["path"]: entry for entry in previous.get("files", [])}
        merged.update({entry["path"]: entry for entry in manifest["files"]})
        manifest["files"] = sorted(merged.values(), key=lambda entry: entry["path"])
    manifest_path.write_text(
        json.dumps(manifest, indent=2, sort_keys=True) + "\n", encoding="utf-8")


if __name__ == "__main__":
    main()
