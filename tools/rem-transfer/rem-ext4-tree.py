#!/usr/bin/env python3
"""Copy a source tree into/out of a stopped REM ext4 image using debugfs."""
from __future__ import annotations

import argparse
import os
from pathlib import Path
import re
import shlex
import stat
import subprocess
import tempfile


def run(args: list[str], *, capture: bool = False) -> subprocess.CompletedProcess[str]:
    return subprocess.run(args, text=True, check=False,
                          stdout=subprocess.PIPE if capture else None,
                          stderr=subprocess.STDOUT if capture else None)


def fail(message: str) -> None:
    raise SystemExit(f"rem-ext4-tree: {message}")


def q(value: str) -> str:
    # debugfs accepts whitespace in double-quoted tokens but has no reliable
    # escape for literal quotes or backslashes in a filename.
    if any(ch in value for ch in ('"', "\\", "\n", "\r")):
        fail(f"debugfs cannot safely represent quote/backslash/newline in path: {value!r}")
    return '"' + value + '"'


def guest_path(value: str) -> str:
    if not value.startswith("/") or "\x00" in value:
        fail(f"guest path must be absolute: {value}")
    parts = value.split("/")
    if any(part in (".", "..") for part in parts):
        fail(f"guest path may not contain dot or parent components: {value}")
    if any(not re.fullmatch(r"[A-Za-z0-9._+ -]+", part) for part in parts if part):
        fail(f"guest path contains unsupported characters: {value}")
    return "/".join(parts) or "/"


def image_offline(image: Path) -> None:
    image_stat = image.stat()
    for proc in Path("/proc").glob("[0-9]*"):
        try:
            comm = (proc / "comm").read_text().strip()
            if not comm.startswith("qemu-system-"):
                continue
            command = (proc / "cmdline").read_bytes().replace(b"\0", b" ").decode(errors="replace")
        except (OSError, UnicodeError):
            continue
        if str(image) in command:
            fail(f"QEMU PID {proc.name} has this image open: {image}")
        try:
            descriptors = (proc / "fd").iterdir()
            for descriptor in descriptors:
                try:
                    opened = descriptor.stat()
                except OSError:
                    continue
                if (opened.st_dev, opened.st_ino) == (image_stat.st_dev, image_stat.st_ino):
                    fail(f"QEMU PID {proc.name} has this image open through FD {descriptor.name}: {image}")
        except OSError:
            continue


def clean_ext4(image: Path) -> None:
    check = run(["e2fsck", "-fn", str(image)], capture=True)
    if check.returncode != 0:
        print(check.stdout or "", end="")
        fail("ext4 image is not clean; recover and verify it before transfer")


def debugfs(image: Path, commands: list[str] | str, *, write: bool = False) -> str:
    if isinstance(commands, str):
        result = run(["debugfs", "-w" if write else "-R", commands, str(image)], capture=True)
    else:
        with tempfile.NamedTemporaryFile("w", encoding="utf-8", prefix="rem-debugfs-") as stream:
            stream.write("\n".join(commands) + "\n")
            stream.flush()
            result = run(["debugfs", "-w", "-f", stream.name, str(image)], capture=True)
    output = result.stdout or ""
    if result.returncode != 0 or any(x in output.lower() for x in (
        "file not found", "command not found", "could not allocate", "invalid",
        "short read", "permission denied", "no such file or directory",
        "couldn't parse")):
        fail(f"debugfs operation failed:\n{output}")
    return output


def exists(image: Path, path: str) -> bool:
    result = run(["debugfs", "-R", f"stat {q(path)}", str(image)], capture=True)
    output = result.stdout or ""
    if "file not found by ext2_lookup" in output.lower():
        return False
    if result.returncode != 0:
        fail(f"could not inspect guest path {path}:\n{output}")
    return "Inode:" in output and "Type:" in output


def is_directory(image: Path, path: str) -> bool:
    if not exists(image, path):
        return False
    result = run(["debugfs", "-R", f"stat {q(path)}", str(image)], capture=True)
    return "type: directory" in (result.stdout or "").lower()


def inode_time_ns(image: Path, path: str, field: str) -> int:
    output = debugfs(image, f"stat {q(path)}")
    match = re.search(rf"^\s*{field}:\s*0x([0-9a-f]+):([0-9a-f]+)", output, re.M | re.I)
    if not match:
        raise ValueError(f"no {field} in debugfs stat for {path}")
    low, extra = (int(part, 16) for part in match.groups())
    seconds = ((extra >> 30) << 32) | low
    nanoseconds = extra & 0x3FFFFFFF
    return seconds * 1_000_000_000 + nanoseconds


def image_arg(value: str) -> Path:
    path = Path(value).resolve(strict=True)
    if not path.is_file():
        fail(f"not a regular image file: {path}")
    image_offline(path)
    return path


def export_tree(image: Path, source: str, destination_parent: Path) -> None:
    source = guest_path(source)
    if not is_directory(image, source):
        fail(f"guest directory does not exist: {source}")
    destination_parent = destination_parent.resolve()
    target = destination_parent / Path(source).name
    if target.exists():
        fail(f"export destination already exists: {target}")
    destination_parent.mkdir(parents=True, exist_ok=True)
    clean_ext4(image)
    result = run(["debugfs", "-R",
                  f"rdump {q(source)} {q(str(destination_parent))}", str(image)],
                 capture=True)
    output = result.stdout or ""
    # Root-owned guest files cannot be chowned by an unprivileged desktop or
    # Android app UID. debugfs still copies bytes, mode bits, and mtimes; owner
    # IDs are outside this source-backup workflow's preservation contract.
    other_errors = [line for line in output.splitlines()
                    if "invalid argument while changing ownership" not in line.lower()
                    and ("error" in line.lower() or "failed" in line.lower())]
    if result.returncode != 0 or other_errors:
        fail(f"debugfs tree export failed:\n{output}")
    if not target.is_dir():
        fail(f"debugfs did not create expected directory: {target}")
    symlink_mtimes_unavailable = 0
    for current, dirnames, filenames in os.walk(target, followlinks=False):
        for name in dirnames + filenames:
            exported = Path(current) / name
            if not exported.is_symlink():
                continue
            relative = exported.relative_to(target).as_posix()
            guest_link = source.rstrip("/") + "/" + relative
            try:
                atime_ns = inode_time_ns(image, guest_link, "atime")
                mtime_ns = inode_time_ns(image, guest_link, "mtime")
                os.utime(exported, ns=(atime_ns, mtime_ns), follow_symlinks=False)
            except (OSError, SystemExit, ValueError):
                # Export itself is still useful; warn if debugfs cannot address
                # a rare quoted/backslash name or host symlink times are denied.
                symlink_mtimes_unavailable += 1
    suffix = f" ({symlink_mtimes_unavailable} symlink mtime(s) unavailable)" if symlink_mtimes_unavailable else ""
    print(f"exported {source} -> {target}{suffix}")


def walk_tree(source: Path) -> tuple[list[Path], list[Path], list[Path]]:
    directories: list[Path] = [source]
    files: list[Path] = []
    links: list[Path] = []
    for current, dirnames, filenames in os.walk(source, topdown=True, followlinks=False):
        here = Path(current)
        retained: list[str] = []
        for name in sorted(dirnames):
            item = here / name
            if item.is_symlink():
                links.append(item)
            else:
                directories.append(item)
                retained.append(name)
        dirnames[:] = retained
        for name in sorted(filenames):
            item = here / name
            if item.is_symlink():
                links.append(item)
            elif item.is_file():
                files.append(item)
            else:
                fail(f"unsupported special file in source tree: {item}")
    return directories, files, links


def relative_guest(root: Path, item: Path, dest: str) -> str:
    rel = item.relative_to(root)
    return str(Path(dest) / rel).replace(os.sep, "/")


def inode_mode(path: Path) -> int:
    mode = path.lstat().st_mode
    return stat.S_IFMT(mode) | stat.S_IMODE(mode)


def import_tree(image: Path, source: Path, destination: str) -> None:
    source = source.resolve(strict=True)
    if not source.is_dir():
        fail(f"source tree is not a directory: {source}")
    destination = guest_path(destination)
    parent = str(Path(destination).parent)
    if destination == "/" or not is_directory(image, parent):
        fail(f"guest parent directory does not exist: {parent}")
    if exists(image, destination):
        fail(f"guest destination already exists: {destination}")
    dirs, files, links = walk_tree(source)

    # Preflight the entire host tree before the first guest filesystem write.
    for item in dirs + files + links:
        if any(ord(ch) < 32 for ch in item.name):
            fail(f"filename contains control characters: {item}")
        if any(ch in item.name for ch in ('"', "\\")):
            fail(f"debugfs restore cannot represent a quote or backslash in filename: {item.name!r}")
    for item in links:
        target = os.readlink(item)
        if any(ch in target for ch in ('"', "\\", "\n", "\r")):
            fail(f"debugfs restore cannot represent quote/backslash/newline in symlink target: {item}")
    clean_ext4(image)

    commands = [f"mkdir {q(destination)}"]
    for item in dirs:
        if item == source:
            continue
        commands.append(f"mkdir {q(relative_guest(source, item, destination))}")
    for item in files:
        target = relative_guest(source, item, destination)
        commands.append(f"write {q(str(item))} {q(target)}")
    for item in links:
        target = relative_guest(source, item, destination)
        commands.append(f"symlink {q(target)} {q(os.readlink(item))}")

    # Set metadata after populating directories so child creation does not
    # replace the saved directory modification times.
    for item in files + links + list(reversed(dirs)):
        target = destination if item == source else relative_guest(source, item, destination)
        fields = [("mode", str(inode_mode(item))),
                  # debugfs parses timestamp fields as hexadecimal values.
                  ("mtime", "0x" + format(int(item.lstat().st_mtime), "x"))]
        for field, value in fields:
            commands.append(f"set_inode_field {q(target)} {field} {value}")
    debugfs(image, commands, write=True)
    clean_ext4(image)
    print(f"imported {source} -> {destination}")


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    sub = parser.add_subparsers(dest="action", required=True)
    export = sub.add_parser("export-tree", help="copy a guest directory to the host")
    export.add_argument("image")
    export.add_argument("guest_directory")
    export.add_argument("host_destination_parent", type=Path)
    restore = sub.add_parser("import-tree", help="copy a host directory into the guest image")
    restore.add_argument("image")
    restore.add_argument("host_directory", type=Path)
    restore.add_argument("guest_destination")
    args = parser.parse_args()
    image = image_arg(args.image)
    if args.action == "export-tree":
        export_tree(image, args.guest_directory, args.host_destination_parent)
    else:
        import_tree(image, args.host_directory, args.guest_destination)


if __name__ == "__main__":
    main()
