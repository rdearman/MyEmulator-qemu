#!/usr/bin/bash
echo BASH_PID1_START
/usr/bin/true
echo BASH_FOREGROUND_TRUE_RC:$?
/usr/bin/cat /tmp/cat-input
echo BASH_FOREGROUND_CAT_RC:$?
/usr/bin/true &
child=$!
wait "$child"
echo BASH_BACKGROUND_TRUE_RC:$?
/usr/bin/cat /tmp/cat-input >/tmp/bash-cat-output &
child=$!
wait "$child"
echo BASH_BACKGROUND_CAT_RC:$?
echo BASH_PID1_READY
exec /usr/bin/bash -i </dev/console >/dev/console 2>&1
