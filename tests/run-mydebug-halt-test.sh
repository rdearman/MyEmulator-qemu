#!/bin/sh
set -eu

root=$(CDPATH= cd -- "$(dirname "$0")/.." && pwd)
qemu=${QEMU_MYEMULATOR:-$root/.qemu-build/qemu-system-myemulator}
test -x "$qemu" || { echo "qemu-system-myemulator is not available" >&2; exit 2; }
tmpdir=$(mktemp -d "${TMPDIR:-/tmp}/myemulator-halt.XXXXXX")
qpid=
cleanup() { test -z "$qpid" || kill "$qpid" 2>/dev/null || true; test -z "$qpid" || wait "$qpid" 2>/dev/null || true; rm -rf "$tmpdir"; }
trap cleanup EXIT HUP INT TERM

"$root/tools/myasm" "$root/examples/asm/arithmetic.asm" -o "$tmpdir/a.bin" \
    --debug-map "$tmpdir/a.debug.json"
"$qemu" -M myemulator -S -display none -serial none \
    -qmp "unix:$tmpdir/qmp.sock,server=on,wait=off" -kernel "$tmpdir/a.bin" \
    >"$tmpdir/out" 2>"$tmpdir/err" &
qpid=$!
for i in $(seq 1 100); do test -S "$tmpdir/qmp.sock" && break; sleep 0.05; done
test -S "$tmpdir/qmp.sock"

{
    printf '%s\n' '{"command":"registers"}'
    printf '%s\n' '{"command":"stepi"}' '{"command":"stepi"}'
    printf '%s\n' '{"command":"stepi"}' '{"command":"stepi"}' '{"command":"stepi"}'
    printf '%s\n' '{"command":"registers"}' '{"command":"reset"}'
    printf '%s\n' '{"command":"continue"}' '{"command":"registers"}'
    printf '%s\n' '{"command":"reset"}' '{"command":"run"}' '{"command":"registers"}'
} | "$root/tools/mydebug" --machine --qmp "$tmpdir/qmp.sock" \
    --symbols "$tmpdir/a.debug.json" >"$tmpdir/results.jsonl"

python3 - "$tmpdir/results.jsonl" <<'PY'
import json
import sys

rows = [json.loads(line) for line in open(sys.argv[1], encoding="utf-8")]
assert rows[0]["registers"]["pc"] == 0
assert [rows[n]["registers"]["pc"] for n in range(1, 6)] == [2, 4, 6, 8, 10]
assert rows[5]["halted"] is True
assert rows[6]["halted"] is True and rows[6]["registers"]["pc"] == 10
assert rows[7]["halted"] is False and rows[7]["registers"]["pc"] == 0
assert rows[8]["halted"] is True and rows[8]["registers"]["pc"] == 10
assert rows[9]["halted"] is True and rows[9]["registers"]["pc"] == 10
assert rows[10]["halted"] is False and rows[10]["registers"]["pc"] == 0
assert rows[11]["halted"] is True and rows[11]["registers"]["pc"] == 10
print("MyEmulator HALT/reset/run debugger tests passed")
PY
