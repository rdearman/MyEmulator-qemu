#!/bin/sh
set -eu
root=$(CDPATH= cd -- "$(dirname "$0")/.." && pwd)
qemu=${QEMU_MYEMULATOR:-$root/.qemu-build/qemu-system-myemulator}
test -x "$qemu"
tmp=$(mktemp -d "${TMPDIR:-/tmp}/myemulator-firmware.XXXXXX")
pid=
cleanup() { test -z "$pid" || kill "$pid" 2>/dev/null || true; test -z "$pid" || wait "$pid" 2>/dev/null || true; rm -rf "$tmp"; }
trap cleanup EXIT HUP INT TERM

"$root/tools/myasm" "$root/tests/programs/minimal-firmware.s" \
    -o "$tmp/firmware.bin" --firmware --debug-map "$tmp/firmware.debug.json"
"$qemu" -M myemulator -S -nographic -serial none -bios "$tmp/firmware.bin" \
    -qmp "unix:$tmp/qmp.sock,server=on,wait=off" >"$tmp/out" 2>"$tmp/err" &
pid=$!
for i in $(seq 1 100); do test -S "$tmp/qmp.sock" && break; sleep 0.05; done
test -S "$tmp/qmp.sock"
cat >"$tmp/commands" <<'EOF'
{"command":"registers"}
{"command":"x","args":{"address":61696,"length":4}}
{"command":"x","args":{"address":65532,"length":4}}
{"command":"w","args":{"address":61696,"data":"00000000"}}
{"command":"x","args":{"address":61696,"length":4}}
{"command":"w","args":{"address":61439,"data":"aa"}}
{"command":"x","args":{"address":61439,"length":1}}
{"command":"x","args":{"address":61472,"length":1}}
{"command":"stepi"}
EOF
"$root/tools/mydebug" --machine --qmp "$tmp/qmp.sock" \
    --symbols "$tmp/firmware.debug.json" <"$tmp/commands" | \
python3 -c '
import json, sys
r = [json.loads(line) for line in sys.stdin if line.strip()]
assert r[0]["registers"]["sp"] == 0xf000
assert r[0]["registers"]["pc"] == 0xf100
assert r[1]["data"] == "2a1080f0"
assert r[2]["data"] == "00f000f1"
assert r[4]["data"] == "2a1080f0"  # ROM write did not change it.
assert r[6]["data"] == "aa"       # 0xEFFF is writable RAM.
assert r[7].get("error")            # 0xF020 is reserved/unmapped.
assert r[8]["registers"]["pc"] == 0xf102
assert r[8]["registers"]["r0"] == 42
'
dd if=/dev/zero of="$tmp/oversized.bin" bs=1 count=3841 status=none
if "$qemu" -M myemulator -display none -bios "$tmp/oversized.bin" \
    >"$tmp/oversized.out" 2>"$tmp/oversized.err"; then
    echo 'oversized firmware was accepted' >&2
    exit 1
fi
grep -q 'maximum is 3840 bytes' "$tmp/oversized.err"
echo 'MyEmulator firmware memory-map test passed'
