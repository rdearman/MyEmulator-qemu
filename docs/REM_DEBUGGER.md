# REM debugger

## State

A first native REM process-debugger prototype is at
[`toolchain/remdbg/remdbg.c`](../toolchain/remdbg/remdbg.c). It builds as a
static ELF32 REM executable with the repository GCC, REM musl CRT/libc, and
REM linker script. It is **not yet a usable debugger in the guest**: its first
launch test fails because `PTRACE_TRACEME` returns `ENOSYS`. No kernel, libc,
compiler, QEMU, or flight-image changes were made.

## Existing debugger reviewed: `mydebug`

The existing debugger is [`tools/mydebug`](../tools/mydebug), documented in
[`mydebug.md`](mydebug.md). It is a Python client for QEMU's newline-delimited
JSON QMP socket, with a structured `myemulator-debug` command backend. Its
intended flow is Emacs/editor → `mydebug` → QEMU; it does not use GDB's remote
protocol. Its command loop, register formatting, address expressions,
breakpoint IDs, and machine-readable request/response mode are reusable UX and
protocol patterns. Its working target is the earlier 8-bit MyEmulator CPU and
QEMU instance; it is not a native Linux-process debugger.

The 8-bit target QMP backend exposes registers, instruction/source stepping,
continue/stop/reset, execution breakpoints, memory read/write, disassembly,
and optional assembler debug-map symbols/locations. The tests are
[`tests/run-mydebug-tests.sh`](../tests/run-mydebug-tests.sh) and
[`tests/run-mydebug-halt-test.sh`](../tests/run-mydebug-halt-test.sh). The
separate REM32 QEMU backend in `qemu/hw/myemulator32/myemulator32-debug.c`
currently exposes register, guest-memory/physical-memory, IRQ, and NMI
operations; it does not provide a user-process debug interface.

## Native REM prototype

The prototype follows familiar `mydebug` command names but targets a Linux
REM process launched as `remdbg PROGRAM [ARG ...]`:

- `regs`: general registers, PC, SR, SP/LR/FP, and nearest ELF symbol;
- `mem ADDRESS LENGTH`: read tracee memory;
- `write ADDRESS BYTE`: modify tracee memory;
- `disasm [ADDRESS [COUNT]]`: decode the fixed-width REM32 instruction at
  the current PC or another address;
- `symbol ADDRESS`: find the nearest ELF32 static/dynamic function symbol;
- `continue`: resume and report exit, signal, or stop state;
- `step` / `next`: request `PTRACE_SINGLESTEP`;
- `break ADDRESS`: explain the current platform breakpoint limitation.

The implementation reads ELF32 little-endian symbol tables directly. It does
not shell out to `nm`, `readelf`, or `objdump`. The decoder follows the REM32
instruction fields used by the target disassembler. Build with:

```sh
make -C toolchain/remdbg \
  REM_CC=/path/to/myemulator2-elf-gcc \
  MUSL_PREFIX=/path/to/rem-musl-prefix \
  OUTPUT=/tmp/remdbg
```

The target compiler and musl prefix must be from the same validated REM ABI
tuple. The Makefile defaults to the in-image musl location for rebuilds from
inside REM.

## Qualification evidence and blocker

The prototype compiled with GCC SHA-256
`07ef3aaab8ce4ce47c0a093a4b7b2590e98c936e6e8a651d7261e0f07f0087fe`; the
final Makefile result `/tmp/remdbg-final` is REM ELF32 `ET_EXEC`, machine
`MyEmulator2`, entry `0x02000094`, SHA-256
`d52ea72951ee88ed8c9f5de5ad06a68759a0d09de8bdbdb19f958129329c7c2a`.
It was launched with a trivial static REM fixture (SHA-256
`1c84e53e073bbcf6fe3c52607055f81e8a532878520c7d42056e48f4a2628464`) in a
disposable copy of the root filesystem (SHA-256
`c469881cbc954556961868dc0bc2aad7258288ea261979cb94205a5cd90469ee`). Before
the debugger could obtain registers or reach its command prompt, the child
reported:

```text
remdbg: PTRACE_TRACEME: Function not implemented
```

The tested kernel SHA-256 is
`c77547060463da37cf9730e41e799c94998ad9f6e24e2c1a3282ee3299215a63`; QEMU
SHA-256 is
`0154db549ad01d0bcdfd79c0dde9544266fc14c75fe7825d14942ff47a75641a`. The
REM syscall table has no case for syscall 117 (`ptrace`), despite musl
providing its declaration and constants. Therefore register, memory, symbol,
disassembly, continue, and fault reporting are source-implemented but not
runtime-qualified. The initial `PTRACE_TRACEME` boundary prevents exercising
them in the guest.

There are two further source-level blockers for the requested execution
controls. Linux's generic ptrace path rejects `PTRACE_SINGLESTEP` with `EIO`
when `arch_has_single_step()` is false; the REM architecture does not define
that hook or stepping state. The REM `break` instruction raises exception
vector 13, which the current architecture exception dispatcher does not
convert into a ptrace stop (unhandled vectors reach the kernel panic path).
Adding a userspace command cannot provide those CPU/kernel stop semantics.

## Status / next work

- **Reviewed:** existing 8-bit QMP debugger architecture, command model, and
  automated tests.
- **Prototype built:** native REM process debugger source and static ELF.
- **Guest result:** blocked at `PTRACE_TRACEME` / `ENOSYS`; none of the
  debugger commands are accepted as guest-tested.
- **Required platform work for this design:** syscall 117 dispatch plus
  functional ptrace launch/attach and register/memory access; architecture
  single-step support; a defined user breakpoint exception-to-`SIGTRAP` and
  ptrace-stop path.
- **Not done:** kernel work is outside this assignment's explicit boundary.
  Until those interfaces exist, the full “load, step, inspect, debug” success
  criterion cannot be met by a native Linux-process debugger. The host-side
  QMP `mydebug` remains the qualified debugger for its original QEMU target.
