REM Fold Make diagnostic: bounded host/guest evidence collection
===============================================================

Purpose
-------
The previous instrumented run coincided with the entire Termux session
disappearing. That result does not establish whether Android killed Termux,
Android killed QEMU, QEMU exited/crashed, or the guest OOM-killed Make and an
independent host event occurred. This bundle does not rerun make-trace.

It pairs two logs from the same run:

* termux-watch-qemu.sh runs OUTSIDE REM and samples the QEMU process RSS,
  anonymous/file RSS, virtual memory, Android MemAvailable/swap, vmstat OOM
  counters, and memory pressure. It records the QEMU executable/command line
  and whether its PID disappears. It never signals or kills QEMU.
* make-bounded.sh runs inside REM against the existing, unmodified Make.
  RLIMIT_AS is the primary guard (default 48 MiB); a 32 MiB RSS cutoff and
  60-second deadline are fallback stops. It refuses to start if Bash cannot
  apply RLIMIT_AS. The workload log is created in the guest and synced.

This intentionally diagnoses only `make clean` first. Do not run the previous
allocation-instrumented binary or remove the bounds. If Make reaches the
address-space limit, allocation failure before the normal 114 MiB point is a
safe diagnostic result, not proof that Make is fixed.

Use
---
1. Put termux-watch-qemu.sh in Termux home and make it executable.
2. Start REM normally. In a second Termux session, identify the QEMU PID:
       ps -A -o PID,ARGS | grep '[q]emu-system-myemulator32'
   Run the watcher with that PID and a new log name, e.g.:
       bash ~/termux-watch-qemu.sh PID ~/rem-qemu-host-watch.log
   Proceed only if it reports `HOST_HEADROOM=READY`; it refuses the run below
   512 MiB reported Android MemAvailable when that value is available. If the
   monitor later warns `HOST_LOW_HEADROOM_STOP_GUEST`, interrupt the bounded
   guest command immediately.
3. In the REM guest, transfer/copy make-bounded.sh to /home/dev, then:
       cd /home/dev/remedit
       bash /home/dev/make-bounded.sh /home/dev/make-bounded-fold.log /usr/bin/make clean
4. The guest command should end by itself, by a logged safety cutoff, or by
   the bounded Make returning an error. Do not rerun it if the Termux session
   disappears. The monitor log is in Termux home; the Make log is in REM.
5. Stop the watcher with Ctrl-C after the guest command ends. If Termux itself
   is killed, reopen Termux and inspect the last periodically synced host log.

Interpretation
--------------
* Guest Make log says `SAFETY_CUTOFF` / allocation error, QEMU PID remains,
  host memory remains available: guest Make growth was bounded; no host death
  observed.
* Host log shows QEMU PID disappearance while Termux remains: QEMU exited or
  was killed; guest log and Android OOM/pressure counters help distinguish.
* Termux session/app disappears and the host log ends without a final marker:
  this is consistent with Android killing the Termux app, but is not by itself
  proof; the last host memory/OOM samples are the evidence to inspect.

Known limit: a sampler running within the Termux app cannot keep running if
Android kills the whole app process. Periodic sync preserves recent samples;
Android system-level `lmkd` logs may require separate device diagnostics.
