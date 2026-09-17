# Instruction Set

The ISA uses fixed-width 16-bit instructions in byte-addressed memory. The
primary opcode is a four-bit group; unused encodings within groups are reserved.

## Instruction encoding

```text
15            12 11        10 9         8 7                         0
+---------------+------------+-----------+----------------------------+
| primary op    |    Rd      |    Rn     | operand / sub-op / mask    |
|    4 bits     |   2 bits   |  2 bits   |          8 bits             |
+---------------+------------+-----------+----------------------------+
```

For data-register fields, `00=R0` through `11=R3`. Address-register fields use
the same two-bit numbering, with `00=A0` through `11=A3`. Instructions occupy
`PC` and `PC+1`; ordinary execution advances `PC` by 2. All 16-bit values use
little-endian byte order. A field named `Rd` or `Rn` selects an 8-bit data
register; `An` selects a 16-bit address register.

## Primary opcode groups

| Op | Group | Current definition |
| --- | --- | --- |
| 0 | LD | `0000 Rd An disp8`; `Rd = mem8[A[n] + sign_extend(disp8)]` |
| 1 | LI | `0001 Rd imm8`; `Rd = imm8` |
| 2 | ST | `0010 Rd An disp8`; `mem8[A[n] + sign_extend(disp8)] = Rd` |
| 3 | ADD | `0011 Rd Rn imm8`; `Rd = Rn + imm8` |
| 4 | SUB | `0100 Rd Rn imm8`; `Rd = Rn - imm8` |
| 5 | BL | `0101 disp8`; `LR = P+2`; target is `P+2 + sign_extend(imm8)*2` |
| 6 | BRANCH | condition field: `0=BEQ`, `1=BNE`, `2=BLT`, `3=BGE`, `4=BLTU`, `5=BGEU`, `6=BR` |
| 7 | ADDRESS | `ADA`, `LDA`, `GTA`, `MVA`, `JA`, `JLA`, `GF`, and `SF`; unused subencodings reserved |
| 8 | CMP | `1000 Rd Rn 00`; 8-bit subtraction for flags; result discarded |
| 9 | AND | `1001 Rd Rn imm8`; `Rd = Rn & imm8` |
| A | OR | `1010 Rd Rn imm8`; `Rd = Rn \| imm8` |
| B | XOR | `1011 Rd Rn imm8`; `Rd = Rn ^ imm8` |
| C | SHL | `1100 Rd Rn count8`; logical left shift |
| D | SHR | `1101 Rd Rn count8`; logical right shift |
| E | PUSH | register-mask form, bits 0..4 defined below |
| F | POP / system | `1111 0000 mask8` is POP for masks `0x00-0x1f`; `0xf040` is RET, `0xf060` is RTI, and `0xf080` is HALT |

All sixteen values are groups, not a claim that each group has only one
instruction. Reserved encodings must remain unused until assigned deliberately.

The `0x77xx` subgroup is the extended register-register ALU family. Its low
byte is `oooo dd nn`: `oooo` selects the operation and `dd`/`nn` select R0-R3.
Selectors `0=ADD`, `1=SUB`, `2=AND`, `3=OR`, `4=XOR`, `5=SHL`, and `6=SHR`.
These are destructive two-operand forms: `add rd,rn` computes `rd = rd + rn`.
Selectors `7` through `f` are reserved.

The free low sub-encodings of the `0x75` address-family subgroup define
indirect control transfers. `JA An` is `0x7500 | (An << 6)` and `JLA An` is
`0x7500 | (An << 6) | 1`; all other low six-bit values are reserved. JA loads
PC from An without changing LR or S0. JLA also writes LR with the following
instruction address, but otherwise leaves S0 unchanged.

The immediate forms retain all 256 operand values. Their syntax is
`op rd,rn,#imm8`; the register forms use `op rd,rn`.

`LD` and `ST` use a signed displacement even though the encoded byte is not
otherwise interpreted as signed. `LI`, the immediate ALU operations, and shift
counts accept every unsigned byte value. `CMP` requires its low two operand
bits to be zero; other encodings in the `0x8` group are reserved.

## PUSH and POP

```text
bit 0 = R0    bit 1 = R1    bit 2 = R2    bit 3 = R3    bit 4 = LR
```

`push {r0,r2,lr}` transfers exactly those registers; `pop {r0,r2,lr}` restores
exactly those registers. There is no implicit `Rd`/`Rn` transfer or duplication.
PUSH processes selected registers in ascending order (`R0` through `LR`),
pre-decrementing `SP` before each byte store. R0-R3 transfer one byte; LR
transfers two bytes little-endian (low byte first on PUSH). POP processes
selected registers in reverse order (`LR` through `R0`), reading at `SP` then
incrementing it; LR reconstructs its high byte followed by its low byte.
Bit 7 is not a syscall selector; historical POP/syscall use is excluded.

## ALU operations

ADD, SUB, AND, OR, XOR, SHL, and SHR all operate on 8-bit data registers.
Immediate forms compute `Rd = Rn op imm8`. Register forms in `0x77xx` compute
`Rd = Rd op Rn`.

ADD and SUB use the arithmetic flag rules below. AND, OR, and XOR recompute ZF
and NF and clear CF and OF. SHL and SHR are logical shifts. An immediate or
register shift count of 0 leaves the value unchanged and preserves CF. Counts
from 1 through 7 shift normally and set CF to the last bit shifted out. Counts
of 8 or greater produce zero and set CF to zero. Shifts recompute ZF and NF and
clear OF.

## CMP and flags

`cmp rA, rB` computes an unsigned 8-bit subtraction and discards its result.
`ZF` is set when the low byte is zero, `NF` is its bit 7, `CF` is 1 when
unsigned `rA >= rB` (no borrow), and `OF` is signed 8-bit subtraction overflow.
Thus signed less-than is `NF != OF`, while unsigned less-than is `CF == 0`.

Opcode `0x6` uses bits 11..8 as its condition and bits 7..0 as a signed
instruction-unit displacement. For instruction PC `P`, `target = P + 2 +
sign_extend(displacement8) * 2`.

| Condition | Mnemonic | Taken when |
| ---: | --- | --- |
| `0x0` | BEQ | `ZF == 1` |
| `0x1` | BNE | `ZF == 0` |
| `0x2` | BLT | `NF != OF` |
| `0x3` | BGE | `NF == OF` |
| `0x4` | BLTU | `CF == 0` |
| `0x5` | BGEU | `CF == 1` |

Conditions `0x7` through `0xf` are reserved. `BR` is unconditional and uses
the same signed instruction-unit displacement as the conditional branches.

## Address operations

Opcode `0x7` uses disjoint subgroups. Low unused bits are reserved so every
R/A selector combination remains available:

```text
ADA:  0111 An 00 imm8                 = 0x7000 | (An << 10) | imm8
LDA:  0111 0001 An Rx Ry 00           = 0x7100 | (An << 6) | (Rx << 4) | (Ry << 2)
GTA:  0111 0010 An Rx Ry 00           = 0x7200 | (An << 6) | (Rx << 4) | (Ry << 2)
MVA:  0111 0011 00 DDD SSS           = 0x7300 | (D << 3) | S
```

`LDA An,Rx,Ry` forms `A[An] = (R[Rx] << 8) | R[Ry]`.
`GTA Rx,Ry,An` is the inverse. `MVA D,S` copies one 16-bit register. MVA
selectors are `0=A0`, `1=A1`, `2=A2`, `3=A3`, `4=LR`, and `5=SP`; selectors
`6` and `7`, plus encodings with bits 7..6 set, are reserved/illegal. PC is
not an MVA operand. The revised MVA encoding replaces the old two-bit
A-register-only selectors so all six permitted registers are available as
both source and destination.
`ADA An,#imm8` adds signed `imm8` with 16-bit wrapping. None modifies flags.
`GF Rn` uses `0x7600 | (Rn << 2)` and copies the complete S0 byte into Rn.
`SF Rn` uses `0x7601 | (Rn << 2)` and copies the complete Rn byte into S0.
Only R0-R3 are valid GF/SF operands; the other `0x76` encodings are reserved.
All remaining `0x7` encodings are reserved.

The `0x77xx` register ALU family is selected by sub-op `0x7` within this
primary group; it is not an address-register operation.

## S0 and flag writes

S0 is an 8-bit architectural register. Bit 0 is ZF, bit 1 is NF, bit 2 is
CF, and bit 3 is OF. Bits 4..6 are the unsigned IPL field; bit 7 is reserved.
SF/GF preserve and expose the complete byte. Reset sets S0 to `0x00`.

Arithmetic and logical instructions update only the defined flag bits. IPL and
reserved bit 7 are preserved by ordinary ALU flag updates. `SF` is the
intentional mechanism for writing all eight S0 bits; bit 7 remains reserved.

| Instruction | ZF | NF | CF | OF |
| --- | --- | --- | --- | --- |
| `ADD` | result is zero | result bit 7 | unsigned carry | signed 8-bit addition overflow |
| `SUB` | result is zero | result bit 7 | no borrow (`lhs >= rhs`) | signed 8-bit subtraction overflow |
| `CMP` | result is zero | result bit 7 | no borrow (`rA >= rB`) | signed 8-bit subtraction overflow |
| `AND`, `OR`, `XOR` | result is zero | result bit 7 | cleared | cleared |
| `SHL`, `SHR` | result is zero | result bit 7 | shifted-out bit, or preserved/zero as specified above | cleared |
| `LD`, `LI`, `ST`, `LDA`, `GTA`, `MVA`, `ADA`, `GF`, `PUSH`, `POP`, branches, `BL`, `RET`, `HALT` | unchanged | unchanged | unchanged | unchanged |
| `SF` | writes S0 bit 0 | writes S0 bit 1 | writes S0 bit 2 | writes S0 bit 3 |

## HALT

`0xf040` is operand-free `ret`; it sets `PC = LR` and changes no other state.
`0xf060` is operand-free `rti`; it restores the saved S0 and PC from the
hardware interrupt frame and changes no other state.
Instruction addresses are even. An odd target from JA, JLA, or RET raises the
synchronous instruction-alignment exception through vector `0xffec-0xffed`.
The faulting instruction PC and S0 are saved in the normal descending frame:
PC low at `SP-1`, PC high at `SP-2`, and S0 at `SP-3`; RTI restores it. The
live S0/IPL is unchanged by this exception. An odd PC found in an RTI frame
raises the same exception for the RTI instruction before that frame is
consumed, allowing a handler to repair the saved state. An odd reset/vector
target halts the CPU deterministically rather than causing recursive entry.
`0xf080` (`1111 0000 1000 0000`) is operand-free `halt`. QEMU stops the virtual
CPU through its normal halt mechanism. The historical `jmp #0xff` convention is
not architectural. `J` is not defined by this specification.

## Complete current mnemonic set

The assembler and QEMU disassembler currently define exactly these mnemonics:

```text
ld st li add sub and or xor shl shr cmp
bl beq bne blt bge bltu bgeu br
lda gta mva ada gf sf
push pop ret rti halt
```

Assembler pseudo-operations are `.byte`, `.word`, `.ascii`, `.asciz`, `.org`,
and `la`; they do not add guest instructions. Undefined or reserved encodings
are illegal to execute and are printed by the disassembler as `.word 0xNNNN`.
