#!/data/data/com.termux/files/usr/bin/bash
# Android/Termux-side QEMU and Android-memory sampler. Run in a second
# Termux session before starting the bounded guest workload. Never kills QEMU.
set -u
if (( $# != 2 )); then
    printf 'usage: %s QEMU_PID LOG\n' "$0" >&2
    exit 2
fi
pid=$1
log=$2
[[ $pid =~ ^[0-9]+$ ]] || { echo 'QEMU_PID must be numeric' >&2; exit 2; }
[[ -d /proc/$pid ]] || { echo "no such process: $pid" >&2; exit 2; }
[[ ! -e $log ]] || { echo "refusing to overwrite: $log" >&2; exit 2; }
cmdline=$(tr '\000' ' ' <"/proc/$pid/cmdline" 2>/dev/null || true)
[[ $cmdline == *qemu-system-myemulator32* ]] || {
    echo "PID $pid does not look like qemu-system-myemulator32" >&2
    exit 2
}
umask 077
printf 'TERMUX QEMU HOST WATCH\nPID=%s\nSTART=%s\nEXE=%s\nCMDLINE=%s\n' \
    "$pid" "$(date '+%Y-%m-%d %H:%M:%S %z' 2>/dev/null || echo unknown)" \
    "$(readlink "/proc/$pid/exe" 2>/dev/null || echo unavailable)" "$cmdline" >"$log"
qemu_sha=$(sha256sum "/proc/$pid/exe" 2>/dev/null | awk '{print $1}' || true)
printf 'QEMU_SHA256=%s\n' "${qemu_sha:-unavailable}" >>"$log"
available_kb=$(awk '$1 == "MemAvailable:" {print $2; exit}' /proc/meminfo 2>/dev/null || true)
if [[ $available_kb =~ ^[0-9]+$ ]] && (( available_kb < 524288 )); then
    printf 'HOST_HEADROOM=LOW MemAvailable_kB=%s; do not run the guest workload\n' "$available_kb" >>"$log"
    echo "host memory headroom below 512 MiB (${available_kb} kB); workload not authorized by this monitor" >&2
    exit 3
fi
printf 'HOST_HEADROOM=READY MemAvailable_kB=%s\n' "${available_kb:-unavailable}" >>"$log"
stopped=0
stopped_signal=
stop_watch() { stopped=1; stopped_signal=$1; }
trap 'stop_watch INT' INT
trap 'stop_watch TERM' TERM
trap 'stop_watch HUP' HUP

sample=0
while (( stopped == 0 )) && [[ -r /proc/$pid/status ]]; do
    stamp=$(date '+%Y-%m-%d %H:%M:%S' 2>/dev/null || echo unknown)
    printf '\nSAMPLE=%s TIME=%s PID=%s\n' "$sample" "$stamp" "$pid" >>"$log"
    if [[ -r /proc/$pid/status ]]; then
        while IFS= read -r line; do
            case $line in
                Name:*|State:*|VmPeak:*|VmSize:*|VmRSS:*|RssAnon:*|RssFile:*|RssShmem:*|VmData:*|VmStk:*|VmExe:*|VmLib:*|VmSwap:*|Threads:*)
                    printf '%s\n' "$line" >>"$log" ;;
            esac
        done </proc/$pid/status
    fi
    if (( sample == 0 || sample % 10 == 0 )); then
        printf '%s\n' '--- QEMU SMAPS ROLLUP ---' >>"$log"
        if [[ -r /proc/$pid/smaps_rollup ]]; then
            while IFS= read -r line; do printf '%s\n' "$line" >>"$log"; done </proc/$pid/smaps_rollup
        else
            printf 'smaps_rollup unavailable\n' >>"$log"
        fi
        printf '%s\n' '--- ANDROID MEMINFO ---' >>"$log"
        if [[ -r /proc/meminfo ]]; then
            while IFS= read -r line; do
                case $line in
                    MemTotal:*|MemFree:*|MemAvailable:*|Buffers:*|Cached:*|SwapTotal:*|SwapFree:*|AnonPages:*|Mapped:*)
                        printf '%s\n' "$line" >>"$log" ;;
                esac
            done </proc/meminfo
        fi
        printf '%s\n' '--- HOST OOM / PRESSURE ---' >>"$log"
        if [[ -r /proc/vmstat ]]; then
            while IFS= read -r line; do
                case $line in oom_kill\ *|pgmajfault\ *|allocstall\ *) printf '%s\n' "$line" >>"$log" ;; esac
            done </proc/vmstat
        else printf 'vmstat unavailable\n' >>"$log"; fi
        if [[ -r /proc/pressure/memory ]]; then cat /proc/pressure/memory >>"$log"; else printf 'memory pressure data unavailable\n' >>"$log"; fi
        available_kb=$(awk '$1 == "MemAvailable:" {print $2; exit}' /proc/meminfo 2>/dev/null || true)
        if [[ $available_kb =~ ^[0-9]+$ ]] && (( available_kb < 262144 )); then
            printf 'HOST_LOW_HEADROOM_STOP_GUEST MemAvailable_kB=%s\n' "$available_kb" >>"$log"
            echo "host MemAvailable below 256 MiB; stop the guest workload now" >&2
            sync
        fi
    fi
    sample=$((sample + 1))
    if (( sample % 5 == 0 )); then sync; fi
    sleep 1
done
if [[ -r /proc/$pid/status ]]; then
    printf '\nWATCH_STOPPED_BY_SIGNAL=%s QEMU_STILL_PRESENT=yes TIME=%s\n' "$stopped_signal" \
        "$(date '+%Y-%m-%d %H:%M:%S' 2>/dev/null || echo unknown)" >>"$log"
else
    printf '\nQEMU_PID_DISAPPEARED=%s TIME=%s\n' "$pid" \
        "$(date '+%Y-%m-%d %H:%M:%S' 2>/dev/null || echo unknown)" >>"$log"
fi
sync
printf 'host watch ended: %s\n' "$log"
