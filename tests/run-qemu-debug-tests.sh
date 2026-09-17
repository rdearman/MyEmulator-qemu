#!/bin/sh
set -eu

qemu=${QEMU_MYEMULATOR:-.qemu-build/qemu-system-myemulator}
test -x "$qemu" || { echo "qemu-system-myemulator is not available" >&2; exit 2; }

tmpdir=$(mktemp -d "${TMPDIR:-/tmp}/myemulator-debug.XXXXXX")
trap 'rm -rf "$tmpdir"' EXIT HUP INT TERM

run_disassembly_case()
{
    name=$1
    hex=$2
    expected=$3
    image="$tmpdir/$name.bin"
    socket="$tmpdir/$name.sock"
    log="$tmpdir/$name.log"

    printf '%s' "$hex" | xxd -r -p >"$image"
    "$qemu" -M myemulator -accel tcg -S -nographic -serial none \
        -d in_asm,cpu,nochain -D "$log" \
        -monitor "unix:$socket,server=on,wait=off" -kernel "$image" \
        >"$tmpdir/$name.out" 2>"$tmpdir/$name.err" &
    qpid=$!
    for attempt in 1 2 3 4 5 6 7 8 9 10 11 12 13 14 15 16 17 18 19 20; do
        test -S "$socket" && break
        sleep 0.05
    done
    test -S "$socket"
    { printf 'cont\n'; sleep 0.1; printf 'quit\n'; } |
        socat - UNIX-CONNECT:"$socket" >"$tmpdir/$name.monitor" || true
    wait "$qpid" || true
    rg -q "$expected" "$log" || {
        echo "FAIL disassembly $name" >&2
        sed -n '1,40p' "$log" >&2
        exit 1
    }
    echo "PASS disassembly $name"
}

# Each image starts with one representative instruction and ends with HALT.
run_disassembly_case ld       000080f0 'ld r0,\[a0\]'
run_disassembly_case li       121080f0 'li r0, #0x12'
run_disassembly_case st       002080f0 'st r0,\[a0\]'
run_disassembly_case add      053080f0 'add r0,r0,#0x05'
run_disassembly_case sub      054080f0 'sub r0,r0,#0x05'
run_disassembly_case jal      0150000080f0 'jal 0x0004'
run_disassembly_case branch   00600080f0 'beq 0x0002'
run_disassembly_case address  0471ab18cd1c6c71107380f0 'lda a0,r0,r1'
run_disassembly_case mva      107380f0 'mva a2,a0'
run_disassembly_case gf       007680f0 'gf r0'
run_disassembly_case sf       017680f0 'sf r0'
run_disassembly_case cmp      008080f0 'cmp r0,r0'
run_disassembly_case and      009080f0 'and r0,r0,#0x00'
run_disassembly_case or       00a080f0 'or r0,r0,#0x00'
run_disassembly_case xor      00b080f0 'xor r0,r0,#0x00'
run_disassembly_case shl      01c080f0 'shl r0,r0,#0x01'
run_disassembly_case shr      01d080f0 'shr r0,r0,#0x01'
run_disassembly_case alu-reg  017780f0 'add r0,r1'
run_disassembly_case push     00e080f0 'push'
run_disassembly_case pop      00f080f0 'pop'
run_disassembly_case ret      40f080f0 'ret'
run_disassembly_case rti      60f080f0 'rti'
run_disassembly_case halt     80f0 'halt'
run_disassembly_case reserved 077f80f0 '.word 0x7f07'

if command -v gdb >/dev/null 2>&1; then
    image="$tmpdir/gdb.bin"
    socket="$tmpdir/gdb-monitor.sock"
    gdb_output="$tmpdir/gdb.out"
    printf '121080f0' | xxd -r -p >"$image"
    "$qemu" -M myemulator -accel tcg -S -nographic -serial none \
        -gdb tcp::12345 -monitor "unix:$socket,server=on,wait=off" \
        -kernel "$image" >"$tmpdir/gdb.qemu.out" 2>"$tmpdir/gdb.qemu.err" &
    qpid=$!
    for attempt in 1 2 3 4 5 6 7 8 9 10 11 12 13 14 15 16 17 18 19 20; do
        if rg -q 'Listening on port 12345' "$tmpdir/gdb.qemu.err"; then break; fi
        sleep 0.05
    done
    gdb -q -nx -batch \
        -ex 'set pagination off' \
        -ex 'set confirm off' \
        -ex 'target remote localhost:12345' \
        -ex 'info registers' \
        -ex 'stepi' \
        -ex 'detach' \
        -ex 'quit' >"$gdb_output" 2>&1
    kill "$qpid" 2>/dev/null || true
    wait "$qpid" 2>/dev/null || true
    if rg -q 'Truncated register|Architecture rejected target-supplied description|Could not load XML target description' "$gdb_output"; then
        echo "SKIP GDB register test: installed GDB has no compatible fictional/8-16-bit architecture"
    else
        for reg in r0 a0 lr sp pc s0; do
            rg -q "^[[:space:]]*$reg[[:space:]]" "$gdb_output" || {
                echo "FAIL GDB register $reg" >&2
                cat "$gdb_output" >&2
                exit 1
            }
        done
        echo "PASS GDB XML/register packet"
    fi
else
    echo "SKIP GDB test: stock gdb is not installed"
fi

echo 'MyEmulator debugging tests passed'
