#!/bin/sh
/usr/bin/exec-cat
echo EXEC_CAT_RC:$?
exec /bin/busybox sh -i </dev/console >/dev/console 2>&1
