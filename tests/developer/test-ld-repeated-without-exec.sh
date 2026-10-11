#!/bin/sh
# Separate repeated linker invocation from running linked programs.
set -u
cd /usr/share/rem-flight/developer || exit 2
round=1
while [ "$round" -le 3 ]; do
    output="/tmp/ld-noexec-$round"
    /root/native/bin/ld -T /lib/myemulator2-user.ld -o "$output" \
        /usr/lib/crt1.o /usr/lib/crti.o \
        /usr/share/rem-flight/developer/hello.o \
        -L/usr/lib -L/root/native/lib --start-group -lc \
        /root/native/lib/libgcc.a --end-group /usr/lib/crtn.o
    status=$?
    echo "LD_NOEXEC_${round}_RC:$status"
    round=$((round + 1))
done
echo REM_LD_NOEXEC_REPEAT_DONE
