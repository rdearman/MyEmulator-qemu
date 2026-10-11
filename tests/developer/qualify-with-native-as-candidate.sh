#!/bin/sh
# Run after staging the experimental non-TLS assembler and smoke files into a
# disposable guest image. No production tool or flight image is replaced.
set -eu
project=/usr/share/rem-flight/developer
cd "$project"
/root/native/bin/chibicc -S -o /tmp/no-tls-hello.s hello.c
/root/native/bin/as -o /tmp/no-tls-hello1.o /tmp/no-tls-hello.s
/root/native/bin/as -o /tmp/no-tls-hello2.o /tmp/no-tls-hello.s
/root/native/bin/ld -T /lib/myemulator2-user.ld -o /tmp/no-tls-hello \
    /usr/lib/crt1.o /usr/lib/crti.o /tmp/no-tls-hello1.o \
    -L/usr/lib -L/root/native/lib --start-group -lc /root/native/lib/libgcc.a \
    --end-group /usr/lib/crtn.o
[ "$(/tmp/no-tls-hello)" = 'REM developer workflow PASS' ]
echo REM_NO_TLS_AS_C_LINK_PASS
if sh qualify.sh; then
    exit 0
fi
/root/native/bin/ld -T /lib/myemulator2-user.ld -o /tmp/relative-hello \
    /usr/lib/crt1.o /usr/lib/crti.o hello.o \
    -L/usr/lib -L/root/native/lib --start-group -lc /root/native/lib/libgcc.a \
    --end-group /usr/lib/crtn.o
if [ "$(/tmp/relative-hello)" = 'REM developer workflow PASS' ]; then
    echo REM_MAKE_LINK_COMPARE_RELATIVE_PASS
else
    echo REM_MAKE_LINK_COMPARE_RELATIVE_FAIL
fi
echo REM_MAKE_LINK_COMPARE_DONE
