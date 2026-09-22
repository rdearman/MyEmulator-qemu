# REM Linux port plan

Status: reconnaissance / not implemented

This plan is for a future Linux architecture port. It does not add
`arch/myemulator2/` and does not claim that the current freestanding GCC
backend is Linux-ready.

## Reconnaissance basis

The plan follows the structure used by current Linux release trees (the
generic architecture contract in `arch/Kconfig`, `arch/*/Kconfig`, the
architecture Makefile, `kernel/head.S`, `kernel/entry.S`, traps, process,
signal, time, MMU, and linker-script files). The exact Linux release should
be pinned when implementation begins; the current project ABI is independent
of a particular kernel release.

## Required implementation order

1. Add `arch/myemulator2/Kconfig`, `Kconfig.cpu`, `Makefile`, and a linker
   script with the frozen ELF32 target and ABI. Add `boot/head.S` to consume
   the physical reset words at `0x400/0x404`, establish the supervisor stack,
   and enter the C kernel entry point.
2. Implement early exception/vector setup under `kernel/entry.S` and
   `kernel/traps.c`. Install VBR, exception-frame decoding, RFE return paths,
   user/supervisor transitions, and the syscall/breakpoint paths.
3. Bring up an early console using the future UART/console device; this is
   needed before diagnosing MMU and scheduler failures.
4. Implement `mm/init.c`, page-table creation, context switch PTBR updates,
   TLB flushes, user/kernel permissions, and `A/D` handling. Keep the first
   boot physical-only with MMU disabled if needed, then enable paging for the
   kernel/userspace transition.
5. Implement `kernel/process.c`, `kernel/ptrace.c`, signal return, and the
   saved GPR/USP/SSP/PC/SR context required by software context switching.
6. Implement the TIME/TIMECMP clocksource and IRQ1 clock-event driver, then
   scheduler ticks and pre-emption. NMI and all seven maskable IRQ vectors
   must retain their frozen priority semantics.
7. Add `init/main.c` integration and an initramfs-only userspace boot. This
   permits a BusyBox shell before a block device exists.
8. Add the block-device driver and ext4 only after the initramfs path is
   stable. The block device is not part of the CPU ABI and must be shared by
   RIKMON and Linux through a separately reviewed device ABI.

## Compiler requirements

Linux will require reliable GCC output for the frozen register ABI, stack
alignment, 64-bit integer expansion, inline assembly constraints, atomic CAS
loops, `__builtin_*` operations, switch lowering, and exception-safe volatile
MMIO access. The current GCC milestone does not yet provide a Linux syscall
ABI, kernel inline-assembly contract, dynamic TLS model, or hosted headers.

## Hardware still required

The CPU already supplies privilege, MMU/TLB, exception frames, IRQ priority,
NMI, TIME/TIMECMP, TP, and CAS. A Linux boot needs at least a reviewed UART
or console device, timer interrupt wiring, and a block device for an ext4
root filesystem. Initramfs can provide the first shell without the block
device. No device is implemented by this document.
