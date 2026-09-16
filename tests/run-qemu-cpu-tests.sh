#!/bin/sh
set -eu

qemu=${QEMU_MYEMULATOR:-qemu-system-myemulator}
test -x "$qemu" || {
    echo "QEMU_MYEMULATOR is not an executable qemu-system-myemulator" >&2
    exit 2
}

tmpdir=$(mktemp -d "${TMPDIR:-/tmp}/myemulator-tests.XXXXXX")
trap 'rm -rf "$tmpdir"' EXIT HUP INT TERM

run_image()
{
    name=$1
    hex=$2
    expected=$3
    image="$tmpdir/$name.bin"
    socket="$tmpdir/$name.sock"
    output="$tmpdir/$name.out"

    printf '%s\n' "$hex" | xxd -r -p >"$image"
    "$qemu" -M myemulator -accel tcg -S -nographic -serial none \
        -monitor "unix:$socket,server=on,wait=off" -kernel "$image" \
        >"$tmpdir/$name.qemu.out" 2>"$tmpdir/$name.qemu.err" &
    qpid=$!
    ready=0
    for attempt in 1 2 3 4 5 6 7 8 9 10; do
        if test -S "$socket"; then
            ready=1
            break
        fi
        sleep 0.05
    done
    test "$ready" -eq 1
    {
        printf 'cont\n'
        sleep 0.1
        printf 'info registers\n'
        sleep 0.05
        printf 'quit\n'
    } | socat - UNIX-CONNECT:"$socket" >"$output"
    wait "$qpid"
    if ! rg -q -U -P "(?s)$expected" "$output"; then
        echo "FAIL $name" >&2
        rg -A6 'CPU#0' "$output" >&2 || true
        exit 1
    fi
    echo "PASS $name"
}

run_image basic 0210033080f0 'R0: 0x05.*FLAGS: ZF=0 NF=0 OF=0 CF=0'
run_image branch-forward 0010003001600110071080f0 'R0: 0x07.*FLAGS: ZF=1 NF=0 OF=0 CF=0'
run_image branch-backward 0210ff30fe6180f0 'R0: 0x00.*FLAGS: ZF=1 NF=0 OF=0 CF=1'
run_image jal 01500110071080f0 'LR: 0x02.*R0: 0x07'
run_image push-pop 0150ff100b101614211817e0001000140018001c17f080f0 \
    'R0: 0x0b.*R1: 0x16.*R2: 0x21'

cmp_case()
{
    a=$1
    b=$2
    flags=$3
    run_image "cmp-$a-$b" "${a}10 ${b}14 0081 80f0" "FLAGS: $flags"
}

cmp_case 00 00 'ZF=1 NF=0 OF=0 CF=1'
cmp_case 01 01 'ZF=1 NF=0 OF=0 CF=1'
cmp_case 02 01 'ZF=0 NF=0 OF=0 CF=1'
cmp_case 01 02 'ZF=0 NF=1 OF=0 CF=0'
cmp_case ff 01 'ZF=0 NF=1 OF=0 CF=1'
cmp_case 01 ff 'ZF=0 NF=0 OF=0 CF=0'
cmp_case 80 01 'ZF=0 NF=0 OF=1 CF=1'
cmp_case 7f ff 'ZF=0 NF=1 OF=1 CF=0'
cmp_case 80 7f 'ZF=0 NF=0 OF=1 CF=1'
cmp_case 7f 80 'ZF=0 NF=1 OF=1 CF=0'

branch_case()
{
    name=$1
    cond=$2
    a=$3
    b=$4
    expected=$5
    branch_high=$(printf '%02x' $((0x60 + cond)))
    run_image "$name" "${a}10 ${b}14 0081 01$branch_high 0118 80f0" \
        "R2: 0x$expected"
}

branch_case beq-taken 0 01 01 00
branch_case beq-not-taken 0 01 02 01
branch_case bne-taken 1 01 02 00
branch_case bne-not-taken 1 01 01 01
branch_case blt-taken 2 ff 01 00
branch_case blt-not-taken 2 01 ff 01
branch_case bge-taken 3 01 ff 00
branch_case bge-not-taken 3 ff 01 01
branch_case bltu-taken 4 01 02 00
branch_case bltu-not-taken 4 ff 01 01
branch_case bgeu-taken 5 ff 01 00
branch_case bgeu-not-taken 5 01 02 01
