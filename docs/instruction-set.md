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
| 3 | ADD | immediate arithmetic form used for bring-up; final operand form pending |
| 4 | SUB | reserved for cleaned-up subtract definition |
| 5 | JAL | `LR = P+2`; target is `P+2 + sign_extend(imm8)*2` |
| 6 | BEQ | branch when `ZF=1`; target is `P+2 + sign_extend(imm8)*2` |
| 7 | BNE | branch when `ZF=0`; target is `P+2 + sign_extend(imm8)*2` |
| 8 | CMP | compare definition pending cleanup |
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

## HALT

`0xf080` (`1111 0000 1000 0000`) is operand-free `halt`. QEMU stops the virtual
CPU through its normal halt mechanism. The historical `jmp #0xff` convention is
not architectural. `J`, `JALR`, `RET`, and other future control-flow forms are
not defined by this specification.
