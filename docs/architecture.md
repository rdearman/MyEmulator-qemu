# MyEmulator Architecture

This document defines the cleaned-up architecture implemented by QEMU. The
historical Python project at `rdearman/MyEmulator` is evidence and a source of
software examples, not an authority where it conflicts with this document.

## Machine model

MyEmulator is a single-CPU, 8-bit-data, 16-bit-address computer. It has four
four 8-bit data registers (`R0`-`R3`), an 8-bit status register (`S0`), four
16-bit address registers (`A0`-`A3`), 16-bit `LR`, 16-bit byte-addressed
program counter (`PC`), and a 16-bit byte-addressed stack pointer (`SP`). The
address space is 64 KiB.

Memory is byte addressed. Instructions are fixed-width 16-bit values occupying
two consecutive bytes; sequential `PC` advance is 2. Data loads and stores
access one byte. All 16-bit values are little-endian: the low byte is stored at
the lower address.

| State | Width | Reset source |
| --- | ---: | --- |
| `R0`-`R3` | 8 | zero |
| `A0`-`A3`, `LR` | 16 | zero |
| `S0` | 8 | zero; bits 0..3 are ZF/NF/CF/OF, bits 4..6 are IPL, bit 7 reserved |
| `SP` | 16 | `read16(0xfffc)` |
| `PC` | 16 | `read16(0xfffe)` |

The reset vectors are architectural. Initial QEMU testing maps flat RAM across
the full address space and places the little-endian vectors in its top four
bytes.

The Python post-branch increment, branch-label workaround, unsupported-opcode
double increment, `PC == 0xff` halt convention, old syscall meanings, and old
PUSH/POP side effects are historical bugs or compatibility behaviours. They are
not preserved. Banked RAM/EPROM is intended future hardware, but is deferred.

`LD` and `ST` use `A0`-`A3` plus a signed 8-bit displacement, allowing address
registers to reach the complete 16-bit address space. `LDA`, `GTA`, `MVA`, and
`ADA` manipulate the address-register file without changing S0. MVA also
supports LR and SP, but not PC. `GF` and `SF` transfer S0 through R0-R3.

`0x6` is a branch family: conditions `0x0` through `0x5` are
`BEQ`, `BNE`, `BLT`, `BGE`, `BLTU`, and `BGEU`, while `0x6` is unconditional
`BR`. `0x5` is `JAL`; these branches use signed
PC-relative instruction-unit displacements. `0x7` is the address-operation
family. `CMP` is a
register-to-register comparison which updates all four S0 flag bits without
storing a result. See [architecture-decisions.md](architecture-decisions.md)
for details.

`ADD`, `SUB`, and `CMP` update the S0 condition bits. `CMP` updates all four;
`LD`, `LI`, `ST`, `LDA`, `GTA`, `MVA`, `ADA`, `PUSH`, `POP`, branches, `JAL`,
and `HALT` leave them unchanged. `CF` for subtraction and comparison means no
borrow. `RET` sets `PC=LR`.

## Hardware interrupts

There are seven level-sensitive hardware interrupt inputs, IRQ1 through IRQ7,
with IRQ7 highest priority. The three-bit IPL field is S0 bits 4..6. An IRQ is
eligible only when `IRQ level > IPL`; therefore IPL=7 masks all ordinary IRQs.
If several eligible inputs are asserted, the highest numbered one is accepted.

Interrupts are accepted only between complete instructions. Entry saves the
next PC and the complete pre-interrupt S0, sets live IPL to the accepted level,
and loads the handler PC from the little-endian vector table:

| IRQ | Vector |
| --- | --- |
| IRQ1 | `0xffee-0xffef` |
| IRQ2 | `0xfff0-0xfff1` |
| IRQ3 | `0xfff2-0xfff3` |
| IRQ4 | `0xfff4-0xfff5` |
| IRQ5 | `0xfff6-0xfff7` |
| IRQ6 | `0xfff8-0xfff9` |
| IRQ7 | `0xfffa-0xfffb` |

The stack frame follows the existing descending PUSH convention. Starting with
SP=`S`, entry stores PC low at `S-1`, PC high at `S-2`, and S0 at `S-3`, leaving
SP=`S-3`. RTI reads S0, then PC high, then PC low, incrementing SP after each
byte; it restores both values exactly and leaves SP at `S`.

R0-R3, A0-A3, LR, and SP are not automatically saved. Handlers must preserve
any working registers they modify. Nested higher-priority interrupts create
another frame; lower or equal levels remain blocked. IRQ inputs are level
sensitive, so a source that remains asserted can be accepted again after RTI
if the restored IPL permits it. A halted CPU wakes for an eligible asserted IRQ;
a masked IRQ does not wake it. Future QEMU devices should call the CPU-side
per-level assert/deassert interface and deassert their level after servicing it.
