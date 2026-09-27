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

## MMU correctness includes permissions and translation lifetime

REM32 exposed failures from incorrect permission checks, stale read-only TLB
entries after a writable fault, missing TLB invalidation, and private writable
pages created without the requested permission. Fixes required coordinated
QEMU, page-table, fault-handler, and TLB changes (`4b0b354`, `3b9c244`).

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

**REM64 implication:** the console launch contract must define when a session
leader acquires its controlling terminal (`setsid`/`TIOCSCTTY`) and how its
foreground process group is selected. **Status:** required for reliable
job-control terminal handoff; whether init, login, or shell owns the setup is
an integration decision.

**Status:** termios and controlling-tty mechanisms are demonstrated, but the
tested Yash launch lacks a controlling-terminal association; the launch-path
owner remains to be integrated.

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
QEMU target; it does not debug Linux REM processes. A separate native REM
process-debugger prototype was built and launched against a static REM test
program. `PTRACE_TRACEME` returned `ENOSYS` before the debugger could inspect
the initial registers. The tested syscall table has no dispatch case for
Linux syscall 117. Source inspection also shows the architecture has no
single-step hook (`arch_has_single_step()` therefore defaults false, making
generic `PTRACE_SINGLESTEP` fail with `EIO`) and the REM breakpoint exception
vector is not routed to a ptrace stop. The prototype and exact guest result
are recorded in [`REM_DEBUGGER.md`](REM_DEBUGGER.md).

**Required Linux behavior for ptrace compatibility:** implement the advertised
ptrace requests, return a coherent architectural register set, allow stopped
tracee memory reads/writes, and report execution stops consistently. An
architecture claiming `PTRACE_SINGLESTEP` must stop after one user
instruction. **Recommended:** define user breakpoint/trap delivery as a
debugger-visible signal/ptrace stop instead of an unhandled kernel exception;
the CPU may provide hardware stepping, or the kernel may implement a safe
software method. **Open question:** select and validate REM64's debug-exception
and step mechanism, including saved PC semantics for restart/resume. This is
a Linux software/debug contract, not evidence for a new instruction-set
requirement.

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

**REQUIRED by OpenSSH deployment configuration:** provide working user/group
lookup records for the login account and configured privilege-separation
account, plus the configured empty chroot directory with suitable ownership
and permissions. Runtime behavior on REM is **OPEN / unvalidated** because
the tested binaries fault before these paths. Whether this belongs in the
base image or an SSH package setup step is an **OPEN QUESTION**. See
[REM_OPENSSH_COMPATIBILITY.md](REM_OPENSSH_COMPATIBILITY.md).

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
