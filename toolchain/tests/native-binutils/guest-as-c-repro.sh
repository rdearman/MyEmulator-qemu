#!/bin/sh
set +e
AS=/root/binqual/bin/as
W=/root/binqual/work
cd "$W" || exit 90
echo AS_C_REPRO_BEGIN
echo '.text
.globl _start
_start:
  li r1, 0
  halt' > min.s
$AS min.s -o as-no-c.o >no-c.log 2>&1
no_rc=$?
echo AS_NO_C_RC:$no_rc
$AS -c min.s -o as-c.o >as-c.log 2>&1
c_rc=$?
echo AS_C_RC:$c_rc
test -f as-c.o
echo AS_C_EXISTS:$?
sync
exit 0
