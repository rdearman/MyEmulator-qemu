#!/bin/sh
# Test-only wrapper to preserve the exact arguments Make passes to native ld.
echo "CWD=<$PWD>" >>/tmp/ld-argv.log
for arg do
    echo "<$arg>" >>/tmp/ld-argv.log
done
exec /root/native/bin/ld.real "$@"
