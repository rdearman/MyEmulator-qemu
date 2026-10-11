SUPERSEDED — DO NOT RUN THIS ALLOCATION-TRACE DISCRIMINATOR

The Fold run of this paired Make/make-trace audit caused the entire Termux
session to disappear. It is retained below as historical documentation only.
Use README-host-bounded.txt and the host watcher plus bounded unmodified Make
instead. Do not rerun rem-make-memory-audit.sh or make-trace for this issue.

Historical REM Fold Make OOM discriminator (diagnostic only)

This is a paired capture, not a Make replacement and it does not modify the
R4 rootfs configuration. Run it from the exact project directory and pass the
same arguments that caused the known OOM. It runs the current Make once,
then the separate allocation-instrumented Make once, sequentially in one
guest session.

On Fold, put these two files in ~/remedit:
  rem-make-memory-audit.sh
  make-trace

Then run from ~/remedit:
  bash ./rem-make-memory-audit.sh /home/dev/make-memory-audit.log \
    /usr/bin/make ./make-trace

Add the failing Make arguments after ./make-trace, unchanged, for example
`clean`. Do not replace /usr/bin/make. The script records both executable
hashes, each child's PID/exit status, RSS/data/stack fields, statm, process
maps and smaps_rollup at startup and every 8 MiB RSS increase. It runs the
trace executable with REM_ALLOC_TRACE=1 so periodic allocation reports are
captured in the same log. OOM SIGKILL of Make is recorded as the child exit
status; the shell/audit script should remain alive to finish the capture.

Interpret the combined evidence, rather than RSS alone:

* A — Make heap allocations: [heap]/VmData/RssAnon grow and traced live
  requested bytes/sites broadly account for the resident growth.
* B — mappings/page residency: RSS grows in anonymous non-[heap] VMAs; maps,
  smaps_rollup, statm and minor-fault samples show where pages reside.
* C — allocator/brk discrepancy: heap/RSS grows far beyond traced live bytes,
  with valid tracer bookkeeping. Nonzero untracked/table_overflow makes this
  comparison incomplete and must be reported.
* D — allocation loop: cumulative bytes/count and a small set of allocation
  sites rise rapidly during the same command. This identifies hot sites for
  reduction; it does not by itself prove whether the loop is intended.

The traced binary has about 1.8 MiB extra BSS and changes timing. The baseline
run is first and is the uninstrumented measurement. The trace run's RSS may
reach OOM earlier. Both sections are required to interpret the result.
`smaps_rollup` may be absent on this kernel; maps/status/statm remain captured.

Host-side validation on 2026-10-04:
* bash -n passed.
* A memory-holding Python control with a log path containing spaces passed
  through both paired runs and exited 0.
* The audit captured rising VmRSS, statm, [heap]/anonymous mappings and
  smaps_rollup, and wrote AUDIT_COMPLETE.

No Fold run has been made with this paired capture. No root cause is claimed.
