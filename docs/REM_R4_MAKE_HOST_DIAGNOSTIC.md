# R4 Fold Make failure: host-side bounded diagnostic

## Evidence and revised classification

The Fold previously produced guest-kernel OOM evidence for the uninstrumented
Make process: about 114 MiB anonymous RSS in a guest with about 120 MiB managed
memory. A later run of an allocation-instrumented Make plus audit script made
the entire Termux session disappear. That later observation is a distinct
failure boundary. It is not evidence that guest OOM alone killed Termux, and
does not yet identify whether Android killed the app/QEMU or QEMU exited.

The host-side cause is therefore **unresolved**. Plausible paths include
Android low-memory killing the Termux app or QEMU, QEMU host allocation growth,
or an independent QEMU exit. The old instrumentation may increase guest
pressure, but this has not been measured on the Fold. No new Fold crash is
needed to proceed.

Guest RAM is backed by memory owned by the QEMU host process. As Make touches
more guest pages, host QEMU RSS can rise too; TCG translation/cache allocations
can add separate host RSS. The paired samples compare those curves.
`smaps_rollup` reports VMA categories, but does not identify TCG code-cache
ownership by itself.

## Bounded next diagnostic

`tools/rem-make-diagnostics/termux-watch-qemu.sh` samples QEMU's `/proc`
memory totals, selected Android `/proc/meminfo` and `/proc/vmstat` counters,
memory-pressure data where available, and periodically syncs its host log. It
records the QEMU executable and arguments and only observes; it never signals
QEMU.

`tools/rem-make-diagnostics/make-bounded.sh` runs the **existing unmodified**
`/usr/bin/make clean` inside REM. It first applies a 48 MiB `RLIMIT_AS`, then
uses a 32 MiB RSS cutoff and 60-second deadline as additional guards. If Bash
cannot apply the address-space limit, it does not start Make. It writes and
syncs the guest log. This avoids both the old allocation instrumentation and
the known ~114 MiB failure point.

Run the host watcher in a separate Termux session before the bounded guest
command. Full instructions are in
`tools/rem-make-diagnostics/README-host-bounded.txt`.

## How to interpret the pair

* Guest log records a cutoff or allocation failure, while QEMU remains and
  Android memory remains available: the guest growth was bounded; this run
  provides no host-failure evidence.
* QEMU PID disappears while Termux remains: inspect its last host sample and
  guest log to separate process exit/kill from guest OOM.
* The whole Termux app disappears: a same-app watcher cannot report afterward;
  the periodically synced host log preserves recent process and pressure
  samples. This is consistent with app-level termination, not conclusive proof
  of Android LMK without system-side kill evidence.

Do not use the previous `make-trace`/allocation-audit command again for this
diagnostic. Keep Make investigation separate from the accepted Stage-7
chibicc artifact and Samurai work.
