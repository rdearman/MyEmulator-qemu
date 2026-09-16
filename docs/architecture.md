# MyEmulator Architecture

This document defines the cleaned-up architecture implemented by QEMU. The
historical Python project at `rdearman/MyEmulator` is evidence and a source of
software examples, not an authority where it conflicts with this document.

## Machine model

MyEmulator is a single-CPU, 8-bit-data, 16-bit-address computer. It has four
8-bit general registers (`R0`-`R3`), an 8-bit link register (`LR`), a 16-bit
byte-addressed program counter (`PC`), a 16-bit byte-addressed stack pointer
(`SP`), and `ZF`, `NF`, `OF`, and `CF` status flags. The address space is 64 KiB.

Memory is byte addressed. Instructions are fixed-width 16-bit values occupying
two consecutive bytes; sequential `PC` advance is 2. Data loads and stores
access one byte. All 16-bit values are little-endian: the low byte is stored at
the lower address.

| State | Width | Reset source |
| --- | ---: | --- |
| `R0`-`R3`, `LR` | 8 | zero |
| `ZF`, `NF`, `OF`, `CF` | 1 | clear |
| `SP` | 16 | `read16(0xfffc)` |
| `PC` | 16 | `read16(0xfffe)` |

The reset vectors are architectural. Initial QEMU testing maps flat RAM across
the full address space and places the little-endian vectors in its top four
bytes.

The Python post-branch increment, branch-label workaround, unsupported-opcode
double increment, `PC == 0xff` halt convention, old syscall meanings, and old
PUSH/POP side effects are historical bugs or compatibility behaviours. They are
not preserved. Banked RAM/EPROM is intended future hardware, but is deferred.

`0x6` is a conditional branch family: conditions `0x0` through `0x5` are
`BEQ`, `BNE`, `BLT`, `BGE`, `BLTU`, and `BGEU`. `0x5` is `JAL`; both use signed
PC-relative instruction-unit displacements. `0x7` is reserved. `CMP` is a
register-to-register comparison which updates all four flags without storing a
result. See [architecture-decisions.md](architecture-decisions.md) for details.

`ADD`, `SUB`, and `CMP` update the condition flags. `CMP` updates all four;
`LD`, `LI`, `ST`, `PUSH`, `POP`, branches, `JAL`, and `HALT` leave them
unchanged. `CF` for subtraction and comparison means no borrow.
