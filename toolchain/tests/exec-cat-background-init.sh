#!/bin/sh
/bin/busybox sh -c '/usr/bin/cat /tmp/cat-input > /tmp/cat-output & wait'
echo EXEC_CAT_BACKGROUND_RC:$?
exec /bin/busybox sh -i </dev/console >/dev/console 2>&1
