#!/bin/sh
set -eu
root=$(CDPATH= cd -- "$(dirname "$0")/.." && pwd)
qemu=${QEMU_MYEMULATOR:-$root/.qemu-build/qemu-system-myemulator}
test -x "$qemu"
tmp=$(mktemp -d "${TMPDIR:-/tmp}/myemulator-timer.XXXXXX")
pid=
cleanup() { test -z "$pid" || kill "$pid" 2>/dev/null || true; test -z "$pid" || wait "$pid" 2>/dev/null || true; rm -rf "$tmp"; }
trap cleanup EXIT HUP INT TERM

"$root/tools/myasm" -I "$root/rikmon/include" "$root/tests/programs/timer-test.s" -o "$tmp/one.bin"
"$root/tools/myasm" -I "$root/rikmon/include" "$root/tests/programs/timer-periodic-test.s" -o "$tmp/periodic.bin"
"$root/tools/myasm" -I "$root/rikmon/include" "$root/tests/programs/timer-irq-firmware.s" -o "$tmp/irq.bin" --firmware

"$qemu" -M myemulator -S -nographic -serial none -kernel "$tmp/one.bin" -qmp "unix:$tmp/reset.sock,server=on,wait=off" >"$tmp/reset.out" 2>"$tmp/reset.err" &
pid=$!
for i in $(seq 1 100); do test -S "$tmp/reset.sock" && break; sleep .02; done
printf '%s\n' '{"command":"x","args":{"address":61472,"length":6}}' | "$root/tools/mydebug" --machine --qmp "$tmp/reset.sock" | grep -q '"data": "000000000000"'
kill "$pid" 2>/dev/null || true; wait "$pid" 2>/dev/null || true; pid=

run_case() {
    name=$1; option=$2; image=$3; socket="$tmp/$name.sock"
    "$qemu" -M myemulator -nographic -serial none "$option" "$image" -qmp "unix:$socket,server=on,wait=off" >"$tmp/$name.out" 2>"$tmp/$name.err" &
    pid=$!
    for i in $(seq 1 100); do test -S "$socket" && break; sleep .02; done
    sleep .2
    printf '%s\n' '{"command":"x","args":{"address":61473,"length":3}}' | "$root/tools/mydebug" --machine --qmp "$socket" >"$tmp/$name.json"
    kill "$pid" 2>/dev/null || true; wait "$pid" 2>/dev/null || true; pid=
}

run_case one-shot -kernel "$tmp/one.bin"
grep -q '"data": "010000"' "$tmp/one-shot.json"
run_case periodic -kernel "$tmp/periodic.bin"
grep -q '"data": "03' "$tmp/periodic.json"

"$qemu" -M myemulator -nographic -serial none -bios "$tmp/irq.bin" -qmp "unix:$tmp/irq.sock,server=on,wait=off" >"$tmp/irq.out" 2>"$tmp/irq.err" &
pid=$!
for i in $(seq 1 100); do test -S "$tmp/irq.sock" && break; sleep .02; done
sleep .2
printf '%s\n' '{"command":"x","args":{"address":57344,"length":1}}' '{"command":"x","args":{"address":61473,"length":1}}' | "$root/tools/mydebug" --machine --qmp "$tmp/irq.sock" >"$tmp/irq.json"
kill "$pid" 2>/dev/null || true; wait "$pid" 2>/dev/null || true; pid=
grep -q '"data": "a5"' "$tmp/irq.json"
grep -q '"data": "00"' "$tmp/irq.json"
echo 'MyEmulator timer tests passed'
