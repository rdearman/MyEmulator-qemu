# REM R4 Android idle termination investigation

Date: 2026-10-04

## Result

The latest Fold observation does not identify why QEMU disappeared. `ps` showing
no QEMU process after the event proves only that the process was gone when
checked. The supplied report has neither the QEMU exit status nor the serial
tail/Android kill reason, so it cannot distinguish a guest shutdown/panic path
from a host crash or Android process kill. No root cause for the Fold process
termination is established, and no change is approved for the R4 installation.

## Exact package tuple checked

From `/tmp/REM-FLIGHT-R4-ANDROID-20261002`:

| Artifact | SHA-256 |
| --- | --- |
| Android `qemu-system-myemulator32` | `88bf42458d88d1fbe5169cc49a3d3d250bb07695185164ac3ad4c861a7422de8` |
| `vmlinux` | `576c1f4fc599e02554e625463d1cdc0455c3e99caff4f047c06fe3c2e537d619` |
| `rootfs.ext4` | `3420b0e67bd252c985839b29a7b72d3668f21b50e3aef870a25e858a0cbc946f` |

The packaged kernel and rootfs remained unchanged during this work. Desktop
tests used a disposable copy at
`/tmp/rem-r4-android-termination-soak-rootfs.ext4`.

## HALT, exit, wake and re-entry audit

The HALT instruction in `qemu/target/myemulator32/translate.c` emits
`gen_next_pc`, explicitly stores `sr`, `r13`, and `r15`, then invokes the
noreturn helper. `helper_halt` sets both target and generic halted flags,
assigns `EXCP_HLT`, then calls `cpu_loop_exit`. TCG's ordinary helper-call path
in `.qemu-upstream/tcg/tcg.c` saves TCG globals unless call metadata explicitly
sets `TCG_CALL_NO_WRITE_GLOBALS`; `noreturn` alone does not suppress saving.
Thus the architectural PC and global register state are committed before the
helper. On wake, QEMU checks `has_work`; `cpu_exec_interrupt` handles EXITTB,
NMI and eligible hard IRQ; target interrupt entry saves the exception frame;
RFE restores PC/SR/visible SP and clears halted state. This follows the same
overall pattern as ARM WFI (`target/arm/tcg/op_helper.c`) and RISC-V WFI
(`target/riscv/op_helper.c`). This source audit found no additional missing
TCG global at the HALT call boundary.

## Confirmed source defect and limited fix

`myemulator32_cpu_set_irq` previously changed `env->irq_asserted` before
acquiring the BQL on one path, while the vCPU's `has_work` and interrupt
selection read the byte concurrently. That is a C data race; weakly ordered
AArch64 can expose stale/lost pending state more readily than x86. The source
now serializes IRQ updates under BQL and uses QEMU atomic fetch-or/fetch-and/read
for the shared pending byte. All target reads, including diagnostics, use
atomic reads. This fixes the identified IRQ-state race. It is not proven to
cause QEMU process termination; a lost IRQ would more directly explain a missed
wakeup than a host process exit.

Changed source copies:

- `qemu/target/myemulator32/cpu.c`
- `qemu/target/myemulator32/helper.c`
- `.qemu-upstream/target/myemulator32/cpu.c`
- `.qemu-upstream/target/myemulator32/helper.c`

The files contain other pre-existing local changes. Do not replace whole files
from this note; review the focused IRQ-state changes in the diff.

## Host build and tests

Android cross-build reused `/tmp/rem-r3-android-arm64-build`:

```sh
ninja -C /tmp/rem-r3-android-arm64-build qemu-system-myemulator32
```

It completed. Candidate SHA-256:
`d543024c046e61905506f27edd79df08a223020ec0ef521c60979d594e1729f5`.
`file`/`readelf` confirmed AArch64 Android PIE with interpreter
`/system/bin/linker64`; it was not run on an Android device and is not approved
for Fold installation.

Desktop native build completed with `ninja -C .qemu-build
qemu-system-myemulator32`; resulting SHA-256:
`52174d980682da16e193308f2f40283f1873d2f829c9300c1dc6bed150eb1214`.

The patched desktop QEMU previously passed a 600-second idle-at-login run
(HMP quit, exit 0, login prompt present, no guest panic). The preserved R4
desktop control at
`/tmp/rem-qemu-halt-fix-correct/qemu-system-myemulator32.clean`, SHA-256
`dfe02146fe089825abb15996b0f8fdf9daecf4fbf902303ce8fdc1ed36da816e`, also
passed a matched 600-second idle-at-login run (HMP quit, exit 0, login prompt
present, no guest panic). These runs are regression checks, not proof against a
Fold-specific failure or a long-duration failure.

An attempted extra 1,200-second test was stopped before launch when copying the
1 GiB disposable rootfs exhausted `/tmp`; the partial copy was removed. The
R4 source rootfs was not modified. All QEMU processes started for these tests
were stopped; no QEMU process remains from this investigation.

## Android build comparison and remaining discriminator

The Android build uses NDK Clang 17 targeting AArch64 Android API 28, `-O2`,
and the `myemulator32-softmmu` target. The desktop build is x86-64 with GCC 13
and `-O2`. Both configurations use `-fno-strict-aliasing`, `-fwrapv`, and
zero-initialization options recorded in their build configurations. The main
remaining host execution difference is AArch64 TCG code generation/runtime;
desktop-only testing cannot establish Android process stability.

The next useful Fold capture must preserve evidence outside the guest: timestamp
and serial tail, QEMU exit status if the launcher survives, and Android process
termination/low-memory evidence if available. Without that, further idle
soaks cannot classify guest failure versus host termination. Do not retry
the Fold until a controlled capture can distinguish these cases.

## Passive capture prepared for the next natural termination

The separate Termux wrapper is `android/r4/capture-r4-session.sh`. Its
installer and instructions are `android/r4/install-capture-r4.sh` and
`android/r4/CAPTURE-R4-README.txt`. It verifies the qualified QEMU hash before
launch, then calls the existing `~/rem/launch-rem.sh` without changing that
launcher or its machine/device arguments. It does not set trace variables,
restart QEMU, or touch the guest image. A FIFO plus `tee` displays the
interactive serial/monitor stream while saving the combined output; QEMU input
is still read directly from `/dev/tty`. Host sampling uses plain `ps -A` for
Termux compatibility, `/proc/<pid>/status`, and `/proc/meminfo`.

Each capture directory is written under `~/rem/r4-captures/` with restrictive
permissions. It records start/end UTC and epoch times, QEMU PID and `/proc`
argv, QEMU/kernel/rootfs hashes and resolved paths, periodic host process/memory
samples, QEMU wait status, elapsed seconds, full console output, and a separate
200-line tail. Immediately after an exit it attempts logcat all/crash buffers,
`dumpsys activity exit-info`, `dmesg`, tombstone listing and device properties.
Each unavailable or denied source is recorded without aborting collection.
The finished archive is available at `~/rem/r4-captures/latest.tar.gz`, with a
timestamped archive and SHA sidecar retained alongside it.

If Android terminates Termux itself, no in-process wrapper can run post-exit
collection. Logs and periodic samples are still written as the VM runs. The
wrapper's `--finalize` mode, run after reopening Termux, archives interrupted
capture directories without launching another VM and marks the unavailable
QEMU wait status honestly.

The next observation is to classify evidence, not presume a cause:

| Case | Evidence to inspect |
| --- | --- |
| A. Guest panic/shutdown | Console tail shows guest panic/poweroff sequence; QEMU wait status shows whether it exited normally. |
| B. QEMU host crash/abort | QEMU wait status/signal, host stderr, crash log or tombstone reference. |
| C. Android kills QEMU | QEMU wait status commonly indicates SIGKILL if the wrapper survives; inspect `lmkd`, memory-pressure and application-exit records. |
| D. Other Termux/process termination | Interrupted capture has no final wait status; inspect available Android process/Termux evidence and use `--finalize`. |

This wrapper is passive but has not been run on the Fold. Termux may deny
logcat, dumpsys, dmesg or tombstone access; those denials are captured as
UNAVAILABLE. Desktop syntax and package checks do not prove Android terminal
behavior; the qualified R4 QEMU remains unchanged and is the only binary the
wrapper will launch.

## Fold capture received 2026-10-04

The capture archive received from the Fold is
`/tmp/REM-R4-FOLD-CRASH.tar.gz` (SHA-256
`99547f4a3a6f593754c8f27fb0408947183be8277d3679917d01e32f0da68e9a`).
It records the qualified QEMU (`88bf42458d88d1fbe5169cc49a3d3d250bb07695185164ac3ad4c861a7422de8`),
kernel (`576c1f4fc599e02554e625463d1cdc0455c3e99caff4f047c06fe3c2e537d619`),
and rootfs (`b59f2ef952c1328129dd0a632d2e57b01cbd3fe4e1b4755bac2d43d750c7a9d5`).
The session ran for 232 seconds and QEMU exited with status 0. The console ends
with `qemu-system-myemulator32: terminating on signal 1`; this is SIGHUP, not a
guest panic or a QEMU crash report. No kernel panic or guest fault appears in
the captured console tail.

This event is classified as D (Termux/process or terminal-session lifecycle),
with evidence pointing to the Termux activity/PTY teardown: logcat records the
activity becoming invisible at 22:24:05 and TermuxService `onUnbind` plus
activity destruction at 22:25:05.943–22:25:05.949, matching QEMU's 13:25:06Z
exit. The wrapper survived. Host memory samples show QEMU RSS rising to about
248 MiB and then plateauing; Android reported ample available memory and no
matching LMK/crash record. `dmesg` was denied (exit 159), tombstone listing was
denied, and ActivityManager exit-info required unavailable DUMP permission.
Thus this capture does not support A (guest panic/shutdown), B (QEMU abort), or
C (memory-pressure kill). It does not establish why the activity was destroyed
or prove whether a deliberate Termux/app lifecycle event caused it. It also
does not establish the cause of the earlier idle Fold termination; retain that
as unresolved. No QEMU, kernel, rootfs, or launcher changes were made from this
capture.

The captured launcher invocation reports the QEMU path twice in its recorded
argv (executable and first argument); it still used the verified QEMU binary
and exact hashes above. The Fold launcher hash was
`0ecffd6512f7bf5a570d354eb93a1e90640db5e699bd99d7338dff5a2bb59518`, so it is
not byte-identical to the launcher in the earlier Android package. This is
provenance to retain for a later launcher review, not evidence that the
duplicate argument caused this SIGHUP.

### Bundle and exact Fold procedure

Bundle:
`/tmp/REM-R4-PASSIVE-CAPTURE-20261004.tar.gz`

Bundle SHA-256:
`cfac7fa997c3c3a8239b97679d63286e78128900dfa815c8bfac585cd6f3448a`

On Termux, the complete copy/install block is:

```sh
cd "$HOME"
scp rick@100.70.35.84:/tmp/REM-R4-PASSIVE-CAPTURE-20261004.tar.gz .
printf '%s  %s\n' 'cfac7fa997c3c3a8239b97679d63286e78128900dfa815c8bfac585cd6f3448a' 'REM-R4-PASSIVE-CAPTURE-20261004.tar.gz' | sha256sum -c -
mkdir -p "$HOME/rem-capture-update"
tar -xzf "$HOME/REM-R4-PASSIVE-CAPTURE-20261004.tar.gz" -C "$HOME/rem-capture-update"
cd "$HOME/rem-capture-update/REM-R4-PASSIVE-CAPTURE-20261004" && sha256sum -c SHA256SUMS && bash install-capture-r4.sh
```

Use this one command for normal human operation with capture:

```sh
cd "$HOME/rem" && ./capture-r4-session.sh
```

After QEMU exits, the archive is at
`$HOME/rem/r4-captures/latest.tar.gz`; the timestamped archive and raw session
directory remain there too. If Termux itself was killed, reopen Termux and run
`bash "$HOME/rem/capture-r4-session.sh" --finalize`; this archives the saved
session without launching the VM. No immediate crash reproduction is requested.

## Second Fold capture received 2026-10-04

`/tmp/REM-R4-FOLD-CRASH-2.tar.gz` has SHA-256
`62123d8667fdb59e25d428524d1f5aedaee9b64e3d5c4813ebb0c939654d78c3`.
It again used the verified R4 QEMU (`88bf42458d88d1fbe5169cc49a3d3d250bb07695185164ac3ad4c861a7422de8`)
and kernel (`576c1f4fc599e02554e625463d1cdc0455c3e99caff4f047c06fe3c2e537d619`).
The rootfs path/inode/size are unchanged from the prior capture, while its
content hash is now `6e4e01972c5f9a2f65ac9842d366afa76843fa63ed3a33bbd1b1512ab54e5bcf`.

This run lasted 124 seconds and QEMU again exited status 0 after printing
`terminating on signal 1` (SIGHUP). The guest reached the dev Bash prompt and
listed its home directory; there is no guest panic in the console. Logcat again
has no QEMU crash or low-memory-killer match, and privileged dmesg/tombstone/
ActivityManager exit details remain unavailable. TermuxActivity became
invisible/stopped at 22:36:26; TermuxService `onUnbind` and activity destruction
occurred at 22:37:26.605–22:37:26.616, coinciding with QEMU's 13:37:30Z exit.
This independently repeats the prior D-class lifecycle/SIGHUP signature and
further argues against a guest panic, QEMU abort, or Android memory-pressure
kill for these two captured exits. It still does not identify why Android
destroyed the activity or explain the earlier idle termination. No runtime
artifacts were changed.

## Fold/Termux Regression Timeline and Change Audit

### Baseline and observed chronology

The last human-reported stable/useful Fold system was the installed R4 image:
normal login and shell worked; the Fold QEMU was the qualified R4 binary, SHA
`88bf42458d88d1fbe5169cc49a3d3d250bb07695185164ac3ad4c861a7422de8`, and the
kernel was SHA `576c1f4fc599e02554e625463d1cdc0455c3e99caff4f047c06fe3c2e537d619`.
The guest rootfs was writable and mutable; no contemporaneous Fold hash proves
its exact contents at the last stable report. The original published R4 ZIP
contains rootfs SHA `3420b0e67bd252c985839b29a7b72d3668f21b50e3aef870a25e858a0cbc946f`
and launcher SHA `9bc1c6e9a5f438ddb9c57b84d9bc89075d6d3e55f831209f463beb69660f4676`.

Chronology supported by conversation records and artifacts:

1. Fold Make OOM was reproduced. The user then ran the guest-side
   `rem-make-memory-audit.sh` with `/usr/bin/make` and `make-trace`; immediately
   afterward they reported that the whole Termux session disappeared. The
   first report has no host capture, timestamp, Termux process state, or Android
   exit reason, so temporal order is known but causality is not.
2. Termux instability was then reported even without another Make diagnostic.
   No exact first-event time or Android system log was preserved.
3. A diagnostic Android QEMU was used in an earlier Fold investigation. Its
   package contains only QEMU plus README/checksum, and its documented action
   was to save the old QEMU, copy the diagnostic binary over the active QEMU,
   and invoke `MYEMU_HALT_TRACE=1 ./launch-rem.sh`. Subsequent Fold captures
   prove the original qualified QEMU hash is active again.
4. The R4 admin changes were applied inside the guest rootfs. Separately, the
   scripts-only audit update could replace launcher/audit scripts under
   `~/rem` and `~/remvm`, backing up prior scripts and hashing rootfs before
   and after. The current Fold launcher hash equals the audit-update ZIP's
   `launch-rem.sh` exactly (`0ecffd6512f7bf5a570d354eb93a1e90640db5e699bd99d7338dff5a2bb59518`).
5. The Make host-diagnostic bundle was prepared. Its host watcher was not
   evidenced as installed or run; the Fold was explicitly told to stop Make
   testing. The corrected chibicc/Samurai bundle README says not to install
   until Termux is stable; no Fold-side install is evidenced.
6. The passive capture package was installed later. Captures on Oct 4 used the
   exact qualified QEMU/kernel, but both ended when Termux's activity/session
   was torn down. Therefore that later wrapper cannot explain the earlier
   first disappearance.

The first precise captured teardown is capture 1: activity stopped/invisible
at local 22:24:05; about 60 seconds later TermuxService logged `onUnbind` and
activity destruction, and QEMU exited after SIGHUP at 13:25:06Z. Capture 2
repeats this: activity stopped at 22:36:26; `onUnbind`/destroy at 22:37:26.605
and QEMU's SIGHUP exit at 13:37:30Z. Both QEMU exits were status 0. These are
precise Termux lifecycle correlations, not evidence identifying what caused
Android to stop/destroy the activity. The historical idle `pid=0` guest fault
is a separate failure class and is not present in either captured console.

### Fold-side artifacts and effect audit

| Artifact/action | Persistent scope and code effects | Finding |
| --- | --- | --- |
| Original R4 launch path | `launch-rem.sh` execs the QEMU directly with the R4 machine, disk, serial mux, `signal=off`, `-icount`, and no `-snapshot`; sets child-only `LD_LIBRARY_PATH`. | Baseline serial/stdin/stdout are directly attached to Termux stdio. |
| `REM-R4-ROOTFS-AUDIT-UPDATE.zip` | Replaces only `launch-rem.sh`, recovery launcher, and audit helper in existing package directories; saves timestamped script backups; asserts rootfs hash unchanged. Audit helper logs path/inode/size/hash/argv via `tee`; launcher then `exec`s QEMU with the same R4 argument list. | This did persist host-side scripts and `~/rem-r4-rootfs-audit.log`; no profile, service, QEMU, or rootfs write is in the updater. Current launcher hash exactly matches this package. |
| Admin update/recovery | Installer writes account/sudo files inside guest `/`; recovery launcher starts the same package rootfs with `init=/bin/sh`. | Guest-only filesystem changes; no Termux host settings/process controls. |
| Make OOM discriminator package | Guest script runs baseline then trace Make sequentially, sets `REM_ALLOC_TRACE=1` only for the trace child, samples guest `/proc`, and waits. No signal trap, `stty`, `ulimit`, `setsid`, `nohup`, host service or profile command. | Can create guest logs and Make side effects; it cannot persistently set Termux environment or service state. It does not signal QEMU. First disappearance followed this run, but no contemporaneous host evidence proves a causal link. |
| Make host-diagnostic bundle | `make-bounded.sh` uses `ulimit` only in the guest shell and TERM/KILL only against its child Make PID. `termux-watch-qemu.sh` samples `/proc`/memory and traps INT/TERM/HUP only to stop its own loop; it contains no QEMU kill. | No evidence the watcher/bounded bundle was run on the Fold. Even if run as written, limits are process-local; it does not persistently alter Termux. |
| HALT diagnostic QEMU | ZIP has a QEMU binary, README, checksums; no installer, profile or service scripts. The documented `MYEMU_HALT_TRACE=1 ./launch-rem.sh` is a one-command environment assignment. | The variable lasts only for that invocation. A binary replacement was temporary; captured active hash proves qualified QEMU restored. The documented action replaces only QEMU, but running any QEMU uses the writable rootfs and can persist guest-file changes; no evidence it changed Termux configuration or Android service state. |
| chibicc/Samurai update | Installer is intended for the REM guest; stages and backs up `~/.local/bin/chibicc` and `samu` inside that guest. | No host Termux changes; Fold execution is unconfirmed and package README explicitly says wait for stability. |
| Passive capture package | Installer copies only `capture-r4-session.sh` beside launcher. While active it starts launcher as a child, redirects QEMU stdout/stderr through a FIFO and `tee`, gives QEMU `/dev/tty` for stdin, samples host state, and waits. No `stty`, `setsid`, `nohup`, `disown`, wake-lock, `am`, service, profile, or persistent environment edits. | It changes QEMU parentage/I/O only for that invocation. It was installed after the first reported Termux loss, so it cannot be the original cause. It may affect the precise HUP/PTY behavior of the two captures and must not be conflated with the earlier event. |
| Termux transfer commands | `scp`, `tar`, `sha256sum`, `bash`, `mkdir`, and script copy operations write package/log files under `$HOME`/`~/rem`; SSH may add a host key to `~/.ssh/known_hosts`. | No command in supplied procedures changes Termux profiles, shell startup, global environment, job-control configuration, Android services, wake locks, or system permissions. Actual Fold shell history is unavailable. |

### Launcher/process comparison from evidence

The audited launcher changed from package hash `9bc1c6e9…` to
`0ecffd6512…`; the latter adds rootfs identity logging and sources the audit
helper. Its QEMU option vector is otherwise the same as the original R4
launcher: `myemulator32`, 128 MiB, same kernel/rootfs paths, raw writable disk,
user netdev/forward, `-display none`, stdio mux with `signal=off`, serial and
monitor on that mux, `-icount shift=0,sleep=off`, and `-no-reboot`. No snapshot
option is present. Both use `exec` for QEMU and do not configure a background
service or wake lock.

The capture wrapper changes the process topology: QEMU's PPID is the capture
script; captured `/proc/stat` shows its process group and terminal foreground
PGID are the wrapper PID, session is the Termux shell session, and TTY is
`pts/0`. QEMU stdin is `/dev/tty`; QEMU stdout/stderr go to a FIFO whose reader
`tee`s to the terminal and file. The plain launcher instead execs QEMU in the
foreground job with normal stdio. Exact signal dispositions and open-FD
ownership at teardown were not captured. `signal=off` configures the stdio
character backend's Ctrl-C handling; it does not suppress the observed SIGHUP.
The QEMU `/proc/PID/cmdline` capture shows the executable path twice; the
launcher source's explicit argument vector does not add this second path.
This representation needs Android-specific argv confirmation and is not, by
itself, evidence of an extra QEMU option.

### Ranked assessment, eliminated changes, and gaps

1. **Strong evidence for the two captured exits:** Termux activity/terminal
   teardown coincides to the second with QEMU receiving SIGHUP; QEMU handles
   it and exits status 0. Captured host memory is ample; there is no matching
   LMK/crash evidence. This explains those captured exits, not necessarily the
   earlier uninstrumented disappearance.
2. **Possible contributor limited to captured runs:** the capture wrapper's
   child process and FIFO/`tee` console plumbing. It changes per-run parentage
   and stream FDs but not persistent Termux settings; no A/B capture using the
   plain launcher exists.
3. **Temporal association, causality unproven:** the guest Make OOM run
   preceded the first report that Termux disappeared. Script inspection finds
   no host-side process/session/service mutation. The guest may OOM-kill Make;
   the first Termux event has no host evidence to classify.
4. **Eliminated as the current active binary:** diagnostic QEMU is absent from
   both captures; QEMU hash matches qualified R4. The one-shot trace environment
   assignment cannot survive the command. This does not prove what files were
   present immediately after its earlier manual replacement.
5. **No evidence for a persistent host configuration regression:** audited
   artifacts do not edit `.profile`, `.bashrc`, Termux properties, Android
   service state, wake locks, resource limits, or global PATH. The known
   persistent host-side changes are scripts/backups, audit log, capture
   wrapper/logs, and QEMU backup/restore files. Rootfs hashes changed from the
   published base to capture 1 and capture 2 (`3420…` -> `b59f…` -> `6e4e…`),
   showing guest image bytes changed; no offline Fold rootfs diff identifies
   which guest writes account for it. They do not alter Termux process state.

**Answer to the primary question:** repository and package evidence does not
show Make/QEMU diagnostics persistently changing Termux's execution
environment. The Make run is the earliest reported temporal antecedent, but
the contemporaneous evidence needed to prove or disprove causality is absent.
The current captures instead show an explicit SIGHUP coincident with Termux
activity/service teardown. The later capture wrapper changes live process
topology and console FDs, so it remains a candidate for the captured-run
behavior only; it cannot explain the earlier first report.

Missing evidence: Fold shell history and timestamps; hashes of Termux startup
files and Termux properties before/after; Android logcat/exit reason from the
first disappearance; direct-launch process-group/FD snapshot; and the exact
manual sequence that replaced/restored diagnostic QEMU. No Fold reproduction
is requested. If a read-only snapshot is later authorized, one command can
record current launcher, startup files, environment, and Termux properties:

```sh
cd "$HOME/rem"; printf 'time=%s pwd=%s\n' "$(date -Is)" "$PWD"; sha256sum launch-rem.sh launch-rem-admin-recovery.sh capture-r4-session.sh qemu-system-myemulator32 vmlinux rootfs.ext4 "$HOME/.bashrc" "$HOME/.bash_profile" "$PREFIX/etc/bash.bashrc" "$PREFIX/etc/profile" 2>&1; env | sort; ls -la "$HOME/.termux" "$HOME/.ssh" 2>&1; getprop | grep -i termux
```

This snapshot cannot recover historical before-state; it only records the
current one. `dumpsys activity exit-info`/logcat around a naturally occurring
event would be needed to attribute future activity destruction, but no new
event should be induced for this audit.

Git limitation: the R4 Termux scripts and many investigation artifacts are
untracked in this checkout, and the relevant R4 packaging state is not
represented by a sequence of committed changes. Git history therefore cannot
prove when Rick copied or ran those files; the packaged files and supplied
Fold captures are the available evidence. The HALT QEMU ran against the normal
writable guest disk, so guest files could have changed during its session;
that is distinct from changing Termux host configuration. The two rootfs
hashes prove the image contents changed after the original package, but not
which session made each change.

## Persistent Termux console candidate (separate from guest idle fault)

Date: 2026-10-04

This candidate addresses the captured host/session sequence only:

```text
Termux terminal/activity teardown -> SIGHUP to foreground QEMU -> QEMU status 0
```

It does not fix or claim to fix the historical guest `pid=0,
arch_cpu_idle+0x30` fault. It does not change QEMU, kernel, rootfs, launcher,
or guest account/tool files. The candidate sources are in
`android/r4/persistent/`. The deployable package is
`/tmp/REM-R4-PERSISTENT-TERMUX-20261004.tar.gz`, SHA-256
`b0b4055e8c0ecffee6787c6bfa99a991e0649b7c889396ae2d072d1a38d88fbb`.
The installer accepts only the installed QEMU SHA-256
`88bf42458d88d1fbe5169cc49a3d3d250bb07695185164ac3ad4c861a7422de8`.

### Candidate design

`start-rem.sh` uses QEMU `-daemonize -pidfile` and replaces stdio serial/monitor
with a private Unix socket character device (`server=on,wait=off`). QEMU's
normal daemonization creates a separate session without a controlling terminal;
the interactive Termux client is not its parent or terminal process group.
`rem-console-client` puts the attaching terminal in raw mode, forwards all
bytes (including Ctrl-C / 0x03), treats Ctrl-] as detach, and restores the
terminal on detach, HUP, INT, TERM, or a broken socket. `rem-console.sh` attaches
to an existing instance; `start-rem.sh` reconnects if already active.

A per-install atomic startup directory prevents concurrent managed starts.
Before launch, scripts inspect `/proc` for QEMU processes that have the same
rootfs by pathname or open-file device/inode identity. A running managed PID is
validated against the executable, live process state, and image. Stale pidfiles
and locks are removed only when no process owns the image. The old
`launch-rem.sh` remains untouched for rollback; do not invoke it while the
managed candidate is running. Since that legacy launcher does not participate
in the candidate's startup lock, this guard cannot stop someone deliberately
starting the legacy launcher after the candidate. The candidate commands do
prevent accidental duplicate starts through the new interface and refuse an
already-running legacy QEMU.

QEMU's existing R4 arguments are retained: `myemulator32`; the same append
string and 42x132 geometry; 128 MiB; exact installed `vmlinux` and
`rootfs.ext4`; raw writable `myemulator2-disk` with `file.locking=off`; user
network and `127.0.0.1:2304` forward; `-icount shift=0,sleep=off`; and
`-no-reboot`. `LD_LIBRARY_PATH` includes the package `lib` directory and
Termux `$PREFIX/lib`. No monitor is exposed on the human console. The bounded
manager log rotates above 256 KiB to one previous file; guest serial output is
not written to an unbounded log. The Unix socket and state directory are
private to the Termux home directory.

No wake lock or Termux service is added. The observed event is a terminal
session SIGHUP, which daemonization/session separation is designed to avoid.
A wake lock addresses CPU sleep, not terminal HUP. This does not guarantee
survival if Android force-stops or kills the Termux application process/UID.
That distinction requires the Fold test.

### Off-Fold checks

- Android console client is an AArch64 Android PIE using `/system/bin/linker64`;
  dynamic dependencies are only `libc.so` and `libdl.so`. It was rebuilt from
  the included C source with NDK 26.1 API 28 Clang; SHA-256
  `6701f14e852838e48363f2741c401efa71e5a20c802efb8974b2f6ac6d8798d9`.
- Shell syntax checks pass. QEMU, kernel, and source rootfs hashes were checked.
- With the desktop QEMU and a `-snapshot` test invocation, the guest reached
  `dev`/Bash. Shell command, foreground `sleep`, Ctrl-C interruption, socket
  detach/reconnect to the same shell, and second-start refusal (same QEMU PID)
  passed. A local Unix-socket client test separately confirmed byte 0x03
  passthrough, Ctrl-] detach, and clean client handling of SIGHUP.
- The desktop R4 package rootfs has no `sudo`; direct `root/remdev` login was
  rejected. Therefore clean guest poweroff could not be tested on that exact
  rootfs. Only the disposable `-snapshot` QEMU was terminated by host signal
  after the console checks. Source rootfs remained SHA-256
  `3420b0e67bd252c985839b29a7b72d3668f21b50e3aef870a25e858a0cbc946f`.
  This is not a clean-shutdown PASS.
- During a separate daemonized desktop boot, login output took longer than the
  initial 20-second wait; reconnecting and pressing Enter displayed the login
  prompt. The README documents Enter as a prompt redraw after a blank reattach.
- A second public-launcher test exercised start, login, shell command, guest
  Ctrl-C during `sleep`, Ctrl-] detach, reconnect to the same shell, command
  execution after reconnect, and client SIGHUP. All passed. QEMU stayed alive
  after client SIGHUP. The disposable QEMU was then stopped by host SIGTERM
  because this desktop rootfs has no working guest shutdown credentials.
- No Android lifecycle test has been performed. Do not describe the candidate
  as Fold-qualified until it survives a natural Termux activity/terminal
  teardown and reconnects to the same guest.

The archive was freshly extracted under
`/tmp/REM-R4-PERSISTENT-TERMUX-20261004-EXTRACT-CHECK`; all `SHA256SUMS`
entries passed. Installer fixture tests passed for the approved QEMU hash,
preserved the prior `launch-rem.sh`, and refused a deliberately wrong QEMU.
Shell syntax, a package path containing spaces, stale pidfile/lock cleanup,
and the Android helper's ELF interpreter/dependencies were checked. The source
rootfs hash remained unchanged. These host checks do not replace the Fold
activity-teardown test.

### #6 REM32 fix integration queue

### #6 qualified fix review (2026-10-04)

`docs/CODEX-6-REM32-FIX-HANDOFF.md` now marks
`REM32-EMACS-IRQ-STACK-001` **QUALIFIED**, limited to initializing Linux's
architectural double-fault stack pointer (DFSP). Review of the exact source
diff confirms `head.S` assigns `__myemulator2_df_stack_top` to `dfsp` before
installing vectors and reserves a page-aligned 4-KiB BSS emergency stack.
Only `vmlinux` needs rebuilding for this source change; QEMU/rootfs do not.

I independently reran `tests/kernel/check-dfsp-initialized.py` against kernel
`/home/rick/Development/Active/MyEmulator-qemu/.linux-build/emacs-dfsp-build/vmlinux`
(SHA-256 `58ec11e8280f1a9f7b421e26b08f691d6b4f1477b59f69837cf5a8e5e8c2945d`)
and QEMU SHA-256 `52174d980682da16e193308f2f40283f1873d2f829c9300c1dc6bed150eb1214`.
The test passed: DFSP was `0x00643000`, matching the kernel symbol; QEMU
exited and the focused test found no remaining instance. Result:
`/tmp/rem-r4-dfsp-independent/result.json`.

This fix is eligible for the next kernel release candidate, but is not included
in the persistent-console update because that update explicitly preserves the
Fold's kernel. It is not evidence that the historical `arch_cpu_idle` panic
or Emacs loadup is fixed. `r15` corruption and `nanosleep` remain
`INVESTIGATING` / recorded deficiency, respectively.

I then attempted R4 distribution boot/login with this candidate kernel on the
same disposable snapshot tuple. It emitted repeated `MYEMU_TTYTRACE switch`
console lines during early boot; I stopped that exact test QEMU before human
login acceptance. The focused DFSP regression remains PASS, but this debug
kernel build is not suitable for a human Fold candidate. A clean kernel build
with the DFSP change and without trace flooding is required before integrating
the fix into a distributable R4 tuple. No Fold files were changed.

### Persistent-console candidate final archive refresh

The candidate source now invokes the console shell through Bash so its startup
path is testable on Linux while retaining Termux's installed Bash interpreter.
It also recovers a lost pidfile only when one live QEMU matches the approved
executable, rootfs identity, and exact private console socket. The refreshed
archive/checksum are recorded in the final handoff. During one disposable test
setup error, a non-snapshot desktop QEMU briefly opened the package rootfs; I
stopped that exact process, preserved the changed image as
`/tmp/rem-r4-persistent-test/rootfs.ext4.after-raw-open`, restored the release
rootfs byte-for-byte from the untouched R4 ZIP, and verified SHA-256
`3420b0e67bd252c985839b29a7b72d3668f21b50e3aef870a25e858a0cbc946f` plus clean
read-only `e2fsck -fn`. All subsequent public-launch tests used `-snapshot`.

### Fold auto-attach update (2026-10-04)

Rick's Fold test confirms the persistent-console lifecycle basics on the
installed approved R4 QEMU: Termux activity teardown did not stop the guest;
opening Termux and running `cd "$HOME/rem"; ./rem-console.sh` attached to the
same running VM (reported PID 6888); Ctrl-C reached the guest. The remaining
UX gap is that this manual command must not be needed after returning to
Termux.

Termux upstream documents `TermuxService` as a foreground service that may
outlive `TermuxActivity`, with the Activity reopened to access existing
sessions. Bash reads `.bashrc` for interactive non-login shells and
`.bash_profile` for interactive login shells; the R4 update uses these user
startup files because no per-Activity resume shell hook is provided in the
documented lifecycle. References: [TermuxService](https://github.com/termux/termux-app/blob/master/app/src/main/java/com/termux/app/TermuxService.java),
[Bash startup files](https://www.gnu.org/software/bash/manual/html_node/Bash-Startup-Files.html).

Separate update source: `android/r4/autoconnect/`; archive:
`/tmp/REM-R4-AUTO-ATTACH-20261004.tar.gz`, SHA-256
`ed2ea9c141c5d28839afc26f1f519ef439cd5587e88829cc51280c1d0846683b`. It
installs a guarded user-level hook in `~/.bashrc` and the
first existing Bash login startup file (`~/.bash_profile`, `~/.bash_login`, or
`~/.profile`). The hook invokes `~/rem/rem-auto-console.sh`, which uses the
existing qualified `start-rem.sh` to start-or-attach, retries attachment after
unexpected console-client/terminal disconnects only while the existing guest
remains alive, and returns to the ordinary Termux shell on Ctrl-]. The client
now distinguishes intentional Ctrl-] (status 0) from terminal loss (125),
socket loss (1), and signals (128+signal). The installer verifies the exact
approved R4 QEMU SHA before modifying files and backs up existing startup
files under a unique directory in `$HOME`. It does not change QEMU, kernel,
rootfs, existing `launch-rem.sh`, Android-global settings, or acquire a wake
lock.

Desktop checks cover syntax, marker idempotence, Bash interactive and login
startup, noninteractive bypass, recursion guard, maintenance bypass, HUP
reconnect policy, Ctrl-] return, installer refusal on an unapproved QEMU,
Termux home paths containing spaces, profile fallback, and existing shell-file
preservation.
The Fold result above verifies persistent QEMU, same-VM manual reconnect, and
Ctrl-C passthrough. Automatic attach for a newly created Termux session and
auto-reconnect when a surviving shell receives HUP still need Fold testing;
desktop checks cannot validate Android Activity recreation behavior.

### Fold auto-attach maintenance-mode UX update (2026-10-04)

Rick reports that normal Termux startup successfully detected and reattached to
existing REM QEMU PID 15191. The remaining failure was shell access: the Bash
startup hook immediately attached again whenever he tried to return to the
Termux host to run maintenance commands.

The incremental update in `android/r4/autoconnect/` now makes Ctrl-] a durable
maintenance transition. An intentional detach writes
`$HOME/rem/.rem-maintenance-mode`; the startup hook checks it before invoking
the auto-console and exports a guard so child interactive Bash shells (including
`exec bash -i`) also stay in Termux. `rem-maintenance.sh` gives an explicit
maintenance-shell entry; `rem-resume.sh` removes the marker and execs an
interactive Bash so automatic REM start/attach resumes. `rem-status.sh` prints
running/not-running state, QEMU PID(s), matching image-owner count, and console
socket state. `rem-console.sh` remains a manual reconnect path and marks
maintenance after Ctrl-].

The Android console client's exit status was tightened: only the Ctrl-] byte
returns success (intentional detach); socket/terminal loss returns failure so
the auto-console can reconnect; Ctrl-C remains forwarded as a guest byte in
raw terminal mode. This update changes only Termux scripts/startup files and
the small Android console client. It does not modify QEMU, kernel, rootfs,
`launch-rem.sh`, or guest files. The prior package's QEMU hash check remains
mandatory.

Desktop checks passed for interactive startup-hook invocation, Ctrl-] marker
handling, fresh and nested interactive-shell maintenance bypass, explicit
maintenance entry, resume-to-auto-entry, install paths containing spaces, and
package checksums. The console client was rebuilt as an Android AArch64 PIE
with `/system/bin/linker64`; desktop cannot execute that Android binary. The
Fold has already demonstrated start/attach detection for PID 15191, but this
maintenance-mode update has not yet been installed on the Fold. In particular,
Fold acceptance remains required for Ctrl-] returning to a persistent Termux
shell, status output, manual reattach, and activity/session recreation without
a second QEMU.

Incremental maintenance UX package:
`/tmp/REM-R4-AUTO-ATTACH-20261004.tar.gz`, SHA-256
`fa9272d8d06bd054537cb58e3023ff8923d563bed9ea0bf1d80df8e83ac95825`.
The earlier archive hash in the initial auto-attach section is superseded by
this maintenance-mode revision. Fresh extraction and all package SHA256SUMS
entries passed. Disposable Termux-home tests passed install with spaces,
interactive non-login and login Bash auto-entry, Ctrl-] maintenance marker,
fresh/nested-shell bypass, explicit maintenance entry, resume-to-auto-entry,
manual reconnect wrapper marker handling, stopped/duplicate status formatting,
and reconnect-after-client-disconnect policy. An Android AArch64 console
client was rebuilt and statically checked for `/system/bin/linker64` and
`libc`/`libdl` dependencies. Desktop cannot prove Android Activity recreation;
the revised Ctrl-] flow/status command still require Fold UAT. The approved
QEMU, kernel, rootfs and `launch-rem.sh` were not modified.

### Fold foreground/resume UX report (2026-10-04)

Rick reports that after switching from Termux to another Android app and back,
the REM console appears unusable until he presses Ctrl-C. Ctrl-C is a guest
interrupt and must not be required for ordinary resume. This is a new UX
failure, separate from the earlier Termux SIGHUP/QEMU-exit events. It does not
show that QEMU exited or that the console disconnected: the user has not
reported a changed PID or reconnect message for this case.

The existing console client blocks in `poll()` for host terminal input or
serial-socket output and forwards received input bytes unchanged. It does not
send Ctrl-C on resume. Termux upstream's Activity calls its session/view
`onStop()` and `onResume()` handlers; the service/session may outlive the
Activity. Those sources establish Activity/view lifecycle callbacks, but do not
explain this specific Fold symptom. The present report cannot distinguish a
prompt/display redraw issue from host PTY input/focus or an active guest command;
no evidence proves that a guest command is stuck, that PTY input is lost, or
that TERMUX recreated the terminal session.

A new separate candidate was added to `android/r4/autoconnect/`. The client
handles SIGCONT and SIGWINCH as host-side events. It samples the Termux PTY
foreground process group, raw termios state and window size; if raw mode was
lost, it restores the original raw mode. It writes only bounded host-side
event records to `~/rem/.rem-console-client.log` (64 KiB plus one previous
file). It sends NO byte to the REM guest on these events; in particular there
is no synthetic Ctrl-C or other wake-up input. This can expose whether an
activity/size transition reaches the client and whether the PTY mode changed,
and can repair lost raw mode without disturbing the guest.

This is not yet a proven zero-keystroke fix. If the next Fold return shows no
client resume/resize event and the UI still requires Ctrl-C, the remaining
boundary is Termux Activity/view/keyboard focus or guest screen redraw; a
Termux-side test must establish which. If SIGWINCH is logged with changed
window dimensions, the fixed guest tty size (`myemu.tty_rows/cols`) and lack of
serial resize signaling are relevant, but no dynamic resize fix is included
without that evidence. Do not claim ordinary app-switch qualification from
desktop tests.

Candidate archive:
`/tmp/REM-R4-CONSOLE-RESUME-20261004.tar.gz`, SHA-256
`cc94ae6d48185892193bb4f43088c3442b5702f0d3f2fb50e130ac76fa44eaff`.
Fresh extraction checksum validation passed. Desktop shell/profile and
maintenance-marker tests still pass. The client compiles as Android AArch64
PIE for `/system/bin/linker64` and retains Ctrl-C byte forwarding and Ctrl-]
detach logic; the resume handler contains no guest socket write. Actual
repeated Fold foreground/background cycles and zero-key resume remain
untested and are required before calling this fixed. QEMU, kernel, rootfs and
rollback launcher remain unchanged.

### Termux command consolidation candidate (2026-10-05)

The next UX candidate removes the global Bash auto-attach hook instead of
adding another hook guard. It exposes only two supported commands in Termux
PATH: `rem` (start-or-attach) and `remctl` (`status`, `console`, `stop`,
`shell`, `recover`). Both call one implementation in `~/rem/.remctl/remctl`.
New Termux shells remain ordinary host shells; `remctl shell` opens a regular
maintenance Bash with explicit bypass variables. The user types the one-word
`rem` when they want the REM console. A second Termux session that invokes
`rem` while another console client owns the console is told that REM is already
attached and does not start QEMU or a competing client. No startup profile is
created or hooked.

The manager holds an atomic start lock before launching and checks QEMU
processes by executable plus rootfs path/inode. The console has a separate
atomic client lock and scans active console clients. `remctl status` reports
VM state, QEMU PID/count, console attachment, rootfs writer count, and stale
state. `remctl recover` never starts or kills a process: with one managed QEMU
it repairs only its PID record; with zero writers it clears state only when no
live manager/client is present; with multiple or unmanaged writers it refuses
all cleanup and prints their PIDs. `remctl stop` does not inject text into a
guest being used by another session; orderly shutdown remains `sudo poweroff`
at the REM shell.

The installer checks the complete update package and the exact qualified R4
QEMU SHA-256 `88bf42458d88d1fbe5169cc49a3d3d250bb07695185164ac3ad4c861a7422de8`
before changing scripts. It preserves old persistent/autoconnect scripts in
`~/rem/.remctl/legacy/`, backs up startup files before removing only the
previous marked/known auto-attach lines, and leaves `launch-rem.sh`, QEMU,
kernel, rootfs, and guest data untouched. This package has not been installed
or tested on the Fold. Desktop qualification can establish shell parsing,
installer safety, process/lock classification, refusal on duplicate writers,
second-session exclusion, client-death recovery, and no QEMU launch during
recovery. Android Activity retention/recreation and real Termux PTY behavior
remain device-only checks. In particular, no claim is made that a replacement
Termux session is auto-attached: avoiding the auto-attach trap means the user
opens Termux to a host shell and enters `rem` if the old session was destroyed.

This explicit-command design supersedes the previous Bash auto-attach and
Ctrl-] maintenance candidates. Desktop qualification was run with
`android/r4/rem-command/qualify-desktop.sh`: path-with-spaces install,
startup-hook removal, preserved legacy/rollback files, ordinary Bash shell,
`remctl shell` without attach, stale PID/socket/locks, stale PID repair for one
managed VM, second-session refusal, competing startup lock, two simultaneous
start commands using a QEMU stub (one synthetic writer), client death and
reconnect lock cleanup, and duplicate-writer recovery refusal all passed.
The wrong-QEMU hash refusal also passed without changing startup files. The
test used a synthetic `/proc` tree and QEMU stub; it did not execute the
Android QEMU or open/modify the approved rootfs. No QEMU was started by the
qualification. Desktop tests do not prove Android Activity retention or real
Termux console interaction. If duplicate rootfs writers already exist,
`remctl recover` intentionally refuses to kill either and therefore cannot
automatically converge that unsafe state; shut each down from its own guest
console, then run recovery. A legacy direct-launch QEMU is detected as a
writer and blocks `rem` until it is orderly powered off.

Candidate bundle:
  directory `/tmp/REM-R4-REMCTL-20261005`
  archive `/tmp/REM-R4-REMCTL-20261005.tar.gz`
  archive SHA-256 `47235b5b8652ce238a4204a3240217fbf09919d8a2dba570cf0cd2d5d7985d2c`

The archive was extracted into a fresh directory and all packaged SHA256SUMS
entries and shell syntax checks passed. Desktop qualification remains the
only performed runtime qualification; the candidate is not yet Fold-tested.

### Fold console-client TTY and stop-control finding (2026-10-05)

Fold report: `rem` started QEMU PID 12954, announced attach, then returned to
Termux with `console client requires an interactive terminal`; status showed
one managed writer and a detached console. The prior packaged manager was
verified from `/tmp/REM-R4-REMCTL-20261005.tar.gz` (remctl SHA-256
`9a7d783f06ecdc1dfeab2b2ac33a86f8c985d95f94fcb62ae80e651e49d02b72`). Its
`rem_attach` started the client with `&`, saved `$!`, and waited. Bash connects
an asynchronous job's stdin to `/dev/null` by default; this explains the
client's `isatty(STDIN_FILENO)` failure even when `rem` starts from a visible
Termux terminal. The current update invokes the client in the foreground. A
desktop PTY regression exercises `rem` start-and-attach and explicit attach to
an existing synthetic managed VM; the client asserts stdin/stdout/stderr are
TTYs and `/dev/tty` exists. Both tests pass. No Fold retest has yet occurred.

The Fold process listing reported exactly one QEMU/rootfs writer, PID 12954.
The manager's launch argv contains `-monitor none` and configures no QMP or
other management socket. A running process cannot acquire a management
endpoint after launch. The custom machine's clean shutdown path is its guest
MMIO poweroff request (`sudo poweroff` inside REM). QEMU `quit`, SIGTERM, and
SIGKILL do not invoke that guest path and are not safe substitutes for guest
sync/unmount. The scripts-only update restores console access to the exact
current guest through `rem`; it cannot provide a host-side graceful shutdown
for PID 12954. A future out-of-band shutdown requires a cooperative control
path and cannot be retrofitted into this running process. `remctl stop` now
states this limitation and sends no signal.

Scripts-only foreground-TTY update candidate:
`/tmp/REM-R4-REMCTL-PTYFIX-20261005.tar.gz`, SHA-256
`0a5996465fac63620508130dd249c4588d083dce0974e6e5437f9d068168bc1b`.
Fresh extraction SHA256SUMS validation passed. The desktop qualification passed
all manager tests, including `rem` first-start under a controlling PTY, attach
to the same synthetic running VM under a PTY, all three standard descriptors
as terminals, `/dev/tty`, and no-signal stop refusal when no management
endpoint exists. The package installer only replaces Termux commands/scripts;
it does not change QEMU, kernel, rootfs, or launch-rem.sh. It can be installed
while the single managed QEMU remains running. Fold attachment still requires
verification after installation.
# 2026-10-05 Fold HALT-fix candidate package

The Android candidate `/tmp/rem-qemu-android-current` matches SHA-256
`63e979cd890881cf7ec36249ea376a8911ba29328edc0f676be42e82a444d252`.
The binary is an AArch64 Android PIE with `/system/bin/linker64`; DWARF names
`/tmp/rem-android-qemu-current-src/target/myemulator32/translate.c`. That file
matches the canonical workspace translator, which writes SR/R13/R15 before
the HALT helper. Source/build details and #1's reported 1,921-cycle and
30-minute desktop results are recorded in the R4 idle-HALT update package.
Neither result is a physical Fold qualification.

`/tmp/REM-R4-IDLE-HALTFIX-20261005.tar.gz` is a reversible QEMU-only update.
It installs the candidate beside the old executable and selects it through
the existing `rem`/`remctl` implementation. The original
`qemu-system-myemulator32` remains intact; `qemu-system-myemulator32.r4-backup`
is created only if absent and is never overwritten. The installer accepts
only the original R4 QEMU hash or this candidate lineage, checks the existing
PTY-fixed console client hash, and does not start/stop the VM. An already
running original R4 instance remains attachable and is not replaced. After
guest `sudo poweroff`, the next `rem` starts the candidate. The one-command
rollback script refuses while any process has the rootfs open, restores the
previous PTY-fixed manager, and leaves both QEMU binaries in place.

The manager explicitly unsets inherited MYEMU trace environment toggles for
the candidate launch. It does not set tracing, change the kernel/rootfs, edit
guest files, or alter `launch-rem.sh`. Static tests verified package hashes,
installer lineage refusal logic, disposable install/rollback, unchanged
original QEMU/kernel/rootfs hashes, and preservation of the console-client
PTY fix. Android execution and the idle soak remain required on the Fold.

Static ELF inspection also found that this candidate has no `libslirp.so.0`
DT_NEEDED entry and no `slirp_*` dynamic imports, unlike the original R4 QEMU.
User-mode SLIRP networking is therefore not claimed for this candidate; use
the rollback script after orderly guest shutdown to return to the original
network-capable QEMU if needed.

Package archive SHA-256: `24ca038f9292cfeac187ac0fa795760621ee30dd19fe8f1ee1de33f52909b186`.
Fold rollback command after a clean guest shutdown:
`bash ~/rem/rollback-r4-idle-haltfix.sh`. Install can run while the old
managed VM remains active; it neither opens nor changes the rootfs.
