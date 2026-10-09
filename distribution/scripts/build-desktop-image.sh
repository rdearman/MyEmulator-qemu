#!/bin/sh
set -eu
ROOT=$(CDPATH= cd -- "$(dirname -- "$0")/../.." && pwd)
OUT=${1:-/tmp/rem-linux-0.2-desktop}
SIZE=${2:-1536}
case "$OUT" in /tmp/*) ;; *) echo "output must be under /tmp" >&2; exit 2 ;; esac
[ -d "$OUT" ] && [ -n "$(ls -A "$OUT")" ] && {
    echo "refusing to overwrite non-empty output: $OUT" >&2
    exit 2
}
mkdir -p "$(dirname -- "$OUT")"
command -v fakeroot >/dev/null 2>&1 || { echo "fakeroot is required" >&2; exit 2; }
fakeroot -- python3 "$ROOT/distribution/scripts/assemble-desktop-image.py" "$OUT" "$SIZE"
