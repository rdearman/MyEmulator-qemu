# Instruction Set

This is the implemented instruction set of `rdearman/MyEmulator` at commit
`b6141f978fc44537a7a5b3ea53c9250cf7699e3d`.

## Encoding

All currently executed instructions are 16-bit words:

```
bits 15..12  opcode
bits 11..10  Rd
bits  9..8   Rn
bits  7..0   operand/immediate/mask
```

`Rd` and `Rn` encode `R0`-`R3` only. `LR` is not addressable through these
fields; it is accessed by jump and stack masks.

## Opcodes

| Opcode | Mnemonic | Implemented behaviour |
| --- | --- | --- |
| `0x0` | `LD` | `Rd = mem[Rn]`; sets `ZF` from `Rd`. |
| `0x1` | `LI` | `Rd = operand`; flags unchanged. |
| `0x2` | `ST` | `mem[Rn] = Rd & 0xff`; flags unchanged. |
| `0x3` | `ADD` | If operand is not `None`, `Rd = Rn + operand`; otherwise `Rd = Rd + Rn`. Sets `ZF`, `OF`, `CF`. |
| `0x4` | `SUB` | If operand is not `None`, `Rd = Rn - operand`; otherwise `Rd = Rd - Rn`. Sets `ZF`, `OF`, `CF`. |
| `0x5` | `JMP` | `LR = PC`; `PC = operand`; post-execute increment still applies. |
| `0x6` | `BEQ` | If `ZF`, `PC = operand`; post-execute increment still applies. |
| `0x7` | `BNE` | If not `ZF`, `PC = operand`; post-execute increment still applies. |
| `0x8` | `CMP` | Compares `Rd` against `Rn`, except `Rd == 0 && Rn == 0` means compare `R0` against operand. Sets `ZF`, `OF`, `CF`. |
| `0x9` | `AND` | Stores result in `R0`, not `Rd`. If `Rn == 0`, uses operand as immediate; otherwise uses `Rn`. Sets `ZF`. |
| `0xa` | `OR` | Stores result in `R0`, not `Rd`. If `Rn == 0`, uses operand as immediate; otherwise uses `Rn`. Sets `ZF`. |
| `0xb` | `XOR` | Stores result in `R0`, not `Rd`. If `Rn == 0`, uses operand as immediate; otherwise uses `Rn`. Sets `ZF`. |
| `0xc` | `SHL` | `Rd = (Rn << operand) & 0xff`; sets `ZF`, `OF`, clears `CF`. |
| `0xd` | `SHR` | `Rd = (Rn >> operand) & 0xff`; sets `ZF`, `OF` from old bit 7, clears `CF`. |
| `0xe` | `PUSH` | Stack operation using `Rd`, `Rn`, and operand mask; see below. |
| `0xf` | `POP` / `SYSCALL` | Stack pop using mask, or syscall when operand bit 7 is set. |

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
  only 8 bits and the implementation checks `1 << 8`, which cannot be set by
  current instructions.

`POP` implementation details:

- If operand bit 7 is set, no stack pop happens. The low 7 bits are dispatched
  as a syscall number.
- Otherwise bit 0 can pop into `Rd`, bit 1 can pop into `Rn`, then bits 0..4
  pop into fixed `R0`..`LR`.
- If bit 4 is set, LR is popped again and `PC = LR`.

## Syscalls

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
- Some test files are stale and do not assemble under the current assembler.

## Differential Testing Seeds

Good initial tests for QEMU-vs-Python comparison:

- `LI r0, #imm`; final `R0`.
- `ADD r0, r0, #1`; final `R0`, `ZF`, `CF`, `OF`.
- `SUB r0, r0, #1`; final `R0`, flags.
- `CMP r0, #imm`; flags.
- `JMP #0xff`; halt behaviour.
- `SYS_PRINT` using `.data .asciz` and `syscall 1`.
