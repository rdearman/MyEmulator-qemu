#!/bin/sh
set -eu
root=$(CDPATH= cd -- "$(dirname "$0")/.." && pwd)
qemu=${QEMU_MYEMULATOR:-$root/.qemu-build/qemu-system-myemulator}
test -x "$qemu"
tmp=$(mktemp -d "${TMPDIR:-/tmp}/myemulator-floppy.XXXXXX")
pid=
cleanup() { test -z "$pid" || kill "$pid" 2>/dev/null || true; test -z "$pid" || wait "$pid" 2>/dev/null || true; rm -rf "$tmp"; }
trap cleanup EXIT HUP INT TERM
"$root/tools/myasm" "$root/tests/programs/minimal-firmware.s" \
    -o "$tmp/firmware.bin" --firmware --debug-map "$tmp/firmware.debug.json"
"$root/tools/myasm" "$root/examples/asm/floppy-sector.asm" -o "$tmp/sector.bin" --flat-64k
head -c 256 "$tmp/sector.bin" >"$tmp/sector"
truncate -s 1474560 "$tmp/disk.img"
dd if="$tmp/sector" of="$tmp/disk.img" conv=notrunc status=none
"$qemu" -M myemulator -S -nographic -serial none -bios "$tmp/firmware.bin" \
    -drive "file=$tmp/disk.img,format=raw,if=none,id=myemulator-floppy" \
    -qmp "unix:$tmp/qmp.sock,server=on,wait=off" >"$tmp/out" 2>"$tmp/err" &
pid=$!
for i in $(seq 1 100); do test -S "$tmp/qmp.sock" && break; sleep 0.05; done
test -S "$tmp/qmp.sock"
printf '%s\n' \
  '{"command":"registers"}' \
  '{"command":"continue"}' |
  "$root/tools/mydebug" --machine --qmp "$tmp/qmp.sock" |
  python3 -c 'import json,sys; r=[json.loads(x) for x in sys.stdin if x.strip()]; assert r[0]["registers"]["pc"] == 0xf100; assert r[1]["registers"]["pc"] == 0xf104; assert r[1]["registers"]["r0"] == 42'
echo 'MyEmulator firmware/floppy reset test passed'
