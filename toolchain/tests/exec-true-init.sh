#!/bin/sh
/usr/bin/exec-true
echo EXEC_TRUE_RC:$?
exec /bin/busybox sh -i </dev/console >/dev/console 2>&1
