#!/bin/sh
# REM update 20261008 — in-guest verification (BusyBox-safe; no awk/objdump dependency).
#   sh /home/dev/rem-update-20261008/verify.sh          userland/kernel checks (~1 min)
#   sh /home/dev/rem-update-20261008/verify.sh full     + compiler qualification + Samurai (long)
PATH=/usr/local/bin:/usr/bin:/bin:/usr/sbin:/sbin; export PATH
KIT=${REM_KIT:-/home/dev/rem-native-userland}
LOG=/tmp/rem-update-verify.log; : > $LOG
f=0
ck() { if "$@" >>$LOG 2>&1; then echo "PASS $*"; else echo "FAIL $*"; f=1; fi; }
ck test -r /proc/mounts
ck test -r /proc/partitions
ck test -r /proc/cmdline
ck test -d /sys/block/myemu0
ck sh -c 'mount | grep -q " / "'
ck sh -c 'df / | grep -q /'
ck sh -c 'df -h / | grep -q /'
ck sh -c 'stat -f / | grep -q -i blocks'
ck sh -c 'echo abc | grep -E "a|z"'
ck sh -c 'echo abc | wc -l | grep -q 1'
ck sh -c 'printf AB | od -An -tx1 | grep -q 41'
ck sh -c 'find /etc -type f -name inittab | grep -q inittab'
ck sh -c 'echo 1 2 | awk "{print \$2}" | grep -q 2'
ck sh -c 'echo /bin/bu* | grep -q busybox'
ck sh -c 'rm -f /tmp/.rl; ln -s /etc /tmp/.rl && test -L /tmp/.rl && rm /tmp/.rl'
sh -c 'exit 3'; rc=$?
if [ "$rc" = 3 ]; then echo "PASS exit status 3"; else echo "FAIL exit status ($rc)"; f=1; fi
echo "--- root filesystem"
df -h /
grep ' / ' /proc/mounts
if [ "${1:-}" = full ]; then
    [ -r /etc/profile ] && . /etc/profile >/dev/null 2>&1
    PATH=/usr/local/bin:/usr/bin:/bin:/usr/sbin:/sbin:$PATH; export PATH
    cd "$KIT" || { echo "FAIL kit missing: $KIT"; exit 1; }
    echo "--- compiler qualification (log: $KIT/rem-update-compiler.log)"
    if bash bootstrap/rebuild-required-native.sh > rem-update-compiler.log 2>&1 &&
        grep -q '^COMPILER_REQUIRED_PORTING_TESTS_OK$' rem-update-compiler.log; then
        grep '^PASS \|^COMPILER_REQUIRED' rem-update-compiler.log
    else
        echo "FAIL compiler qualification"; tail -n 15 rem-update-compiler.log; f=1
    fi
    if [ "$f" = 0 ]; then
        echo "--- Samurai (log: $KIT/rem-update-samurai.log)"
        if bash bootstrap/build-samurai-native.sh > rem-update-samurai.log 2>&1 &&
            grep -q '^SAMURAI_NATIVE_EXECUTION_OK$' rem-update-samurai.log; then
            grep '^SAMURAI_' rem-update-samurai.log
        else
            echo "FAIL Samurai"; tail -n 15 rem-update-samurai.log; f=1
        fi
    fi
fi
if [ "$f" = 0 ]; then echo REM_UPDATE_VERIFY_OK; else echo "REM_UPDATE_VERIFY_FAILED (details: $LOG)"; fi
exit $f
