#!/bin/sh
set -eu

root=$(CDPATH= cd -- "$(dirname "$0")/.." && pwd)
qemu=${QEMU_MYEMULATOR:-$root/.qemu-build/qemu-system-myemulator}
test -x "$qemu" || { echo "qemu-system-myemulator is not available" >&2; exit 2; }
tmpdir=$(mktemp -d "${TMPDIR:-/tmp}/myemulator-mydebug.XXXXXX")
qpid=
cleanup() { test -z "$qpid" || kill "$qpid" 2>/dev/null || true; test -z "$qpid" || wait "$qpid" 2>/dev/null || true; rm -rf "$tmpdir"; }
trap cleanup EXIT HUP INT TERM

"$root/tools/myasm" "$root/examples/asm/arithmetic.asm" -o "$tmpdir/a.bin" \
    --debug-map "$tmpdir/a.mdbg"
"$qemu" -M myemulator -S -nographic -serial none \
    -qmp "unix:$tmpdir/qmp.sock,server=on,wait=off" -kernel "$tmpdir/a.bin" \
    >"$tmpdir/out" 2>"$tmpdir/err" &
qpid=$!
for i in $(seq 1 100); do test -S "$tmpdir/qmp.sock" && break; sleep 0.05; done
test -S "$tmpdir/qmp.sock"
cat >"$tmpdir/commands" <<EOF
{"command":"registers"}
{"command":"step"}
{"command":"step"}
{"command":"x","args":{"address":0,"length":2}}
{"command":"w","args":{"address":32,"data":"deadbeef"}}
{"command":"x","args":{"address":32,"length":4}}
{"command":"b","args":{"address":"after_sub"}}
{"command":"bl"}
{"command":"print","args":{"expression":"r0 + 4"}}
{"command":"set","args":{"register":"r0","expression":"0xff"}}
{"command":"set","args":{"register":"a0","expression":"0x2000"}}
{"command":"reset"}
{"command":"list"}
{"command":"breakpoints"}
{"command":"delete","args":{"number":1}}
{"command":"info locals"}
{"command":"b","args":{"address":6}}
{"command":"continue"}
{"command":"dis","args":{"address":6,"count":1}}
EOF
"$root/tools/mydebug" --machine --qmp "$tmpdir/qmp.sock" --symbols "$tmpdir/a.mdbg" \
    <"$tmpdir/commands" | python3 -c '
import json, sys
r = [json.loads(x) for x in sys.stdin if x.strip()]
assert r[0]["registers"]["pc"] == 0 and r[0]["registers"]["r0"] == 0
assert r[1]["registers"]["pc"] == 2 and r[1]["registers"]["r0"] == 7
assert r[1]["source"]["line"] == 3
assert r[2]["registers"]["pc"] == 4 and r[2]["registers"]["r0"] == 12
assert r[3]["data"] == "0710"
assert r[5]["data"] == "deadbeef"
assert 6 in r[7]["breakpoints"]
assert r[8]["value"] == 16
assert r[9]["registers"]["r0"] == 255
assert r[10]["registers"]["a0"] == 8192
assert r[11]["registers"]["pc"] == 0 and r[11]["registers"]["r0"] == 0
assert r[12]["available"] and r[12]["line"] == 2
assert r[13]["breakpoints"][0]["number"] == 1
assert r[14]["deleted"]["number"] == 1
assert not r[15]["available"]
assert r[15]["available"] is False
assert r[16]["number"] == 2
assert r[17]["registers"]["pc"] == 6 and r[17]["registers"]["r0"] == 10
assert r[18]["instructions"][0]["text"] == "gf r1"
'
echo 'MyEmulator native debugger tests passed'
