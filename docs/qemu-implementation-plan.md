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
immediate `ADD`, `BEQ`, `BNE`, `JAL`, mask-based `PUSH`/`POP`, and `0xf080 HALT`
are the current defined bring-up subset. Illegal encodings stop visibly.

## Staged work

1. Add remaining CPU instructions and proper exceptions.
2. Add differential tests and a corrected assembler/image tool.
3. Add ROM/EPROM, console, storage, timers, interrupts, and banking as their
   architecture is specified.
5. Boot machine software through the new image format.

The initial milestone ends at `LI; ADD; HALT`, with `R0=0x05` visible through
`info registers`.
