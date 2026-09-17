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

write_word_le()
{
    image=$1
    offset=$2
    value=$3
    word=$(printf '%02x%02x' $((value & 255)) $(((value >> 8) & 255)))
    printf '%s' "$word" | xxd -r -p | dd of="$image" bs=1 seek="$offset" conv=notrunc status=none
}

run_irq_image()
{
    name=$1
    mask=$2
    oneshot=$3
    program=$4
    handler=$5
    expected=$6
    image="$tmpdir/$name.bin"
    socket="$tmpdir/$name.sock"
    output="$tmpdir/$name.out"

    printf '%s' "$program" | xxd -r -p >"$image"
    truncate -s 65536 "$image"
    printf '%s' "$handler" | xxd -r -p | dd of="$image" bs=1 seek=256 conv=notrunc status=none
    for level in 1 2 3 4 5 6 7; do
        write_word_le "$image" $((0xffec + level * 2)) 0x0100
    done
    MYEMULATOR_INITIAL_SP=${MYEMULATOR_INITIAL_SP:-} \
    MYEMULATOR_INITIAL_S0=${MYEMULATOR_INITIAL_S0:-} \
    MYEMULATOR_IRQ_MASK=$mask MYEMULATOR_IRQ_ONESHOT=$oneshot \
        MYEMULATOR_IRQ_AFTER=${MYEMULATOR_IRQ_AFTER:-} "$qemu" \
        -M myemulator -accel tcg -S -nographic -serial none \
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
        sleep 0.15
        printf 'info registers\n'
        sleep 0.05
        printf 'quit\n'
    } | socat - UNIX-CONNECT:"$socket" >"$output"
    wait "$qpid"
    if ! rg -q -U -P "(?s)$expected" "$output"; then
        echo "FAIL $name" >&2
        rg -A7 'CPU#0' "$output" >&2 || true
        exit 1
    fi
    echo "PASS $name"
}

run_irq_vector()
{
    level=$1
    expected_value=$2
    name="irq-vector-$level"
    image="$tmpdir/$name.bin"
    socket="$tmpdir/$name.sock"
    output="$tmpdir/$name.out"
    printf '80f0' | xxd -r -p >"$image"
    truncate -s 65536 "$image"
    printf '%02x10' "$expected_value" | xxd -r -p | dd of="$image" bs=1 seek=256 conv=notrunc status=none
    printf '60f0' | xxd -r -p | dd of="$image" bs=1 seek=258 conv=notrunc status=none
    write_word_le "$image" $((0xffec + level * 2)) 0x0100
    MYEMULATOR_INITIAL_SP=${MYEMULATOR_INITIAL_SP:-} \
    MYEMULATOR_IRQ_MASK=$((1 << level)) MYEMULATOR_IRQ_ONESHOT=1 "$qemu" \
        -M myemulator -accel tcg -S -nographic -serial none \
        -monitor "unix:$socket,server=on,wait=off" -kernel "$image" \
        >"$tmpdir/$name.qemu.out" 2>"$tmpdir/$name.qemu.err" &
    qpid=$!
    for attempt in 1 2 3 4 5 6 7 8 9 10; do
        test -S "$socket" && break
        sleep 0.05
    done
    {
        printf 'cont\n'
        sleep 0.15
        printf 'info registers\n'
        sleep 0.05
        printf 'quit\n'
    } | socat - UNIX-CONNECT:"$socket" >"$output"
    wait "$qpid"
    if ! rg -q "R0: 0x$(printf '%02x' "$expected_value")" "$output" || \
       ! rg -q "S0: 0x00" "$output"; then
        echo "FAIL $name" >&2
        rg -A7 'CPU#0' "$output" >&2 || true
        exit 1
    fi
    echo "PASS $name"
}

run_image basic 0210033080f0 'R0: 0x05.*S0: 0x00.*FLAGS: ZF=0 NF=0 OF=0 CF=0'
run_image branch-forward 0010003001600110071080f0 'R0: 0x07.*FLAGS: ZF=1 NF=0 OF=0 CF=0'
run_image branch-backward 0210ff30fe6180f0 'R0: 0x00.*FLAGS: ZF=1 NF=0 OF=0 CF=1'
run_image jal 01500110071080f0 'LR: 0x0002.*R0: 0x07'
run_image push-pop 0150ff100b101614211817e0001000140018001c17f080f0 \
    'LR: 0x0002.*R0: 0x0b.*R1: 0x16.*R2: 0x21'
run_image address-ops a5100176121034140471ab18cd1c6c7110732a731d732373047380f0 \
    'SP: 0x1234.*LR: 0x1234.*R0: 0x12.*R1: 0x34.*R2: 0xab.*R3: 0xcd.*A0: 0x1234.*A1: 0xabcd.*A2: 0x1234.*A3: 0x1234.*S0: 0xa5'
run_image mva-pc-invalid 121034140471307380f0 \
    'PC: 0x000a.*A0: 0x1234.*A1: 0x0000.*A2: 0x0000.*A3: 0x0000'
run_image high-memory ab184310741404710028000c80f0 \
    'R2: 0xab.*R3: 0xab.*A0: 0x4374'
run_image address-wrap 10101004047101707f70ff70807080f0 \
    'A0: 0x0fff'

run_image alu-immediate 0f10f014009180a1ffb180f0 \
    'R0: 0x0f.*S0: 0x00.*FLAGS: ZF=0 NF=0 OF=0 CF=0'
run_image alu-register 0f10f014217731774177017711775177617780f0 \
    'R0: 0x00.*S0: 0x01.*FLAGS: ZF=1 NF=0 OF=0 CF=0'
run_image shift-zero-preserves-cf 811001c000d080f0 \
    'R0: 0x02.*FLAGS: ZF=0 NF=0 OF=0 CF=1'
run_image shift-large-count ff1008c080f0 \
    'R0: 0x00.*FLAGS: ZF=1 NF=0 OF=0 CF=0'
run_image shift-register-count 01100814517780f0 \
    'R0: 0x00.*FLAGS: ZF=1 NF=0 OF=0 CF=0'

run_image s0-gf-sf a5100176001404760576001808760976001c0c760d76007680f0 \
    'R0: 0xa5.*R1: 0xa5.*R2: 0xa5.*R3: 0xa5.*S0: 0xa5.*FLAGS: ZF=1 NF=0 OF=0 CF=1'
run_image sf-flags 0f10017680f0 \
    'S0: 0x0f.*FLAGS: ZF=1 NF=1 OF=1 CF=1'
run_image cmp-s0 00100114008180f0 'S0: 0x02.*FLAGS: ZF=0 NF=1 OF=0 CF=0'
run_image br-forward 01660010071080f0 'R0: 0x07'
run_image br-backward 01100266021080f0fd6680f0 'R0: 0x02'
run_image ret 0250047680f0071040f0 'LR: 0x0002.*R0: 0x07'
run_image nested-ret 0250047680f010e0025010f040f0421040f0 \
    'SP: 0xffff.*LR: 0x0002.*R0: 0x42'

high_jal=7f50
i=0
while test "$i" -lt 254; do
    high_jal=${high_jal}00
    i=$((i + 1))
done
run_image high-jal "${high_jal}005080f0" 'PC: 0x0104.*LR: 0x0102'

high_ret=7f66
i=0
while test "$i" -lt 254; do
    high_ret=${high_ret}00
    i=$((i + 1))
done
run_image high-ret "${high_ret}025080f00000421040f0" \
    'PC: 0x0104.*LR: 0x0102.*R0: 0x42'

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

MYEMULATOR_INITIAL_SP=0x8000
run_irq_image irq-entry 2 1 80f0 007660f0 \
    'PC: 0x0002.*SP: 0x8000.*R0: 0x10.*S0: 0x00'
MYEMULATOR_INITIAL_S0=0xb5
run_irq_image irq-frame 32 1 80f0 007660f0 \
    'PC: 0x0002.*SP: 0x8000.*R0: 0xd5.*S0: 0xb5'
MYEMULATOR_INITIAL_S0=0x40
run_irq_image irq-blocked 16 1 80f0 421060f0 \
    'PC: 0x0002.*SP: 0x8000.*R0: 0x00.*S0: 0x40'
run_irq_image irq-accepted-above 32 1 80f0 5a1060f0 \
    'PC: 0x0002.*SP: 0x8000.*R0: 0x5a.*S0: 0x40'
MYEMULATOR_INITIAL_S0=0x20
run_irq_image irq-highest 0x68 1 80f0 661080f0 \
    'PC: 0x0104.*SP: 0x7ffd.*R0: 0x66.*S0: 0x60'
unset MYEMULATOR_INITIAL_S0
run_irq_image irq-handler-stack 2 1 0010017680f0 aa10551403e00010001403f060f0 \
    'PC: 0x0006.*SP: 0x8000.*R0: 0x00.*R1: 0x55'
MYEMULATOR_IRQ_AFTER=6
run_irq_image irq-nested 2 1 0010017680f0 771060f0 \
    'PC: 0x0006.*SP: 0x8000.*S0: 0x00'
unset MYEMULATOR_IRQ_AFTER
run_irq_image irq-retrigger 2 0 01140010017680f0 01810260011060f080f0 \
    'PC: 0x010a.*SP: 0x7ffd.*R0: 0x00.*S0: 0x15'

run_irq_vector 1 11
run_irq_vector 2 22
run_irq_vector 3 33
run_irq_vector 4 44
run_irq_vector 5 55
run_irq_vector 6 66
run_irq_vector 7 77
unset MYEMULATOR_INITIAL_SP
