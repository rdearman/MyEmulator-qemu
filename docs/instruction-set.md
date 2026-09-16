# Instruction Set

This is the implemented instruction set of `rdearman/MyEmulator` at commit
`b6141f978fc44537a7a5b3ea53c9250cf7699e3d`.

## Encoding

All currently executed instructions are 16-bit words. Current evidence from the
design notes, assembler, and emulator agrees on this top-level layout:

```
 15          12 11      10 9        8 7                       0
+-------------+----------+----------+-------------------------+
| opcode[3:0] | Rd[1:0]  | Rn[1:0]  | operand / imm / mask[7:0]|
+-------------+----------+----------+-------------------------+
```

Field meanings currently understood:

| Bits | Field | Meaning |
| --- | --- | --- |
| `15..12` | Primary opcode | One of sixteen primary opcodes, `0x0` through `0xf`. |
| `11..10` | `Rd` | Two-bit destination/general register selector: `00=R0`, `01=R1`, `10=R2`, `11=R3`. |
| `9..8` | `Rn` | Two-bit source/address/general register selector: `00=R0`, `01=R1`, `10=R2`, `11=R3`. |
| `7..0` | Operand | Immediate value, branch target, register-list mask, or secondary selector depending on opcode. |

`LR` is not addressable through `Rd` or `Rn`; it is accessed by `JMP` and by
stack/register-list masks.

The byte-vs-word meaning of instruction addresses remains unresolved. See
`docs/architecture-decisions.md`.

## Opcodes

| Opcode | Mnemonic | Status | Implemented behaviour / notes |
| --- | --- | --- | --- |
| `0x0` | `LD` | ARCHITECTURAL opcode | Python: `Rd = mem[Rn]`; sets `ZF` from `Rd`. |
| `0x1` | `LI` | ARCHITECTURAL opcode | Python: `Rd = operand`; flags unchanged. |
| `0x2` | `ST` | ARCHITECTURAL opcode | Python: `mem[Rn] = Rd & 0xff`; flags unchanged. |
| `0x3` | `ADD` | ARCHITECTURAL opcode | Python: `Rd = Rn + operand` for assembled programs. Sets `ZF`, `OF`, `CF`. Documentation describes register-register add. |
| `0x4` | `SUB` | ARCHITECTURAL opcode | Python: `Rd = Rn - operand` for assembled programs. Sets `ZF`, `OF`, `CF`. Documentation describes register-register subtract. |
| `0x5` | `JMP` | ARCHITECTURAL opcode | Intended: jump to target, update `LR`. Python has extra post-jump increment; QEMU should not preserve that bug. |
| `0x6` | `BEQ` | ARCHITECTURAL opcode | Intended: branch to target when `ZF` is set. Python has extra post-branch increment. |
| `0x7` | `BNE` | ARCHITECTURAL opcode | Intended: branch to target when `ZF` is clear. Python has extra post-branch increment. |
| `0x8` | `CMP` | ARCHITECTURAL opcode | Python compares `Rd` against `Rn`, except `Rn == 0` encodes immediate compare. Sets `ZF`, `OF`, `CF`. |
| `0x9` | `AND` | ARCHITECTURAL opcode, semantics partly unresolved | Documentation says destination register; Python stores into `R0` regardless of `Rd`. |
| `0xa` | `OR` | ARCHITECTURAL opcode, semantics partly unresolved | Documentation says destination register; Python stores into `R0` regardless of `Rd`. |
| `0xb` | `XOR` | ARCHITECTURAL opcode, semantics partly unresolved | Documentation says destination register; Python stores into `R0` regardless of `Rd`. |
| `0xc` | `SHL` | ARCHITECTURAL opcode | Python: `Rd = (Rn << operand) & 0xff`; sets `ZF`, `OF`, clears `CF`. |
| `0xd` | `SHR` | ARCHITECTURAL opcode | Python: `Rd = (Rn >> operand) & 0xff`; sets `ZF`, `OF` from old bit 7, clears `CF`. |
| `0xe` | `PUSH` | ARCHITECTURAL opcode, mask semantics partly unresolved | Register-list stack operation using operand mask; Python also pushes `Rd` unconditionally. |
| `0xf` | `POP` / secondary space | ARCHITECTURAL opcode, bit-7 escape UNRESOLVED | Register-list pop when operand bit 7 is clear; bit 7 selects syscall/IRC path in assembler and Python. |

All 16 primary opcode values are currently assigned. There are no unused primary
opcodes in the observed ISA, which supports the theory that `PUSH`/`POP`
operand bits may have been used deliberately as secondary opcode space.

## Arithmetic Flags

`ADD`:

- `CF = result > 255`.
- `OF = result > 127 or result < -128`.
- `Rd = result & 0xff`.
- `ZF = Rd == 0`.

`SUB`:

- `OF = result > 127 or result < -128` before masking.
- `Rd = result & 0xff`.
- `ZF = Rd == 0`.
- `CF = Rd < Rn` after masking. This is not a conventional borrow test.

`CMP`:

- Computes `destination_value - source_value`.
- `ZF = result == 0`.
- `OF = result > 32767 or result < -32768`.
- `CF = destination_value < source_value`.

## Addressing Modes

Implemented effective modes:

- Immediate: 8-bit operand field.
- Register: `Rd`, `Rn`.
- Register indirect memory: `LD Rd, [Rn]`, `ST Rd, [Rn]`.
- Assembler-only immediate memory syntax: `LD/ST Rd, [#imm]` encodes `Rn = 0`
  and operand `imm`, but the emulator ignores operand for `LD/ST`; it still
  uses the contents of `R0` as the address. This is a discrepancy.
- Branch target: 8-bit operand field.

There is no implemented indexed addressing, absolute 16-bit addressing, or
separate code/data address space.

## Stack and Syscall Encoding

The assembler encodes register-list stack operations as:

```
opcode = PUSH or POP
regs = 0
operand bit 0 = R0
operand bit 1 = R1
operand bit 2 = R2
operand bit 3 = R3
operand bit 4 = LR
operand bit 7 = IRC/syscall for POP
```

`PUSH` implementation details:

- Decrements `SP`, writes `Rd`.
- If operand bit 0 is set, decrements `SP`, writes `Rn`.
- Then iterates bits 0..4 and pushes each selected register.
- This can duplicate pushes when `Rd`/`Rn` overlap the mask.
- Comment mentions bit 8 for an interrupt control register, but the operand is
  8 bits. The assembler names bit 7 `IRC`.

`POP` implementation details:

- If operand bit 7 is set, no stack pop happens in the Python emulator. The low
  7 bits are dispatched as a syscall number. This is currently classified
  UNRESOLVED, not as an emulator artefact, because the assembler deliberately
  emits it and all primary opcodes are already consumed.
- Otherwise bit 0 can pop into `Rd`, bit 1 can pop into `Rn`, then bits 0..4
  pop into fixed `R0`..`LR`.
- If bit 4 is set, LR is popped again and `PC = LR`.

## Syscalls

Guest-visible encoding:

```
bits 15..12 = 1111  POP primary opcode
bits 11..8  = 0000
bit  7      = 1     IRC/syscall selector
bits 6..0   = syscall/service number
```

Implemented syscall numbers:

| Number | Name | Behaviour |
| --- | --- | --- |
| `0` | `SYS_EXIT` | CLI sets `PC = 0xff` and exits. |
| `1` | `SYS_PRINT` | Reads a NUL-terminated string from memory at `R0`; CLI prints it. |
| `63` | `SYS_UNAME` | CLI prints emulator version string. |

Constants also exist for open/read/write/close/seek/malloc/free/time/sleep and
directory operations, but they are not implemented in the CLI dispatcher.

## Assembler

The current Python assembler is `assembler4emulator_v8.1.py`.

Recognized instructions:

`ld`, `li`, `st`, `add`, `sub`, `jmp`, `beq`, `bne`, `cmp`, `and`, `or`,
`xor`, `shl`, `shr`, `push`, `pop`, `syscall`.

Recognized directives:

`.global`, `.text`, `.data`, `.org`, `.asciz`, `.include`, `.section`,
`.align`.

Only `.text` and `.data` have meaningful implemented handling. `.org`,
`.global`, `.include`, and `.section` are placeholders.

Output is an Intel-HEX-like text file:

- Instruction records use record type `0x11`, which is nonstandard Intel HEX.
- Data records use record type `0x00`.
- EOF is `:00000001FF`.
- No checksum is generated or validated.

## Discrepancies and Defects

- `Reference.txt` lists `add Rd, Rn, Rm`, but the current encoding has no `Rm`
  field. The assembler accepts a third register syntactically for some
  instructions in examples, but current `ADD/SUB` implementation effectively
  uses an 8-bit immediate operand.
- Logical operations store to `R0` in the emulator even when assembler syntax
  appears to designate another destination.
- `LD/ST [#imm]` are encoded by the assembler but ignored by the emulator.
- Branch target labels are recorded as `address - 1`, likely compensating for
  the emulator's unconditional post-branch PC increment.
- A branch probe confirmed this: a label at instruction slot `1` encoded as
  operand `0x00`, which reaches slot `1` only because the Python emulator
  increments `PC` after the branch handler returns.
- Some test files are stale and do not assemble under the current assembler.

## Differential Testing Seeds

Good initial tests for QEMU-vs-Python comparison:

- `LI r0, #imm`; final `R0`.
- `ADD r0, r0, #1`; final `R0`, `ZF`, `CF`, `OF`.
- `SUB r0, r0, #1`; final `R0`, flags.
- `CMP r0, #imm`; flags.
- `JMP #0xff`; halt behaviour.
- `SYS_PRINT` using `.data .asciz` and `syscall 1`.
