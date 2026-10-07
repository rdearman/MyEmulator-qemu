#!/usr/bin/env python3
"""Regenerate the separated source patch against preserved pristine upstream."""
import os
import difflib
from pathlib import Path

root = Path(os.environ.get("REM_NATIVE_WORK", "/tmp/rem-native-userland-work"))
upstream = root / "upstream-chibicc"
ported = root / "kit/bootstrap/chibicc"
patch = []
for path in sorted(ported.rglob("*")):
    if not path.is_file() or path.suffix not in (".c", ".h", ".sh"):
        continue
    relative = path.relative_to(ported)
    original = upstream / relative
    before = original.read_text().splitlines(keepends=True) if original.exists() else []
    after = path.read_text().splitlines(keepends=True)
    patch.extend(difflib.unified_diff(before, after,
                 fromfile="a/" + str(relative), tofile="b/" + str(relative)))
(root / "kit/patches/chibicc-rem-native.patch").write_text("".join(patch))
