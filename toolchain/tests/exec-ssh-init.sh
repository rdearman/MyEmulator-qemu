#!/bin/sh
echo SSH_START
/usr/bin/ssh -V </dev/null > /tmp/ssh-version.out 2>&1
echo SSH_RC:$?
exec /bin/busybox sh -i </dev/console >/dev/console 2>&1
