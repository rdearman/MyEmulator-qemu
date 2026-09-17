# QEMU Implementation Plan

MyEmulator is a genuine QEMU target and machine. The Python project remains
read-only historical evidence.

AVR is the main structural reference because it is a compact 8-bit softmmu CPU
with 16-bit instruction handling and small CPU state. RX is the secondary
reference for a modern translator loop and compact machine/image wiring.

The repository overlay contains `target/myemulator` and `hw/myemulator`: a TCG
CPU, QOM machine, 64 KiB RAM, raw `-kernel` loading, monitor register dump, and
GDB register hooks. Fetch is explicit byte-addressed, two-byte little-endian
decode. Reset reads little-endian vectors at `0xfffc` and `0xfffe`. `LI`,
immediate `ADD`, register-to-register `CMP`, the six conditional plus one
unconditional `0x6` branches, `JAL`, `RET`, address/status-register operations
under `0x7`, displacement `LD`/`ST`,
mask-based `PUSH`/`POP`, and `0xf080 HALT` are the current defined bring-up
subset. The debugger exposes R0-R3 as 8-bit and A0-A3, LR, SP, and PC as 16-bit.
Illegal encodings stop visibly.

## Staged work

1. Add remaining CPU instructions and proper exceptions.
2. Add differential tests and a corrected assembler/image tool.
3. Add ROM/EPROM, console, storage, timers, and banking as their architecture
   is specified.
4. Boot machine software through the new image format.

The initial milestone ends at `LI; ADD; HALT`, with `R0=0x05` visible through
`info registers`. The GDB core-register order is R0-R3, A0-A3, LR, SP, PC, S0.

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
