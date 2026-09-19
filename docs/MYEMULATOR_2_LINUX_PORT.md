# MyEmulator2 Linux port

Status: initial architecture bring-up in progress.

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
port. The current first milestone is kernel entry and early console output;
MMU page-table activation, full exception register frames, scheduler context
switching, userspace ELF setup, and initramfs are tracked next in the source
and this document.

The Linux-specific syscall convention will use the frozen C ABI argument
registers (`r1-r4` first, remaining arguments on the user stack) with the
syscall number in `r1`; this is a kernel/userspace convention, not a CPU ISA
change. It remains subject to a Linux port review before userspace ABI is
declared stable.
