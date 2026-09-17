#!/bin/sh
set -eu
root=$(CDPATH= cd -- "$(dirname "$0")/.." && pwd)
qemu=${QEMU_MYEMULATOR:-$root/.qemu-build/qemu-system-myemulator}
test -x "$qemu"
tmp=$(mktemp -d "${TMPDIR:-/tmp}/myemulator-alignment.XXXXXX")
pid=
cleanup() { test -z "$pid" || kill "$pid" 2>/dev/null || true; test -z "$pid" || wait "$pid" 2>/dev/null || true; rm -rf "$tmp"; }
trap cleanup EXIT HUP INT TERM

"$root/tools/myasm" "$root/tests/programs/alignment-firmware.s" \
    -o "$tmp/firmware.bin" --firmware --debug-map "$tmp/firmware.debug.json"
"$qemu" -M myemulator -S -nographic -serial none -bios "$tmp/firmware.bin" \
    -qmp "unix:$tmp/qmp.sock,server=on,wait=off" >"$tmp/out" 2>"$tmp/err" &
pid=$!
for i in $(seq 1 100); do test -S "$tmp/qmp.sock" && break; sleep 0.05; done
test -S "$tmp/qmp.sock"
cat >"$tmp/commands" <<'EOF'
{"command":"continue"}
{"command":"registers"}
{"command":"x","args":{"address":61437,"length":3}}
EOF
"$root/tools/mydebug" --machine --qmp "$tmp/qmp.sock" \
    --symbols "$tmp/firmware.debug.json" <"$tmp/commands" | \
python3 -c '
import json, sys
r = [json.loads(line) for line in sys.stdin if line.strip()]
assert r[0]["registers"]["pc"] == 0xf112
assert r[0]["registers"]["sp"] == 0xeffd
assert r[1]["registers"]["pc"] == 0xf112
assert r[2]["data"] == "00f106"
'
echo 'MyEmulator alignment-exception test passed'
