#!/bin/sh
# Compare bare, ./-prefixed, and absolute object names with native REM ld.
set -u
cd /usr/share/rem-flight/developer || exit 2
try_link() {
    label=$1
    object=$2
    output="/tmp/ld-path-$label"
    /root/native/bin/ld -T /lib/myemulator2-user.ld -o "$output" \
        /usr/lib/crt1.o /usr/lib/crti.o "$object" \
        -L/usr/lib -L/root/native/lib --start-group -lc \
        /root/native/lib/libgcc.a --end-group /usr/lib/crtn.o
    status=$?
    echo "LD_${label}_RC:$status"
    if [ "$status" -eq 0 ] && [ "$("$output")" = 'REM developer workflow PASS' ]; then
        echo "LD_${label}_RUN_PASS"
    else
        echo "LD_${label}_RUN_FAIL"
    fi
}
try_link bare hello.o
try_link dot ./hello.o
try_link absolute /usr/share/rem-flight/developer/hello.o
echo REM_LINK_RELATIVE_TEST_DONE
