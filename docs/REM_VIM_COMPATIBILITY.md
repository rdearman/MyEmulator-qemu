# REM Vim runtime compatibility

This note records the first verified runtime boundaries behind Vim 9.1's
startup `E1286` and default-save `E667` reports. It is a defect report, not a
claim that Vim currently passes its usability target.

## Verified tuple

- Kernel `/tmp/rem-yash-unified-build/vmlinux`, SHA-256
  `5bd96e5d7e9163d95e8470eb550504357d84e78d38cd269781dfb096a511638a`
- QEMU `REM-FLIGHT-TTY/qemu/desktop/qemu-system-myemulator32`, SHA-256
  `0154db549ad01d0bcdfd79c0dde9544266fc14c75fe7825d14942ff47a75641a`
- Probe: `toolchain/tests/rem-vim-syscall-probe.c`, statically linked against
  the available REM musl toolchain and run inside a disposable ext4 clone.

## Results and diagnosis

The guest probe reported:

```text
libc_fsync rc=-1 errno=38
raw_fsync_74 rc=0 errno=0
raw_fsync_82 rc=-1 errno=38
pthread_create rc=11 errno=0
libc_timer_create_sigev_thread rc=-1 errno=11
raw_timer_create_107 rc=-1 errno=38
raw_timer_settime_409 rc=-1 errno=38
```

`E667` is explained by a syscall-number ABI mismatch: REM musl maps `fsync` to
82, while the current REM kernel dispatcher implements it at 74. Raw 74
succeeds and raw 82 returns `ENOSYS` (38). The filesystem and Vim's file
writing path are therefore not the first failing boundary.

`E1286` occurs in Vim's `start_timeout()` path for `timer_create` with
`SIGEV_THREAD`. Musl implements that notification by starting a pthread;
`pthread_create` returns `EAGAIN` (11) in this guest, so this Vim error happens
before timer syscalls are attempted. Separately, raw timer syscall numbers 107
and 409 return `ENOSYS`. Vim's cross-configuration cache had guessed that
`timer_create` works, enabling this timeout feature in the build.

The source probe isolates fsync numbering and thread/timer behavior from Vim.
Build it as a static REM executable with the existing musl toolchain, copy it
to a disposable rootfs, and run `/root/rem-vim-syscall-probe` from Yash. The
output above is the expected reproduction on the tuple listed here.

## Current usability status

An existing Vim binary previously completed edit/save/readback only with an
`nofsync` workaround. Default save is not qualified. Attempts to rebuild Vim
with timeout-related features disabled also faulted before rendering the
buffer (`MYEMU_BAD_USER_FAULT`, low address); this is an independent userspace
runtime/ABI investigation and is not localized by the syscall probe. No
kernel, compiler, libc, Vim production binary, or flight image was changed by
this investigation.

Required follow-up belongs with the syscall/runtime owners: align the musl
and kernel fsync ABI; make thread creation and the POSIX timer interface work
or ensure Vim's build configuration disables unsupported timeout features;
then retest Vim from Yash using default settings.

## Runtime handoff

### Issue: `fsync` syscall number mismatch

- **Observed:** REM musl calls syscall 82. The guest gets `ENOSYS` (38).
  Direct syscall 74 succeeds; inspection of the current kernel dispatcher
  shows fsync handled at 74.
- **Expected:** libc and kernel use the same Linux-compatible syscall number
  and semantics for `fsync`.
- **Impact:** Vim's normal write path reports `E667: Fsync failed`; saving
  requires the previously used `nofsync` workaround, which is not accepted as
  normal-save validation.
- **Suggested owner:** REM syscall/kernel and musl ABI maintainers jointly.

### Issue: POSIX timer syscalls unavailable

- **Observed:** direct calls to `timer_create` 107 and `timer_settime` 409
  return `ENOSYS` (38) on the verified tuple.
- **Expected:** implement the timer syscall ABI used by musl, or make the
  unsupported capability visible to cross-build configuration so applications
  do not enable timer-backed features on a false positive.
- **Impact:** Vim's timeout feature cannot work through the kernel timer API;
  Vim's cross-config cache had nevertheless enabled it.
- **Suggested owner:** REM syscall/kernel owner, with toolchain configuration
  owner updating cross-build feature detection after runtime support is known.

### Issue: `pthread_create` returns `EAGAIN`

- **Observed:** the probe's `pthread_create` returns error 11 (`EAGAIN`);
  musl `timer_create` with `SIGEV_THREAD` consequently returns `EAGAIN` before
  reaching the timer syscalls. The probe establishes the failure, but not its
  underlying kernel/runtime cause.
- **Expected:** a supported thread-creation path for musl's pthread ABI, with
  a documented resource/error contract.
- **Impact:** Vim startup reports `E1286: Could not set timeout: Resource
  temporarily unavailable` when it enables this timeout path.
- **Suggested owner:** REM process/thread syscall and musl runtime owners.

## Deferred Vim validation

After the runtime issues are resolved, run terminal Vim from Yash with normal
settings and verify, in order:

1. Startup completes without `E1286` or another fatal startup error.
2. Open a new or existing file and insert text.
3. Save with ordinary `:w`/`:wq` and confirm there is no `E667`.
4. Quit back to a responsive Yash prompt.
5. Read the file back from Yash and compare the saved text.

The fresh default-settings attempt on the disposable clone stalled at the
terminal handoff and supplied no acceptance result. Do not count it as a Vim
startup or edit/save pass. No runtime changes are included in this handoff.

## Latest runtime retest (2026-09-27)

Retested the installed Vim without rebuilding it, using the latest documented
Yash-capable tuple and a disposable copy of its seed rootfs:

- QEMU SHA-256: `0154db549ad01d0bcdfd79c0dde9544266fc14c75fe7825d14942ff47a75641a`
- Kernel SHA-256: `5bd96e5d7e9163d95e8470eb550504357d84e78d38cd269781dfb096a511638a`
- Seed rootfs SHA-256: `8649389a39aefa45d9a1ed0b2b8438cde5396cbb1fe1991ee26d4633cff48d59`
- Vim: `VIM - Vi IMproved 9.1 (2024 Jan 02, compiled Sep 25 2026 08:33:01)`;
  binary SHA-256 previously recorded as
  `35874ba096f8036a15ae07a09985a5182ea86eacde8d287bae63138812ea06c4`.

`vim --version` returned to Yash and printed the version. Starting
`/usr/bin/vim /root/vim-retest.txt` did not produce a visible buffer, `E1286`,
or a shell prompt within the 60-second capture window. The guest trace showed
Vim's process reading in 1 KiB chunks and faulting in mapped pages; no
`MYEMU_BAD_USER_FAULT` or segmentation fault was captured. Since Vim did not
reach a visible editor state, `:w`, quit, and Yash readback were not reached.
This retest therefore confirms the version command only; it does not pass the
interactive workflow.

The latest retest did not reach the fsync path, so it neither confirms nor
clears the known fsync mismatch (`82` from musl versus `74` in the kernel),
timer syscall gaps (`107` and `409` return `ENOSYS`), or
`pthread_create`'s `EAGAIN`. The newly observed startup stall is at the Vim
interactive startup/terminal handoff boundary; its underlying cause remains
unresolved. Detailed log: `/tmp/rem-vim-final/latest-vim-retest.log`.

The next test, after the startup boundary is resolved, remains: start Vim;
open a file; insert text; save with ordinary `:w` or `:wq`; quit to a
responsive Yash prompt; and read the file back from Yash. Record separately
whether `E1286` appears at startup and whether `E667` appears on save.

## Terminal/session isolation

Added and ran `toolchain/tests/rem-vim-terminal-probe.c` as a static REM
binary (`rem-vim-terminal-probe`, SHA-256
`849f32fc72e832ddc74b9fa1c18779109b7b8b3cccacb0d08bc979d12611f537`) from
Yash on the QEMU/kernel tuple above. Results:

```text
isatty(stdin) rc=1 errno=0
tcgetattr(stdin) rc=0 errno=0
ioctl(TCGETS) rc=0 errno=0
ioctl(TCSETS) rc=0 errno=0
tcsetattr(TCSANOW) rc=0 errno=0
tcsetpgrp(stdin,getpgrp) rc=-1 errno=25
tcgetpgrp(stdin) rc=-1 errno=25
ioctl(TIOCGPGRP) rc=-1 errno=25
process_pgrp=21 process_sid=0
child_setpgid(0,0) rc=0 errno=0
child_setsid() rc=23 errno=0
controlling_tty_setsid rc=24 errno=0
ioctl(TIOCSCTTY) rc=0 errno=0
controlling_tty_tcsetpgrp rc=0 errno=0
controlling_tty_tcgetpgrp rc=24 errno=0
```

Here `isatty()==1` is success. Normal termios access works. The Yash-launched
probe has a process group but no controlling terminal: `tcsetpgrp` and
`tcgetpgrp` both fail with `ENOTTY` (25). `fork`, `wait`, `setpgid`, and
`setsid` work on this kernel. A child that calls `setsid()` and then
`ioctl(TIOCSCTTY)` successfully acquires `/dev/ttyMY0`, after which process
group get/set both succeed. This localizes the demonstrated handoff defect to
session/controlling-terminal setup in the init/login/shell launch path; it does
not demonstrate a missing `setpgid` syscall or broken termios ioctl ABI.

The prior Bash report that `setpgid` returned `ENOSYS` used kernel SHA-256
`c77547060463da37cf9730e41e799c94998ad9f6e24e2c1a3282ee3299215a63` and
rootfs SHA-256 `2c608032df8fddf82240d67a4ae42790ce2a439a5ad4968f6e9e8d495594c365`
(`/tmp/rem-bash-interactive-acceptance-20260927/report.json`). On the current
kernel `5bd96e…`, this probe's libc `setpgid(0,0)` succeeds. These results are
from different tuples; the old Bash failure is not evidence that `setpgid`
causes the present Vim stall.

The current kernel trace is selective rather than a complete syscall trace.
For direct Vim launch, its last captured syscall-class event is repeated
`read(fd=4, count=1024) -> 1024`; no terminal ioctl, process/session call, or
exit is captured before the 60-second timeout. Therefore this is the last
*observed* syscall, not proof that it was the last executed syscall. A
disposable launcher that performs `setsid()` and `TIOCSCTTY` logged successful
terminal acquisition and Vim `TCGETS` queries, but Vim still did not display a
buffer within 60 seconds. Missing controlling-terminal setup is a real
environment defect, but fixing it in the test launcher did not resolve the
Vim startup stall. The remaining stall owner is unresolved; save, quit, and
readback remain untested in this run.

The session/TTY setup requirement is documented in
`docs/REM_ARCHITECTURE64.md`. Suggested owner for integrating a controlling
session at boot is the REM init/login/shell-launch path. Do not attribute the
remaining Vim read-loop stall to that issue without further evidence.

## Controlled-TTY startup trace and ownership

Retested the unchanged Vim binary through the disposable launcher at
`/tmp/rem-vim-final/rem-vim-ctty-launcher`, which performs `setsid()` and
`TIOCSCTTY` before `exec`. On the Yash/current kernel tuple, the kernel trace
records successful `TIOCSCTTY` for Vim PID 22 and successful `TCGETS` on fds 0
and 1. Vim still did not paint a buffer or prompt during the 60-second wait.
This confirms that assigning a controlling terminal alone does not clear the
startup stall. Captured console/kernel output is in
`/tmp/rem-vim-final/ctty-vim-uat.log`. The tested kernel hash is
`5bd96e5d7e9163d95e8470eb550504357d84e78d38cd269781dfb096a511638a`, QEMU
hash is `0154db549ad01d0bcdfd79c0dde9544266fc14c75fe7825d14942ff47a75641a`,
and Vim hash is
`35874ba096f8036a15ae07a09985a5182ea86eacde8d287bae63138812ea06c4`.

The available guest trace is not a complete syscall trace: this kernel build
prints selected `read`, `ioctl`, `ppoll`, and exit events, but does not log all
syscall entry/return pairs, `open`, or `mmap`. Host `strace` cannot trace guest
syscalls, and QEMU `-d strace` produced no system-emulation guest trace. The
controlled-launch capture therefore establishes these observed phases, not a
complete syscall chronology:

- After terminal acquisition and mode queries, PID 22 performs repeated
  successful `read` calls on fds 4, 3, 5, 6, and 7, with requests of 1024 bytes
  and several EOF returns. No `read` from fd 0 is recorded before the timeout.
- The kernel records file-backed page faults for the Vim process while these
  reads occur. The mapped file paths are not included in this trace.
- No user fault, segmentation fault, Vim `E1286`, `E667`, or exit was observed.

**Ownership classification:** the stall is not explained by the previously
confirmed missing controlling terminal, and it is not waiting for keyboard
input on fd 0. The last observable startup activity is file/data reads and
file-backed page faults, so the current evidence points to the Vim
startup/runtime-loading path (application or libc file/mapping interaction).
The trace cannot distinguish Vim configuration from bundled/runtime data or a
libc/kernel file-loading path because it omits opened pathnames, mmap calls,
and other syscalls. No root cause or owner within that boundary is proven.
Timer/thread behavior is also not excluded by this selective trace; the
earlier timer syscall and `pthread_create` probes remain independent evidence.

For the next isolation pass, compare the same controlled launch with Vim
configuration disabled (`-Nu NONE -U NONE -i NONE -n`) and capture opened
paths plus all syscall entry/return values using a guest-capable tracer. The
kernel, libc, and Vim were not changed in this retest; save/quit/readback were
not reached.

## Configuration-disabled startup comparison (2026-09-27)

Compared `vim -u NONE -N` with plain `vim` on disposable copies of the older
REM Flight Development tuple. This is **not** the Yash-compatible kernel tuple
used by the preceding 2026-09-27 retest; the earlier kernel and rootfs under
`/tmp/rem-vim-final` were no longer available.

- QEMU SHA-256: `0154db549ad01d0bcdfd79c0dde9544266fc14c75fe7825d14942ff47a75641a`
- Kernel SHA-256: `c77547060463da37cf9730e41e799c94998ad9f6e24e2c1a3282ee3299215a63`
- Source test-rootfs clone SHA-256: `548e4c63d42b29fea07492687b9d474bf1039cd1cde79b23b97a39936ccb03bb`
- Post-test disposable images: minimal `6b9b9a5192980e4b4b59eae75ea2836d809d19406dfedca165412343961ad54d`; normal `6bca29dc4fe3880811424a6e9da2563fa36b93c3ee2c724c530433428e066548`
- Vim: 9.1.1724; interactive shell in this image: BusyBox ash
- Environment: `TERM=xterm`, `LANG=C`, `LC_ALL=C`; QEMU serial console,
  128 MiB guest memory
- Captures: `/tmp/rem-vim-config-compare/minimal-noinput.log`,
  `normal-noinput.log`, `minimal-edit.log`, and `ctty-edit.log`

Both modes emitted Vim's 9.1 splash before test input. Opening a new file
with `-u NONE -N` emitted the `[New]` buffer screen. Therefore skipping the
user vimrc did not change this tuple's initial screen behavior. This does not
show that configuration is irrelevant to the separate stall on kernel
`5bd96e…`; the tested tuples differ.

A temporary static launcher called `setsid()`, `TIOCSCTTY`, and
`tcsetpgrp()` before `exec`ing Vim. The launcher reported no setup error, but
Vim did not paint a buffer within 60 seconds on this c775 tuple. The kernel
logged `MYEMU_USER_IDENTITY_REPLACE pid=24 addr=00100000 user=1 cause=5` from
`linux/arch/myemulator2/mm/fault.c`; no fatal fault or Vim error was captured.
The message is an identity-map replacement diagnostic, not evidence by itself
that this event caused the stall. The capture cannot distinguish terminal
response handling, Vim/runtime file loading, or an unrelated kernel/runtime
fault after `exec`.

The observed failure remains at the interactive-input boundary: the splash
and new-file screen render, but the controlled-TTY attempt did not reach a
confirmed editing screen or return to the shell. No save/readback acceptance
occurred in this comparison; the attempted keystrokes from the direct,
non-controlling-terminal run are not counted as input validation. No
`E1286` or `E667` was present in these captures. Prior syscall-probe evidence
still explains the independent timer/thread and fsync issues; see the
runtime-handoff section above.

**Workaround status:** `vim -u NONE -N` is a useful configuration-isolation
command, but was not a workaround for the controlled-TTY stall. The previously
demonstrated `:set nofsync` workaround applies only to `E667` during saving;
it does not address startup or input delivery. The smallest next test is to
repeat this A/B and controlled input/save sequence on the exact
Yash-compatible tuple, with a guest trace that includes tty reads, file opens,
and their return values.

## Current package A/B: kernel user fault before screen (2026-09-27)

The current `REM-FLIGHT-TTY` package was tested from fresh disposable clones
after `e2fsck -fn` passed. The seed rootfs itself was not modified.

- QEMU SHA-256: `0154db549ad01d0bcdfd79c0dde9544266fc14c75fe7825d14942ff47a75641a`
- Kernel SHA-256: `e2516e8af9ffc0ee6ddb2113592f98d6c65f9b376961ee99c2ab24bc5fa546de`
- Seed rootfs SHA-256: `eff360514d9bb49d214cf2b60dd656524fdff493e7354249e5edf8e7d43f12ce`
- Vim: 9.1.1724, SHA-256
  `35874ba096f8036a15ae07a09985a5182ea86eacde8d287bae63138812ea06c4`
- Shell: Bash (`REM#` prompt); `TERM=xterm`, `LANG=C`, `LC_ALL=C`; 128 MiB

Both `vim -u NONE -N` and plain `vim` failed before painting a screen. Each
produced the same kernel report:

```text
MYEMU_BAD_USER_FAULT pid=36 comm=vim pc=021521c8 addr=2613fa48 cause=6
...
r5=2613fa48 ... start=3f930000 end=3f951000 ... brk=024ee000
```

REM binutils maps PC `0x021521c8` to `syn_name2id`; the instruction is
`lw r2,0(r5)`, dereferencing the faulting address. The address is outside the
reported stack region; the recorded process break (`0x024ee000`) is well
below the failing address. Thus Vim's normal and configuration-disabled paths
reach the same invalid user-memory access before terminal display. No
`E1286` or `E667` appears before this fault, so timer startup and save
behavior were not reached. The clean pre-boot ext4
check plus subsequent `mpage_prepare_extent_to_map` “page ... does not have
buffers attached” warnings leave the kernel MMU/page-cache/filesystem path as
a separate suspect; neither the warnings nor the Vim trace proves which
component corrupted or supplied the invalid pointer.

**Classification:** application execution reaches a bad user address and the
kernel reports it. The fault site is known, but ownership of the bad pointer
among compiler-generated code, musl/runtime state, Vim/syntax initialization,
and kernel memory/file-backed mapping remains **unresolved**. Because both
command modes fail at the same PC/address, editing the user vimrc is not a
workaround. No Vim editing, save, quit, or readback pass is claimed.

A separate controlled-TTY attempt on a test clone with a `debugfs`-staged
launcher ended in an unhandled kernel exception after ext4 inode warnings;
this altered-image run is excluded from the A/B result and needs a safely
staged launcher before reuse. The actionable next step is for the runtime
owner to investigate the `syn_name2id` invalid pointer together with the
ext4/page-fault warnings, then repeat the two command modes on clean images.
Until then, use the independently validated Kilo editor; there is no Vim
startup configuration workaround established by these tests.

## Minimal libc/memory probes for the pre-screen fault (2026-09-27)

On disposable clones of the same `REM-FLIGHT-TTY` tuple above, I built the
same standalone probe with GCC and chibicc, linked against the same flight-kit
musl runtime. Source: [`rem-vim-runtime-stress-probe.c`](../toolchain/tests/rem-vim-runtime-stress-probe.c).
The 16 MiB malloc/free plus realloc-preservation/end-point test passed;
reading `/bin/busybox` to EOF, checking its ELF header/checksum, and closing
the fd passed; file-backed and 8 MiB anonymous mmap access/unmap passed.
These results do not reproduce Vim's bad pointer through those operations.

The string phase passes `strcpy`, `strcat`, `strcmp`, and `strlen`, then faults
inside the shared musl `strstr` implementation for both GCC and chibicc caller
builds. GCC O2, GCC O0, and chibicc all fault at the `strstr` saved-register
restore: GCC O2 PC `0x02001b4c`, chibicc PC `0x02008464`, the bad load is
`0x46c`, and the reported frame pointer is `0x480`. Both PCs map to the same
restore sequence (`lw r9,0(r12)` with `r12=fp-20`).
The run therefore reproduces a non-Vim runtime failure and narrows the first
failing boundary to `strstr`/its call-frame state. It does **not** prove whether
musl's implementation, the compiler/ABI boundary, or other runtime state
causes the corrupted frame. The kernel reports the invalid user access; no
kernel change was made. The recurring ext4 warnings concern inode 258
(`/usr/bin/bash`) and pages 464-466 on an `e2fsck`-clean pre-boot clone; no
causal link to the `strstr` fault is established.

The probe's `/proc/self/maps` phase could not run: the tested rootfs has no
`mount` command and procfs is not mounted. This is a packaging/test-environment
limitation, not evidence that the kernel lacks procfs. The probe and logs are
under `/tmp/rem-vim-runtime-probe` for this session; no Vim, kernel, libc, or
flight artifact was changed.

## Rebuilt candidate edit/save/readback (2026-09-30)

A full Vim 9.1.1724 O0 rebuild was tested on a clean disposable ext4 guest
image. Candidate package and build notes are in
[`tools/rem-flight-image/staging/applications/vim-rem-9.1.1724`](../tools/rem-flight-image/staging/applications/vim-rem-9.1.1724/README.md).

- QEMU SHA-256: `9d1f47b2a6fb2e2f808e4538d316265cf220534dc2cb42c76a486391a4f27f16`
- Kernel SHA-256: `f6f5f57ef5bf18af984b8b11febb40d76204109ec3533621ba5969006af19138`
- Pre-test disposable rootfs SHA-256: `1f0e44b19fd782c8431728ccc67e4870ac8cd73b6c5cf3ed9158333e961e3232`
- Vim binary SHA-256: `c28488ace99d02e133db37594e141325a3f092f114be7c1fc67c0fce0a431ae3`
- Launcher SHA-256: `dc3a93b3645f4b33073b7855a5ea09a3738afe3d2987f52472e88112c9495a15`

With `TERM=xterm`, Vim opened `/root/vim-uat.txt` as a new buffer. The test
inserted `VIM_QUIET_ACCEPTANCE` and `SECOND_LINE`; `:wq` reported two lines and
33 bytes written, returned to the shell, and guest `cat` returned exactly those
two lines. After guest `sync`, QEMU was quit through its monitor and exited
with status 0. Host `debugfs` readback matched the 33 expected bytes (SHA-256
`0cd3c6f01886581a745c10b28b6c7292577704d6d1feaf50a99f80fa31bda879`). This
proves an interactive create/edit/save/exit/readback cycle for this candidate;
it does not prove persistence across a reboot.

The clean O0 build without an extra call still faulted during `estack_init`.
Retaining a call to a gated allocation trace helper at the large-allocation
site allowed the editor cycle to complete with tracing disabled by default.
This is evidence of layout-sensitive behavior, not a root cause or a compiler,
ABI, or kernel fix. A small `strstr` fallback was also linked because the
tested musl `strstr` path faults in separate GCC and chibicc caller probes.
The package records both modifications; the candidate is not yet integrated
or validated in the current flight distribution.

The launcher applies `set nofsync` because the tested musl fsync syscall
number (82) differs from the kernel's handled number (74); the unwrapped Vim
save had returned E667. The tested write followed by guest `sync` succeeded.
Post-write `e2fsck` found the free-block and free-inode counts each off by one,
so filesystem accounting after the save remains a separate reliability issue.

The packaged wrapper was then tested with normal startup on fresh rootfs images.
The guest reached the Vim screen, accepted two lines, reported `2L, 41B
written`, returned to the shell, and `cat` returned both exact lines. In one
synced image, host `debugfs` readback matched the 41 expected bytes. Guest
`poweroff` did not exit QEMU within 20 seconds; the tracked QEMU was then quit
through its monitor and exited 0. Post-write e2fsck found free-block and
free-inode counts each off by one.

Repeatability was not established: another fresh image showed the same Vim
save and guest `cat`, but host `debugfs` could not find the file after monitor
quit. That image's ext4 superblock reported zero free blocks and inodes, while
e2fsck's recount found 51,053 free blocks and 37,225 free inodes. Both images
passed pre-boot e2fsck. This is evidence of an unresolved filesystem/writeback
or shutdown problem, not a Vim rendering or editing failure. Do not treat the
candidate's source saves as reliable across guest shutdown until that system
issue is resolved. QEMU PID, command line, purpose, artifact hashes, and exit
status are recorded under `/tmp/rem-vim-rebuild-20260929/test/package-uat/`.

## Quiet-hook candidate rebuild (2026-09-30)

To remove the trace helper from the staged candidate, Vim was rebuilt from the
same source tree and toolchain at O0 with a side-effect-free, non-inlined hook
at the two large-allocation call sites. The static MyEmulator2 ELF32 binary is
SHA-256 `4cd43926d08faa52ef67ad997c087514752c73335e6bc6d876490b0b52e8bb61`.
On a fresh rootfs that passed pre-boot e2fsck, normal `/usr/bin/vim` startup,
interactive insertion, save (`2L, 41B written`), return to shell, exact guest
`cat`, and `sync` all passed. QEMU ran as tracked PID 8 and exited 0 after
monitor `quit`.

After shutdown, host `debugfs` could not find the saved file. Post-write e2fsck
reported zero free blocks/inodes in the superblock while recounting 51,053
blocks and 37,225 inodes free. This reproduces the storage reliability
boundary with the quiet-hook candidate. Vim's interactive edit/save path is
working; durable persistence is not accepted. No kernel or filesystem code
was changed.
