# QEMU Implementation Plan

MyEmulator is a genuine QEMU target and machine. The Python project remains
read-only historical evidence.

AVR is the main structural reference because it is a compact 8-bit softmmu CPU
with 16-bit instruction handling and small CPU state. RX is the secondary
reference for a modern translator loop and compact machine/image wiring.

The repository overlay contains `target/myemulator` and `hw/myemulator`: a TCG
CPU, QOM machine, split RAM/MMIO/firmware-ROM map, raw `-kernel` development
loading, external `-bios` firmware loading, native debugger control, and
disassembly. Fetch is explicit byte-addressed, two-byte little-endian decode.
Reset reads little-endian vectors at `0xfffc` and `0xfffe`. The current ISA
includes LD/ST, LI, immediate and `0x77xx` register-register ALU operations,
CMP, six conditional branches plus BR, BL, LDA/GTA/MVA/ADA, GF/SF, PUSH/POP,
RET, RTI, and HALT. Illegal encodings stop visibly.

## Historical implementation plan

The original bring-up plan is retained here for historical context. ROM, console,
floppy storage, assembler image tooling, interrupts, and the native debugger
are now implemented; banking and broader exception handling remain
future work.

The original `LI; ADD; HALT` milestone is retained as a regression test. The
native debugger register order is R0-R3, A0-A3, LR, SP, PC, S0. Stock GDB is
not the supported debugger for this fictional architecture.

The target exposes IRQ1-IRQ7 as level-sensitive CPU inputs through
`myemulator_cpu_set_irq()`. The `MYEMULATOR_IRQ_MASK` environment variable is
a bring-up/test injection for initially asserted levels; it is not an interrupt
controller. Future QEMU devices should assert and deassert their level through
the same CPU-side interface.

`MYEMULATOR_IRQ_ONESHOT` and `MYEMULATOR_IRQ_AFTER` are test-only injection
controls for consuming one asserted level or asserting one additional level
after entry; they do not change the architectural level-sensitive device API.

`MYEMULATOR_INITIAL_S0` and `MYEMULATOR_INITIAL_SP` are test-only controls
used to establish interrupt-mask and safe-stack scenarios before execution.

## Build and test workflow

The repository is an overlay rather than a complete QEMU source tree. Use
[`docs/qemu-build.md`](qemu-build.md) and
`qemu/tools/build-myemulator.sh --test` to clone the pinned QEMU source,
apply the overlay, build `qemu-system-myemulator`, and run the CPU tests
against that binary.
