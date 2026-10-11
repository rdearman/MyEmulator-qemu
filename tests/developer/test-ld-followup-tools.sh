#!/bin/sh
# Check file reads and independent tools after the first native link succeeds.
set -u
cd /usr/share/rem-flight/developer || exit 2
object=/usr/share/rem-flight/developer/hello.o
assembly=/usr/share/rem-flight/developer/hello.s
/root/native/bin/ld -T /lib/myemulator2-user.ld -o /tmp/ld-followup-1 \
    /usr/lib/crt1.o /usr/lib/crti.o "$object" \
    -L/usr/lib -L/root/native/lib --start-group -lc \
    /root/native/lib/libgcc.a --end-group /usr/lib/crtn.o
echo "LD_FIRST_RC:$?"
test -r "$object"
echo "TEST_OBJECT_READABLE_RC:$?"
/root/native/bin/readelf -h "$object" >/tmp/readelf-after-link.log 2>&1
echo "READELF_AFTER_LINK_RC:$?"
cat "$object" >/dev/null
echo "CAT_AFTER_LINK_RC:$?"
/root/native/bin/as -o /tmp/as-after-link.o "$assembly"
echo "AS_AFTER_LINK_RC:$?"
/root/native/bin/ld -T /lib/myemulator2-user.ld -o /tmp/ld-followup-2 \
    /usr/lib/crt1.o /usr/lib/crti.o "$object" \
    -L/usr/lib -L/root/native/lib --start-group -lc \
    /root/native/lib/libgcc.a --end-group /usr/lib/crtn.o
echo "LD_SECOND_RC:$?"
echo REM_LD_FOLLOWUP_TOOLS_DONE
