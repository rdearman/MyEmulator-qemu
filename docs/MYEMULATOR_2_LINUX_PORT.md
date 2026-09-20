# MyEmulator2 Linux port

Status: kernel boot, serial TTY, user ELF execution, basic process
creation/exit/wait and a small hosted C syscall runtime are verified; a full
libc and BusyBox remain future work.

The selected reproducible baseline is Linux **6.12.1**, downloaded from
`https://cdn.kernel.org/pub/linux/kernel/v6.x/linux-6.12.1.tar.xz` and checked
against the SHA-256 recorded by `toolchain/scripts/fetch-linux.sh`. The Linux
source tree and all generated output live under the ignored `.linux-build/`
directory; only the maintained `linux/arch/myemulator2/` overlay is tracked.

## Current machine contract

The 2.0 machine has 16 MiB default RAM, physical reset words at `0x400`
(initial SSP) and `0x404` (initial PC), and ELF32 loading through QEMU's
generic ELF loader. The MyEmulator2 console device is mapped at
`0xF0000000-0xF000000F`:

| Offset | Meaning |
| ---: | --- |
| 0 | RX/TX data, 8-bit |
| 1 | status: bit 0 RX ready, bit 1 TX ready |
| 2 | control: bit 0 RX IRQ enable |
| 3 | IRQ status: bit 0 RX pending |

Console input/output is connected to QEMU `-serial` chardev 0 and RX uses
IRQ4. The architected TIME/TIMECMP timer remains IRQ1. These are machine
device assignments inside the ABI-reserved MMIO window; they do not change
the CPU architecture.

## Reproducible commands

```sh
toolchain/scripts/fetch-linux.sh
toolchain/scripts/configure-linux.sh
toolchain/scripts/build-linux.sh
toolchain/scripts/run-linux.sh
```

The overlay is deliberately small and is not yet a claim of a complete Linux
port. Kernel entry, MMU page-table activation, full exception frames, timer
scheduling, userspace ELF setup, initramfs, the serial TTY, and a basic
clone/exit/wait4 lifecycle are exercised by the runtime tests. A hosted libc,
signals, the complete syscall table and BusyBox are not yet implemented.

The Linux-specific syscall convention uses `r1` for the syscall number and
result, with user arguments in `r2-r7` (the first four follow the frozen ABI;
the additional two are used by six-argument Linux calls such as `mmap`). The
numbers currently follow the Linux asm-generic/RISC-V-style 32-bit table used
by the bring-up fixtures: read/write 63/64, clone 220, execve 221, wait4 260,
and the standard openat/descriptor/memory calls. This is a Linux userspace
convention, not a CPU ISA change; the architecture syscall table and libc
ABI still need completion before it is declared stable.

The verified process-management regression is:

```sh
python3 toolchain/scripts/test-linux-process.py
```

The hosted C runtime and shell regressions are:

```sh
python3 toolchain/scripts/test-linux-minilibc.py
python3 toolchain/scripts/test-linux-minilibc-shell.py
```

The first userspace-C failure was in the exception-return trampoline, not in
the compiler's stack setup: the trampoline temporarily stored its returned
`pt_regs *` in the native frame's Cause word, which the frame-construction
helper correctly cleared. The pointer is now kept in ABI-preserved `r5`.
Early `enter_lazy_tlb()` also uses an explicit paging-ready flag, avoiding a
premature switch to the not-yet-built swapper tables during early boot.
