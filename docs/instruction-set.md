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

`Rd` and `Rn` encode `00=R0`, `01=R1`, `10=R2`, `11=R3`. `LR` is selected only
by the PUSH/POP mask. Instructions occupy `PC` and `PC+1`; ordinary execution
advances `PC` by 2. All 16-bit values use little endian byte order.

## Primary opcode groups

| Op | Group | Current definition |
| --- | --- | --- |
| 0 | LD | `Rd = mem[Rn]` |
| 1 | LI | `Rd = imm8` |
| 2 | ST | `mem[Rn] = Rd` |
| 3 | ADD | immediate arithmetic form used for bring-up |
| 4 | SUB | reserved for cleaned-up subtract definition |
| 5 | JAL | `LR = P+2`; target is `P+2 + sign_extend(imm8)*2` |
| 6 | BRANCH | condition field: `0=BEQ`, `1=BNE`, `2=BLT`, `3=BGE`, `4=BLTU`, `5=BGEU` |
| 7 | RESERVED | intentionally unassigned |
| 8 | CMP | register-to-register subtraction for flags; result discarded |
| 9 | AND | reserved until destination/operand form is specified |
| A | OR | reserved until destination/operand form is specified |
| B | XOR | reserved until destination/operand form is specified |
| C | SHL | reserved until shift encoding is specified |
| D | SHR | reserved until shift encoding is specified |
| E | PUSH | register-mask form, bits 0..4 defined below |
| F | POP / extensions | POP mask form; `0xf080` is HALT |

All sixteen values are groups, not a claim that each group has only one
instruction. Reserved encodings must remain unused until assigned deliberately.

## PUSH and POP

```text
bit 0 = R0    bit 1 = R1    bit 2 = R2    bit 3 = R3    bit 4 = LR
```

`push {r0,r2,lr}` transfers exactly those registers; `pop {r0,r2,lr}` restores
exactly those registers. There is no implicit `Rd`/`Rn` transfer or duplication.
PUSH processes selected registers in ascending order (`R0` through `LR`),
pre-decrementing `SP` before each byte store. POP processes selected registers
in reverse order (`LR` through `R0`), reading at `SP` then incrementing it.
Bit 7 is not a syscall selector; historical POP/syscall use is excluded.

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

Conditions `0x6` through `0xf` are reserved.

## Flag writes

| Instruction | ZF | NF | CF | OF |
| --- | --- | --- | --- | --- |
| `ADD` | result is zero | result bit 7 | unsigned carry | signed 8-bit addition overflow |
| `SUB` | result is zero | result bit 7 | no borrow (`lhs >= rhs`) | signed 8-bit subtraction overflow |
| `CMP` | result is zero | result bit 7 | no borrow (`rA >= rB`) | signed 8-bit subtraction overflow |
| `LD`, `LI`, `ST`, `PUSH`, `POP`, branches, `JAL`, `HALT` | unchanged | unchanged | unchanged | unchanged |

## HALT

`0xf080` (`1111 0000 1000 0000`) is operand-free `halt`. QEMU stops the virtual
CPU through its normal halt mechanism. The historical `jmp #0xff` convention is
not architectural. `J`, `JALR`, `RET`, and other future control-flow forms are
not defined by this specification.
