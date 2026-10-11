# REM64 architecture observations

## Terminal geometry and interactive line editing

The first canonical Kilo and C Yash builds use ordinary POSIX termios and
`ioctl(TIOCGWINSZ)` interfaces. Kilo falls back to an ANSI cursor-position
query when the window size is unavailable; Yash performs terminal and line
editor initialization before its first interactive prompt. On the current
REM32 serial environment, both programs build correctly but do not yet reach
a reliable interactive startup boundary under the disposable QEMU console.

**REM64 implication:** a usable terminal ABI should define reliable window
geometry reporting and a documented response path for cursor-position queries,
alongside raw/canonical mode transitions and byte reads. These interfaces
should be tested independently of any particular shell or editor.

**Status:** open question; likely requirement for a flight-quality interactive
development environment.

## ABI must be frozen before libc and compiler work

REM32 reached Linux userspace only after the register, stack, syscall, and
exception-return rules were made explicit. The early GCC frame-layout work,
the `r15` syscall-return preservation fix, and the later Bug #6 fix all came
from real musl, BusyBox, or Linux failures rather than from isolated compiler
tests (`e1d5215`, `2cc3080`, `b4dedb3`).

**REM64 implication:** define and executable-test the calling convention,
dynamic-stack, varargs, aggregate-return, TLS, and syscall ABI before porting a
large libc or application set. Treat compiler output plus QEMU execution as
the ABI test.

**Status:** likely requirement.

## Dynamic stack allocation needs an ABI-aware contract

REM32's register-argument home slots were safe for fixed frames but aliased
`alloca()`/VLA storage after the stack pointer moved. The two-part GCC fix had
to change both the dynamic offset and the actual reservation padding; changing
only one side still corrupted callers (`b4dedb3`).

**REM64 implication:** specify how callee argument homes, outgoing arguments,
red zones, stack probes, VLAs, and interrupts interact. Add nested-call and
VLA tests before declaring the compiler ABI usable.

**Status:** likely requirement.

## Exception entry is part of the calling convention

The Linux port found failures where the exception trampoline stored a returned
`pt_regs *` in a frame word that frame construction cleared, and where banked
stacks, timer entry, RFE, and syscall return did not preserve the visible
architectural state. QEMU therefore has to materialize fault state before an
MMU fault, RFE, or CAS can leave a translation block (`MYEMULATOR_2_LINUX_PORT.md`,
`0e77f38`, `f1f8864`).

**REM64 implication:** specify exception-frame ownership, banked-stack
selection, register visibility, and restart semantics architecturally. Do not
leave them as emulator or Linux-port conventions.

**Status:** likely requirement.

### Asynchronous interrupts must tolerate transient kernel SP states

During the GNU Emacs 30.1 loadup trace, timer IRQ1 repeatedly interrupted
`arch_do_signal_or_restart` at `0x00103374`. The exact kernel disassembly shows
its prologue splits a 256-byte stack adjustment into `subi ...,255` followed by
`subi ...,1`; the captured SP after the first instruction was `0x81097d15`,
which is not word-aligned. The trace then repeated the same IRQ PC/SP without a
matching RFE return, and HMP showed the CPU halted. The QEMU exception-entry
path requires aligned exception-stack storage and diverts a misaligned entry
to the double-fault stack; repeated level IRQ delivery is consistent with the
interrupted continuation not resuming. The trace does not yet establish which
layer owns the failure or whether the IRQ controller/timer is also involved.

A later matched run captured 6,832,337 timer IRQ entries (`cause=16`) at the
same PC, with zero RFE returns to that continuation. HMP samples over 600 s
showed `halted=1`, timer IRQ asserted/requested, and virtual time advancing past
an unchanged `TIMECMP`; loadup made no further progress. This confirms a
timer/exception livelock in the tested kernel/QEMU tuple, but does not isolate
whether the cause is guest interrupt acknowledgement, exception return, or
emulator delivery. A disposable QEMU experiment that held IPL=7 through
synchronous exception dispatch then triggered the kernel's
`fs/buffer.c:1312/check_irqs_on()` BUG before the init script ran; an otherwise
identical unmodified-QEMU control completed filesystem operations and powered
off. Thus holding interrupts masked through ordinary syscall/filesystem work
is not a valid general workaround. The implementation must protect exception
entry/stack transitions while preserving normal interrupt state for kernel
work. This evidence does not select between an aligned exception stack,
entry-only masking, or another mechanism. Reproduction tuple and compressed
trace are recorded in
[`CODEX-6-HANDOFF.md`](CODEX-6-HANDOFF.md), section “2026-10-04 timer-IRQ
livelock capture after the memset-boundary runs.”

**REQUIRED behavior:** an asynchronous interrupt may arrive between any two
instructions, including a split stack adjustment; exception entry must retain
the interrupted state and reach a return path without losing or livelocking
the interrupt. **OPEN:** determine whether the architecture should provide an
emergency exception stack, whether the kernel must mask interrupts around
non-aligned prologues, and how level-triggered sources are acknowledged during
double-fault handling. Evidence and exact tuple are in
[`docs/CODEX-6-HANDOFF.md`](CODEX-6-HANDOFF.md), section “2026-10-03 IRQ
prologue trace follow-up.”

A later HMP sample at `do_page_fault+0x34` disambiguated the stack value: the
PC was the second half of a 272-byte local allocation, so the observed
one-modulo-four SP was transient; the C entry SP reconstructed to aligned
`0x8102be94`. This confirms that an interrupt can observe a temporarily
unaligned SP even when the function entry obeyed the ABI. It still does not
prove that this particular state caused the halt. See the correction and
capture in [`CODEX-6-HANDOFF.md`](CODEX-6-HANDOFF.md).

The same halted register dump showed architected DFSP still at reset value
zero. The focused Linux boot fix now reserves a page-aligned physical emergency
stack and writes DFSP before vector setup; a host regression observes the
register value after kernel early RAM setup. **REQUIRED:** initialize and
validate the emergency exception stack before enabling normal exception paths.
This qualification does not yet prove recovery from a real nested/double-fault
event.

Persistence testing also exposed a userspace illegal-instruction exception
after a `printf` bad-address fault: REM32 panicked the whole kernel instead
of terminating the process (`docs/REM_EXT4_PERSISTENCE.md`). **REQUIRED
behavior:** Linux-facing user instruction faults must be reported to that task
as `SIGILL`; they must not take down the kernel. **OPEN QUESTION:** the exact
REM32 userspace fault-to-stack-PC sequence needs separate ABI/runtime
debugging. A repeatable REM32 `printf` data fault also showed a saved `r15`
and signed load offset implying address `0x00000ab4`, while exception `info`
reported `0x000002f4`. **REQUIRED behavior:** the fault address and saved
register frame exposed to Linux must describe the same faulting instruction.
**OPEN QUESTION:** determine whether this mismatch is in QEMU state restore,
exception-frame capture, or the Linux fault handler.

### User-fault records need the complete register state

In the paired OpenSSH RNG probe, the GCC-built executable faulted at
`0x02068320` with bad address `0x23c`. Disassembly identifies that PC as
`lw r1,-36(r15)`: the 13-bit displacement is signed. The kernel fault logger
labels `regs->r[15]` as `fp`, so the captured `fp=0x260` independently
reconstructs the effective address `0x260-36=0x23c`. This confirms that the
faulting GCC-built probe had a low, invalid frame-pointer value at that site;
it does not explain how that value arose. The same-source chibicc probe also
failed after the initialization marker, but its transcript has no separately
attributable fault PC. See the serial capture and binary hashes in
[REM_OPENSSH_COMPATIBILITY.md](REM_OPENSSH_COMPATIBILITY.md).

**Classification:** user-space frame/register state in a GCC-built libcrypto
provider path; compiler/backend, ABI, and runtime causes remain unresolved.
The kernel logger's `fp` field is sufficient to verify this instruction's
effective address, though broader diagnostics still benefit from recording
the entire saved frame. **REM64 recommendation:** specify frame-pointer and
callee-saved-register obligations in the ABI and provide instruction-level
fault records. This is not evidence of a CPU fault-semantics defect.

## MMU correctness includes permissions and translation lifetime

REM32 exposed failures from incorrect permission checks, stale read-only TLB
entries after a writable fault, missing TLB invalidation, and private writable
pages created without the requested permission. Fixes required coordinated
QEMU, page-table, fault-handler, and TLB changes (`4b0b354`, `3b9c244`).

The storage qualification exposed a further concrete case: the MMU contract
requires a dirty-bit update when a writable page is first stored through a
translation that was populated by a prior load. QEMU's software TLB otherwise
kept the page writable without updating the guest PTE, so ext4 writeback could
leave file pages clean and omit them from `sync`. The REM32 fix forces a store
refill for writable pages, where the architectural dirty bit is set. **REM64
requirement:** define accessed/dirty behavior for TLB hits, or provide an
explicit software-dirty contract that Linux writeback can rely on.

**REM64 implication:** make PTE permission bits, accessed/dirty behavior,
fault-upgrade rules, COW, and TLB invalidation testable independently. A page
table update is incomplete until the translation cache behavior is specified.

**Status:** likely requirement.

## Fork, exec, and signals require complete architectural state

The working Linux baseline needed explicit process-context copying, correct
TP inheritance, exec address-space replacement, signal-frame construction,
`sigreturn`, and restart-code handling. Dropping or merely dequeuing signals
made shells appear to work while violating application expectations (`ca65421`,
`docs/REM_LINUX_SIGNALS.md`).

**REM64 implication:** define the user signal frame and return ABI together
with fork/exec and context switching. Test handlers, interrupted syscalls,
COW, and exec teardown as one process-state contract.

**Status:** likely requirement.

REM32 GNU Make 4.4.1 was cross-configured with `HAVE_WORKING_VFORK`, but its
recipe launcher cannot start `/bin/sh` in the guest and reports `Function not
implemented`. GNU Make's selected child path calls `vfork()`; the test did not
trace which syscall or clone flags produced `ENOSYS`.

**REM64 implication:** provide Linux-compatible child creation for build tools
(`fork`/`vfork` or compatible `clone` semantics), and runtime-test the selected
libc/compiler configuration instead of accepting cross-compile guesses.
**Status:** child process creation is required by real build tools; the exact
`vfork`/`clone` contract remains an open question.

## Atomics need both an instruction and a language-level ABI

REM32 has an architected naturally aligned CAS instruction, but GCC's generic
`__atomic` lowering and the Linux atomic ABI were not automatically defined.
Inline assembly can exercise CAS while portable C atomics remain a separate
backend/runtime task (`MYEMULATOR2_GCC.md`, architecture manual §3.4).

**REM64 implication:** specify ordering, alignment, failure semantics, and
compiler/libc lowering before claiming C11 atomics. Instruction availability
alone is not a userspace atomic ABI.

**Status:** open ABI requirement.

## Syscalls are a compatibility contract, not just numbers

The Linux port had to preserve six argument registers for calls such as
`mmap`, preserve `r15` across syscall returns, add missing calls such as
`mprotect`, `pselect6`, `rt_sigsuspend`, `sync`, and session/TTY operations,
and keep the userspace structure layout 32-bit (`89d8f6a`, `2cc3080`,
`8cc3399`). Interactive Yash required both `pselect6` and `rt_sigsuspend`;
omitting the latter made its prompt fail with `ENOSYS`.

During Emacs qualification, a target `nanosleep()` probe returned
`-1/ENOSYS` immediately. This is a demonstrated syscall coverage gap in the
tested REM32 tuple; it has not been connected to Emacs's current failure.
**REM64 requirement:** include and test a working sleep syscall or a libc
fallback whose elapsed-time and interruption semantics are verified.

A REM Vim 9.1 test exposed two more concrete mismatches: the musl overlay
numbers `fsync` as 82 while the current REM dispatcher handles it at 74
(default Vim save reports `E667`), and cross-configuration cached POSIX timer
support as available although the corresponding timer syscalls are absent
(raw calls 107 and 409 return `ENOSYS`). In Vim's `SIGEV_THREAD` timer path,
musl first tries `pthread_create`; that returns `EAGAIN` on REM, producing
Vim's `E1286` before the missing timer syscalls are reached.

**REM64 implication:** publish syscall argument width, restart behavior,
structure packing, signal interaction, and register clobbers as one ABI.
Generate the syscall-number table for both kernel and libc from that contract,
and require runtime probes or explicit cross-build answers before enabling
optional timer-backed application features.

**Status:** required for Linux signal-wait compatibility demonstrated by Yash.

## TTYs need a complete terminal-state model

A serial byte stream was insufficient for Bash, Vim, Kilo, and Yash. The Linux
port required a registered console driver, controlling sessions, foreground
process groups, termios transitions, ioctl behavior, and correct poll/select
wakeups. Editors additionally depend on terminal geometry and cursor-query
responses.

**REM64 implication:** specify console geometry, `TIOCGWINSZ`, raw/canonical
mode, signal-generating control characters, process-group ownership, and
cursor-query behavior. Validate the stack with a shell and a raw-mode editor.

A focused Yash guest probe found that `isatty`, `tcgetattr`, `TCGETS`, `TCSETS`,
and `tcsetattr` succeed, but `tcgetpgrp`/`tcsetpgrp` return `ENOTTY` while the
interactive shell has no controlling terminal. `fork`, `setpgid`, and `setsid`
succeed on the tested kernel. In a child session, `TIOCSCTTY` successfully
attaches the serial terminal and process-group get/set then succeed. This
isolates a missing session/controlling-terminal setup in the boot/login/shell
launch path on that tuple, rather than a missing `setpgid` call or generic
termios ioctl failure. The probe and tuple-specific Bash comparison are in
`docs/REM_VIM_COMPATIBILITY.md` and
`toolchain/tests/rem-vim-terminal-probe.c`. A test launcher that acquired the
controlling terminal still did not get Vim to display a buffer; that separate
startup stall remains unresolved.

The 2026-09-28 debugger image integration independently reproduced the launch
boundary in the packaged developer seed: its `/sbin/init` shell script runs
`/bin/bash --login -i` with `/dev/console` redirection, without a new session
or controlling-tty acquisition. Bash reports `ENOTTY` for terminal process
group setup, and the serial acceptance runner cannot send commands to it. The
debugger binary is present and image hashes verify; this failed before the
debugger process started. See [`REM_DEBUGGER.md`](REM_DEBUGGER.md) for the
candidate tuple and failure evidence.

The Fold integration candidate repeats the same launch gap with Yash: its init
script forks Yash with `/dev/console` attached but makes no session or
controlling-terminal setup. Its BusyBox build has `CONFIG_SETSID` and
`CONFIG_CTTYHACK` disabled, so those usual launch helpers are unavailable in
that image. Separately, the candidate pins a kernel whose syscall dispatcher
lacks `rt_sigsuspend` (133); Yash reports `ENOSYS` while waiting for input.
These are two independent integration requirements: the shell needs both a
controlling-terminal session and the Linux signal-wait syscall contract. The
candidate trace and status are in [`REM_FLIGHT_READINESS.md`](REM_FLIGHT_READINESS.md).

**REM64 implication:** the console launch contract must define when a session
leader acquires its controlling terminal (`setsid`/`TIOCSCTTY`) and how its
foreground process group is selected. **Status:** required for reliable
job-control terminal handoff; whether init, login, or shell owns the setup is
an integration decision.

**Status:** termios and controlling-tty mechanisms are demonstrated, but the
tested Yash launch lacks a controlling-terminal association; the launch-path
owner remains to be integrated.

**Follow-up evidence (2026-09-28):** a disposable C init supervisor successfully
created the Yash session, acquired `/dev/console` with `TIOCSCTTY`, and set the
foreground process group (`sid=20 pgrp=20 fg=20`). Yash reached an interactive
prompt and Bash `-c` completed on this tuple. External command startup still
triggered repeated `sigsuspend: Function not implemented`, including with
Yash job control disabled. A second existing kernel artifact reproduced the
same syscall failure; another booted its embedded `/init` and panicked before
mounting the candidate rootfs. **Classification:** integration/session setup
was sufficient for the prompt; the remaining demonstrated blocker is the
kernel syscall contract (`rt_sigsuspend`, syscall 133), with one alternate
kernel also exposing an incompatible init/rootfs configuration. **REM64
REQUIRED:** qualify both the Linux signal-wait syscall behaviour and the
kernel/rootfs init contract with an interactive shell plus external-command
tests; successful console attachment alone is insufficient. Exact test hashes
and outcomes are in [`REM_FLIGHT_READINESS.md`](REM_FLIGHT_READINESS.md).

A controlled Vim launch then acquired its own session and controlling tty:
`TIOCSCTTY` and `TCGETS` succeeded, but Vim still produced no visible buffer
within 60 seconds. The selective kernel trace showed repeated file-descriptor
reads and file-backed page faults, with no read from fd 0 before timeout. It
does not trace all syscalls or identify opened paths, so this rules out a
keyboard-input wait as the observed next step but does not establish whether
Vim configuration/runtime loading, libc file handling, timer/thread activity,
or another untraced call owns the stall. **OPEN:** provide guest syscall
tracing with pathname and return-value capture before assigning a more
specific owner. See [REM_VIM_COMPATIBILITY.md](REM_VIM_COMPATIBILITY.md).

## Filesystem persistence must be tested across a real reboot

REM32's ext4 result became trustworthy only after a dedicated write, sync,
shutdown/reboot, and remount test. Early shell success could coexist with
dirty images or page-cache warnings (`432248`, `3b9c244`).

**REM64 implication:** make cache writeback, shutdown ordering, block-device
completion, and remount persistence part of the platform test plan. Do not
equate a successful `write()` with durable storage.

**Status:** likely requirement.

The 10 GiB REM32 development-image follow-up (2026-09-27) persisted and
host-recovered a single-source chibicc executable, source, object, and
development files after `sync`, supervised-shell exit, QEMU stop, and reboot;
verified guest and recovered-host hashes matched. This did not validate a
multi-source build, Kilo/Samurai workflow, near-full behavior, or clean guest
unmount. The guest lacked a working poweroff/remount-read-only path; tests
stopped QEMU after `sync`, and ext4 journal recovery ran on the next boot.
An immediate unsynced QEMU kill lost a recent file. A private-copy `e2fsck`
journal replay repaired post-stop metadata before read-only verification.

**Classification:** filesystem durability and kernel shutdown integration.
**REM64 REQUIRED:** preserve completed writes after the documented sync
boundary and make journal recovery safe after interruption. **RECOMMENDED:**
provide a guest shutdown path that flushes writes, unmounts or remounts the
filesystem read-only, then halts; test this separately from abrupt-stop
recovery. **OPEN:** REM32 clean unmount and full multi-source developer-build
acceptance remain unproven. Details and hashes: [REM_EXT4_PERSISTENCE.md](REM_EXT4_PERSISTENCE.md).

The Fold Termux package passed host-side checks for its ARM64 ELF executable,
interpreter, shared-library dependencies, hashes, and ext4 image format, but
the Android QEMU binary has not yet booted on the physical Fold. Separately,
host-QEMU ext4 tests preserved synced data across QEMU stop/reboot and recovery,
but the guest kernel still lacks a proven clean unmount/poweroff path; an
immediate unsynced QEMU kill lost a recent file (`REM_PHONE_DEPLOYMENT.md`,
`REM_EXT4_PERSISTENCE.md`).

**REM64 deployment requirement:** a valid host package/checksum audit does not
establish device execution or durable guest storage. Acceptance must run the
exact packaged emulator/kernel/rootfs on target hardware, sync and reboot the
same writable image, verify user files, then test offline backup and restore.
**Open:** Fold execution and a guest clean-shutdown path remain unverified.

The inspected Fold MVP ext4 image contains utility binaries such as Git, tar,
and Python, but has no Yash, Kilo, chibicc, Samurai, or guest GCC at their
expected executable paths; several of these were tested only in separate
staging images. **REM64 deployment requirement:** distinguish a bootable base
rootfs from a development image with a machine-checkable manifest of required
programs, resolved paths, hashes, and executable modes. Host-side staging or
an adjacent source tree is not evidence that a tool is installed in the image.
**Status:** deployment integration requirement demonstrated by REM32 package
inspection.

## ELF, binutils, and the loader form one platform boundary

REM32 required coordinated GAS/LD relocations, a target linker script, ELF
program headers, BSS zeroing, and QEMU/Linux loading. A compiler pass or host
`readelf` result was insufficient; the executable had to load and run under
the target emulator (`667b304`, `MYEMULATOR_2_LINUX_PORT.md`).

**REM64 implication:** freeze ELF class, endianness, relocations, load
addresses, auxiliary-vector rules, and linker-script assumptions before
building a userspace ecosystem. Test compiler → assembler → linker → loader
→ execution as one pipeline.

**Status:** likely requirement.

## Small compilers are valuable architectural probes

The chibicc-REM work demonstrates a complementary path: a small C compiler can
emit, assemble, link, and execute native REM programs while exposing the
minimum ABI needed for self-hosting. Its simplicity makes generated code and
feature gaps easier to inspect than a full GCC port.

**REM64 implication:** maintain a small reference compiler and executable
conformance suite alongside the production compiler. Use it to validate the
ABI and bootstrap tools, not as a substitute for a complete optimizing
toolchain.

**Status:** demonstrated strategy; open question is the required feature set.

## Linux requires ABI conformance before the ecosystem

REM32 repeatedly changed ABI details after Linux, musl, BusyBox, and compiler
failures exposed assumptions that unit tests had missed. The practical lesson
is stronger than “document the ABI”: a Linux port should not begin until
calling convention, exception return, syscall, signal, TLS, aggregate, and
varargs conformance tests pass together.

**REM64 implication:** make ABI conformance a gate for Linux bring-up, rather
than porting libc and discovering the contract through application crashes.

**Status:** hard process requirement.

## Register-file choices are compiler choices

REM32’s fixed argument registers, link register, callee-saved set, frame-base
register, and limited register file became visible in generated GCC and musl
code. Register homes and frame layout were not merely encoding details; they
determined whether ordinary library calls remained safe after dynamic stack
allocation.

**REM64 implication:** evaluate register count, argument-register count,
link-register policy, zero-register policy, spill pressure, and debugger
visibility with a real compiler before freezing the ISA.

**Status:** open REM64 architectural question.

## Compiler-friendly addressing must cover real stack layouts

REM32’s large-frame and VLA failures showed that an instruction set can be
functionally complete yet awkward for compiler-generated addressing. Large
stack offsets, frame-pointer elimination, dynamic allocation, stack probing,
and TLS access all need practical code-generation paths.

**REM64 implication:** test load/store offset range, address materialization,
frame elimination, stack probes, and TLS sequences against generated code
before selecting the final addressing modes.

**Status:** likely requirement.

## Compiler runtime is part of the hosted ABI

REM32 hosted programs required compiler-runtime helpers alongside musl. Integer
division and multiplication helpers, atomics, floating-point helpers, stack
protection, and unwind support can be called by otherwise ordinary C code.

**REM64 implication:** define a compiler-runtime ABI and ship its implementation
with the compiler/libc toolchain. Treat `libgcc`-equivalent archives or their
replacement as a platform component, not an optional compiler detail.

**Status:** likely requirement.

## The atomic memory model must be selected early

REM32 has CAS in the ISA, but portable C11 atomic lowering and the Linux atomic
ABI remain separate work. That gap is visible only when real compiler and
library code uses atomics.

**REM64 implication:** decide whether the platform uses CAS, LL/SC, or AMOs;
define acquire/release/sequentially-consistent ordering, alignment, failure
results, and compiler lowering before SMP or threaded libc work.

**Status:** open REM64 architectural question.

## Console is an ABI, not only a device

REM32’s current terminal lessons extend beyond serial bytes. A usable console
needs a defined geometry source, serial and/or virtual-terminal model, raw and
canonical modes, cursor addressing, control-key behavior, Unicode policy, and
an explicit answer about optional framebuffer or mouse protocols.

**REM64 implication:** specify the console ABI independently of any one shell
or editor. A serial console may remain the bootstrap device, but terminal
geometry and input semantics must be testable platform services.

**Status:** open REM64 subsystem question.

## Debugging facilities are architectural productivity features

REM32 bring-up relied heavily on emulator register/memory inspection and
targeted traces because native breakpoint and single-step facilities were not
available as a complete workflow.

**REM64 implication:** decide early whether the architecture exposes hardware
breakpoints, watchpoints, single-step state, debug registers, a trace buffer,
and a stable GDB remote-protocol mapping. These should be specified so QEMU,
the kernel, and debuggers expose the same state.

**Status:** likely development requirement; exact mechanism remains open.

## Hosted libc depends on a compiler-runtime archive

The native chibicc pipeline exposed a packaging requirement that object-only
assembly did not: the REM musl `libc.a` calls the compiler helper `__muldi3`
from `memset.o`. A rootfs containing `crt1.o`, `crti.o`, `crtn.o`, and
`libc.a` therefore cannot link an ordinary hosted C program unless the
matching REM `libgcc.a` is present. Adding the GCC-produced archive to an
isolated copy of the rootfs made the native link and `Hello from REM` test
complete successfully.

**REM64 implication:** define and package the compiler-runtime ABI together
with libc. Record which helper archive supplies integer, division, atomic, and
soft-float routines, and test a hosted C link with the exact runtime tuple.

**Status:** likely requirement for a hosted REM64 C environment.

## The C data model must be frozen before the compiler port

REM32 uses an ILP32 model: `int` is 32 bits, `long` is 32 bits, and pointers
are 32 bits. The chibicc port could therefore match the existing GCC ABI with
one word-sized integer and pointer representation. A 64-bit target cannot
silently carry that assumption forward.

**REM64 implication:** choose and document the data model before implementing
the C ABI. LP64 means `int=32`, `long=64`, and `pointer=64`; LLP64 means
`int=32`, `long=32`, and `pointer=64`. The choice affects `sizeof`, structure
layout, syscall structures, varargs, format strings, compiler helpers, and
interoperability with libc.

**Status:** open REM64 ABI decision.

## The compiler, runtime, and linker must be tested as one toolchain

REM32 bring-up showed that a working instruction backend is insufficient. The
compiler needed the established stack alignment and calling convention,
register allocation rules, and a matching `libgcc.a`. Hosted linking also
depended on the CRT objects, the REM linker script, and archive ordering:
musl `libc.a` references helpers such as `__muldi3`, so the compiler-runtime
archive must be available and presented in an order the native linker resolves.

**REM64 implication:** define stack alignment, argument and return registers,
callee/caller preservation, and compiler-helper ABI before completing register
allocation. Treat `crt1.o`/`crti.o`/`crtn.o`, libc, libgcc-equivalent runtime,
linker script, and loader as one tested release tuple.

**Status:** likely requirement.

## Linux support requires a complete minimum CPU platform

REM32 Linux bring-up depended on more than instruction execution. The kernel
required an MMU and page-table support, exceptions and interrupt delivery,
atomics, a timer, the syscall register convention, and defined memory-ordering
behaviour. Omitting any one of these moved the failure into libc, process
startup, or filesystem code instead of producing a small, obvious CPU test
failure.

**REM64 implication:** maintain an explicit Linux CPU checklist covering MMU,
exceptions, interrupts, atomics, timers, syscall entry/return, privilege
transitions, and memory ordering. Each item needs a kernel test before libc
and userspace are used as the first probe.

**Status:** likely requirement.

## A usable target needs a minimum native userland

The REM32 flight environment demonstrated that native development is much more
useful when the target ships its own basic workshop: `/bin/sh`, an editor, a
compiler, assembler, linker, a build tool such as `make`, and practical
inspection/debugging tools. Assembly-only work can start with the binutils
subset, but compiling hosted programs additionally requires CRT objects, libc,
and the compiler-runtime archive.

**REM64 implication:** define a minimum self-hosted userland profile rather
than treating the shell or compiler as optional afterthoughts. At minimum,
plan for a shell, editor, compiler, assembler, linker, build tool, and
debugging/inspection utilities, with each binary tied to the same kernel,
ABI, libc, and runtime tuple.

**Status:** likely product requirement.

## A native build tool is a compiler conformance test

The Samurai probe showed that a small Ninja-compatible build tool exercises
more of the C and POSIX boundary than a hello program: variadic diagnostics,
`switch`, 64-bit hash arithmetic, aggregate structures, process spawning,
`wait`, `stat`, directory creation, and dependency-file I/O. The current REM
chibicc compiler can run simple hosted programs but cannot yet compile Samurai
from source.

**REM64 implication:** include a small real build tool in compiler and libc
acceptance tests. It is a better self-hosting checkpoint than a trivial
program because it simultaneously tests the language ABI, compiler runtime,
filesystem calls, process creation, and incremental-build primitives.

**Status:** demonstrated compiler/toolchain requirement.

## Terminal geometry cannot be optional

REM32’s serial console initially provided byte I/O without a complete terminal
model. Kilo calls `TIOCGWINSZ` and, when the returned geometry is unusable,
falls back to the ANSI cursor-position query `ESC [ 6 n` and waits for a
response. A tty that reports ioctl success with zero or invalid dimensions can
therefore make a correct editor block forever. Yash exercises the same gap
through `isatty`, `TCGETS`, `TCSETS`, raw-mode setup, `tcdrain`, and polling
before it can present a usable line editor.

**REM64 implication:** specify the console subsystem as an architectural
component rather than as a UART byte stream. Define sane serial-console
geometry (for example, a documented default window size), termios raw and
canonical modes, cursor addressing and response semantics, control-key input,
polling, and process-terminal ownership. Test the contract with both an
interactive shell and a full-screen editor; do not make application-specific
workarounds substitute for the terminal model.

**Status:** likely requirement; the exact ANSI/virtual-terminal scope remains
an open REM64 subsystem question.

Disposable validation confirmed the minimum serial-console contract: reporting
`TIOCGWINSZ` as 24×80 let canonical Kilo enter its full-screen editor without
issuing the unanswered cursor-position query, save an edit, cleanly shut down,
reboot, reopen the file, and save a second edit.

**REM32 resize finding (2026-09-30):** the current syscall shim in
`linux/arch/myemulator2/kernel/syscall.c` returns a literal 24×80 for
`TIOCGWINSZ`; it does not derive dimensions from the host terminal. Kilo painted
at 24×80 in the automated console, while the reported human view used only a
small top-left area. This is separate from Kilo's text-buffer fault.
**REQUIRED Linux behavior:** full-screen applications must receive meaningful
window geometry. **REM64 RECOMMENDED:** provide a resize notification path
(including the appropriate foreground-process signal) as well as an initial
geometry. **OPEN:** choose how a serial or remote console supplies and updates
that geometry.

An image-level failure then produced a visible `REM#` prompt while Bash was
redirected to a regular-file `/dev/console`; typed commands were echoed by the
serial path but never reached shell stdin. **REM64 implication:** console
device wiring is part of the platform contract. Image init must bind the
interactive shell to the real tty device and boot validation must prove a
command round-trip, not only prompt visibility.

## ELF BSS growth must be tested with large static images

An apparent REM ELF-loader failure was traced through `padzero()` and
`filemap_fault()`: the final file-backed page was validly in a file VMA, but
the disposable ext4 image contained a truncated executable. The image’s
`/sbin/init` inode was 413 bytes although the ELF being tested was 14,632
bytes. `filemap_fault()` therefore correctly computed `max_idx=1` and returned
`VM_FAULT_SIGBUS` for page index 1. Replacing the image entry with the complete
ELF made the same 1 KiB-BSS reproducer reach `BSS_PASS`; no kernel or
`padzero()` change was required. The same packaging error also invalidated the
earlier direct-Yash result, so it must not be reported as an architecture
fault without first verifying the guest inode size and hash.

**REM64 implication:** make partial final pages and additional BSS pages an
explicit ELF-loader conformance test, and verify deployed ELF sizes/hashes
before debugging the loader.

**Status:** loader path validated by the corrected 1 KiB reproducer; image
construction remains a release-process requirement.

## Virtual hardware must match the software contract

The current REM32 QEMU machine exposes a custom serial console and custom
block device only. Its launch command has no `-netdev` or NIC device, QEMU’s
device list contains no virtio/e1000/rtl8139 network hardware, and the kernel
configuration has `CONFIG_NET` disabled. Therefore network-dependent
userland tests cannot distinguish missing software from absent virtual
hardware.

**REM64 implication:** publish a machine-peripheral inventory and make each
software capability conditional on a defined device contract. Decide the
network, entropy, storage, console, and optional display devices before
porting applications that require them; test the QEMU command line and the
guest-visible device nodes/interfaces together.

**Status:** proven REM32 limitation; network and entropy devices remain open
platform requirements.
# Network device contract

REM32 networking work shows that a standard Linux NIC can use virtio-net over
virtio-mmio without adding PCI: the machine still must specify an MMIO window,
interrupt line, device-memory mapping, and kernel driver configuration. REM64
will need these resources defined as part of its virtual motherboard contract;
the open question is whether to reserve a fixed virtio window/IRQ or provide a
discoverable device tree/ACPI description.

The networking bring-up also exposed that a Linux-compatible NIC is not enough
without the userspace syscall surface: mount, socket, connect, send/receive,
and netlink message syscalls are required before ordinary tools can configure
an interface. REM64 should treat the syscall ABI and netlink behavior as part
of the platform contract.

## Virtio requires a defined DMA/ring visibility contract

REM32 networking reached `eth0`, link-up, address and route configuration, and
the virtio interrupt path. Early observation appeared to show no consumable TX
descriptor, but a side-by-side trace later showed Linux and QEMU using the same
legacy split-ring addresses and QEMU consuming the descriptors. QEMU's packet
capture contains valid ARP and TCP SYN frames, so this run found no DMA/ring
implementation defect.

**REM64 implication:** still define and test virtio DMA addressability,
cache/coherency rules, ring endianness/barriers, queue-notify semantics, and
descriptor consumption before declaring a virtio device Linux-compatible.
**Status:** these are required conformance tests; REM32 disposable evidence
shows the split-ring contract working for TX.

## Network backend acceptance is separate from guest NIC correctness

REM32 produced valid ARP and TCP SYN frames and processed the QEMU-generated
ARP reply, while a disposable host TCP listener received no connection or
SYN-ACK. This separates guest virtio/IP correctness from QEMU user-network
host reachability.

**REM64 implication:** networking acceptance must include a known local
host/backend TCP endpoint in addition to guest-side interface and packet
tests. **Status:** required validation boundary; backend behavior remains an
environment-specific integration concern.

### Virtio-MMIO discovery must be part of the boot contract

In the 2026-09-29 RC3 UAT tuple, QEMU's `myemulator32` machine source creates
virtio-net at MMIO `0xf0200000`, IRQ 5, when the `net0` backend is present.
The runner supplies `-netdev user,id=net0`; the guest boot log reports only
`console=ttyMY0,115200 earlycon=myemulator2,0xf0000000 root=/dev/myemu0 rw
init=/sbin/init`. Runtime probes in this tuple found no usable guest interface
(`eth0` lookup returned `ENODEV`, and `/proc/net/dev` was absent), so interface,
loopback, external-IP, and DNS acceptance did not pass. **Classification:**
device discovery / kernel integration. Reviewing the selected kernel config
identified a concrete discovery gap: `CONFIG_VIRTIO_MMIO` and
`CONFIG_VIRTIO_NET` are enabled, but `CONFIG_CMDLINE_FORCE=y` forces a command
line with `ip=off` and no `virtio_mmio.device=...` declaration. The QEMU
`-netdev user,id=net0` backend therefore does not result in a guest-visible
interface in this tuple. The RC3 BusyBox also lacks the `ifconfig`, `ip`,
`ping`, `route`, `udhcpc`, and `nslookup` applets, so network configuration
and diagnostics are separately unavailable. **REM64 RECOMMENDED:** expose MMIO
devices through a standard firmware device tree or another explicit, validated
discovery contract, and check that the kernel's discovered address and IRQ
match the machine implementation before application network tests.

## Native linker is a first-class ABI/runtime acceptance test

REM32 native `as` can assemble ordinary objects, while the currently staged
native `ld` faults even on a minimal object; the same objects link with the
host REM linker.  This demonstrates that self-hosting requires a validated
native binutils/runtime tuple, not merely a compiler backend and ELF format.

**REM64 implication:** make native linker execution and a minimal
assemble/link/run test part of the architecture/toolchain acceptance criteria.
Treat CRT ordering, linker-script semantics, allocator/runtime behaviour and
debuggable fault reporting as an explicit host-tool contract.

**Status:** required toolchain acceptance behaviour; the REM32 native-link
failure remains an implementation blocker, not an architectural redesign.

## Call-boundary alignment must be defined independently of argument staging

The REM chibicc backend initially padded the complete temporary argument list
before every call.  Because register arguments are popped before the call,
that left a one- or two-argument call with an unaligned `sp`; a real Samurai
binary then trapped in an otherwise ordinary library call.  The fix computes
padding from the words that remain on the outgoing stack (the ABI home area is
already 16-byte sized).

**REM64 implication:** specify call-boundary stack alignment in terms of the
actual outgoing stack layout, including register-save/home areas, and require
compiler tests for zero, register-only, and stack-passed argument calls.
**Status:** required ABI/toolchain rule demonstrated by REM32; the exact
REM64 register and home-area layout remains an open design choice.

### Variadic calls must model array decay before ABI word counting

REM32 exposed a compiler bug when a string literal (array type in the AST) was
counted by its byte size at a variadic call boundary.  The caller then staged
the literal as a two-word aggregate instead of one pointer word, shifting all
following arguments.  REM64 code generators should apply C array-to-pointer
decay before ABI register/stack word allocation and test variadic calls with
pointer and scalar arguments together.  This is a demonstrated compiler
requirement, not a REM64 register-layout decision.

### Variadic register-save areas must survive callees

REM32 chibicc initially homed variadic `r1`--`r4` inside the active callee
frame.  A call to GCC/musl `vfprintf` overwrote that memory while constructing
its own frame, changing the `va_list` cursor and producing the named format
argument as the first `%s` value.  Moving the save area to old-SP+16, outside
the active frame, made the mixed compiler/libc probe print the expected values.

**REM64 implication:** define the variadic register-save/home-area lifetime
explicitly and require it to remain valid across ordinary calls.  This is a
required ABI/toolchain property demonstrated by REM32; the REM64 register
layout and exact save-area size remain open design choices.

### Static pointer tables require ordinary data relocations

The REM chibicc backend initially emitted the byte image of global aggregates
but dropped their recorded pointer relocations.  Samurai's static keyword
table consequently contained null names and failed in `strcmp` while parsing a
build graph.  Emitting 32-bit relocations for pointer fields fixed an isolated
struct-pointer probe under REM.

**REM64 implication:** define and test static data relocation types and require
the compiler/assembler/linker path to preserve pointers in arrays of structs.
This is a required ELF/toolchain property; exact relocation encoding remains
an implementation choice for REM64.

### Wide-value stores need a distinct address register

REM32 chibicc exposed a backend error when assigning a plain 64-bit integer:
the high value word occupied `r2`, while the generated store also treated
`r2` as the destination address.  Samurai's hash implementation then stored
through a stale register and trapped.  The fix reserves `r3` for the address
when storing any two-word value.

**REM64 implication:** document wide-value return/store conventions so a
compiler never aliases an address register with a value word.  This is a
compiler/ABI requirement demonstrated by REM32; the REM64 register assignment
is an open design choice.

## Loop header labels must be explicit in compiler IR

The REM chibicc backend exposed a fragile control-flow boundary in Samurai's
nested ARGBEGIN shape: loop headers were generated from a separate codegen
counter while `break` and `continue` targets were parser-owned labels. The
backend now stores an explicit header label on every loop node and keeps it
distinct from the continue and break labels. The focused nested
`for`/`switch` regression assembles and links successfully.

**REM64 implication:** compiler-facing control-flow labels should have explicit
IR ownership and stable nesting semantics; do not make loop back-edges depend
on an unrelated emission counter. **Status:** required compiler implementation
practice demonstrated by REM32; exact IR representation remains open.

## Narrow-to-wide returns must define the high word

The Samurai metadata path exposed a REM chibicc defect independent of the
kernel and musl `stat` implementation: a function returning `int64_t` from a
32-bit expression populated `r1` but left `r2` stale.  Comparing the missing
file sentinel then produced incorrect graph state.  The backend now sign-
extends signed casts and clears the high word for unsigned widening; the
focused regression is `toolchain/chibicc-rem/test/rem-wide-return.c`.

**REM64 implication:** specify integer widening and scalar return extension
rules in the ABI, and test narrow-to-wide returns at compiler and libc
boundaries. **Status:** required ABI/compiler rule demonstrated by REM32.

## Userspace feature configuration is part of the tested artifact

On 2026-09-27, BusyBox 1.37.0 was rebuilt with the current REM GCC and staged
musl/libgcc runtime, then booted from a clean disposable ext4 image. Its
`--help`, `ls`, `cp`, `mv`, `sh`, and `printf` paths passed. Earlier BusyBox
artifacts had `cp`/`mv` low-address faults (`0x30c`), while this build had no
user fault or ext4 error. The historical handover attributes that failure to
a stale compiler artifact predating the alloca/VLA fix; this retest found no
new compiler defect.

The first test binary lacked the BusyBox dispatcher/help applet because
`CONFIG_BUSYBOX` was unset. Enabling it in the disposable config made
`busybox --help` print usage and return zero. This was a build-configuration
omission, not a CPU, ABI, libc, syscall, or filesystem defect.

**REM64 implication:** record the selected application feature configuration
with each tested binary and exercise the requested applets; a successful link
or presence of a multicall binary does not establish that its dispatcher and
features were built. This is a reproducibility and validation recommendation,
not a hardware or ABI requirement. **Status:** demonstrated by the REM32
BusyBox retest; broader application/configuration coverage remains future
work. See [the BusyBox compatibility report](REM_BUSYBOX_COMPATIBILITY_20260927.md).

The guest log also contained a non-fatal `MYEMU llist bad add` kernel
diagnostic while all six tested BusyBox commands completed. Its cause was not
established in this work; it is an open kernel investigation, not evidence of
a BusyBox applet or filesystem failure.

## Vim exposed syscall and cross-build runtime assumptions

REM Vim work tested terminal Vim 9.1 with Yash, the REM musl overlay, and a
static guest probe on kernel SHA-256
`5bd96e5d7e9163d95e8470eb550504357d84e78d38cd269781dfb096a511638a` and QEMU
SHA-256 `0154db549ad01d0bcdfd79c0dde9544266fc14c75fe7825d14942ff47a75641a`.
The probe exercised libc/raw `fsync`, `pthread_create`, and libc/raw POSIX
timer calls. Results and the reproducible probe are in
`docs/REM_VIM_COMPATIBILITY.md` and
`toolchain/tests/rem-vim-syscall-probe.c`.

The tested musl `fsync` call (number 82) returned `ENOSYS` while raw call 74
succeeded; the kernel dispatcher currently implements fsync at 74. This is a
**syscall ABI/kernel** mismatch and explains Vim's default-save `E667`.
`pthread_create` returned `EAGAIN`; musl's `SIGEV_THREAD` timer path consequently
failed with `E1286` before entering the kernel. Raw timer calls 107 and 409
also returned `ENOSYS`. Vim's cross-build cache had guessed timer support was
available, an **application/build-configuration** assumption unsupported by
the runtime. Rebuild variants with timeout features disabled separately
faulted before drawing the buffer (`MYEMU_BAD_USER_FAULT`, address `0x46c`);
the exact cause is not established and remains a **userspace ABI/runtime**
investigation, not a demonstrated CPU or filesystem defect.

No kernel, libc, compiler, or Vim production source was changed for this work.
The focused syscall probe was added and run in a disposable rootfs; the
existing Vim binary had only previously completed edit/save/readback with a
`nofsync` workaround. A retest on the same QEMU/kernel tuple and seed rootfs
confirmed `vim --version` returns Vim 9.1, but launching Vim interactively
stalled before a visible buffer or shell prompt. No `E1286` or user fault was
captured in that interval; save, quit, and readback were not reached, so this
does not clear earlier timer/thread or fsync findings. Normal Vim startup and
save are therefore not accepted. Exact tuple and test evidence are recorded
in `docs/REM_VIM_COMPATIBILITY.md`.

**REM64 implication:** share syscall numbers between kernel and libc from one
ABI definition, and validate cross-build feature answers against guest runtime
probes. Thread creation and timer support must be tested as separate layers:
library emulation may depend on process/thread primitives before reaching a
timer syscall. **Required:** consistent syscall numbering and defined thread
creation semantics for applications using `SIGEV_THREAD`. **Future work:**
resolve timer syscall support, Vim's low-address fault, and rerun default
terminal editing and durable-save acceptance; see
`docs/REM_VIM_COMPATIBILITY.md`.

A configuration A/B test on the older `c7754706…` development kernel and the
same QEMU found that both plain Vim 9.1 and `vim -u NONE -N` rendered their
startup screen; disabling the user vimrc did not change that initial display.
On that tuple, a separate launcher acquired a session and controlling tty,
but Vim did not paint a buffer within 60 seconds; the kernel logged an
identity-map replacement during `exec`, without a fatal fault. This is
**application/runtime validation** evidence only: it does not identify the
cause of the stall on the distinct Yash tuple, nor prove terminal input or
save behavior. **RECOMMENDED:** test startup configuration, terminal input,
and save as separate steps on one hash-pinned tuple; do not infer that
`-u NONE` disables compiled timer dependencies or repairs tty handoff. Details
and test limitations: `docs/REM_VIM_COMPATIBILITY.md`.

A later clean-rootfs run on the current TTY package (`QEMU 0154db…`, kernel
`e2516e8…`) found a sharper pre-terminal failure: both normal Vim 9.1 and
`vim -u NONE -N` faulted at the same PC in `syn_name2id`, dereferencing
`r5=0x2613fa48` outside the reported stack/heap ranges. The kernel also logged
ext4 `mpage_prepare_extent_to_map` pages without attached buffers, despite a
clean pre-boot `e2fsck`. This is a **kernel/MMU or filesystem/page-cache plus
userspace-state** boundary; current evidence does not establish which side
originates the invalid pointer. **REQUIRED validation practice:** compare
normal and configuration-disabled modes on the same hash-pinned clean image,
and capture both the app PC/registers and page-cache/filesystem diagnostics.
Do not classify it as a vimrc or terminal problem: no screen, timer error, or
save attempt was reached. See `docs/REM_VIM_COMPATIBILITY.md`.

## Runtime probes: process, PTY, mapping, signals, and timers

Three static REM probes exercised the current Yash-compatible kernel
`5bd96e5d7e9163d95e8470eb550504357d84e78d38cd269781dfb096a511638a` and QEMU
`0154db549ad01d0bcdfd79c0dde9544266fc14c75fe7825d14942ff47a75641a`. The
tests were run from Yash against a disposable rootfs clone; probes and a
`/dev/ptmx` node were staged there, not in the flight image. The repaired
post-test clone hash is `7208a549f687cb6161590b779f2097a3e78703493603e5c56bbeaa9957813a7f`.
Reproducers: [process/session](../toolchain/tests/rem-process-session-probe.c),
[terminal/PTY](../toolchain/tests/rem-terminal-pty-probe.c), and
[VM/runtime](../toolchain/tests/rem-vm-runtime-probe.c).

**Process/syscall and ABI:** `fork`, `execve` of BusyBox `true`, `waitpid` for
zero exit, `setpgid`, and child `setsid` succeeded. The shell's `getsid(0)` was
0, consistent with the separately observed missing login-session setup.
Libc `clone(SIGCHLD)` returned `ENOSYS`, while raw syscall 220 with fork-style
flags and no new stack created a child. This proves the syscall entry exists
for that limited form, but not that the public libc `clone` API or independent
child stacks work. **OPEN:** isolate the libc clone wrapper versus kernel
clone-variant contract. The kernel dispatcher also currently passes literal
zero as `sys_clone`'s TLS argument (`linux/arch/myemulator2/kernel/syscall.c`,
`MYEMU2_NR_CLONE`); this is a **kernel ABI defect candidate** for
`pthread_create=EAGAIN`, not a proven explanation of that return.

The process probe and a BusyBox shell command both showed `_exit(24)` as
termination by signal 24 (`SIGXCPU`); Yash reported status 408 for the child.
Current dispatch calls `do_group_exit(a0)` with the unencoded user exit value
(`linux/arch/myemulator2/kernel/syscall.c`, `MYEMU2_NR_EXIT`/
`MYEMU2_NR_EXIT_GROUP`). This is a confirmed **syscall/kernel process-ABI**
failure, consistent with the nonzero-exit wait-status issue recorded below.
Linux-compatible `exit`/`exit_group` must preserve normal exit codes as
`WIFEXITED`/`WEXITSTATUS` values distinct from signal termination.

**Terminal and device setup:** termios and `TIOCGWINSZ` worked (80x24).
`tcgetpgrp` returned `ENOTTY` while Yash lacked a controlling terminal, as in
the focused session probe above. UNIX98 PTY mount, `grantpt`, `unlockpt`,
`ptsname`, slave open, and bidirectional master/slave data transfer all passed
after the disposable rootfs was given a mode-0666 `/dev/ptmx` character node
(major 5, minor 2). Direct `mknodat` syscall 33 returned `ENOSYS`; the source
dispatcher has no mknodat case. Therefore PTY core and devpts work, but the
tested rootfs lacks the node and the guest cannot create it through this
syscall. This is a **kernel syscall plus rootfs/device-provisioning** gap;
provide the node in the image or implement Linux-compatible mknodat.

**Memory and runtime:** anonymous and read-only file-backed `mmap`,
`mprotect`, and both corresponding `munmap` calls passed. `msync` returned
`ENOSYS` (musl syscall 227; no dispatcher case), a **kernel syscall** gap.
`sigaction`, `sigprocmask`, self-`kill`, handler delivery, and `sigsuspend`
passed (`sigsuspend` returned `EINTR` as expected). POSIX `timer_create` 107
and `timer_settime` 409 both returned `ENOSYS`; these **kernel syscall** gaps
are independent of the signal path. `pthread_create` returned 11 (`EAGAIN`);
the exact failing clone flags/TLS path remains untraced.

**REM64 implications:** **REQUIRED** Linux process-status encoding, clone TLS
argument forwarding for pthreads, mapped-file synchronization, and timer
syscalls when advertised by libc. **RECOMMENDED** a defined device-node
provisioning path for `/dev/ptmx` and integration tests that boot with the
actual rootfs layout. **OPEN** the public `clone()`/raw syscall discrepancy
and the precise kernel or musl cause of `pthread_create` failure. No kernel,
libc, shell, or application source was changed for these probes.

The OpenSSH 10.5p1 guest tools still fault before reaching network, process,
signal, or PTY workloads; these results do not explain their earlier user
fault. Keep that compiler/ABI/runtime attribution open as recorded in
[REM_OPENSSH_COMPATIBILITY.md](REM_OPENSSH_COMPATIBILITY.md).

## Large-frame compiler output must preserve the caller frame

Emacs 30.1's old REM build failed in `make_pure_string()` at PC `0x021b1bf8`,
address `0x460`, with frame pointer `0x450`. A minimal freestanding test in
`toolchain/tests/emacs-frame-regression.c` isolated this to the older GCC
backend: for a function with a 1104-byte local frame, generated prologue code
used `r12` for the frame size after saving the incoming frame pointer there,
then restored that size as the caller's FP. The caller's later frame-relative
load consequently returned the wrong result. The old compiler's test returned
1 instead of 0; current repository GCC and host-built REM chibicc both returned
0 in QEMU. This is a **compiler/backend ABI-preservation defect**, not evidence
of a CPU, libc, syscall, or filesystem defect.

A complete Emacs 30.1 rebuild in an isolated target tree using current GCC,
REM `as`/`ld`, musl CRT and libraries, and the REM linker script produced a
static ELF32 REM executable with a real `_start` entry. In the disposable
guest, `emacs --version` then remained silent and did not return. The rebuilt
ELF SHA-256 is
`bb4d31623a70a3e314de017791202b8ef0610202c41f4ac6c33c121f627022d6`; the
test used kernel SHA-256
`c77547060463da37cf9730e41e799c94998ad9f6e24e2c1a3282ee3299215a63`. The
process later terminated with `MYEMU_BAD_USER_FAULT`: PC `0x022f8464` in
musl `__malloc_alloc_meta`, writing at SP `0x3f7d6fe4`, with no VMA. The
initial stack pointer reported in `r11` was `0x3ffd6dbc`, 8,388,056 bytes
higher; disassembly shows the faulting instruction stores a saved register
to the current stack frame. A minimal current-GCC probe,
`toolchain/tests/emacs-stack/startup-stack-probe.c`, confirmed the guest's
8 MiB `RLIMIT_STACK`: 7 MiB alloca and malloc after 7.5 MiB alloca pass;
4 KiB malloc/free passes; `brk` remains fixed during stack allocation and
advances by 8 KiB for the small malloc. This locates Emacs's immediate fault
at stack-growth limit exhaustion, with the allocator merely executing when
the stack-frame save reaches the boundary. A 9 MiB alloca passes after
raising `RLIMIT_STACK` to 16 MiB, showing the guest can grow the stack beyond
8 MiB when its configured limit permits. The reason Emacs startup consumes
that stack remains open. Procfs maps are unavailable in this guest (`mount`
of procfs returns `ENOSYS`); the kernel diagnostic reports no accepted VMA
after expansion fails. A 512 MiB guest sample landed in a timer interrupt
path; extra RAM alone did not demonstrate startup success. No fix or
regression for Emacs's excessive stack use has been validated. Batch,
terminal editing, save, and Yash readback remain untested. The earlier
small-frame regression passes do not establish full application acceptance.
Temporary `main` markers on a corrected-toolchain build returned through
argument sorting, `getcwd`, stack-limit handling, locale setup, and signal
initialization; finer markers reached `init_random()` and did not reach the
following `init_xfaces()`. Emacs calls `getrandom(..., flags=0)` there. A
native REM blocking probe remained in the call; a `GRND_NONBLOCK` probe
returned the `EAGAIN` branch (`GETRANDOM_NOT_READY`). The syscall is present
at number 278, so this is not a missing syscall: the guest urandom source was
not initialized at test time. The inspected kernel build config has
`CONFIG_HW_RANDOM` and `CONFIG_CRYPTO_JITTERENTROPY` disabled, and the current
QEMU machine source creates console, block, and optional virtio-net devices,
with no RNG device. That config was not matched to the deployed kernel hash;
the exact entropy-pool initialization path remains unverified. The earlier
default-stack fault remains observed; it is unknown
whether it is independent or appears after this block is cleared. See the
phase record and probe in
[`REM_EMACS_QUALIFICATION.md`](REM_EMACS_QUALIFICATION.md#startup-phase-trace-still-incomplete).

**REM64 implication:** specify callee-saved register obligations independently
of frame size and require generated-code execution tests where a caller uses
its frame after calling a large-frame callee. Compiler version/provenance and
the matching CRT/runtime/linker tuple are part of the tested binary. **Status:**
the old backend defect is reproduced and current GCC passes the focused test;
the corrected Emacs guest startup remains in progress. **Required Linux
compatibility:** define the initial process stack, its growth-down mapping,
and its configured size limit consistently; the tested 8 MiB soft limit
rejects Emacs startup's further stack growth. Keep large real-application
stack workloads in compiler/runtime validation because a small frame
regression does not establish bounded stack use. This failure does not
indicate a CPU requirement. **Required Linux behavior:** `getrandom()` with
zero flags blocks until the urandom source is initialized, while
`GRND_NONBLOCK` can report `EAGAIN`; the REM guest's implementation follows
that distinction but starts userspace with the source not ready. **Recommended:**
provide an entropy source or initialize the pool from a boot seed before
launching applications that make blocking entropy requests. **Open question:**
choose and validate the REM boot/device/persistence mechanism for supplying
that seed. See
[`REM_EMACS_QUALIFICATION.md`](REM_EMACS_QUALIFICATION.md) and
[`toolchain/tests/emacs-frame-regression.c`](../toolchain/tests/emacs-frame-regression.c).

The corrected Emacs trace further distinguishes a startup wait from the
separate earlier stack fault: `main` reaches `init_random()` after successful
locale and signal setup, then blocks in flags-zero `getrandom()` before the
next `init_xfaces()` marker. `GRND_NONBLOCK` returns `EAGAIN`, showing the
syscall is present while the urandom source is not ready. This establishes an
application-triggered boot-readiness dependency; it does not establish a
stack or allocator fault in that traced run. The earlier default-stack fault
remains open and must not be conflated with the entropy wait. Full call-stack
capture and startup beyond `init_random()` remain future work; see
[`REM_EMACS_QUALIFICATION.md`](REM_EMACS_QUALIFICATION.md#current-first-stop-classification).

## Native process debugging needs ptrace and a stop contract

The existing `tools/mydebug` is a host-side QMP client for the earlier 8-bit
QEMU target; it does not debug Linux REM processes. The REM32 native debugger
work tested syscall 117, `PTRACE_TRACEME`, register get/set, tracee memory
read/write, continue, software breakpoints, single-step, process exit, and
fault stops against a chibicc-built ELF in QEMU. A real breakpoint at `main`
was hit, the reported PC/registers and symbolized stack were inspectable, a
single instruction was stepped, normal execution exited with status 0, and a
deliberate bad load stopped as `SIGSEGV` with the faulting PC/address visible.
See [`REM_DEBUGGER.md`](REM_DEBUGGER.md) and
[`tests/debugger/run-qualification.py`](../tests/debugger/run-qualification.py).

The first software-step implementation tried `ptrace_access_vm()` to restore a
temporary breakpoint from inside the tracee's own trap handler. That self
access returned `-EIO`, left the temporary `BREAK` in the code page, and caused
an immediate second trap. Restoring through `access_process_vm()` in trap
context fixed the reproduced case. Kernel POKEs also need instruction-cache
coherency: REM32 now uses a privileged code-flush instruction and QEMU drops
translated blocks before execution resumes. The debugger advances saved PC
past an explicit `BREAK`; the stop reports PC at the trapping instruction.

The qualification single-stepped legal self-targeting `j .` and `jal .`
instructions. The kernel initially rejected the successor because the
temporary trap address equaled the current PC. Placing BREAK at that PC gives
the non-linking jump its unchanged observable state; for `jal`, the trap path
also applies the instruction's LR=`PC+4` effect before reporting the stop. Both
checks passed. This is a **kernel/debugger stepping** edge case, not a REM32 ISA
defect. **Required Linux/toolchain behavior:** single-step must preserve the
observable effects of a legal instruction even when its control-flow target
is itself. **Recommended:** include self-targeting linking and non-linking
branches/jumps in debug-step tests. **Open question:** REM64 may use another
trap mechanism, but its single-step contract should cover control transfers
to the current PC.

Adding debugger `load FILE` exposed the exit-event part of the ptrace contract:
after killing a stopped tracee with `PTRACE_O_TRACEEXIT`, the first `waitpid()`
can report an exit event rather than final process death. Continuing that event
and reaping the final status prevents an orphaned stopped tracee. This is a
**Linux syscall/process-control** requirement, not a CPU requirement.
**Required:** debugger clients must drain ptrace exit stops before discarding a
tracee ID. **Recommended:** test repeated load/kill/reap cycles. REM64 should
preserve the Linux event and wait semantics if it supports ptrace.

**Required Linux/toolchain behavior:** provide coherent ptrace register state,
tracee memory access, stop/resume and signal-delivery semantics; a claimed
single-step operation must execute one instruction and stop. Code modification
through ptrace must be visible to instruction fetch. **Recommended:** keep the
register, stop-PC, breakpoint, and executable-code-coherency contracts explicit
across CPU, kernel, emulator, and debugger. **Open question:** choose REM64's
debug trap/step mechanism and define any hardware debug state; REM32 evidence
does not require dedicated debug registers or a new instruction-set feature.

A separate GCC `-O0 -g` qualification compared DWARF frame-relative
locations with emitted REM32 instructions. The `main` DIE described `argc`
at `DW_OP_fbreg 0`, while the function stored it at `16(r15)`; `add_values`
described its local at `DW_OP_fbreg -20`, while generated code stored it at
`-4(r15)`. The debugger reads formal argument values from the ABI registers
only at a function's entry breakpoint; it lists but does not evaluate the
inconsistent stack locations. This is a **compiler/debug-info
consistency failure**, not a CPU or ptrace failure. **Required toolchain
behavior:** debug location expressions must identify the values produced by
the same executable's machine code. **Recommended:** add executable
DWARF-location checks to ABI qualification. The test chibicc `-g` artifact
did not emit a source compilation unit, so its source-variable support also
remains unavailable. Details are in [`REM_DEBUGGER.md`](REM_DEBUGGER.md).

## Chibicc backend lessons from the native build chain

The isolated REM chibicc backend has reached the level where it emits,
assembles, and links ordinary hosted programs and all thirteen unmodified
Samurai translation units. Its compatibility target is the existing REM GCC
ABI: ILP32 data sizes, `r1`--`r4` arguments, 16-byte call-boundary stack
alignment, `r13` stack pointer, `r14` link register, static ELF linking, and
matching CRT/libc/libgcc objects. The native path is:

`chibicc -> REM assembly -> native as -> native ld -> CRT/libc/libgcc -> ELF`.

Confirmed backend defects and focused regressions are recorded in
[REM_CHIBICC_BUGLOG.md](REM_CHIBICC_BUGLOG.md), including call alignment,
variadic save-area lifetime, array decay at calls, static pointer relocations,
wide stores, and nested loop/switch labels. These are **fixed** in the
backend source and have passed assembly/link-level regressions; final guest
Samurai acceptance remains a separate runtime test.

**REM64 implication:** a new target needs an executable ABI conformance suite
before libc and build tools are trusted. The suite must include mixed compiler
calls, variadic calls into libc, pointer-containing static aggregates, wide
values, nested control flow, and compiler-to-linker relocation checks.
**Status:** required toolchain process demonstrated by REM32.

## Application-driven compatibility evidence

Real applications exposed different layers of the platform contract. Samurai
exercised compiler control flow, static data relocation, variadic diagnostics,
and wide-value stores. GNU Make exercised child-process and runtime assumptions
in its recipe-launch path. Vim exposed syscall-number consistency and build
configuration assumptions for fsync, timers, threads, and terminal operation.
Emacs and coreutils probes exposed remaining compiler frame-generation and
large-local-frame investigations. These findings are tracked in
[REM_SAMURAI_STATUS.md](REM_SAMURAI_STATUS.md),
[REM_MAKE_FALLBACK_QUALIFICATION.md](REM_MAKE_FALLBACK_QUALIFICATION.md), and
the application compatibility reports.

**REM64 implication:** qualify the platform with representative build tools,
editors, and utilities rather than hello-world binaries alone. Record whether
a failure belongs to CPU, ABI, compiler, libc, syscall, filesystem, or
application configuration before changing the architecture. **Status:**
required validation discipline; individual unresolved application failures
remain future work.

### Current chibicc hosted-application probes

On 2026-09-27, the current REM chibicc frontend/backend was used with the
native REM assembler and linker, musl CRT/libc, and libgcc to compile and
statically link three small hosted applications: `linegrep` (one translation
unit), `memo` (multiple translation units plus an archived object), and
`hexscope` (multiple translation units). They were run from disposable ext4
guest images under QEMU; these probes did not alter the flight image. A
32-bit byte-shift probe ran successfully.

The runtime tuple was QEMU
`.rem-phone-deploy-20260925/REM-FLIGHT/qemu/qemu-system-myemulator32`
(SHA-256 `0154db549ad01d0bcdfd79c0dde9544266fc14c75fe7825d14942ff47a75641a`),
kernel `.rem-phone-deploy-20260925/REM-FLIGHT/boot/vmlinux` (SHA-256
`c77547060463da37cf9730e41e799c94998ad9f6e24e2c1a3282ee3299215a63`), and
seed rootfs `/tmp/rem-samu-current.ext4` (SHA-256
`a15d0c90d596e5e3afacf2472ad74ac31d43319f6eb3d13e128eb38c26eba5f1`).
Application runs used fresh disposable ext4 copies. The compiler was
`toolchain/chibicc-rem/chibicc`, with staged REM `as`/`ld` and CRT/libc/libgcc
extracted from the seed root.

The hosted application runs did **not** pass as a set. `linegrep` printed a
path in place of the expected matching line and exited 129 (`Hangup`). `memo`
stalled during its first `add` command; the test did not capture the blocking
operation. `hexscope` printed a text dump, then faulted in libc `ferror` at
PC `0x0200268c` on an unaligned address `0x02000562` and exited 135. Its
`--elf32` mode returned success while printing byte-swapped ELF offsets. A
minimal four-argument control/six-argument C-call fixture also failed in the
guest (shell status 130; expected 0). The fixture is
[`toolchain/chibicc-rem/test/rem-six-argument-call.c`](../toolchain/chibicc-rem/test/rem-six-argument-call.c).

**Classification:** the six-argument result is a compiler/backend and C ABI
compatibility failure; generated caller/callee stack argument placement is
the leading suspect, but the cause has not yet been corrected or fully
isolated. The `hexscope` fault is a user-space memory fault in libc reached
after application output; its origin is unresolved. The malformed ELF output
is application behavior and has not been tied to compiler code generation.
The `memo` stall has no captured syscall or fault evidence and remains
unclassified. None of these results demonstrates an ext4 or kernel defect.

**REM64 implication:** the compiler/ABI acceptance suite must execute C calls
with more arguments than fit in argument registers, not merely assemble or
link them. Hosted acceptance should include multi-translation-unit programs,
stdio file I/O and close/error paths, and an application that reads structured
binary data, with output and exit status checked against expected results.
Require diagnostics or a failed conformance test for unsupported calling
conventions; successful linking alone is insufficient. Whether the observed
libc fault and the application-specific failures share the argument-placement
issue remains an **open question**. See the fixture above and
[`REM_CHIBICC_BUGLOG.md`](REM_CHIBICC_BUGLOG.md); application probe details are
in [`REM_APPLICATION_COMPATIBILITY_20260927.md`](REM_APPLICATION_COMPATIBILITY_20260927.md).

**Status:** compile/link coverage completed for these probes; runtime
compatibility is incomplete. No backend, libc, kernel, filesystem, or
application fix was made as part of this evidence collection. Further
isolation and a passing hosted-application regression run remain future work.

## A cross C++ toolchain is not yet a self-hosting GCC

The disposable GCC 15.2.0 bootstrap reached a complete target C++ frontend:
`myemulator2-linux-musl-g++` compiled and statically linked a REM C++ program,
and target `libgcc`, `libsupc++`, and a freestanding `libstdc++.a` were built.
The build used a sysroot with the conventional `usr/include` and `usr/lib`
layout and aliases for the target binutils names. These results classify the
REM backend, CRT, assembler, linker, and basic C++ runtime path as working for
cross compilation.

The attempted Canadian-cross native GCC build then stopped in GCC's host-side
`libcody`: the target libstdc++ had been configured with
`--disable-hosted-libstdcxx`, so its installed headers deliberately omitted
hosted headers such as `<string>` and defined `_GLIBCXX_HOSTED` as zero. Adding
header paths exposed the next missing hosted header (`bits/requires_hosted.h`),
but did not produce a native GCC executable. A disposable target-library link
also showed that static C++ linking depends on correct ordering of libstdc++,
libgcc, and musl; an intermediate test that added `strlen.o` to `libgcc.a`
was reverted and is not a valid platform fix.

**Root-cause classification:** toolchain/integration and hosted C++ runtime
packaging, not a demonstrated CPU or kernel defect. No permanent source or
flight artifact was changed, and no REM-native `gcc` acceptance test passed.

**REM64 implication:** define the hosted compiler-runtime contract before
claiming self-hosting. It must cover target C and C++ frontends, a hosted
libstdc++ (complete standard headers and ABI libraries), libgcc or its
replacement, CRT objects, libc, binutils naming, sysroot layout, and static
link ordering. A cross compiler that emits target code is an earlier milestone;
it is not evidence that the compiler itself can run on the target.

**Status:** cross C++ toolchain demonstrated; REM-native GCC remains
in-progress. The next milestone is a hosted target libstdc++ and a reproducible
native-GCC build, followed by running `gcc --version` and compiling both C and
C++ programs inside REM.

The Canadian-cross retry also demonstrated that GCC's build/host/target
variables must remain distinct: native x86 build helpers use `gcc`/`g++`, while
REM-hosted GCC components use the REM cross compiler and target libraries.
After stale build caches were removed, build-side `libcpp` compiled; the next
failure was in REM-hosted `libcody`, first because `-lstdc++` selected an
incompatible shared artifact and then because the freestanding C++ headers did
not provide the hosted C/POSIX declarations required by `libcody`.

**REM64 implication:** treat build/host/target compiler variables, sysroot
search paths, static-vs-shared runtime selection, and hosted-header coverage
as independently testable bootstrap contracts. **Status:** demonstrated
toolchain integration requirement; native GCC acceptance remains incomplete.

## Bash failures need per-binary provenance and fault evidence

A historical disposable userspace run started `/usr/bin/bash -c 'x=6; [
$((x*7)) -eq 42 ] && echo BASH_OK'` under BusyBox `ash`; Bash returned 139
before the test marker could report success. The tested Bash 5.2.37 binary's
SHA-256 was
`393c6188fea2776d0b91579c879c0bc72dacf53e34593295dab9ffc299731a43`.
The tuple used QEMU SHA-256
`0154db549ad01d0bcdfd79c0dde9544266fc14c75fe7825d14942ff47a75641a`, kernel
SHA-256
`c77547060463da37cf9730e41e799c94998ad9f6e24e2c1a3282ee3299215a63`, and
rootfs SHA-256
`46604d9530953065df0dc74fea7eacf5ac21dfbebeb7194ba355f178cb91f4cd` at test
time. The current file at that rootfs path no longer matches the recorded
hash. The serial log contains the test begin/end markers and status 139, but
no fault PC. The Bash ELF `.comment` identifies GCC 15.2.0, although its exact
build command and matching runtime provenance are unavailable. This
establishes an application failure, not its cause. CPU, compiler/backend,
ABI, libc/runtime, syscall/kernel, and filesystem attribution remain open.

The expanded guest run verified `bash --version` (5.2.37, exit 0), but simple
`bash -c 'echo hello'`, variable, function, command-substitution, redirection,
child-script, and environment cases each terminated with status 139. In a
repeat run the pipeline also returned 139; its first run returned 13 after a
`Segmentation fault` message, so that case is not fully repeatable. The
interactive probe reported `setpgid: Function not implemented`, then Bash faulted at
`PC 0x0207f214`, address `0x000040e8`, inside `check_mail+0x1b4`. The fault's
cause remains unresolved. `setpgid` returning `ENOSYS` is a syscall/kernel or
userspace syscall-ABI mismatch in the tested tuple; the current source has a
dispatcher case, so source-versus-binary skew remains possible. The ENOSYS
message has not been shown to cause the separate Bash memory fault. BusyBox
`dmesg` also returned `klogctl: Function not implemented`, limiting retrieval
of fault records for noninteractive cases. Exact results and reproductions are
in [the Bash compatibility report](REM_BASH_COMPATIBILITY.md).

**REM64 implication:** a Linux-compatible interactive job-control shell needs
a consistent libc/kernel `setpgid` ABI; exact syscall availability must be
verified on the tested binaries, not inferred from current source. The Bash
memory fault does not yet establish a CPU, compiler, ABI, libc, or filesystem
requirement. As a recommended diagnostic practice, retain artifact hashes,
compiler provenance, fault PC, and minimal reproducer; a nonzero exit status
alone cannot identify an architectural defect.

**Status at the time of that exploratory binary run:** version output worked;
command and interactive workloads failed. A later controlled-source rebuild
and rerun is recorded below and in
[REM_BASH_COMPATIBILITY.md](REM_BASH_COMPATIBILITY.md). The initial
failure-attribution question remains open for the old binary.

## Hosted target sysroot requirements for native GCC

The disposable GCC bootstrap audit found that the REM musl sysroot already
contains the basic hosted C/POSIX headers (`stdio.h`, `string.h`, `errno.h`,
`wchar.h`, pthread and memory headers), CRT objects, and `libc.a`. The immediate
C++ failure came from the freestanding libstdc++ installation and its header
search layout: compatibility wrappers were selected as system headers, so
hosted declarations such as `mbstate_t`, `strlen`, and `errno` were unavailable.

A disposable header overlay made `<string>` compile and allowed GCC's
`libcody` component to build. The next GCC frontend failure was then in
generated host configuration (`SIZE_MAX` preprocessing and missing
`GATHER_STATISTICS`), so no REM-native GCC executable was produced.

**Root-cause classification:** hosted C++ runtime and GCC bootstrap
integration; no missing basic musl header or REM kernel defect was established.

**REM64 implication:** keep target C headers, generated libstdc++ headers, and
GCC build-time headers in separate, correctly ordered sysroot namespaces. Test
representative hosted headers and C++ runtime linking before starting the
compiler bootstrap.

**Status:** cross C++ compilation works; hosted target libstdc++ and native GCC
remain incomplete.

The subsequent generated-configuration audit found that `gcc/auto-host.h` was
generated with fallback macros such as `pid_t` and `caddr_t`, while the REM
musl headers already define those types. The frontend command then exposed a
second issue: copying target C headers into the C++ standard-header directory
made GCC select them before its generated host configuration, producing
`SIZE_MAX`, `uintptr_t`, and internal GCC type failures. This is configuration
and include-order contamination, not evidence that those types are absent from
musl.

**REM64 implication:** bootstrap configuration must generate host feature
headers only after the final target sysroot/search order is installed. Keep
target libc headers out of the libstdc++ standard-header directory and test
`auto-host.h` against the same include paths used by frontend compilation.

The next disposable bootstrap used GCC's normal `libstdc++-v3` install rules
with a clean prefix (rather than copying headers into the C++ include tree).
It installed hosted standard headers, generated `bits/c++config.h`,
`libstdc++.a`, `libsupc++.a`, and `libgcc.a`. A fresh Canadian-cross build then
completed `all-gcc` and produced ELF32 MyEmulator2 `gcc`, `cc1`, and `cc1plus`
executables. The compiler binaries have not yet been booted inside REM, so
native GCC runtime acceptance remains open.

**REM64 implication:** a self-hosting compiler requires reproducible staged
installation rules for target headers and runtime archives; a successful
cross compiler alone does not prove that the installed native driver can find
its frontend, C++ headers, and compiler runtime.

Runtime validation of that disposable GCC reached the driver successfully:
`gcc --version` ran inside REM. Compilation then failed before frontend
execution because musl's `posix_spawn` path returned `ENOSYS` while trying to
launch `cc1`/`cc1plus` (`posix_spawn: Function not implemented`). The current
REM syscall generation also reports syscall 220 (`clone`) as unimplemented.
This is a syscall/kernel compatibility blocker, not evidence of a GCC code
generation fault. A hosted compiler must be able to spawn its frontend and
assembler subprocesses.

**REM64 implication (REQUIRED):** provide and test the process-creation
syscalls used by libc `posix_spawn`, in addition to `fork`/`exec`, before
claiming a self-hosting compiler runtime.

## Build tools require a defined working-directory and diagnostic errno contract

REM32 Samurai testing showed that a relative manifest (`build.ninja`) is
resolved against the process current directory, while the flight image stores
project files under `/root`; invoking the tool from `/` therefore fails before
metadata checks. The same path also exposed that formatting a fatal diagnostic
can overwrite `errno` before `perror()`, producing a misleading `Hangup` instead
of `ENOENT`.

**REM64 implication:** document the native build tool working-directory
contract and require diagnostics to preserve the syscall error before calling
formatting or other library code. This is a required software/platform
integration lesson; the remaining Samurai argument-state fault is still open.

## Variadic register-save area is a callee ABI contract

REM32 GCC output shows that callers do **not** reserve a home area for
register-only variadic calls. The variadic callee saves incoming `r1-r4` at
`old_sp+16..28`; an experimental caller reservation shifted that address and
made musl read the fixed `stderr`/format arguments as varargs. The chibicc
backend follows the GCC rule; stack-passed calls retain their normal outgoing
area.

**REM64 implication:** specify the variadic register-save area, its relation
to the incoming stack pointer, and whether callers reserve anything. This is
**REQUIRED** for hosted C compatibility and must be tested against libc,
compiler-generated variadic callees, and multi-argument formatting calls.

## Bash build inputs and job-control requirements

A controlled Bash 5.2.37 build was produced with REM GCC 15.2.0 and the REM
musl sysroot. The default compiler language mode rejected Bash's historical
empty-parameter declaration at `list_reverse()` call sites; explicit
`-std=gnu17` compiled it without source edits. Linking also required removing
the unsupported `-rdynamic` option for a static executable. These are
toolchain/build-input constraints, not evidence of a CPU/backend defect.
Build provenance and the complete test results are in
[REM_BASH_COMPATIBILITY.md](REM_BASH_COMPATIBILITY.md).

The selected musl `setjmp.h` declares `sigsetjmp`, but its `libc.a` contains no
definition. Bash's cross configure therefore had to be told that
`sigsetjmp` was unavailable. **RECOMMENDED:** validate configured library
features by linking target probes against the exact archive; header presence
alone is insufficient. Whether the REM musl build should provide that symbol
is an **OPEN QUESTION** for its runtime owner.

On the tested Linux tuple, Bash version, command execution, variables,
functions, command substitution, a pipeline, redirection, scripts, and
environment inheritance passed twice. Interactive input/output and
`fork`/`exec`/`waitpid` also passed. On kernel `c77547060463da37cf9730e41e799c94998ad9f6e24e2c1a3282ee3299215a63`, the separate
process probe observed `setpgid(0,0) = -1/ENOSYS` and
`tcsetpgrp(0,getpgrp()) = -1/EINVAL`; Bash therefore runs without POSIX job
control on that tuple. A later terminal/session probe on a different kernel
found `setpgid` succeeds and `tcsetpgrp` succeeds after acquiring a controlling
terminal; see [REM_VIM_COMPATIBILITY.md](REM_VIM_COMPATIBILITY.md). These
results show a tuple-specific process-group/terminal setup gap, not a general
claim that the syscall is absent. **REQUIRED for Linux-compatible interactive
job control:** provide a coherent `setpgid` and `tcsetpgrp` contract across the
syscall ABI, process groups, sessions, and controlling terminal. Ownership of
the old tuple's `ENOSYS` remains unisolated; this is not a CPU-specific
requirement. See [REM_BASH_COMPATIBILITY.md](REM_BASH_COMPATIBILITY.md) and
[REM_RUNTIME_ISSUES.md](REM_RUNTIME_ISSUES.md).

## Wait status must use the Linux process ABI encoding

A GCC-built REM probe forked a child that called `_exit(2)` and collected it
with `waitpid`. The returned status was `0x0002`, so the standard
`WIFEXITED`/`WEXITSTATUS` macros reported “signaled by SIGINT” instead of a
normal exit with code 2. BusyBox ash consequently reported an external
`exit(2)` as status 130 (and `exit(1)` as 129). This is a **syscall/kernel
wait-status ABI** failure, not a compiler or Samurai fault. The exit syscall
itself was observed with argument 2; the mismatch is in the wait result
encoding exposed to user space.

**REM64 implication (REQUIRED):** implement Linux wait status encoding so
normal exit codes and terminating signals are distinguishable by
`waitpid`/`wait4` consumers. Include fork/exec tests for zero and nonzero
normal exits as well as signal termination, and verify both raw wait status
and shell `$?`. The same Samurai comparison showed GCC-built `samu -n`
successfully parsed the test graph while the same-source chibicc-built binary
faulted at `strcmp+0x38` on a null pointer; this is evidence to keep compiler
conformance and process/wait ABI tests separate. See
[REM_SAMURAI_STATUS.md](REM_SAMURAI_STATUS.md#gcc-cross-compiler-comparison-2026-09-27).

## OpenSSH server account and privilege-separation filesystem contract

The candidate REM rootfs used for the OpenSSH 10.5p1 guest harness has no
`/etc/passwd` or `/etc/group`. The disposable test setup stages minimal `root`
and `sshd` account records and a privilege-separation directory. The guest
booted, but `ssh -V`, `ssh-keygen`, and `sshd -t` each faulted in user mode
before account lookup or daemon setup could be validated. This is a packaging
observation, not a demonstrated kernel defect or a successful OpenSSH test;
fault attribution remains unresolved among compiler-generated code, ABI, and
libc/runtime. See the exact tuple, PCs, and repro commands in
[REM_OPENSSH_COMPATIBILITY.md](REM_OPENSSH_COMPATIBILITY.md).

A separate read-only audit of the REM development rootfs
(`a8443f8b3cc8ead472b0be67cd01bb5f9c32f42116834a3ce308f1b90d8c24bd`)
found only `profile`, `ssh`, and `ssl` under `/etc`, an empty `/home`, and an
init script that launches an interactive root shell. Its BusyBox config has
`adduser`, `addgroup`, `login`, `getty`, `passwd`, `su`, and shadow support
disabled. This is a demonstrated distribution packaging/session-lifecycle
gap, not a CPU-architecture defect. A usable REM Linux release needs tested
account databases, account creation/password policy, a login/session owner,
and a normal-user shell path; details are tracked in
[`REM_USER_MANAGEMENT.md`](REM_USER_MANAGEMENT.md).

**REQUIRED by OpenSSH deployment configuration:** provide working user/group
lookup records for the login account and configured privilege-separation
account, plus the configured empty chroot directory with suitable ownership
and permissions. Runtime behavior on REM is **OPEN / unvalidated** because
the tested binaries fault before these paths. Whether this belongs in the
base image or an SSH package setup step is an **OPEN QUESTION**. See
[REM_OPENSSH_COMPATIBILITY.md](REM_OPENSSH_COMPATIBILITY.md).

## Crypto RNG readiness is a separate runtime acceptance boundary

REM32 OpenSSH 10.5p1 was built as static ELF with staged OpenSSL 3.4.1. An
initial unverified probe faulted in `provider_conf_init` at PC `02068320`,
address `0x23c`, after `OPENSSL_init_crypto()`. A later run staged both GCC and
chibicc progression ELFs into a disposable rootfs, dumped them back out, and
checked exact hashes before execution. Both callers passed crypto init, SSL
init, explicit default-provider loading, and `RAND_poll()` (cases 1–3 and 5–6).
`RAND_status()` remained false in both case 4 and after `RAND_poll()` in case 6;
adding fixed test-only data with `RAND_add()` also did not make it ready
(case 8). The earlier provider and RAND_poll faults were not reproduced by
this verified pair; the old tuple record mismatched the actual QEMU hash, and
its runner did not reject `debugfs`'s silent missing-file result. That earlier
fault PC/disassembly is therefore not assigned to the verified code.

A native host build of the same OpenSSL 3.4.1 source passed all eight
progression cases. The guest entropy probe returned `EAGAIN` from nonblocking
`getrandom()` while the kernel reported its urandom source uninitialized.
**Classification:** confirmed guest entropy-readiness / kernel-device
configuration blocker; initial provider crash is an unvalidated integration
observation. OpenSSH seeds before handling even `ssh -V`, so successful init
does not establish an SSH-capable runtime. See the exact hashes, transcripts,
and disposable-tuple caveat in [REM_OPENSSH_COMPATIBILITY.md](REM_OPENSSH_COMPATIBILITY.md).

The matching kernel config enables `CONFIG_VIRTIO_MMIO` but has
`CONFIG_HW_RANDOM` unset. The current machine source creates virtio-net only;
`linux/arch/myemulator2/include/asm/io.h` maps UART, block, and virtio-net MMIO,
with no RNG mapping. **REQUIRED for this deployment path:** the boot contract
must supply a kernel-recognized entropy source or seed before applications
requiring cryptographic randomness start. **OPEN:** determine whether REM64
should use a virtio hwrng device, a trusted boot seed, or another validated
source. A disposable virtio-RNG experiment stalled before reaching userspace,
so it is not evidence that this mechanism already works.

**REQUIRED for secure SSH userspace:** OS entropy must become ready and
provider-backed cryptographic operations must complete before accepting
SSH/SCP. **RECOMMENDED:** provide a validated entropy device or boot-seeded
pool, and require staged guest-file hash checks plus exact tuple hashes in
runtime tests. **OPEN:** choose and validate the platform entropy source and
boot-seed contract. The synthetic test seed is not secure and is not a
production workaround.

## Linux time64 sleep syscall must be mapped consistently

In the 2026-09-27 disposable OpenSSH network-profile run, the REM32 kernel
repeatedly logged `MYEMU_NET unknown nr=407` after init and returned
`ENOSYS`. Linux's generic UAPI identifies syscall 407 as
`clock_nanosleep_time64`; the exact network-kernel dispatcher had no case for
it. This was observed independently of OpenSSH's user faults, whose cause
remains unresolved. See the pinned tuple, transcript, and probe results in
[REM_OPENSSH_COMPATIBILITY.md](REM_OPENSSH_COMPATIBILITY.md).

**REQUIRED Linux behavior:** support the ABI's clock-nanosleep operation,
including relative/absolute deadlines and interruption behavior, when that
operation is exposed to Linux userspace. **RECOMMENDED:** generate or verify
kernel and libc syscall-number/time-structure definitions from one ABI
contract. **OPEN:** identify which guest callers issued syscall 407 in this
run; REM64's syscall number and native time structure should follow its own
ABI rather than copying REM32's time64 slot blindly.

## Process creation requires an architecture-correct libc clone wrapper

REM32 evidence (2026-09-27): Linux syscall 220 (`clone`) and the generic
kernel `sys_clone`/`copy_thread` path were present. Native GCC initially failed
with `posix_spawn: Function not implemented` because the REM musl
`src/thread/clone.c` implementation was an `-ENOSYS` stub. A disposable musl
port supplied an assembly `__clone` wrapper; validation then exposed the
actual MyEmulator2 C ABI (arguments in `r1`--`r4`, additional arguments on the
stack), 16-byte child-stack alignment, and preservation of the compiler's
callee-saved frame register `r15` as requirements. A wrapper that used the
Linux syscall register convention as its C-call convention corrupted the
caller; preserving only `r5`--`r12` was also insufficient.

**REM64 implication (REQUIRED):** define and test the C ABI and Linux syscall
ABI independently. The libc process-creation wrapper must translate argument
registers, stack arguments, child stack alignment, return paths, and all
callee-saved registers. Include clone/fork/vfork/exec and `posix_spawn` probes
before claiming a hosted compiler runtime is usable. The kernel clone path is
not a substitute for an architecture-correct libc wrapper.

The corrected disposable wrapper allowed direct custom-stack clone/exec probes
to pass. A full GCC compile still reaches a separate kernel/MMU/console fault
in the tested historical kernel after launching `cc1`; that remaining failure
is **OPEN** and must not be conflated with the proven musl `clone` ENOSYS/ABI
problem.

## MMIO accesses must use the mapped kernel address

The first post-`clone` GCC compile on REM faulted in
`myemulator2_console_read_user` at physical address `0xf0000001`. The UART
driver already mapped the device through the architecture's `ioremap` alias
(`MYEMU2_UART_VIRT`, `0x7fc00000`), but the early console status macro still
used the physical address directly. Under the MMU that address was not a
valid kernel virtual mapping. Changing the status read to
`MYEMU2_UART_VIRT + 1` removed that specific fault; the disposable kernel
booted to `REM#` and executed `/bin/true` afterward.

**REM64 implication (REQUIRED):** every kernel MMIO access, including early
console paths, must use a defined mapped virtual address (or an explicitly
defined identity mapping), and the rule must be shared by early printk,
drivers, and exception paths. Add a boot-time MMIO access test before hosted
toolchain workloads. The broader GCC compile remains **OPEN** pending a
fresh runtime artifact, but this fault was an architecture/MMU integration
error rather than a GCC code-generation defect.

## Pinned image inputs must fail closed on drift

During native binutils integration, the flight-image builder checked the
recorded QEMU, kernel, and rootfs hashes before assembling a candidate. The
original desktop seed path had changed from the tuple's recorded SHA-256
`46604d9530953065df0dc74fea7eacf5ac21dfbebeb7194ba355f178cb91f4cd` to
`04eaef9b630891818ff5dfeaa4b525465da345057f606158eacae01f0245d906`; the
builder stopped before construction. A second source copy hashed exactly to
the pinned seed and was used to build and boot the binutils candidate. The
native inspection tools then passed a focused small-executable workload in
that guest. Older overlay `objdump`, `nm`, and `strip` faulted on guest
executables (PCs and fault addresses are recorded in
`REM_NATIVE_BINUTILS_QUALIFICATION.md`); build-tree replacements passed the
small-executable probes. Fault causes remain unassigned, and full
`objdump -d` performance on a larger executable is open.

**Classification:** tooling/integration, not a demonstrated CPU, ABI, or
kernel requirement. **REM64 recommendation:** retain per-artifact provenance
and exact image-tuple hashes, and reject assembly when any selected input
drifts. Developer utilities should also have target ELF identity and guest
qualification recorded before they are treated as part of a reproducible
workstation. See [REM_FLIGHT_READINESS.md](REM_FLIGHT_READINESS.md) and
[REM_NATIVE_BINUTILS_QUALIFICATION.md](REM_NATIVE_BINUTILS_QUALIFICATION.md).

## Same-tuple compiler comparisons must be reproducible

A GCC/chibicc Samurai comparison was initially invalidated by a reboot and stale binaries. Rebuilding both from the same unmodified source commit and running both in one newly-built ext4 guest showed GCC `samu -n` passing while fresh chibicc `samu -n` faulted in `strcmp`. A smaller static-buffer reducer faulted for both compilers in that guest, so the result is not yet sufficient to assign the Samurai fault to the compiler backend.

**REM64 implication (REQUIRED tooling discipline):** compiler/runtime comparisons must record source, compiler, executable, QEMU, kernel and rootfs hashes, and reject stale or truncated injected binaries. **OPEN:** the remaining fresh chibicc `strcmp` failure requires a chibicc-only minimal reproducer before an ABI or code-generation conclusion is justified.

### Matched runtime reducers did not reproduce the application faults

In a later disposable run on the same recorded QEMU and kernel hashes, matched
GCC and chibicc binaries for direct `strcmp`, struct/array access,
allocation/reallocation, and a Samurai-like keyword scan produced no captured
user-fault report. The scan's expected nonzero return was rendered by ash as
`Hangup`/129, consistent with the separately recorded wait-status ABI issue;
that shell status alone is not evidence of a crash. These probes therefore do
not reproduce or explain the Samurai, Vim, or OpenSSH pointer faults. The
Vim and OpenSSH reports retain distinct fault addresses and unresolved
provenance among generated code, ABI/runtime state, application state, and
memory mapping; no common defective layer is established.

Further artifact checking changed the OpenSSH interpretation. The OpenSSH
runtime harness records hashes for its newly built executables, but the
captured fault PCs decode differently in those build outputs than in the same
paths extracted from the pinned seed rootfs. Seed `ssh` hash `849526a4…` has
`lw r1,16(r15)` at `0x02000180`, matching the report's `fp=0x1060` and
`addr=0x1070`; reported build-output `ssh` hash `100e38d3…` has a non-memory
instruction there. Seed `sshd` similarly has `lw r2,-12(r15)` at
`0x0203665c`, matching `fp=0x580` and `addr=0x574`, while the reported build
output has a call there. The fault logs therefore do not establish failures
in the newly built OpenSSH executables; the guest appears to have run seed
binaries or the report mixed artifacts. The exact staging failure is not
isolated. This is a tooling/integration failure in the test evidence, not an
OpenSSH runtime root-cause finding.

**Classification:** compiler/runtime comparison and application compatibility;
no CPU, ABI, libc, or MMU root cause was demonstrated by these reducers.
**REM64 implication (RECOMMENDED):** runtime fault suites should pair compiler
builds from identical sources, confirm installed guest-file hashes after
staging, and explicitly validate program output/state, not infer success from
shell exit status alone. **OPEN:** matched GCC/chibicc fork/exec and
file-backed-mapping probes, and instruction-level comparison of the faulting
OpenSSH build outputs against their in-guest executables, remain outstanding.
Separately, the matched OpenSSL `RAND_status` probe fails after successful
initialization under both caller compilers. The shared GCC-built libcrypto
provider path is localized, while the cause within generated code, ABI, or
runtime remains open (see the crypto RNG note above).
See
[`REM_SAMURAI_STATUS.md`](REM_SAMURAI_STATUS.md),
[`REM_VIM_COMPATIBILITY.md`](REM_VIM_COMPATIBILITY.md), and
[`REM_OPENSSH_COMPATIBILITY.md`](REM_OPENSSH_COMPATIBILITY.md).

## A hosted compiler needs a complete, separated target runtime

REM32 evidence (2026-09-27): the target C++ frontend, libgcc, libsupc++,
libstdc++, and target GMP/MPFR/MPC/ISL could be built with the REM backend.
The subsequent REM-hosted GCC configure passed its C++14 checks, but the
frontend build failed while generating host configuration macros (`SIZE_MAX`,
`SIZEOF_LONG`, `GATHER_STATISTICS`, and related GCC internal types). Earlier
experiments also showed that copying target libc headers into C++ header
directories contaminates GCC's include/configuration boundary.

**Classification:** compiler/toolchain integration; no CPU defect was
demonstrated. **REM64 implication (REQUIRED):** a self-hosting compiler
bootstrap must provide a complete target math/runtime stack and keep build,
host, and target headers, libraries, and feature tests strictly separate.
Bootstrap qualification must test the compiler executable inside the guest,
not only cross-produced `cc1` or target libraries. The hosted-GCC failure is
**OPEN** pending a clean host-configuration fix.

The next disposable configuration pass showed that a Canadian-cross build
cannot run REM binaries while probing host sizes. Supplying measured cache
values for the REM host (`sizeof(long)=4`, `sizeof(void *)=4`,
`sizeof(long long)=8`) and the existing `stdint.h` feature result generated
the expected `SIZEOF_LONG` and `GATHER_STATISTICS` definitions. This is a
configuration requirement, not a source-level compatibility define; the full
frontend build remains **OPEN**.

## libc string-call frame integrity is not covered by allocator/mapping passes

REM32 Vim isolation (2026-09-27) tested a small static C probe linked to the
flight-kit musl runtime, built with GCC and chibicc on the same disposable
flight tuple. Large `malloc`/`free`, `realloc`, file `read`/`close`, and
file-backed plus anonymous `mmap` access passed. `strcpy`, `strcat`,
`strcmp`, and `strlen` also passed. Both compiler builds then faulted in the
same linked musl `strstr` restore sequence: frame pointer `0x480`, load
address `0x46c`; GCC O0 and O2 showed the same failure. This is an
application/runtime reproduction, not a Vim-only failure. The immediate
ownership among libc implementation, compiler/ABI interaction, and runtime
state remains **OPEN**; it does not establish a kernel VM defect. Separate
ext4 warnings were observed on a clean pre-boot clone, with no demonstrated
causal relationship. `/proc/self/maps` could not be inspected because the
test rootfs had no mount utility and procfs was unmounted.

The focused disassembly also exposed a contract discrepancy: the public REM32
ABI table labels `r15` caller-saved, while the tested GCC backend reserves it
as the hard frame pointer and emits function prologues/epilogues that preserve
the incoming frame base. The failing `strstr` restore record has `r15/fp=0x480`.
This mismatch is concrete ABI/toolchain evidence, but does not by itself prove
that it caused the bad restore. **REM64 REQUIRED:** publish one register-role
and preservation contract and validate that the compiler backend, libc, and
assembly wrappers all implement it. The recorded ABI table is in
[`MYEMULATOR_2_ABI.md`](MYEMULATOR_2_ABI.md).

A tracked rerun on the disposable selected image extracted each tested guest
ELF after shutdown and disassembled the recorded PCs with the MyEmulator2
objdump. GCC O0 faulted at `0x02005e20` and chibicc at `0x02008348`; each is
the `lw r9,0(r12)` at `strstr+0x6c`, with `r12=0x46c` and saved `r15/fp=0x480`.
The code window and function prologue match across these builds. The kernel
reported a user fault (cause 7); neither this result nor the concurrent ext4
warnings identifies which layer produced the invalid frame state. The VM was
terminated and host process inspection found no remaining
`qemu-system-myemulator32` process. This confirms the repeatable fault site,
not its root cause.

**Classification:** libc/runtime and ABI interaction under investigation;
the fault is reported by the kernel, but that alone does not assign kernel
ownership. **REM64 REQUIRED:** implement and validate the platform C ABI's
callee-saved-register and stack-frame rules; successful allocation, I/O, or
mapping tests do not establish correct function-call state preservation.
**RECOMMENDED:** qualify each libc's string routines with matched tests from
multiple compiler backends. **OPEN:** isolate why the shared `strstr`
implementation restores from the invalid frame before drawing a CPU, backend,
libc, or syscall conclusion. Probe details are in
[`REM_VIM_COMPATIBILITY.md`](REM_VIM_COMPATIBILITY.md) and the focused source
[`rem-vim-runtime-stress-probe.c`](../toolchain/tests/rem-vim-runtime-stress-probe.c).

## User-fault diagnostics need the complete saved exception frame

During REM32 runtime fault investigation, the existing user-fault record
omitted `r0` and did not include instruction bytes at the saved PC. This
limited comparison of compiler/runtime failures because the log could not
show all call state and the faulting instruction together. The page-fault
reporter now emits `r0`–`r15`, PC/LR/SP, fault address and cause, saved trap
state, process name, and a best-effort 16-byte code window with a validity
mask. The handler object compiled in a private kernel build, but the deliberate
invalid-load boot check has not yet completed; runtime behaviour remains
**UNVALIDATED**.

**Classification:** tooling/diagnostics. **RECOMMENDED:** future REM exception
reports should preserve and expose the complete architectural user register
frame, fault metadata, process identity, and safely sampled nearby instructions.
Unreadable code bytes need an explicit validity indication so diagnostics do
not cause a nested fault or misrepresent missing bytes as zeroes.

## Hosted compiler frontends expose process and image-size requirements

The disposable Canadian-cross GCC build produced valid REM `gcc`/`cc1` frontends,
but the target linker emitted zeroed ELF headers when linking the unstripped,
debug-heavy `cc1` image (over 300 MiB). Stripping target object debug sections
reduced `cc1` to about 47 MiB and `cc1plus` to about 61 MiB, after which both
were valid ELF32 MyEmulator2 executables. This is a toolchain/integration
boundary, not an application workaround. **REM64 RECOMMENDED:** qualify linker
large-image limits and provide a documented debug/production-size path before
bootstrapping a hosted compiler.

The resulting native driver ran `gcc --version` inside REM, but compiling a
source file failed because musl's `posix_spawn` requires the Linux `clone`
process-creation ABI and the tested kernel returned `ENOSYS`. **REM64 REQUIRED:**
the process-creation syscall ABI used by libc (`clone`/`clone3`, `fork`, `exec`)
must be available before a hosted compiler can launch `cc1`; a driver starting
successfully is not sufficient. Supporting evidence is in the disposable logs
under `/tmp/rem-native-gcc-final`.

## Native build executors require process creation

REM32 qualification of unmodified Samurai provides a direct toolchain test:
the current chibicc backend, native assembler and linker produce a valid
Samurai executable, and `samu -n` completes on the pinned guest tuple. A real
build fails in both GCC-built and chibicc-built Samurai at
`posix_spawn("/bin/sh", ...)` with `ENOSYS`. The graph engine and compiler are
therefore usable for dry runs, but a native build executor cannot run commands
until the libc/kernel process-creation path is present.

**Classification:** kernel/syscall and libc/runtime contract. **REM64
REQUIRED:** provide and test the process-creation ABI used by hosted tools
(`clone` or `clone3`, child setup, `exec`, and wait-status propagation) before
claiming a self-hosted build environment. This is a demonstrated requirement,
not a compiler workaround. See `docs/REM_SAMURAI_STATUS.md`.

## Indirect-call lowering must preserve argument registers

REM32 chibicc qualification exposed a compiler/backend defect in Samurai's
callback path: evaluating an indirect function target after loading register
arguments reused `r1`, so the callback received the target address as its first
argument. Saving the target before argument evaluation fixed the issue and
allowed `samu -n` to pass; `test/rem-indirect-call.c` covers the minimal case.

**Classification:** ABI/compiler. **REM64 REQUIRED:** define and test an
indirect-call lowering that preserves all argument registers and maintains
call-boundary alignment. A direct-call-only ABI test is insufficient.

## ISA operand constraints need executable validation

The same qualification exposed an invalid chibicc emission for bitwise NOT:
`not r1,r0,r1` violates the current decoder's requirement that the second
source be the zero register; `not r1,r1,r0` is valid. The backend and regression
were corrected.

**Classification:** ISA/toolchain contract. **REM64 RECOMMENDED:** keep operand
constraints explicit in the assembler/encoding specification and include
compiler-generated instruction forms in decoder regression tests.
### Supported fallback when `posix_spawn` is absent

The same REM32 Samurai qualification passed real, incremental, and clean builds
when compiled with its upstream `NO_POSIX_SPAWN` option: the `fork`/`execvp`
path works even though the default `posix_spawn` path returns `ENOSYS`.
**Classification:** libc/kernel interface. **REM64 REQUIRED:** either provide
the POSIX spawn process-creation contract or document a tested fork/exec
fallback for native build tools; dry-run graph support alone is insufficient.

## Hosted C sources use ignorable declaration attributes

The canonical Kilo source exposed a chibicc parser gap: a parameter annotated
with GNU `__attribute__((unused))` failed before code generation. Consuming the
balanced attribute list fixed the minimal reproducer and allowed Kilo to
compile and link without changing its source.

**Classification:** compiler/frontend compatibility. **REM64 RECOMMENDED:** a
hosted C toolchain should accept common non-semantic declaration attributes
(`unused`, format, visibility, and similar annotations), either preserving
their specified semantics or explicitly ignoring those that do not affect
target code generation. This is a source-compatibility requirement exposed by
real Unix applications.

## Terminal geometry is required by native editors

Kilo compiled and linked with chibicc, but its first guest startup observed
`ioctl(TIOCGWINSZ)` returning success with zero rows and columns. Kilo then
used its cursor-position query fallback and waited for a terminal response.
**Classification:** terminal/device contract. **REM64 REQUIRED:** a usable
TTY must return meaningful window geometry or provide a documented, reliable
cursor-position response path; a successful ioctl with zero dimensions is not
an adequate editor environment.

## Self-hosting qualification needs phase observability and valid images

The REM chibicc bootstrap was instrumented at tokenization, preprocessing,
parsing, code generation, and output boundaries. Host compilation completed
`codegen.c`; native guest attempts require a valid ext4 image before their
phase results are meaningful. **Classification:** toolchain/deployment.
**REM64 RECOMMENDED:** ship phase diagnostics for bootstrap tools and verify
filesystem magic, size, and hashes before attributing a self-hosting failure to
the CPU, compiler, or runtime.
## Self-hosting compilers must avoid unprovided host builtins

The REM-native chibicc bootstrap reached its own `codegen.c` and exposed a
host-toolchain assumption: `__builtin_ctz` was used for an internal alignment
calculation, but the REM compiler did not implement that builtin. Replacing it
with ordinary C allowed host-generated REM assembly to assemble; the full
native translation-unit run remains open because the guest bootstrap did not
finish the large file within the observed timeout.

**Classification:** compiler/toolchain. **REM64 RECOMMENDED:** the native
bootstrap compiler should either implement the compiler builtins used by its
own source or keep its source within a documented portable subset. Self-hosting
qualification must compile every compiler translation unit inside the guest,
not only validate a cross-built executable.

## posix_spawn requires a complete vfork/exec contract

A minimal REM probe showed raw `clone(220)` succeeds, including
`CLONE_VM|CLONE_VFORK|SIGCHLD`, and direct `execve` works. musl's
`posix_spawn("/bin/true")` nevertheless returned `ENOSYS` after the child
reached the exec syscall. This isolates the remaining failure to the
shared-mm/vfork child-to-exec lifecycle (or its error-pipe/close-on-exec
completion), rather than syscall dispatch or GCC. **REM64 REQUIRED:** define
and test the complete `clone`/`vfork`/`execve` transition, including parent
blocking, address-space replacement, close-on-exec descriptors, and the
successful-exec result observed by `posix_spawn`.

## Debug information must match emitted code and advertised flags

Native debugger qualification established two separate toolchain facts:
chibicc currently accepts `-g` while ignoring it, producing no usable source
CU/line table; GCC emits DWARF, but the tested `-O0 -g` local-variable
locations disagree with the actual REM stores. `-gdwarf-2` and
`-fno-omit-frame-pointer` did not correct that mismatch. The debugger
therefore exposes verified arguments/globals and declines to invent stack
local values.

**Classification:** compiler/debug-info contract and debugger/tooling
integration. **REM64 REQUIRED:** compiler debug locations must describe the
actual ABI/code-generation frame layout, and a compiler accepting a debug-info
flag should emit the corresponding information or clearly reject/report the
unsupported option. Qualify location expressions against machine code and
debugger reads before claiming local-variable support. Evidence and supported
subset: [`REM_DEBUGGER.md`](REM_DEBUGGER.md).

## OpenSSL deployment exposed target-header and syscall output contracts

A controlled OpenSSL 3.4.1 build initially selected the rootfs `opensslconf.h`
ahead of OpenSSL's generated target header. The stale header disabled the
`/dev/urandom` fallback for this target, and `RAND_status()` returned false.
Putting the generated OpenSSL header first, then staging the exact rebuilt
archive, made the hash-verified `OPENSSL_init_crypto(); RAND_status()` test pass
on the seeded disposable tuple. **Classification:** build/tooling integration.
**REM64 RECOMMENDED:** configure headers, target feature macros, and libraries
must be versioned and staged as one target tuple; test preprocessing of
entropy-selection code in addition to successful linking.

In a separate libc probe, `clock_gettime(CLOCK_REALTIME, &ts)` and raw syscall
403 both returned success, updated `tv_sec`, and left a sentinel `tv_nsec=456`
unchanged. **Classification:** syscall/kernel or time-ABI implementation;
the exact source function is unavailable in this checkout. **REM64 REQUIRED:**
a successful Linux time syscall must write every field of the selected
timespec ABI. A separate OpenSSL EVP RSA-keygen probe also faulted in the
`clock_gettime` wrapper epilogue, but standalone clock calls did not fault, so
no causal link is established. OpenSSH `ssh-keygen` independently faulted in
OpenSSL `CRYPTO_THREAD_write_lock` while `ossl_method_lock_store` supplied
pointer value `0x2d534544`; the origin of the corrupted method-store field is
still open. See [`REM_OPENSSH_COMPATIBILITY.md`](REM_OPENSSH_COMPATIBILITY.md).

The corrected `scp` runtime probe reported `socketpair: Function not
implemented` (ENOSYS) before it could transfer a file. **Classification:**
syscall/kernel compatibility. **REM64 REQUIRED for this Linux application
path:** implement the Linux `socketpair` contract used for local process
communication, or provide an application path whose requirements are met by
supported primitives. This does not establish that the REM32 failure is in the
kernel rather than syscall dispatch/libc. Network transfer remains unproven.

Follow-up isolation showed that the earlier numeric `getaddrinfo` fault came
from the older rootfs `libc.a` in that executable tuple: the minimal probe
failed with archive hash `30cff68855d3793dedd63020928e65dfc084a5aa6667ef0114b25d6d92bd0924`
and passed at both `-O0` and `-O2` with newer archive hash
`312c6c393d9b879448edd28a822ec4fd56e1f40e364764c5da77a0df884ba21f`. The
precise old-libc defect is not isolated, and this does not validate all
resolver paths. **Classification:** libc/artifact integration. **REM64
RECOMMENDED:** pin libc and headers together in executable provenance and run
resolver smoke tests against the exact archive used for each application.
During the later `sshd -t` timeout, syscall 159 was logged; the target syscall
header maps it to `setgroups`, but no causal link between that call and the
timeout has been established. See
[`REM_OPENSSH_COMPATIBILITY.md`](REM_OPENSSH_COMPATIBILITY.md) for tuple and
timeout details.

A separate RC3 login-to-Yash trace established a concrete `setgroups` failure:
after a valid disposable test login, BusyBox `login` called syscall 159
(`setgroups(1, ...)`), received `ENOSYS`, printed `login: can't set groups:
Function not implemented`, and exited before attempting `execve` of the
account's `/usr/bin/yash` shell. The account used UID 1000, GID 100, and a
valid home directory; Yash was present as a static executable with mode 0755.
The tty had already carried getty and login input/output. **Classification:**
kernel syscall dispatch. **REM64 REQUIRED:** support Linux `setgroups` with
the expected permission and supplementary-group semantics; a login program
must be able to establish the user's group list before handing off to its
shell. The test used only a disposable rootfs copy and did not modify the
kernel or distribution.

## Native preprocessing must scale to compiler-sized translation units

The staged REM-native chibicc can tokenize its own `codegen.c`, but its
preprocessing phase did not finish within several minutes; host chibicc
completes the same input. **Classification:** compiler/runtime performance.
**REM64 RECOMMENDED:** self-hosting qualification should include large
header-rich translation units and measure preprocessing memory/time separately
from parsing and code generation. A compiler that handles small applications
but cannot preprocess its own sources is not yet a practical native tool.

## Source backup needs a qualified end-to-end host connection

A desktop-hosted, standard-library HTTP file service and tar archive workflow
passed a desktop loopback test with nested paths, spaces/Unicode filenames,
matching transferred archive bytes, and restored file mtimes. On the
disposable Fold package tuple (QEMU `0154db54…`, kernel `c7754706…`, rootfs
`a8443f8b…`), packaged Python 3.12 faulted while importing HTTP/socket modules
(PC `0x022b5f40`, address `0x23c`) and packaged curl faulted on `--version`
(PC `0x0220c210`, address `0x23c`). The audited kernel has guest networking
disabled, so no REM-to-desktop transfer was established. **Classification:**
userspace/runtime and packaging; TCP reachability is independently unverified.
The same rootfs has no rsync executable; the only host rsync inspected is a
534,680-byte x86-64 dynamic binary requiring ACL, zlib, popt, lz4, zstd,
xxhash, OpenSSL, and glibc. REM already packages static Python (46,795,332
bytes), tar (1,320,848 bytes), curl (6,125,080 bytes), and sha256sum
(287,520 bytes); the HTTP service itself is 3,862 bytes of Python source.
Thus HTTP/tar has the lower incremental package cost when Python is already
included, but it is not yet the reliable choice because the guest Python and
curl faults have not been resolved. **Classification:** application
compatibility/packaging, with runtime faults and network reachability
unresolved.
**REM64 RECOMMENDED:** accept source transfer only after a REM-to-host TCP
upload and reverse restore exercise recursive names, file contents,
timestamps, and durable filesystem readback on the actual image tuple. This
must compare file hashes and mtimes before and after reboot. This work found
no new CPU or device requirement. See
[`REM_SOURCE_BACKUP.md`](REM_SOURCE_BACKUP.md).

## Offline source exchange needs a tested host-side filesystem bridge

The current MyEmulator32 QEMU machine wires one block device as the root
filesystem and exposes no host shared-folder device. On the packaged
QEMU/kernel/rootfs tuple (`0154db54…` / `c7754706…` / `a8443f8b…`), the guest
tar create/list sequence emitted `MYEMU llist bad add` and panicked at PC
`0x020f9348`; guest tar/gzip backup is not qualified. A host `debugfs` tree
export/import plus host tar.gz round-trip on a recovered copy-on-write ext4
image preserved paths, file bytes, regular-file and directory modes/mtimes,
and symlink targets/mtimes. **Classification:** application/runtime failure
and tooling integration; the panic's root cause is unresolved. **REQUIRED:**
no Linux ABI or hardware requirement was established by this test.
**RECOMMENDED:** provide a documented, bidirectional source-exchange path
whose filename, permission, timestamp, and durability behavior is tested on
the target tuple. A safe offline image tool can serve when networking or a
shared-folder device is unavailable. **OPEN:** Fold execution and the
packaged rootfs checksum mismatch remain unresolved. See
[`REM_BACKUP_WORKFLOW.md`](REM_BACKUP_WORKFLOW.md).

## Preprocessor observability is part of native bootstrap support

Native self-hosting tracing now distinguishes tokenization from preprocessing
and records include reads and token-stream progress. The REM compiler reaches
tokenization completion on its own `codegen.c`; a longer run on a complete
target sysroot eventually reached preprocessing completion, but required many
minutes under the emulated tuple. **REM64 RECOMMENDED:** native compiler
qualification should expose bounded phase and memory diagnostics so a large
input failure can be assigned to preprocessing, runtime resources, or later
code generation rather than reported as an undifferentiated hang.

## Native bootstrap tests must use the target sysroot

REM32 self-hosting tests initially appeared to stall because a disposable image
had omitted `/usr/include`; the native compiler stopped when `chibicc.h`
requested `assert.h`. A complete image using the REM musl headers and an
explicit target include path progressed through the full include tree and
preprocessor. **REM64 REQUIRED:** native compiler qualification must bind the
compiler, headers, CRT and libraries to one target sysroot and verify that
critical headers are present before interpreting a timeout as a CPU/runtime
failure. This is a packaging/tooling requirement, not evidence for a new ISA
feature. The corrected trace also showed that include-heavy preprocessing is
very slow under the current emulated tuple, so phase timing remains a useful
resource diagnostic.

## Flight qualification must bind components to one runnable tuple

A 2026-09-28 REM32 developer candidate was assembled from hash-pinned QEMU,
kernel, rootfs, shell, editor, compiler, build tools, and debugger artifacts.
Host-side image verification and `e2fsck` passed, but boot testing repeatedly
emitted `MYEMU_TTYTRACE switch` records and never reached the shell prompt;
therefore edit/build/debug and persistence tests could not run on that image.
The participating tools had component-level evidence from distinct tuples,
which did not establish an accepted integrated environment. **Classification:**
tooling/integration; this result does not identify a CPU, ABI, or kernel root
cause. **REM64 REQUIRED:** image manifests must pin mutually matched boot
artifacts and runtime dependencies. **RECOMMENDED:** run acceptance workflows
against that exact assembled image before promoting separately qualified
components as a usable workstation. Details and hashes are in
[`REM_FLIGHT_READINESS.md`](REM_FLIGHT_READINESS.md).

## Static rsync can avoid a separate shell and transfer runtime

Canonical rsync 3.5.1 was cross-built against the staged REM musl/CRT/libgcc
tuple with bundled popt and zlib, while optional crypto, compression, locale,
ACL, and xattr features were disabled. The stripped result is a 1,128,676-byte
static ELF32 with no dynamic section. This demonstrates that the current
hosted C toolchain can build a useful network application without introducing
OpenSSH or shared-library deployment. The normal flight tuple has guest
networking disabled. A separate disposable network profile registers
virtio-net and transmits packets, but its QEMU user-network path did not
complete TCP connections to a desktop listener. The REM/desktop qualification
scripts now exercise version, daemon, recursive backup/restore, contents,
timestamps, and modes; both scripts passed on desktop loopback with host
rsync 3.2.7. This validates the harness only: the staged REM rsync ELF has
since run inside a disposable tooling image: `/usr/bin/rsync --version`
passed, but its first `rsync://127.0.0.1:18730/` client invocation faulted at
PC `0x020bee54`, address `0xfffffef4`, cause `8`. This loopback failure
occurred before any desktop peer was involved. **Classification:**
application/runtime boundary; the evidence does not isolate rsync, libc, or
kernel as the fault source. **OPEN QUESTION:** identify why the static binary's
version path works while its URL protocol path faults. **RECOMMENDED:** treat
version output and static linkage as packaging checks only; require a loopback
protocol transfer before calling the application usable. **REM64 REQUIRED:**
provide Linux-compatible IPv4 stream sockets and the socket operations used
by daemon applications (`socket`, `bind`, `setsockopt`, `listen`, `poll`,
`accept`, `connect`, `fork`/child reaping, and stream I/O), plus a reachable
device/backend path for external TCP. **RECOMMENDED:** qualify a target static
binary first on loopback, then against a host peer and persistent filesystem. See
[`REM_SOURCE_BACKUP.md`](REM_SOURCE_BACKUP.md) for artifact hashes, concrete
scripts, network dependencies, and remaining runtime work.

## libc process-entry wrappers must match the C and Linux ABIs

The REM32 kernel's syscall 220 (`clone`) and direct `execve` path were usable,
but the generic musl `src/thread/clone.c` implementation returned `-ENOSYS`.
Replacing that stub with a MyEmulator2 assembly wrapper exposed a second ABI
requirement: clone arguments occupy C registers `r1`--`r7`, the child stack
pointer is installed in `r13`, and the wrapper must preserve the parent's
registers while a `CLONE_VFORK|CLONE_VM` child runs. An initial wrapper used a
zero-extending `andi` to align a high userspace stack and truncated its upper
address bits; the corrected wrapper preserves the full pointer and restores
the caller frame. **Classification:** libc/runtime and ABI integration, not a
missing CPU instruction or syscall number. **REM64 REQUIRED:** every libc
architecture port must provide tested process/thread entry stubs, including
stack setup, child return/exit, register preservation, and `posix_spawn`
semantics, before hosted compilers are qualified.

## Preserve test tuples when booting writable ext4 images

A disposable rsync integration candidate mounted its ext4 image and started
init/Bash, but `MYEMU_TTYTRACE` output obscured the result of the rsync version
command. This run contributes no rsync pass/fail evidence. The image was booted
read-write without a snapshot; its hash changed from the pinned output, so it
cannot be treated as the manifest-pinned test tuple afterward. The QEMU process
was confirmed absent after the run. **Classification:** test tooling and image
handling, not a demonstrated filesystem defect. **RECOMMENDED:** use a fresh
hash-verified disposable image or copy-on-write overlay, record the QEMU process
and arguments, and capture usable serial output before interpreting interactive
acceptance results. See [`REM_SOURCE_BACKUP.md`](REM_SOURCE_BACKUP.md).

Repeating rsync qualification on a separately verified QEMU `0154db54…`,
kernel `c7754706…` tuple with a newly built test-only ext4 image reproduced
the same fault: rsync 3.5.1 `--version` passed, while the first
`rsync://127.0.0.1:18730/` invocation faulted as process `rsync` at PC
`0x020bee54`, address `0xfffffef4`, cause `8`. The fault occurs on loopback
before an external peer is relevant. The candidate ran on a qcow2 COW overlay;
its verified base image remained unchanged. **Classification:** userspace
application/runtime boundary; the evidence still does not distinguish an
rsync code defect from an ABI/libc/syscall interaction. A separate invocation
of the Bash qualification script itself printed `Segmentation fault` before
its progress output, so its transfer checks did not run. Recursive transfer,
restore, contents, hashes, and timestamps remain unqualified. **OPEN QUESTION:**
locate the precise failing operation at the URL parser/client startup boundary.
**REM64 RECOMMENDED:** retain an actual loopback protocol exercise in addition
to static-link and `--version` checks, and report a process fault with its PC
and tuple provenance. See [`REM_SOURCE_BACKUP.md`](REM_SOURCE_BACKUP.md).

A second c775 COW run attempted a local two-file recursive copy with
`rsync -rtp`. Before an exit status or readback, the kernel logged
`MYEMU llist bad add first=808b0000 last=808b0000 head=8083c540 old=00000000`
and `random: rsync: uninitialized urandom read (6 bytes read)`; no copy
acceptance result was obtained. **Classification:** kernel/runtime observation
during local file-copy execution; causal relation to the rsync URL fault is
unknown, and this is not evidence by itself of ext4 data corruption. **OPEN
QUESTION:** determine whether the linked-list warning is caused by process,
workqueue, or file-I/O activity on this workload. **REM64 RECOMMENDED:** include
static network tools in process, entropy, filesystem, and shutdown stress tests,
with byte/hash and timestamp readback rather than treating successful startup as
application qualification. See [`REM_SOURCE_BACKUP.md`](REM_SOURCE_BACKUP.md).

Static disassembly of the rsync 3.5.1 ELF (SHA-256
`66b877748db31550f35ce5b10faef86c8f1017c5c27d0f5a2686d2e2b2a192fa`)
provides a backend-level explanation for the URL-mode user fault. The fault
register record has a valid stack pointer (`r13=0x3fbc3290`) but `r15/fp`
`0x570`. PC `0x020bee54` is the first store after `getaddrinfo` returns from
`__lookup_serv`; it stores through `6532(r15)`. In `__lookup_serv`, the prologue
stores frame size `1392` (`0x570`) at `4(r13)`, and its epilogue reloads `r15`
from that slot before returning. The caller then uses this invalid frame base.
This is evidence of incorrect generated frame/register preservation in the
linked static userspace code, not an ext4 or block-device failure. It matches
the already documented old-GCC large-frame failure pattern. Separately, a
minimal numeric `getaddrinfo` test failed with old musl archive hash
`30cff68855d3793dedd63020928e65dfc084a5aa6667ef0114b25d6d92bd0924` and
passed at `-O0` and `-O2` with rebuilt archive
`312c6c393d9b879448edd28a822ec4fd56e1f40e364764c5da77a0df884ba21f`.
Read-only extraction of `/usr/lib/libc.a` from the tested c775 rootfs confirmed
that it contains the old archive hash above; the ext4 image was not modified.
Together this points to stale static libc/compiler output as the strongest
current explanation, though rsync has not yet been relinked and retested with
the newer archive. **Fix status:** rebuilding/relinking with the corrected
compiler/runtime is the evidence-backed next fix; no production binary was
changed in this investigation. The faulting trace also
shows syscall 221 entering immediately before the user fault, but does not
prove syscall handling caused the bad frame register. **REM64 REQUIRED:**
compile nested-call and large-frame tests that verify frame-register values
across calls and syscalls, and check generated prologue/epilogue preservation
against the published ABI. **OPEN:** establish whether the same backend defect
explains other static musl application faults.

A later local-copy attempt (`rsync -rtp` on a two-file fixture) recorded the
same process fault PC, address, and `fp=0x570` before any copy result. This
reproduces the rsync fault without an rsync daemon or remote peer, so external
network setup is not the common cause. The separate earlier local-copy attempt
that emitted `MYEMU llist bad add` still has no accepted copy result; the list
warning's relationship to the user fault is undetermined.

The separately tracked musl cancellable-syscall probe exercised a one-byte
`/dev/zero` read successfully through `__syscall_cp_c`; its socket connect
returned `ENETUNREACH` on a profile without a route. That probe did not reproduce
a SIGBUS. It does not qualify the OpenSSH syscall path end to end, but prevents
attributing the rsync frame fault to syscall argument handling from the current
evidence alone. Probe records and binaries are described in
[`REM_OPENSSH_COMPATIBILITY.md`](REM_OPENSSH_COMPATIBILITY.md).

## Runtime qualification must use the exact packaged tuple

On 2026-09-28, the rsync test-only integration candidate was booted twice with
disposable qcow2 overlays. QEMU
`34e7a65b11de60b6669c753a8b0b0cf620e21343f4d101cadf6e256c95d1eb85` and kernel
`bde7533833a49357d80b934eb2c92d6cc61c75dca50803a9a76160011b6ef94a` reached
ext4 mount and `/sbin/init`, but neither launch produced a usable shell; an
`-append init=/bin/sh` override was ignored by this kernel configuration.
Consequently rsync was not executed and this run supplies no new application
fault location. The attempted guest poweroffs did not stop QEMU; both recorded
PIDs were forcibly terminated and verified absent afterward. Logs and cleanup
records are `/tmp/rem-codex7-current-rc-rsync-{serial,qemu}.log/json` and
`/tmp/rem-codex7-current-rc-rsync-ash-{serial,qemu}.log/json`.

The tested rootfs bytes hash to
`cae54d7629ca41aed2846d070b31ced3379f65ac2dba7b061ca864ec1cf2e368`, while the
candidate manifest specifies `8465335812f9ce9024f7fb49078bc03a27d22344c3697fd8e2bdc74e0e0f7ae5`.
Read-only extraction confirmed that this rootfs contains rsync
`66b877748db31550f35ce5b10faef86c8f1017c5c27d0f5a2686d2e2b2a192fa` and
`/usr/lib/libc.a` `30cff68855d3793dedd63020928e65dfc084a5aa6667ef0114b25d6d92bd0924`.
The static rsync ELF identifies GCC 15.2.0 in `.comment`; exact compiler build
and link provenance are not embedded. Candidate native `as` and `ld` hashes
are `c56f6b236f15200426c9fe5a81caa8251df71b401c0bddd93827c1a4f68e7d93` and
`edd83609045df123c8a261cf615f70de1bd3e249082a9fe39f392ef0f6ee3b65`, but the
binary manifest does not prove these exact tools produced rsync. This tuple is
not the Fold release image, which does not include rsync at `/usr/bin/rsync`.
**Classification:** current candidate usability is blocked at init/console
integration, before application execution; the responsible subsystem is not
established by this run. The historical rsync frame-register fault remains
separate evidence for a stale static libc/compiler artifact. **REM64
REQUIREMENT:** qualification records must bind runtime results to verified
binary, rootfs, libc, compiler, linker, kernel, and emulator hashes; a passing
result from a different tuple does not qualify the packaged system.

## Rebuild architecture libc when qualifying process creation

The native-GCC process-creation failure initially looked like a missing
kernel feature: syscall 220 `clone` and the child's `execve` both succeeded,
but musl `posix_spawn` reported `ENOSYS`. The failing `posix_spawn` object came
from a stale musl build produced before the corrected REM GCC large-frame
backend. Rebuilding musl's MyEmulator2 process-entry code with the corrected
compiler made `posix_spawn("/bin/true")` return success and the child wait
status report exit 0. **Classification:** compiler/runtime artifact and ABI
integration, not a missing clone syscall. **REQUIRED:** process-creation and
libc wrappers must be rebuilt and hash-checked together after backend fixes;
static applications cannot be repaired by replacing libc files after linking.
A native GCC driver then reached `cc1`; its remaining disposable test failure
was an independent `cc1` virtual-memory exhaustion, so GCC compilation is not
claimed complete here.

## Signal-wait syscall dispatch is part of the Linux application ABI

Yash exercised Linux syscall 133 (`rt_sigsuspend`) while waiting for terminal
input. The generic kernel implementation already provided Linux semantics
(temporary signal-mask replacement, suspension until signal delivery, and
mask restoration), but the MyEmulator2 architecture dispatcher omitted syscall
133 and returned `ENOSYS`. Adding the direct architecture dispatch to
`sys_rt_sigsuspend` in the disposable kernel allowed Yash to execute its
interactive input path; no Yash or libc workaround was required. **Classification:**
kernel syscall-table integration. **REM64 REQUIRED:** architecture syscall
tables must cover the signal-wait ABI used by interactive shells, including
`rt_sigsuspend`, with the published argument size and signal-mask semantics.

## Native compiler predefined macros must match the target data model

During REM chibicc self-hosting, the backend's type model was ILP32 but its
preprocessor still advertised the host LP64/x86-64 model (`__LP64__`, 8-byte
`long`, pointers and `size_t`). Musl headers therefore selected declarations
for a different ABI. The REM fix removes those host markers, reports 4-byte
widths, and defines a REM target marker; the focused regression is
`toolchain/chibicc-rem/test/rem-ilp32-predefined.c` (see
`docs/REM_CHIBICC_BUGLOG.md`). **REM64 implication:** every compiler and libc
port must provide one authoritative target-predefined-macro contract derived
from the ABI; host defaults must never leak into target headers. This is a
compiler/toolchain requirement, with the exact REM64 data model still an open
design choice.

REM32 also exposed that an empty C function parameter list (`f()`) is an
old-style unspecified prototype, not a variadic function. Treating it as
variadic caused an unnecessary register-save area and writes outside small
frames in generated code. **REM64 implication:** the compiler ABI must
distinguish unspecified, prototyped, and variadic functions, including their
call checking and register-save requirements. This is a compiler/ABI
requirement demonstrated by the `rem-oldstyle-main.c` regression.

## Distribution getty configuration and ext4 recovery need tuple-level checks

The 2026-09-28 flight integration booted Codex #3's distribution rootfs
`4a180d70…6f3f80` with kernel `f9280fc0…faf51b` and the selected QEMU. Kernel
boot, ext4 mount, devtmpfs, `/sbin/init`, and SysV `rcS` ran. The distribution
inittab invoked `/sbin/getty -L`; this getty rejected `-L`, exited, and was
respawned. Removing only that option in the disposable candidate made getty
display its login prompt. The subsequent login attempt did not reach a usable
Yash prompt: after credentials were entered, shell input produced a
`MYEMU_USER_ALIGN pid=1` diagnostic and a kernel panic (`Attempted to kill
init`, exit code `0x00000007`). The alignment record omitted the process name,
so it does not establish that Yash itself ran or faulted. The user-fault root
cause was not established. **Classification:** init/getty configuration
compatibility is established; the later fault remains an open kernel/user ABI
or runtime issue. The integration correction is implemented in
`tools/rem-flight-image/package-distribution.py`; the distribution rootfs was
left unchanged.

After forced QEMU termination, host `e2fsck` journal recovery found ext4 free
block and free inode counters equal to zero, although a full scan counted
88593 free blocks and 31022 free inodes. `e2fsck -fy` repaired the disposable
image, after which `e2fsck -fn` passed. Because the guest did not shut down
cleanly, this does not by itself prove a kernel ext4 accounting defect; the
precise cause remains an **OPEN QUESTION**. **RECOMMENDED:** qualify writable
filesystem use with both clean guest shutdown/reboot and forced termination,
then verify journal recovery and filesystem counters with a host checker.
**REM64 REQUIRED:** filesystem recovery must preserve ext4 consistency and
accurate allocation metadata across supported shutdown and crash-recovery
paths. Runtime tests must also bind init/getty options to the exact packaged
implementations rather than assuming options from another getty variant.

In a later RC3 disposable test, the login BusyBox lacked `sync` and `halt`, so
the test-only wrapper routed them to the staged workstation BusyBox. Both
commands then ran, but `halt` still did not stop QEMU; the host had to send
SIGTERM, and `e2fsck` again found free-block and free-inode counter mismatches.
After `e2fsck -fy`, the same-image reboot retained a written `PERSIST` marker,
but the subsequent halt again failed. A dedicated static syscall probe on the
same kernel called Linux syscall 142 (`reboot`) with the documented magic
values and `RB_HALT_SYSTEM`; it returned `-1`, `errno=38` (`ENOSYS`). The
dispatcher in `linux/arch/myemulator2/kernel/syscall.c` has no reboot case and
returns `-ENOSYS` in its default branch. This explains why the packaged halt
applet cannot power off the guest. **Classification:** syscall/kernel, with
the ext4 counter mismatch still an effect observed only after forced QEMU
termination. **Linux compatibility requirement:** provide Linux reboot
syscall semantics so PID 1 can perform orderly halt/reboot after its shutdown
hooks. The same-image marker survived a reboot only after host `e2fsck -fy`
recovery; clean shutdown and persistence remain unaccepted. A console
`Unknown signal` message was also observed, but its source was not isolated.

In the 2026-09-29 follow-up, a disposable assembly helper issued Linux syscall
81 (`sync`) and syscall 93 (`exit`); the selected `as` assembled it, `ld`
linked it, and it returned status 0 in the guest. After host `e2fsck -fy`
recovery, a marker was present, nonempty, and readable after booting the same
image again; the post-boot `e2fsck -fn` passed. This is evidence for
recovery-path file persistence, not clean guest shutdown: the guest still did
not power off, and QEMU was stopped by the tracked host harness. **REM64
REQUIRED:** provide a working Linux reboot syscall to PID 1 and qualify
shutdown hooks plus filesystem consistency on the same image. The test also
shows that the Linux `sync` syscall worked even though the packaged BusyBox
had no `sync` applet; executable utility availability is a separate packaging
requirement.

### Compiler and assembler invocation compatibility

On the 2026-09-29 RC3 tuple, `chibicc -S min.c -o min.s` and direct
`as min.s -o as-no-c.o` passed. A focused disposable guest test then ran both
`as min.s -o as-no-c.o` and `as -c min.s -o as-c.o`; each returned zero and
produced byte-identical relocatable objects (SHA-256
`f36ceb7905ac3776e749631ce0b902a4ec732eb16d3b1033af6fb6163ffb6a5f`). The
earlier report of a fault at `_bfd_clear_error_data` was caused by the test
shell's `printf` process after the assembler command, so it does not establish
an assembler `-c` defect. **Classification:** assembler command-path
qualification passed; remaining RC3 compiler/runtime failures are separate.
**REM64 RECOMMENDED:** keep both direct and compiler-driver command forms in
the binutils regression suite, and avoid shell diagnostics that can turn a
successful tool invocation into a false failure.

### Supplementary groups are part of the login contract

REM32 RC3 login reached password handling and requires Linux syscall 159,
`setgroups`, before it can launch the user's shell. The architecture syscall
dispatcher initially omitted this entry and returned `ENOSYS`; the generic
Linux `sys_setgroups` implementation already provides the required count,
pointer, and permission validation. **REM64 REQUIRED:** the syscall ABI and
dispatcher must expose supplementary-group setup so a normal login can reach
its shell. A later RC3 UAT on kernel
`8b949bc36a0e158d0bc93b82010b76199300b1f7c2306c2c775dfb406ceacb31` reached
the Yash prompt, superseding the earlier pending login result; full workstation
acceptance remains open because of the build, networking, and shutdown failures
recorded below.

### Userspace may need to read supplementary groups

On the RC3 workstation candidate, BusyBox `test -x` exited with
`getgroups: Function not implemented`; the UAT's executable-presence checks
therefore failed even though host-side inspection confirmed the binaries and
modes were present. **Classification:** syscall/kernel compatibility, observed
through a userspace utility. **REM64 REQUIRED:** implement Linux
supplementary-group retrieval as well as setup, so ordinary utilities can
inspect the groups attached to the current process. The broader shell/build
workflow was retested on the later kernel: login and shell checks pass, while
the compiler/assembler build path remains blocked as recorded above.

### REM32 self-hosting boundary: hosted preprocessor state

The REM32 native chibicc seed reaches small translation units, but a hosted
self-compilation reducer can stall while repeatedly revisiting musl include
guards before preprocessing completes. A 32-bit FNV hash substitution did not
resolve the reproduced case and was reverted. This is a compiler/runtime
qualification blocker rather than evidence for a REM64 ISA change. REM64
should nevertheless require deterministic, testable hosted preprocessor
behaviour under its native word-size model before claiming self-hosting.
See `docs/REM_CHIBICC_BUGLOG.md`, entry 23.

### REM32 hosted preprocessor hash-width requirement (fixed)

Native chibicc exposed a concrete REM32 toolchain requirement: the
preprocessor's large macro/include-guard hashmap could not safely depend on
64-bit FNV arithmetic. Repeated musl guard processing caused hosted
self-compilation to stall. Using target-word-size 32-bit FNV-1a fixed the
reproducer (`rem-hosted-null-member.c`) and allowed all compiler phases to
complete in a clean guest. REM64 should define and test a native-word hash
path for compiler/runtime tables, or provide reliable 64-bit arithmetic before
using 64-bit hashes in hosted tools. This is a compiler/runtime requirement,
not an ISA redesign.

## Password hashing workload cost (REM32 evidence)

REM32 standalone BusyBox SHA-crypt probes produced byte-identical results to
host execution at one and 100 rounds, and a matching one-round login reached
`setgroups` and Yash. The full 5000-round SHA-crypt workload is computationally
slow under the emulator and exceeded the short diagnostic timeout. This is an
application/performance observation, not an ABI or cryptographic correctness
requirement; REM64 should still provide predictable integer/64-bit arithmetic
performance for libc and security workloads, with timing limits tested
separately from correctness.

### Native hosted-tool performance is a qualification requirement

After fixing the REM32 preprocessor hash-width defect, native chibicc fully
translated several smaller compiler units, but `codegen.c` exceeded a
600-second bounded guest run after tokenization. Future REM64 toolchains must
qualify not only correctness and ABI compatibility but also practical
throughput for rebuilding their own compiler; otherwise self-hosting can remain
functionally possible but operationally unusable.

## Supplementary groups are part of the login ABI

REM32 login initially worked through authentication and `setgroups`, but a
BusyBox post-login path then exposed missing syscall 158 (`getgroups`). Linux
user environments require both setting and querying supplementary groups.
REM64 must include the paired `setgroups`/`getgroups` syscall ABI with normal
user-pointer, count, and permission semantics.

### Native tool throughput must include assembler qualification

The REM32 self-hosting batch demonstrated that native chibicc can translate
large units with an optimized seed, but native `as` did not complete the large
`parse.c` assembly within a 2400-second guest run after chibicc emitted it.
Future REM64 self-hosting acceptance must measure assembler throughput on large
compiler-generated assembly, not only compiler correctness and linker output.

### Self-hosting validation requires persistent artifact handling

The REM32 work established that native `as` can assemble a large 793 KiB
compiler-generated source file and native `ld` can report a successful stage-2
link. The remaining validation failure was in disposable-image persistence and
artifact staging, not a confirmed ISA or assembler defect. REM64 flight
validation should require checksummed persistence of generated objects and
linked executables across guest restarts before accepting bootstrap results.

### Bootstrap generation equivalence must be tested

REM32 stage 2 was linked and passed `--help` plus a header-free translation,
but failed while compiling hosted `strings.c` immediately after tokenization,
where stage 1 succeeds. This demonstrates that a successful native link does
not establish generation equivalence: each bootstrap generation must compile a
hosted translation unit before stage 3 is claimed. REM64 validation should
include this generation-to-generation hosted test.

### Bootstrap generation must pass internal self-tests

REM32 stage 1 completes the compiler's hashmap self-test, while linked stage 2
can run help and a trivial translation but does not complete `-hashmap-test`.
Future REM64 bootstrap acceptance should require each generation to pass its
own internal data-structure/runtime tests before attempting the next
translation generation.

### Bootstrap artifacts must be generation-consistent

REM32 mixed stage-2 objects (some native-generated, some host-generated)
passed trivial tests but stalled or trapped during hosted preprocessing. This
shows that a successful link is not sufficient evidence of a compiler
generation: all objects must use one target-definition set, header layout,
assembler/linker tuple, and runtime ABI. REM64 bootstrap tooling should record
and verify those inputs before executing a generated compiler.

The intact REM32 stage-2 artifact subsequently translated a hosted
`#include <stdio.h>` unit through preprocessing, parsing, code generation, and
assembly output in a clean guest. This confirms that the architecture can run a
hosted second-generation compiler; full-source and generation-to-generation
bootstrap tests remain separate acceptance requirements.

### ILP32 literal parsing and 64-bit helper qualification

REM32 self-hosting exposed two separate requirements. First, using `strtoul`
for integer-token conversion truncated `ULL` literals because `unsigned long`
is 32 bits; changing the compiler to `strtoull` was required for correct
64-bit constants. A REM64 compiler must use a conversion type at least as wide
as the language value it is parsing, independently of the host ABI.

Second, hosted C callers of `__muldi3` failed to reach normal exit in the
current REM32 runtime tuple, while a bare assembly caller did return from the
helper (a following privileged `halt` was the observed trap). This narrows the
issue to the caller/CRT/runtime boundary rather than proving the helper body
wrong. REM64 must specify and regression-test the compiler-helper ABI,
startup/exit path, and implementation for wide integer operations before
accepting a self-hosting toolchain.
### Native binutils and emulated-TLS startup (2026-09-29)

**Evidence:** the REM-native TLS-enabled assembler repeatedly faulted in
`_bfd_clear_error_data` when BFD first accessed its emulated-TLS error object;
`__emutls_get_address` returned an unmapped `0x10000044`.  A no-TLS native
binutils build assembled the same RC3 input successfully in the same pinned
kernel/QEMU environment.

**REM64 implication:** Linux/toolchain support requires a reliable TLS model
for static native tools, including allocation and first-use initialization
before BFD/libgcc error handling.  A no-TLS build is a compatibility fallback,
not an architectural solution.  **Required:** make the normal TLS path pass
the native binutils startup/assembly regression; **open question:** whether
REM64 should provide hardware TLS state, a fully specified software TLS ABI,
or both.

## 2026-09-29: power-control and network-device contracts

**Observed (REM32, kernel/runtime):** Linux's generic `reboot(2)` path reaches
architecture `machine_halt`, `machine_restart`, and `machine_power_off` hooks.
The original REM hooks only halted the CPU, so QEMU had no guest-visible
shutdown/reset event. A disposable implementation added syscall 142 dispatch,
a fixed MMIO power register, and QEMU reset/shutdown handling. This establishes
that a Linux machine contract needs explicit reset and poweroff semantics in
addition to a CPU halt instruction. The disposable probe reached Linux's
`reboot: System halted`; full journal-clean acceptance remains pending.

**Requirement for REM64:** define a standard firmware/platform power-control
interface (MMIO or equivalent) and its reset, halt, and poweroff semantics
before porting Linux. This is a demonstrated Linux integration requirement,
not an ISA requirement.

**Observed (REM32, networking):** virtio-net is exposed over virtio-mmio at a
fixed MMIO base and IRQ, with Linux networking enabled. Earlier traces proved
virtqueue DMA visibility, ARP TX/RX, and guest IRQ delivery. External TCP was
not completed in the available QEMU user-network environment.

**Requirement for REM64:** document virtio transport addresses/interrupts,
DMA address visibility, and the expected userspace network configuration as a
platform contract. Full TCP/backend behaviour remains an open integration
question.

### Chibicc stage-3 hosted preprocessing (2026-09-29)

**Observed (REM32 compiler/tooling):** a clean stage-2 generation compiled
all nine chibicc units and produced a stage-3 compiler that starts, handles
`--help`, and builds/runs a hosted hello program. The same stage-3 binary
selects musl's `NULL nullptr` branch while preprocessing `strings.c`, whereas
stage-2 selects the C branch. The bounded probe did not establish a single
expression-level cause.

**REM64 implication:** bootstrap acceptance must test each generation against
representative hosted headers, not only a header-free program or successful
link. Generation-consistent artifacts and a reproducible preprocessor/macro
state are toolchain requirements. Full stage-to-stage equivalence remains an
open question; this issue is deferred for REM 0.1 and is not an ISA
requirement.

### Bootstrap differential testing requirement

REM32 stage-2/stage-3 preprocessing probes showed that generation equivalence
must be checked with identical include paths and source environments: missing
include roots can mimic a compiler divergence. REM64 bootstrap tooling should
record compiler options, include roots, and runtime tuple for every differential
probe before attributing a mismatch to the architecture or compiler.

### Ext4 writeback requires device flush and orderly poweroff (2026-09-29)

**Observed (REM32, ext4/block/QEMU):** Guest file creation and modification
appeared in the page cache but were not durable across a second boot. The
custom block path did not propagate `REQ_OP_FLUSH`/`REQ_PREFLUSH`/`REQ_FUA` to
the host image, and generic Linux poweroff fell back to HALT because the
architecture did not register `pm_power_off`. A disposable fix added a block
flush command, QEMU `blk_flush()` handling, syscall 306 (`syncfs`), and the
architecture poweroff registration. With those changes, two independent
same-image two-boot tests preserved new and modified file contents and
`e2fsck -fn` passed without repair.

**REM64 requirement:** the storage contract must specify flush/FUA ordering,
host persistence semantics, and an orderly poweroff path that runs Linux's
shutdown preparation. A device that acknowledges writes without a defined
flush completion or power-control hook is insufficient for a persistent
filesystem. This is a demonstrated Linux/platform requirement, not an ISA
feature.
### REM32 compiler lesson: wide comparisons are a bootstrap requirement

During native chibicc bootstrap, an incorrect 64-bit comparison lowering on
the 32-bit target made a preprocessor conditional select the wrong branch.
The failure was exposed only when stage-3 compiled hosted headers; ordinary
small programs did not exercise it.  The fix gives high-word ordering
precedence and gates low-word comparisons on high-word equality.

**Requirement for REM64:** compiler regression suites must include signed and
unsigned wide comparisons in preprocessor expressions and ordinary C code, and
stage-to-stage differential bootstrap tests must cover them.  This is a
compiler/backend requirement demonstrated by REM32, not a change to REM64's
ISA or ABI.

## Network-triggered task-work fault evidence (REM32)

**Tested:** Linux 6.12.1 with virtio-mmio/virtio-net, static `eth0` address and
route, local UDP, DNS lookup, and nonblocking TCP connect; see
[`REM_NETWORKING.md`](REM_NETWORKING.md) and the reproducer in
[`tests/network`](../tests/network/README.md).

**Observed failure — kernel/syscall boundary:** after one TCP `connect()` returned
`-EINPROGRESS`, Linux faulted in `task_work_run()` while loading
`current->task_works->next`; the list head was `0x10046003`. This is an invalid,
unaligned callback pointer, not a faulting user buffer or skb access. A regular
file close and several isolated network controls passed. The exact corrupting
writer is unknown; this does not establish that TCP itself is the writer.

**REQUIRED Linux behavior:** Linux task-work callbacks queued for a task must
remain valid and naturally aligned until `task_work_run()` drains them at the
syscall return/exit boundary. REM64 must preserve the kernel pointer and atomic
queue semantics used by this Linux mechanism.

**OPEN:** identify whether the REM32 corruption is caused by kernel memory,
atomic/compiler lowering, or a socket lifecycle path. No REM64 design choice or
REM32 fix is established by this evidence.

## Network checksum byte order evidence (REM32)

**Tested:** virtio-net TX under QEMU user networking; packet capture included
IPv4/UDP DNS requests and IPv4/TCP SYNs. `tcpdump -vvv` found malformed checksums.
For one captured IPv4 header, the correct wire checksum is `c6fc`, but the guest
wrote `fcc6`. The current `arch/myemulator2/lib/checksum.c::csum_partial()` sums
bytes as big-endian 16-bit words; `csum_fold()` returns that value to a
little-endian target, reversing its byte order in packet memory.

**REQUIRED network behavior:** IPv4 and transport checksum fields on transmitted
packets must have the network-specified byte order and valid checksum values.
Invalid checksums cause peers to discard packets, as seen in DNS/TCP timeouts.

**RECOMMENDED implementation:** keep the kernel checksum primitive's
`__wsum`/`__sum16` representation consistent with target endianness, and only
advertise checksum offload when the virtio backend completes partial checksums.
A one-file little-endian accumulation candidate is under isolated testing; it
has not passed a guest networking retest yet.

**OPEN:** retest candidate packet checksums and DNS/TCP replies on the same
QEMU/rootfs tuple, then determine whether the separate task-work corruption is
also affected.

### REM32 checksum follow-up: preserve carry from partial sums

The first little-endian checksum candidate fixed the wire byte order but still
failed DNS. Instrumentation on Linux 6.12.1 showed `csum_partial()` receiving
`0xffffffff` from `csum_and_copy_from_user()` and losing a 32-bit carry before
folding; the copied 28-byte DNS question was intact, but its computed sum was
`0x2cb` instead of `0x2cc`. A private candidate using a 64-bit accumulator and
repeated end-around folding produced valid UDP checksums. On the same QEMU
binary, QEMU DNS returned a reply, external TCP completed, and a guestfwd HTTP
fixture was received. See [`REM_NETWORKING.md`](REM_NETWORKING.md) and
[`tests/network/README.md`](../tests/network/README.md).

**REQUIRED Linux behavior:** checksum primitives must preserve carries when
combining an initial partial sum with copied packet data, including the
`0xffffffff` seed used by Linux's user-copy checksum path. Correct packet
checksums are required for peers to accept IPv4 UDP/TCP traffic.

**RECOMMENDED:** use an accumulator wide enough for the maximum supported
buffer and seed, then fold all carries before returning the partial sum.

**OPEN:** the tested private patch has not yet been applied to the shared kernel
or release image. The earlier `task_work_run` fault did not recur in the
combined DNS/TCP probe with this patch, but the precise causal relationship and
the separate loopback UDP failure remain open.

### Checksum fix integration and curl executable fault (2026-09-30)

Kernel owner #1 applied the checksum carry/endianness fix to shared source and
rebuilt a kernel with SHA-256
`e2b7fe6672ee1e37b849815e6fc263844036c44c0255d85a10f67251a15041d1`, matching
the private kernel used for the passing DNS/TCP/HTTP tests above. This closes
the source-integration question for the checksum fix; release-image
qualification remains separate.

**Tested:** the statically linked staged curl binary, including `curl
--version`, on that kernel in a disposable ext4 rootfs. It faults before making
a TCP request at `curl_msnprintf+0x4c` (PC `0x0203698c`), reading address
`0x13f8` with reported FP `0x1400`; `curl --version` exits by signal. The
transfer attempt also emitted ext4 folio warnings at `fs/ext4/inode.c:3554-55`.

**Classification:** application/toolchain/runtime interaction is implicated;
the evidence does not yet distinguish compiler/backend ABI, libc/runtime,
kernel process state, or filesystem behavior. This is not evidence of a
network-stack regression because the version-only reproduction fails before
network I/O.

**OPEN:** compare the curl executable's build tuple and ABI/register-preserving
call behavior with a minimal C call-chain reproducer, and independently
investigate the ext4 folio warnings. Do not infer an ABI defect from this one
binary fault. Detailed logs and hashes are in
[`REM_NETWORKING.md`](REM_NETWORKING.md).

### Native compiler bootstrap differential (REM32)

**Tested:** the same hosted reducer, headers, runtime and disposable image
were passed to the Stage-2 and Stage-3 native chibicc binaries. Stage-2
completed and emitted assembly; Stage-3 faulted before output:

```c
#include "chibicc.h"
int x = 1;
```

Replacing the initializer with a tentative declaration (`int x;`) made
Stage-3 pass. The larger Stage-3 fault was captured by remdbg in
`fnv_hash+0x1a4`, but the reducer has not yet identified the miscompiled
function or data object.

**REQUIRED toolchain behavior:** a compiler rebuilt by the target compiler
must preserve static data, pointer tables and parser state when compiling a
global initializer. Stage-2/Stage-3 differential tests should be part of a
future native bootstrap qualification suite.

**CLASSIFICATION:** compiler self-hosting/backend interaction; not evidence of
a CPU or Linux syscall requirement. **OPEN:** isolate the first differing
generated function/data object before changing the REM64 ABI or object format.

### Stack-passed arguments are distinct from parameter homes (REM32 evidence)

The REM chibicc backend initially used each callee parameter's local home
offset to reload arguments passed on the stack.  A five-argument
`write_gvar_data` call then read outside the caller's outgoing argument area;
Stage-3 self-compilation exposed this as a fault in `write_buf` while handling
`int x = 1`.  The required contract is that incoming stack arguments are
addressed from the caller's stack pointer above the 32-byte register home area,
independently of where the callee stores its local parameter copy.  REM64
should specify this relation directly in the ABI and require compiler tests for
fifth-and-later arguments, including recursive calls.  This is a compiler/ABI
requirement demonstrated by REM32, not a finalized REM64 layout decision.
### REM32 evidence: `double` alignment is part of the compiler ABI

GCC's REM32 ABI lays out an 8-byte `double` at 4-byte alignment: a probe
`struct { char tag; double value; }` is 12 bytes.  A chibicc generation that
assumed 8-byte alignment computed compiler-internal `Token` objects as 72
bytes instead of 68, and its self-hosted token copies corrupted parser state.
REM64 must make floating-point size/alignment rules explicit and require the
compiler, libc and debugger to agree.  This is a required ABI contract, not a
packing workaround.

### REM32 evidence: native linker provenance matters for self-hosting

The corrected native compiler assembled a complete nine-translation-unit
chibicc object set, but the stale packaged `ld` bus-errored while the
qualified native linker (`edd83609045df123c8a261cf615f70de1bd3e249082a9fe39f392ef0f6ee3b65`)
linked the same inputs successfully.  This was a toolchain artifact mismatch,
not a source or ABI change.  REM64 release qualification should bind the
compiler, assembler, linker, CRT, libc and runtime to one hashed tuple and
reject mixed-generation binaries before declaring native self-hosting.

### REM32 evidence: the boot idle task needs an initialized scheduler stack

An idle/context-switch stress run reached `arch_cpu_idle()` with a supervisor
stack near zero (`info=0xffff_ffd4`), producing a kernel page fault while the
editor was simply idle. The boot CPU starts on `init_stack`, but the initial idle
task's `thread.sp` was otherwise zero; switching back to that task loaded the
invalid value. The REM32 switch path now falls back to the boot idle stack when
the idle context is uninitialized. REM64 must define initialization of the boot
CPU's idle task context before the first scheduler switch; a context switch must
never make a zero stack pointer architecturally visible. **Classification:**
scheduler/context-switch ABI requirement demonstrated by REM32.

### REM32 evidence: hosted compiler workloads need large-memory stress

The native chibicc port passes small C programs, but a large header/preprocessor
workload and a 4096-block allocator probe expose corruption or termination that
does not occur in the equivalent REM GCC build. The exact owner is still open
(compiler generated state versus allocator/runtime); this is not evidence for a
new CPU rule. REM64 should nevertheless qualify its native toolchain with large
translation units and allocator/hashmap stress, not only hello-world tests.
**Classification: compiler/toolchain qualification requirement; open
implementation question.**

### REM32 evidence: HALT must commit translated architectural state

On the Fold, two independent workloads resumed from `HALT` into
`arch_cpu_idle+0x30` with a valid SP but `r15=4`, faulting at address
`0xffff_ffd4`. QEMU's TCG translator treated HALT as a noreturn helper and
left `sr`, `r13`, and `r15` in host-side TCG globals rather than committing
 them to the architectural CPU state before parking the CPU. Wakeup therefore
reloaded stale register values. The fix stores those registers before HALT;
a 30-cycle desktop idle/read/sleep stress run passed. **Classification: CPU
emulation/translation architectural requirement.** REM64 must define a precise
TB-exit rule: every instruction that parks, traps, or otherwise leaves a
translation block without a normal return must commit all architecturally
visible register and status state first.

### REM32 evidence: SA_SIGINFO ucontext offsets must match the signal frame

A static REM probe installed an `SA_SIGINFO` handler, protected a writable data
page with `mprotect(PROT_READ)`, then wrote to it. The handler received
`SIGSEGV` and the correct `si_addr` (`0x02003000`). The raw context held the
faulting PC (`0x020004e8`) and SP (`0x3ffe4c1c`), but reading them through the
REM musl `ucontext_t.uc_mcontext.__gregs[16]` and `[13]` fields returned zero.
The observed frame places saved registers immediately after the flags, link and
stack fields (mcontext offset 20 bytes); the tested musl header places
`uc_mcontext` at offset 148 bytes because its `sigset_t` is 128 bytes.

**REQUIRED Linux/ABI behavior:** `SA_SIGINFO` handlers must see a `ucontext_t`
whose register fields match the kernel signal frame, including the faulting PC
and SP, while `siginfo_t.si_addr` identifies the fault address. **RECOMMENDED
REM64 design:** define the signal-frame and libc `ucontext_t` layout from one
UAPI contract, and qualify it with a protected-page fault plus `sigreturn`
round-trip test. **OPEN:** determine the precise REM32 kernel/libc ownership and
compatibility correction; this probe demonstrates a layout mismatch but does
not locate its originating implementation change.

### REM32 evidence: stack locals must preserve explicit type alignment

During pristine-source Emacs startup, a bounded diagnostic captured the first
wrong-type-argument: CHECK_STRING rejected an AUTO_STRING whose backing
struct Lisp_String fields were otherwise valid. The object address was
0x3faa2a0c (4 mod 8), despite GCALIGNMENT=8 and the compile-time
GCALIGNED(struct Lisp_String) check. In vadd_to_log, the generated frame
uses r15-36 for the object and r15-32 for its tagged Lisp value; the
captured incoming frame base was 16-byte aligned. This is a compiler backend
stack-slot alignment failure. The disposable adjustment that aligns total
callee-save/frame decrements does not correct this local offset. No canonical
compiler or Emacs changes were made, and Emacs still fails before pdump.

**REQUIRED compiler behavior:** an automatic object with explicit 8-byte type
alignment must be placed at an 8-byte aligned address, independent of the
function's other locals and callee-save frame. **RECOMMENDED REM64 qualification:**
include stack-local alignment probes for aligned structs and unions, plus a
real workload using tagged pointers to stack objects. **OPEN:** the exact GCC
backend correction and whether other stack-local types are affected.

### REM32 evidence: initial user stack alignment reaches native `main`

A standalone static REM ELF probe ran under QEMU/kernel and printed
`__builtin_frame_address(0) & 15` from `main`: `REM_ENTRY_SP_MOD16=12`.
The tested ELF entry is `_start=0x020000b4`; its startup sequence passes the
incoming stack through the musl CRT, whose observed frame adjustments are
multiples of 16 before calling `main`. A clean GNU Emacs 30.1 target build
independently reported the same `main` entry value. Its `get_minibuffer`
function then placed an `AUTO_STRING_WITH_LEN` backing struct at 4 mod 8,
which produced a wrong-type argument at `Fget_buffer_create`. The Emacs stack
object has a separate compiler local-alignment failure described above; this
probe isolates the initial-entry alignment observation from Emacs.

**REQUIRED target ABI behavior:** the initial userspace stack must satisfy the
same 16-byte alignment contract required at C call boundaries before CRT code
calls `main`; startup code must preserve it. **OPEN ownership question:**
identify whether the current misalignment is introduced by the kernel exec
stack builder, ELF entry handoff, or earlier startup transition. **REM64
recommendation:** specify initial SP alignment in the kernel/ELF/CRT contract
and test it with a static ELF that reports SP alignment at `_start` and `main`.
Evidence: probe source `/tmp/rem-emacs-qualification-20260930/entrysp-probe.c`
(SHA256 `95ce9cb155a08eeac678cece61b741ce8b72dd480fc3314a53234966c635bc94`),
ELF `/tmp/rem-emacs-qualification-20260930/entrysp-probe` (SHA256
`cf17ddc6f29e89e12dbc6691e62bf0b2f321ed8c724907ebdb2337082834797e`), test
image SHA256 `e9a43f8b6507efefd8645aa7a1f6bac5d0ea4104535b64386b36273e9a833444`,
QEMU SHA256 `9d1f47b2a6fb2e2f808e4538d316265cf220534dc2cb42c76a486391a4f27f16`,
and kernel SHA256 `f6f5f57ef5bf18af984b8b11febb40d76204109ec3533621ba5969006af19138`.
The tracked QEMU PID 11 was cleaned (`pid_gone:true`).

## Time ABI width must match userspace

REM32 evidence: musl was built with 64-bit `time_t` (`timespec` 16 bytes,
`tv_sec` at offset 0 and `tv_nsec` at offset 8), while syscall 403 was
initially dispatched through the 32-bit compatibility helper.  The resulting
writes made the nanoseconds word the high half of `time_t`, producing dates
with enormous years and invalid Make timestamps.

REM64 implication (REQUIRED): each time syscall must have one explicit UAPI
layout and dispatch path.  Kernel `__kernel_timespec`, libc `struct timespec`,
time_t width, and syscall numbers must be validated together; a compat helper
must never be selected solely because the CPU is 32-bit.

The corrected REM32 dispatch now uses the time64 helper.  A separate open
platform requirement remains for a wall-clock/RTC provider: the current QEMU
virtual counter starts at zero, so corrected realtime begins at the Unix epoch
until a clock source or userspace clock-setting policy supplies wall time.
### 2026-10-04 — native build-tool qualification boundary

The 32-bit REM toolchain now has a qualified native chibicc (Stage 7) and can compile/link/run Samurai and its generated target. A post-build Samurai fault is reproduced by both chibicc-built and reference Samurai in the same guest, inside musl `memchr`/`strnlen` after child completion. REM64 should retain enough process/file-stream diagnostics to distinguish child execution from parent post-build bookkeeping; this is an application/runtime qualification requirement, not evidence for a new ISA rule.

### 2026-10-04 — 32-bit preprocessor constant width is ABI-visible

The REM32 compiler's preprocessor evaluated `#if UINTPTR_MAX == UINT64_MAX`
through a `long` return value, truncating 64-bit constants. This selected an
LP64 `PRId64` format on an ILP32 target and caused a real Samurai post-build
fault. Required for REM64 and other ports: preprocessor conditional arithmetic
must preserve at least 64-bit integer width independently of host `long`.

The concrete fix also required target-aware integer-literal typing: a 64-bit
hexadecimal `U` constant cannot be assigned the 32-bit `unsigned long` type
on an ILP32 target. REM64 should keep literal typing and preprocessor
conditional evaluation independent of the compiler host's `long` width.

### Final 2026-10-04 toolchain lesson

The original Stage-7 compiler passed its recorded qualification but was
superseded after Samurai exposed ILP32 preprocessor truncation. REM64 must
require 64-bit-safe conditional evaluation and target-correct literal typing;
these are observable through libc format macros and application varargs.

### 2026-10-05 — HALT must publish complete architectural state

REM32 Fold failures reproduced a deterministic `pid=0` fault after
HALT→IRQ→RFE with the same low-address signature.  A desktop trace of the
corrected translator showed that HALT must commit SR, banked SP (R13), and
frame register (R15) before a noreturn halt helper; wakeup and RFE then restore
the exact saved state across repeated cycles.  The deployed Android QEMU was
built from an older translator object lacking those stores.

**REM64 requirement:** a CPU halt/interrupt contract must define which
architectural state is committed before parking and restored on interrupt
return.  Host builds must be reproducible; a stale target object can silently
omit required state publication.  The current REM32 desktop trace is evidence
for the requirement; Android qualification remains open.
