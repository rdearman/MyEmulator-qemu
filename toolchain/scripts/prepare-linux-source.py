#!/usr/bin/env python3
"""Install the maintained REM Linux source overlay."""
from pathlib import Path
import shutil
import sys


def main() -> None:
    if len(sys.argv) != 2:
        raise SystemExit("usage: prepare-linux-source.py LINUX_SOURCE")
    source = Path(sys.argv[1]).resolve()
    root = Path(__file__).resolve().parents[2]
    overlay = root / "linux" / "arch" / "myemulator2"
    destination = source / "arch" / "myemulator2"
    if not source.is_dir() or not (source / "Makefile").exists():
        raise SystemExit(f"not a Linux source tree: {source}")
    if destination.exists():
        shutil.rmtree(destination)
    shutil.copytree(overlay, destination)
    print(f"installed {overlay} -> {destination}")


if __name__ == "__main__":
    main()
