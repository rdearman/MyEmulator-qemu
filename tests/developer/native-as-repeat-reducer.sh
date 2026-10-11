#!/bin/sh
# Isolate whether native REM as fails repeatedly on identical generated input.
set -u
cd /usr/share/rem-flight/developer || exit 2
/root/native/bin/chibicc -S -o /tmp/twice.s hello.c || exit 3
sh strip-file-records.sh /tmp/twice.s /tmp/twice.filtered.s || exit 4
for round in 1 2 3 4; do
    case "$round" in
        1|3) output=/tmp/twice1.o ;;
        2|4) output=/tmp/twice2.o ;;
    esac
    /root/native/bin/as -o "$output" /tmp/twice.filtered.s \
        >"/tmp/as$round.log" 2>&1
    status=$?
    echo "AS${round}_RC:$status"
done
sha256sum /tmp/twice.s /tmp/twice.filtered.s
echo REM_AS_REDUCTION_DONE
