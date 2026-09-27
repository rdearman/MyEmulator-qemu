#!/bin/sh
echo CURRENT_CAT_START
/usr/bin/cat /tmp/cat-input > /tmp/current-cat-output
echo CURRENT_CAT_RC:$?
exec /bin/busybox sh -i </dev/console >/dev/console 2>&1
