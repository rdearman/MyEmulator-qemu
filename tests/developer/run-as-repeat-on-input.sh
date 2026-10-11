#!/bin/sh
# Guest-side repeated native assembler test for an already staged input.
set -u
tool=${1:-/root/native/bin/as}
input=${2:-/tmp/twice.filtered.s}
round=1
while [ "$round" -le 4 ]; do
    output="/tmp/as-repeat-$round.o"
    log="/tmp/as-repeat-$round.log"
    "$tool" -o "$output" "$input" >"$log" 2>&1
    status=$?
    echo "AS${round}_RC:$status"
    round=$((round + 1))
done
echo REM_AS_REPEATED_INPUT_DONE
