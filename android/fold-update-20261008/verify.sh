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
    compiler_log=$KIT/rem-update-compiler.log
    echo "REM_PROGRESS|compiler|start|time=$(date '+%Y-%m-%dT%H:%M:%S%z')"
    bash bootstrap/rebuild-required-native.sh > "$compiler_log" 2>&1 &
    compiler_pid=$!
    while kill -0 "$compiler_pid" 2>/dev/null; do
        sleep 60
        if kill -0 "$compiler_pid" 2>/dev/null; then
            compiler_stage=$(sed -n 's/^REM_STAGE|//p' "$compiler_log" | tail -n 1)
            [ -n "$compiler_stage" ] || compiler_stage=stage-not-reported
            echo "REM_PROGRESS|compiler|heartbeat|time=$(date '+%Y-%m-%dT%H:%M:%S%z')|stage=$compiler_stage"
            tail -n 4 "$compiler_log" | sed 's/^/REM_LOG_TAIL|/'
        fi
    done
    compiler_rc=0
    wait "$compiler_pid" || compiler_rc=$?
    if [ "$compiler_rc" = 0 ] &&
        grep -q '^COMPILER_REQUIRED_PORTING_TESTS_OK$' "$compiler_log"; then
        echo "REM_PROGRESS|compiler|finish|time=$(date '+%Y-%m-%dT%H:%M:%S%z')"
        grep '^PASS \|^COMPILER_REQUIRED' rem-update-compiler.log
    else
        echo "REM_PROGRESS|compiler|failed|exit=$compiler_rc"
        echo "FAIL compiler qualification"; tail -n 15 rem-update-compiler.log; f=1
    fi
    if [ "$f" = 0 ]; then
        echo "--- Samurai (log: $KIT/rem-update-samurai.log)"
        samurai_log=$KIT/rem-update-samurai.log
        echo "REM_PROGRESS|samurai|start|time=$(date '+%Y-%m-%dT%H:%M:%S%z')"
        bash bootstrap/build-samurai-native.sh > "$samurai_log" 2>&1 &
        samurai_pid=$!
        while kill -0 "$samurai_pid" 2>/dev/null; do
            sleep 60
            if kill -0 "$samurai_pid" 2>/dev/null; then
                samurai_stage=$(sed -n 's/^CC samurai /compile-samurai-/' "$samurai_log" | tail -n 1)
                [ -n "$samurai_stage" ] || samurai_stage=build-step-not-reported
                echo "REM_PROGRESS|samurai|heartbeat|time=$(date '+%Y-%m-%dT%H:%M:%S%z')|stage=$samurai_stage"
                tail -n 4 "$samurai_log" | sed 's/^/REM_LOG_TAIL|/'
            fi
        done
        samurai_rc=0
        wait "$samurai_pid" || samurai_rc=$?
        if [ "$samurai_rc" = 0 ] &&
            grep -q '^SAMURAI_NATIVE_EXECUTION_OK$' "$samurai_log"; then
            echo "REM_PROGRESS|samurai|finish|time=$(date '+%Y-%m-%dT%H:%M:%S%z')"
            grep '^SAMURAI_' rem-update-samurai.log
        else
            echo "REM_PROGRESS|samurai|failed|exit=$samurai_rc"
            echo "FAIL Samurai"; tail -n 15 rem-update-samurai.log; f=1
        fi
    fi
fi
if [ "$f" = 0 ]; then echo REM_UPDATE_VERIFY_OK; else echo "REM_UPDATE_VERIFY_FAILED (details: $LOG)"; fi
exit $f
