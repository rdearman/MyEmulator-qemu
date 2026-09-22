# Codex Handover — REM Linux / QEMU GCC-Linux Integration Track

Date: 2026-09-22
Prepared by: Copilot CLI (temporary cover during Codex usage-limit outage)
Repository: `/home/rick/Development/Active/MyEmulator-qemu`
Branch: `integration/emacs-android`
HEAD: `8cc3399` ("Fix defconfig CONFIG_BLOCK conflict and add missing sync(2) syscall")

This document is the **primary entry point** for Codex resuming this track.
Read this file first. It supersedes nothing in `RESUME.md` or
`docs/REM_LINUX_DEBUG_HANDOVER.md` — those still contain valid earlier
history and are referenced, not replaced, below.

## First actions for Codex

1. Read this file completely before touching any source file.
2. Do **not** run `git clean`, `git reset --hard`, or discard any uncommitted
   change listed below — the signal-handling implementation and its
   diagnostics are uncommitted and are the main deliverable of this session.
3. Reproduce the current state with:
   ```
   cd /home/rick/Development/Active/MyEmulator-qemu
   git status --short          # should match "Current uncommitted state" below
   git worktree list           # /tmp/rem-baseline-worktree must remain untouched
   ```
4. Rebuild QEMU + kernel exactly as described in "Exact reproduction commands"
   below, then run the two test commands under "Outstanding problem #1" to
   see the current failure state live before changing anything.
5. Start investigating **Outstanding problem #1 (IRQ-enable panic)** — it is
   the actual blocker; problem #2 (BusyBox SIGBUS) is a pre-existing,
   already-diagnosed userspace/link issue, not a kernel regression (see below).

## Current uncommitted state (do not discard)

```
 M linux/arch/myemulator2/include/asm/ptrace.h
 M linux/arch/myemulator2/include/asm/syscall.h
 M linux/arch/myemulator2/include/uapi/asm/sigcontext.h
 M linux/arch/myemulator2/include/uapi/asm/unistd.h
 M linux/arch/myemulator2/kernel/Makefile
 M linux/arch/myemulator2/kernel/entry.S
 M linux/arch/myemulator2/kernel/process.c
 M linux/arch/myemulator2/kernel/syscall.c
 M linux/arch/myemulator2/kernel/traps.c
 M qemu/target/myemulator32/translate.c
 M toolchain/scripts/prepare-gcc-source.py
?? linux/arch/myemulator2/kernel/signal.c      (NEW FILE — the rt-signal implementation)
?? docs/REM_LINUX_SIGNALS.md                   (design notes for the signal ABI)
?? docs/REM_LINUX_DEBUG_HANDOVER.md             (earlier session's debug log, still valid)
?? toolchain/scripts/test-linux-signal.py        (signal regression test, never yet run to completion)
?? toolchain/scripts/test-linux-selfhosted-gcc.py
?? toolchain/examples/linux-busybox-direct.c
?? toolchain/examples/linux-busybox-shell-direct.c
?? RESUME.md                                   (earlier standing resume doc)
?? .busybox-build/                             (build staging dir, regenerable, not in git)
```

Protected files present but **untracked and untouched** — do not stage,
modify, or delete: `boot.bin`, `myemulator.img`, `rikmon/boot.s`.

`qemu/target/myemulator32/helper.c` and `qemu/target/myemulator32/helper.h`
have **no diff** — all temporary QEMU-side tracing added earlier this session
(`MYEMU_SSP_TRACE`, `MYEMU_PC_TRACE`, `MYEMU_JAL_XLATE`, `helper_debug_pc_trace`)
was fully stripped and both files are back to the committed baseline.
`translate.c`'s diff is a harmless refactor of the JAL case (same codegen,
now computes `jal_target` once) — no diagnostics remain in QEMU.

## What has been completed (this session)

### 1. Root cause found and fixed: signal-handler link-register bug

`setup_rt_frame()` in the new `linux/arch/myemulator2/kernel/signal.c` was
writing the sigreturn-trampoline address into `regs->r[4]` instead of
`regs->r[14]`. This architecture's ABI (confirmed via `qemu/target/myemulator32/translate.c`
JAL/JALR codegen, and via disassembly of BusyBox's own prologue/epilogue)
uses **r14 as the link register**. Because r14 was never pointed at the
trampoline, a signal handler's eventual `jr r14` return used whatever r14
held *before* the signal — wandering through unrelated code until it
dereferenced a zeroed stack slot and jumped to PC=0, producing
`Attempted to kill init!` panics whenever BusyBox's `ash` received a real
SIGCHLD.

Fix (already applied, in the uncommitted `signal.c`):
```c
regs->r[14] = return_pc;   /* was: regs->r[4] = return_pc; */
```
This is verified in isolation: SIGCHLD delivery to `ash` (via `echo`/`pwd`/`sync`
sequences that fork/reap children) completes cleanly with the fix, whereas
it panicked with `r4` before the fix. **This part of the investigation is
solid — do not revert it.**

### 2. All temporary diagnostic instrumentation stripped (mostly)

Removed from the kernel: `MYEMU_SIG_DEBUG_ENTER/HANDLE/SETUP_RT/FINISH`,
`MYEMU_SYSCALL_ENTER/EXIT` pr_warns, the Bug#6 fatal-stack-dump block, and
the "last fault" static tracking variables in `traps.c`. Removed from QEMU:
`MYEMU_SSP_TRACE`/`MYEMU_PC_TRACE`/`MYEMU_JAL_XLATE`, the `helper_debug_pc_trace()`
helper and its `DEF_HELPER_1` declaration, and the `gen_helper_debug_pc_trace()`
call in `myemulator32_insn_start()` (which is now empty again, matching the
pre-diagnostic committed baseline).

**Two diagnostic pr_warns were deliberately left in place** because they are
actively being used to diagnose Outstanding Problem #2 below, and because
they are cheap/harmless (rate-limited implicitly by how rarely these paths
fire):
- `linux/arch/myemulator2/kernel/traps.c:98` — `MYEMU_ALIGN_DIAG` in the
  cause==2/3 (INSN_ALIGN/DATA_ALIGN) user-mode path. Dumps pid/cause/pc/info/sp/r14.
- `linux/arch/myemulator2/mm/fault.c:113` — `MYEMU_FAULT_DIAG` in the page-fault
  path (this one predates this session; left from earlier Bug #4 work).

Both should be removed once problems #1 and #2 below are fully closed out —
do not treat them as permanent.

### 3. Isolated baseline comparison performed

Created a **separate git worktree** at `/tmp/rem-baseline-worktree` (detached
HEAD at `8cc3399`, i.e. the last fully-committed state, before any of this
session's uncommitted signal-handling changes) plus an independent copy of
the Linux source tree at `/tmp/rem-baseline-worktree/.linux-build/linux-6.12.1`
(rsynced from the main tree's `.linux-build/linux-6.12.1`, `build/` excluded)
so it could be built and tested completely independently of the live
integration tree. It reuses the already-built cross-toolchain at
`.toolchain-install/` (shared, read-only) via `CROSS_COMPILE=`, and reuses
the already-known-good `.config` (copied in) for ext4/non-initramfs mode
(the freshly generated default `.config` in the worktree was missing
`CONFIG_BLOCK`/`CONFIG_EXT4_FS` and could not mount the test rootfs at all).

This worktree/kernel build is a **reusable regression baseline** — do not
delete it. It should keep passing the pre-signal-handling regression bar
(reaches shell, `echo`/`pwd` work) independent of whatever else changes on
the integration branch.

## Outstanding problem #1 (ACTIVE BLOCKER): "page fault without mm" panic when IRQs are re-enabled during signal delivery

**Symptom:** With `local_irq_enable()`/`local_irq_disable()` restored to
their natural (uncommented) state in `arch_do_signal_or_restart()`
(`linux/arch/myemulator2/kernel/signal.c`, currently around lines 141 and 173),
running `pwd` (or generally, any command shortly after boot) causes:
```
[   12.750000] MyEmulator2 page fault without mm: pid=0 pc=001014e4 sp=7feda6e4 lr=0053f820 sr=00000021 cause=6 info=ffffffc0 ...
[   12.760000] Kernel panic - not syncing: MyEmulator2 page fault without mm
```
Note `pid=0` — this fires in a context with no `current->mm` (either very
early kernel context, an idle-task/interrupt path, or a task tear-down
race), not in the userspace process that issued the syscall.

**Current workaround (NOT a fix, currently in place):** both calls are
commented out in the checked-out working tree:
```c
/* TEST: keep disabled to isolate IRQ-during-signal-handling issue */
// local_irq_enable();
...
// local_irq_disable();
```
This avoids the panic and lets `echo`/`pwd` run, but is unsafe as a
permanent state: it means the CPU runs `get_signal()`/`handle_signal()`
with local IRQs disabled for the entire signal-dispatch path on every
syscall/fault return, which can silently swallow/delay timer ticks and
other interrupts arbitrarily long. **Do not commit this as-is.**

**What has been ruled out / established so far:**
- This is **not** present on the isolated pre-signal-handling baseline
  (worktree above) — baseline `pwd`/`echo` never panics regardless of IRQ
  state, because the baseline has no `arch_do_signal_or_restart()` call at
  all (it uses the old get_signal()-drain-and-drop workaround documented in
  the git history of `traps.c`).
- It is specifically tied to *re-enabling* IRQs inside
  `arch_do_signal_or_restart()`, called from
  `myemulator2_finish_user_exception()` in `traps.c`, which itself is called
  from the cause==12 (syscall), cause 4-9 (page fault), and cause 2/3
  (align fault) paths, and from the IRQ dispatch path (cause 16-22).
- Not yet established: whether this is a genuine nested-IRQ re-entrancy bug
  (e.g. a timer IRQ firing while `current_thread_info()->regs` or the
  active `pt_regs` frame is in an inconsistent state because
  `arch_do_signal_or_restart()` is being invoked from within the *IRQ path
  itself*, cause 16-22, where re-enabling IRQs during IRQ handling is
  unusual/risky) or a more mundane bug like a missing `set_fs()`/context
  check, or `current->mm` legitimately being torn down (e.g. during exit)
  while an IRQ is still in flight.

**Next concrete steps (not yet performed):**
1. Determine which of the four call-sites into
   `myemulator2_finish_user_exception()` is actually active at the moment
   of the `pid=0` panic — instrument (temporarily) which `regs->cause` led
   into it just before the fault, since `pid=0`/kernel context strongly
   suggests this is firing from the **IRQ path** (cause 16-22) rather than
   the syscall path, i.e. a timer interrupt arriving and calling
   `arch_do_signal_or_restart()` on a `pt_regs` that belongs to no task
   context (`current->mm == NULL`, e.g. still in `swapper`/idle before the
   first real process is scheduled, or during a context switch window).
2. If it is the IRQ path: `myemulator2_finish_user_exception()` currently
   calls `arch_do_signal_or_restart()` unconditionally whenever
   `user_mode(regs)` is true, including after IRQ dispatch (cause 16-22).
   Re-enabling IRQs from *inside* an already-IRQ context (nested IRQ) via
   `local_irq_enable()` there is architecturally suspicious and worth
   checking against how upstream Linux ports normally structure this (they
   typically call `do_signal()` only on the return-to-userspace path after
   `irq_exit()`/`preempt_check_resched()`, not by unconditionally reusing
   one shared "finish" helper for every exception class). Consider whether
   `myemulator2_finish_user_exception()` needs to distinguish "returning
   from a syscall/fault directly" from "returning from an IRQ that
   interrupted userspace" and only allow the local_irq_enable/disable pair
   in the former case.
3. Reproduce with a temporary pr_warn logging `regs->cause` immediately
   before the `arch_do_signal_or_restart()` call inside
   `myemulator2_finish_user_exception()`, correlated with the `pid=0`
   panic's timestamp, to confirm/refute the IRQ-path hypothesis before
   changing any code structure.
4. Once root-caused, restore `local_irq_enable()`/`local_irq_disable()`
   properly (or replace with whatever the correct fix turns out to be),
   rebuild, and re-run the full regression list under "Exact reproduction
   commands" below.

## Outstanding problem #2 (DIAGNOSED, NOT a kernel regression): `ls`/`cat`/`mkdir` → SIGBUS ("Bus error")

**Symptom:** With problem #1's workaround in place (IRQs disabled during
signal delivery, so the guest doesn't panic), running `ls /`, `cat
/proc/version`, or `mkdir` in the BusyBox shell prints `Bus error` and the
command fails (but the shell survives — SIGBUS is delivered and handled
correctly, the process is simply killed by the shell's default SIGBUS
disposition).

**Root cause narrowed down, NOT yet fully fixed:** the fault is
`MYEMU_ALIGN_DIAG cause=2 pc=02028138 info=00000002 r14=00000002 r15=000004a0`.
Disassembly of `/tmp/rem-busybox-fixed-bin` at that address shows a normal
function epilogue:
```
202811c: lw r14,0(r13)      ; reload saved link register from stack
2028130: lw r15,4(r13)      ; reload saved frame pointer
2028134: addi r13,r13,1188  ; pop a large (1188-byte) frame
2028138: jr r14             ; return -- but r14 == 2 here (bogus)
```
Both the reloaded link register (`r14=2`) and frame pointer
(`r15=0x4a0`) are corrupted garbage, not valid addresses — this is **stack
corruption inside BusyBox itself**, most likely from the musl re-link
mentioned in the original task brief (`/tmp/rem-busybox-fixed-bin` was
relinked against "a different musl installation" and has not been fully
ABI-verified against this kernel's calling convention / stack layout
expectations).

**Confirmed via the isolated baseline**: this exact fault (`cause=2
pc=02028138 r14=00000002`) reproduces **identically** on the pre-signal-
handling baseline kernel from the isolated worktree, using the exact same
`/tmp/rem-fixed-busybox-verify.ext4` image. **This means it is not caused
by, or related to, the signal-handling integration work** — it is a
pre-existing BusyBox/musl userspace defect that the old
get_signal()-drain-and-drop workaround also could not fix (it produced the
same `MYEMU_SIGNAL_NO_HANDLER ... dropping` + eventual `Bus error` on
baseline).

**Do not attempt to fix this by changing the kernel's signal handling** —
the kernel is correctly delivering SIGBUS for what is a genuinely invalid
jump target. The real bug is upstream of the kernel: either
- the relinked BusyBox binary itself (stack corruption, possibly a
  musl/libc mismatch in calling convention, red-zone usage, or stack
  frame size vs. actual local variable usage for whatever function sits at
  `0x2028100`-`0x2028138`), or
- possibly a compiler/codegen issue in the myemulator2-elf-gcc toolchain
  used to build that particular musl/BusyBox combination.

**Next concrete step:** identify which C function in BusyBox's source
corresponds to `0x2028100`-`0x2028138` (use `nm`/`addr2line` against the
non-stripped `/tmp/rem-busybox-fixed-bin` if debug symbols are present, or
correlate against the BusyBox build's map file) and inspect its actual
stack usage vs. the 1188-byte frame the epilogue expects to pop — this
smells like a **frame-size mismatch between prologue and epilogue**, i.e.
possibly the same class of bug as the previously-fixed "Bug #6" alloca/VLA
stack corruption (see `docs/REM_LINUX_DEBUG_HANDOVER.md` and commit
`b4dedb3`), but in a *different* function, and specifically introduced or
exposed by the musl relink rather than the original toolchain build.

## Exact reproduction commands

Rebuild QEMU (only needed if `qemu/target/myemulator32/*` changes):
```
cd /home/rick/Development/Active/MyEmulator-qemu
bash qemu/tools/apply-overlay.sh .qemu-upstream
cd .qemu-build && ninja qemu-system-myemulator32
```

Rebuild the kernel (reuses existing `.linux-build/build/.config`, does not
regenerate it if already present):
```
cd /home/rick/Development/Active/MyEmulator-qemu
bash toolchain/scripts/build-linux.sh
```

Run the shell/signal regression (existing harness, drives QEMU via pexpect,
captures kernel log to stderr):
```
timeout 60 python3 /tmp/rem_ssp_trace_test.py /tmp/rem-fixed-busybox-verify.ext4 \
  "echo REM_INTEG_TEST" "pwd" "ls /" "cat /proc/version" \
  "mkdir -p /root/test" "echo hello > /root/test/hello.txt" \
  "cat /root/test/hello.txt" "sync"
```
Expected with the *current* uncommitted state (IRQ toggle disabled):
`echo`/`pwd` succeed; `ls`/`cat`/`mkdir` print `Bus error` (Problem #2,
pre-existing, not a regression); the shell survives; the harness reaches
`DONE-OK`.

Expected if you restore `local_irq_enable()`/`local_irq_disable()` in
`signal.c` (Problem #1): guest panics with `"page fault without mm"`
during/after `pwd`.

Baseline comparison (isolated worktree, does not touch the live tree):
```
timeout 40 python3 /tmp/rem_baseline_test.py /tmp/rem-fixed-busybox-verify.ext4 \
  "echo BASELINE_TEST" "pwd" "ls /"
```
(This script points its `-kernel` at
`/tmp/rem-baseline-worktree/.linux-build/build/vmlinux` — see the script
body if it needs regenerating; it was written this session at
`/tmp/rem_baseline_test.py`, not committed since it's a throwaway test
harness, not source.)

## Test artifacts and their status

| Path | Status | Notes |
|---|---|---|
| `/tmp/rem-busybox-fixed-bin` | Input artifact, unmodified this session | SHA-256 `b35851f850ca7bc0a71aed90c4dc26ec0b64e88aac63794460f7866cabbd0860`. The relinked BusyBox under investigation for Problem #2. |
| `/tmp/rem-fixed-busybox-verify.ext4` | Current working test image | SHA-256 `9f709c72f2244fe4549e5dc07b97cfce4423a99c003253951a74b34af210c5c8`. Contains the binary above, built via `toolchain/scripts/build-ext4-rootfs.sh MYEMU_BUSYBOX_BINARY=/tmp/rem-busybox-fixed-bin`. Verified via debugfs+sha256 to actually contain the intended binary (a prior stale-image mixup earlier in the session is documented in `docs/REM_LINUX_DEBUG_HANDOVER.md`). Use this for all further testing. |
| `/tmp/rem-verified-rootfs.ext4` | **NOT updated this session** | Still the OLDER verified image from before this session's signal-handling work. Do **not** promote `/tmp/rem-fixed-busybox-verify.ext4` to this path until Problems #1 and #2 above are both resolved and the full regression list (shell, persistence, `test-linux-signal.py`) passes cleanly. |
| `/tmp/rem-baseline-worktree` | Isolated git worktree, keep | Detached HEAD at `8cc3399`. Independent `.linux-build/linux-6.12.1` source copy + built `vmlinux` at `.linux-build/build/vmlinux` inside it. Reusable regression baseline; do not delete. |
| `/tmp/rem_ssp_trace_test.py`, `/tmp/rem_baseline_test.py` | Throwaway pexpect test harnesses | Not committed (scratch tooling). Reusable; regenerate from this doc's inline description if lost. |
| `.qemu-build/qemu-system-myemulator32` | Rebuilt this session, clean | No diagnostics; matches the (nearly) unmodified `translate.c` + fully-reverted `helper.c`/`helper.h`. |
| `.linux-build/build/vmlinux` | Rebuilt this session | Contains the r14 signal fix, the two remaining diagnostic pr_warns (`MYEMU_ALIGN_DIAG`, `MYEMU_FAULT_DIAG`), and the IRQ-toggle **disabled** (workaround for Problem #1). This is the kernel used for the "current" test results described above. |

## Dependencies / integration scope

- This work is entirely within `linux/arch/myemulator2/*` and
  `qemu/target/myemulator32/*` plus supporting `toolchain/scripts/*` — it
  does not touch, and has no dependency on, `REM-emacs` or `REM-android`
  working directories (neither was opened or modified this session).
- The signal-handling implementation (`signal.c`) is a **prerequisite**
  for the broader integration plan (emacs-cross userspace utilities,
  android-arm64 packaging) only in the sense that those later steps assume
  a working shell + real POSIX signal semantics; per the user's most recent
  instruction, **signal-handling is no longer being treated as a strict
  blocking prerequisite for every other component** — other integration
  tracks may proceed independently while Problems #1/#2 here are resolved,
  as long as they do not depend on POSIX signal delivery actually working.
- `toolchain/scripts/prepare-gcc-source.py` has a large uncommitted diff
  (349 insertions) from earlier native-GCC investigation work in this
  session's history; it has not been touched in this handover pass and its
  state should be treated as whatever the prior segment left it in (see
  `docs/REM_LINUX_DEBUG_HANDOVER.md` for that history if needed).

## Why nothing was committed this session

The signal.c fix, the diagnostic strip-down, and the IRQ-toggle workaround
are all interdependent and only partially verified (Problem #1 is an open
regression against the *previous* working state introduced by properly
enabling IRQs during signal delivery). Committing now would either (a)
commit a known-broken IRQ-enable path, or (b) commit the workaround
(IRQs-disabled) as if it were the final design, which it explicitly is not.
Per working rules, verified fixes should be committed separately from
diagnostics/workarounds — that separation isn't safely possible yet because
the "verified fix" (proper IRQ handling during signal delivery) doesn't
exist yet. Codex should commit once Problem #1 has an actual fix, splitting
into: (1) the r14 link-register fix + full signal.c/traps.c/entry.S/etc.
signal-handling implementation, and (2) the Problem #1 IRQ fix, as separate
commits, with the two remaining diagnostic pr_warns removed before either
commit lands.
