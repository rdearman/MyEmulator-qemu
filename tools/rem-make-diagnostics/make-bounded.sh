#!/bin/bash
# Run unmodified Make with both an address-space ceiling and an early RSS/time
# cutoff. The address-space limit is the primary safety barrier: it prevents a
# rapid allocation burst from racing the /proc sampler into the known OOM.
set -u
if (( $# < 2 )); then
    printf 'usage: %s LOG MAKE [ARGS...]\n' "$0" >&2
    exit 2
fi
log=$1
shift
if [[ -e $log ]]; then
    printf 'refusing to overwrite diagnostic log: %s\n' "$log" >&2
    exit 2
fi
umask 077
: >"$log" || { printf 'cannot create log: %s\n' "$log" >&2; exit 2; }
printf 'BOUNDED MAKE DIAGNOSTIC\nCWD=%q\nCOMMAND=' "$PWD" >>"$log"
printf '%q ' "$@" >>"$log"
rss_limit=${REM_MAKE_DIAG_RSS_KB:-32768}
as_limit=${REM_MAKE_DIAG_AS_KB:-49152}
wall_limit=${REM_MAKE_DIAG_WALL_SECONDS:-60}
for n in "$rss_limit" "$as_limit" "$wall_limit"; do
    [[ $n =~ ^[0-9]+$ ]] || { echo 'limits must be decimal integers' >&2; exit 2; }
done
(( rss_limit >= 8192 && rss_limit <= 32768 )) || { echo 'RSS cutoff must be 8192..32768 KB' >&2; exit 2; }
(( as_limit >= 32768 && as_limit <= 49152 )) || { echo 'address-space limit must be 32768..49152 KB' >&2; exit 2; }
(( wall_limit >= 10 && wall_limit <= 60 )) || { echo 'wall cutoff must be 10..60 seconds' >&2; exit 2; }
if ! ulimit -v "$as_limit" 2>/dev/null; then
    printf 'RLIMIT_AS_UNAVAILABLE: refusing unsafe unbounded run\n' >>"$log"
    echo 'cannot apply virtual-memory limit; workload was not started' >&2
    exit 2
fi
printf '\nRSS_CUTOFF_KB=%s RLIMIT_AS_KB=%s WALL_CUTOFF_SECONDS=%s SAMPLE_SECONDS=0.10\n' \
    "$rss_limit" "$as_limit" "$wall_limit" >>"$log"
printf 'RUNNER_LIMITS:\n' >>"$log"
ulimit -a >>"$log" 2>&1

"$@" >>"$log" 2>&1 &
child=$!
start=$SECONDS
cutoff=none
sample=0
while [[ -r /proc/$child/status ]]; do
    rss=0
    state='?'
    vmdata='?'
    vmpeak='?'
    while read -r key value rest; do
        case $key in
            State:) state=$value ;;
            VmRSS:) rss=$value ;;
            VmData:) vmdata=$value ;;
            VmPeak:) vmpeak=$value ;;
        esac
    done <"/proc/$child/status" 2>/dev/null || true
    [[ -r /proc/$child/status ]] || break
    printf 'SAMPLE=%s ELAPSED=%s PID=%s STATE=%s VmRSS_kB=%s VmData_kB=%s VmPeak_kB=%s\n' \
        "$sample" "$((SECONDS - start))" "$child" "$state" "$rss" "$vmdata" "$vmpeak" >>"$log"
    if (( rss >= rss_limit )); then
        cutoff="rss-${rss_limit}kB"
        break
    fi
    if (( SECONDS - start >= wall_limit )); then
        cutoff="wall-${wall_limit}s"
        break
    fi
    if [[ $state == Z || $state == X ]]; then break; fi
    sample=$((sample + 1))
    sleep 0.10
done

if [[ $cutoff != none ]]; then
    printf 'SAFETY_CUTOFF=%s; sending TERM to Make PID %s\n' "$cutoff" "$child" >>"$log"
    kill -TERM "$child" 2>/dev/null || true
    for _ in 1 2 3 4 5; do
        [[ -r /proc/$child/status ]] || break
        sleep 0.10
    done
    if [[ -r /proc/$child/status ]]; then
        printf 'TERM did not stop Make; sending KILL to PID %s\n' "$child" >>"$log"
        kill -KILL "$child" 2>/dev/null || true
    fi
fi
wait "$child"
rc=$?
printf 'MAKE_EXIT_STATUS=%s CUTOFF=%s\n' "$rc" "$cutoff" >>"$log"
sync
printf 'bounded run complete: %s\n' "$log"
exit "$rc"
