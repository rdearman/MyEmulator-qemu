#!/bin/sh
set -eu
qemu=${QEMU_MYEMULATOR:-../qemu-build-myemulator/qemu-system-myemulator}
test -x "$qemu" || { echo "qemu-system-myemulator is not available" >&2; exit 2; }
tmpdir=$(mktemp -d "${TMPDIR:-/tmp}/myasm-qemu.XXXXXX")
trap 'rm -rf "$tmpdir"' EXIT HUP INT TERM
image="$tmpdir/arithmetic.bin"
socket="$tmpdir/qemu.sock"
./tools/myasm examples/asm/arithmetic.asm -o "$image"
"$qemu" -M myemulator -accel tcg -S -nographic -serial none \
  -monitor "unix:$socket,server=on,wait=off" -kernel "$image" \
  >"$tmpdir/qemu.out" 2>"$tmpdir/qemu.err" &
pid=$!
for i in 1 2 3 4 5 6 7 8 9 10; do test -S "$socket" && break; sleep 0.05; done
test -S "$socket"
{ printf 'cont\n'; sleep 0.1; printf 'info registers\n'; sleep 0.05; printf 'quit\n'; } |
  socat - UNIX-CONNECT:"$socket" >"$tmpdir/monitor.out"
wait "$pid"
rg -q 'R0: 0x0a' "$tmpdir/monitor.out"
echo 'PASS assembler -> QEMU arithmetic integration'
