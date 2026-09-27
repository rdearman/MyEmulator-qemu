# CODEX #1 Handover

## Assignment

REM process creation syscall enablement and GCC runtime validation.

## Current status

**PARTIAL.** The original GCC process-launch failure was fixed at the musl
architecture layer. Native GCC now starts and reports its version inside REM,
but compiling a program still exposes a separate kernel/MMU/console fault.

## Proven findings

- Linux syscall 220 (`clone`) was already implemented in the MyEmulator2 kernel
  dispatcher and generic `sys_clone` path. No kernel syscall-table addition was
  needed.
- The immediate `gcc` failure was musl's
  `src/thread/clone.c`, which returned `-ENOSYS`.
- MyEmulator2 uses a C calling convention with the first arguments in `r1`–`r4`
  and additional arguments on the stack. This differs from the Linux syscall
  convention, which uses `r2`–`r7` after the syscall number in `r1`.
- The working wrapper must translate those conventions, pass the seventh
  `ctid` argument from the stack, align the child stack to 16 bytes, and
  preserve the compiler's callee-saved registers, including `r15`.
- A direct custom-stack clone probe passed with `CLONE_STACK_OK`.
- Ordinary `/bin/true` fork/exec continues to work.

## Permanent source change

Added:

```text
toolchain/musl/myemulator2/src/thread/myemulator2/clone.S
```

The wrapper implements musl `__clone` and performs the Linux syscall 220
translation. It is the main source change from this assignment.

## Disposable build artifacts

The disposable validation artifacts were built under `/tmp`:

```text
/tmp/rem-native-gcc-hosted2/
/tmp/rem-bash-diag/musl-clean-prefix/
/tmp/rem-gcc-rootfs.ext4
```

The tested GCC executable was staged at the corresponding disposable rootfs
path:

```text
/tmp/rem-native-gcc-hosted2/prefix/bin/myemulator2-linux-musl-gcc
```

Hashes from the final disposable run:

```text
clone.S       ae3d2657a530bb95e5254cf012e3c1c7e6aafbc94edac688be10ba3bbfebfe77
GCC xgcc      3337f64f52b95802e1fbd1362dac74c96e4b9972089e7297c240c3af404a8f2b
rootfs        f6f2d31c4bc6e0ae70c3c1670df5d737ba35bfe89fe7218800fbe146c35a696e
kernel        c77547060463da37cf9730e41e799c94998ad9f6e24e2c1a3282ee3299215a63
QEMU          0154db549ad01d0bcdfd79c0dde9544266fc14c75fe7825d14942ff47a75641a
```

## Validation

Passed:

```text
gcc --version                 PASS
direct custom-stack clone     PASS: CLONE_STACK_OK
/bin/true fork/exec           PASS
```

Not yet passing:

```text
gcc hello.c                   BLOCKED
g++ hello.cpp                 NOT COMPLETED
```

When GCC launches `cc1`, the tested historical kernel reaches:

```text
MyEmulator2 kernel fault:
pc=0x0010080c
addr=0xf0000001
cause=6
```

The PC resolves to `myemulator2_console_read_user`, performing an MMIO UART
status read. This occurs after process creation and is not evidence that
syscall 220 is still missing.

## Remaining investigation boundary

Investigate the post-`cc1` kernel/MMU/console fault. Likely areas include
address-space/MMIO mapping or page-table state after the compiler child runs,
but no permanent kernel fix has been established. Do not conflate this with
the fixed musl `clone` ENOSYS/ABI issue.

The production flight package was not modified. QEMU sessions started for the
validation were stopped.

## Architecture documentation

The process-creation lesson was appended to:

```text
docs/REM_ARCHITECTURE64.md
```

It records the need to define the C ABI and syscall ABI separately and to test
clone/fork/vfork/exec and `posix_spawn` before claiming a hosted compiler
runtime is usable.

## Follow-up: disposable console MMIO fix

The fault address `0xf0000001` was traced to the early-console status macro in
`linux/arch/myemulator2/kernel/console.c`. It used the physical UART address
directly while the MMU-enabled kernel requires the `MYEMU2_UART_VIRT` alias.
The status read now uses `MYEMU2_UART_VIRT + 1`. A clean disposable kernel
rebuilt with the known-good configuration (`cacb8510f24228722203d2271f5428c319da8d2652b42b2ca07d08906313af8c`) booted the disposable rootfs to `REM#` and `/bin/true` completed without that fault. A fresh native-GCC rootfs was not available after the earlier temporary artifacts expired, so `gcc hello.c` and C++ compilation remain unrerun after this fix.
