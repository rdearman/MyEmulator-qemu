#!/bin/sh
# Link an object left by the Make smoke test without Make's recipe launcher.
set -u
root=/usr/share/rem-flight/developer
/root/native/bin/ld -T /lib/myemulator2-user.ld -o /tmp/ld-from-make-object \
    /usr/lib/crt1.o /usr/lib/crti.o "$root/hello.o" \
    -L/usr/lib -L/root/native/lib --start-group -lc /root/native/lib/libgcc.a \
    --end-group /usr/lib/crtn.o
status=$?
echo "LD_RC:$status"
if [ "$status" -eq 0 ] && [ "$(/tmp/ld-from-make-object)" = 'REM developer workflow PASS' ]; then
    echo REM_LD_MAKE_OBJECT_PASS
else
    echo REM_LD_MAKE_OBJECT_FAIL
fi
