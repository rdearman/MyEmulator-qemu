#!/usr/bin/env python3
"""Inventory executable files and links in a REM userspace archive/tree."""

import argparse
import json
import os
import stat
import tarfile
from pathlib import Path


def archive_inventory(path):
    records = []
    with tarfile.open(path, "r:*") as archive:
        for member in archive.getmembers():
            guest = "/" + member.name.lstrip("./")
            if member.issym():
                records.append({"path": guest, "kind": "symlink", "target": member.linkname})
            elif member.isfile() and member.mode & 0o111:
                records.append({"path": guest, "kind": "executable", "mode": member.mode & 0o7777})
    return records


def tree_inventory(path):
    records = []
    root = Path(path)
    for item in root.rglob("*"):
        guest = "/" + item.relative_to(root).as_posix()
        if item.is_symlink():
            records.append({"path": guest, "kind": "symlink", "target": os.readlink(item)})
        elif item.is_file() and item.stat().st_mode & 0o111:
            records.append({"path": guest, "kind": "executable", "mode": stat.S_IMODE(item.stat().st_mode)})
    return records


def main():
    parser = argparse.ArgumentParser()
    source = parser.add_mutually_exclusive_group(required=True)
    source.add_argument("--archive", type=Path)
    source.add_argument("--tree", type=Path)
    parser.add_argument("--output", type=Path)
    args = parser.parse_args()
    records = archive_inventory(args.archive) if args.archive else tree_inventory(args.tree)
    records.sort(key=lambda item: item["path"])
    by_target = {}
    for item in records:
        target = item.get("target")
        if target:
            by_target.setdefault(target, []).append(item["path"])
    result = {
        "source": str(args.archive or args.tree),
        "executables": [item for item in records if item["kind"] == "executable"],
        "symlinks": [item for item in records if item["kind"] == "symlink"],
        "symlink_groups": [{"target": target, "paths": paths} for target, paths in sorted(by_target.items())],
        "counts": {
            "records": len(records),
            "executables": sum(item["kind"] == "executable" for item in records),
            "symlinks": sum(item["kind"] == "symlink" for item in records),
        },
    }
    encoded = json.dumps(result, indent=2, sort_keys=True) + "\n"
    if args.output:
        args.output.write_text(encoded)
    else:
        print(encoded, end="")


if __name__ == "__main__":
    main()
