# QEMU Implementation Plan

MyEmulator is a genuine QEMU target and machine. The Python project remains
read-only historical evidence.

AVR is the main structural reference because it is a compact 8-bit softmmu CPU
with 16-bit instruction handling and small CPU state. RX is the secondary
reference for a modern translator loop and compact machine/image wiring.

The repository overlay contains `target/myemulator` and `hw/myemulator`: a TCG
CPU, QOM machine, 64 KiB RAM, raw `-kernel` loading, monitor register dump, and
GDB register hooks. Fetch is explicit byte-addressed, two-byte, provisional
little-endian decode. Reset reads `0xfffc` and `0xfffe`. `LI`, immediate `ADD`,
and `0xf080 HALT` prove the first milestone. Illegal encodings stop visibly.

## Staged work

1. Close byte order, branch encoding, and PUSH/POP ordering.
2. Add remaining CPU instructions and proper exceptions.
3. Add differential tests and a corrected assembler/image tool.
4. Add ROM/EPROM, console, storage, timers, interrupts, and banking as their
   architecture is specified.
5. Boot machine software through the new image format.

The initial milestone ends at `LI; ADD; HALT`, with `R0=0x05` visible through
`info registers`.
