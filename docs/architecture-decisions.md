# Architecture Decisions

This file records how discrepancies between the original design notes, old
software, and the Python emulator are classified while MyEmulator is being
formalized as a QEMU machine.

## Resolution Hierarchy

Use this order when evidence conflicts:

1. Clear architectural intent expressed in original design documentation.
2. Consistent behaviour demonstrated by assembler, programs, and tests.
3. Behaviour of the Python emulator.
4. Accidental implementation quirks and bugs.

QEMU becomes the new authoritative hardware definition. Python emulator
behaviour is evidence, not automatically architecture.

## Classifications

| Topic | Classification | Decision / Evidence |
| --- | --- | --- |
| 4-bit primary opcode field | ARCHITECTURAL | Design docs, assembler, and emulator all use bits 15..12 as the primary opcode, giving opcodes `0x0`-`0xf`. |
| 16-bit instruction word | ARCHITECTURAL | Assembler emits 16-bit words; emulator decodes `opcode`, `regs`, and `operand` from a 16-bit value. |
| `Rd`/`Rn` 2-bit fields | ARCHITECTURAL | Assembler register map exposes only `r0`-`r3`; emulator decodes bits 11..10 and 9..8 as `Rd` and `Rn`. |
| Normal branch target semantics | INTENDED | Documentation says jump/branch to a target address. QEMU should set `PC` to the encoded target without an extra hidden `+1`. |
| Python branch/jump `target + 1` result | BUG / COMPATIBILITY | Python increments `PC` after every instruction. The assembler compensates by storing labels as `current_address - 1`. Old binaries assembled from labels may therefore encode `target - 1`. |
| Unsupported opcode double increment | BUG | Python increments `PC` in unsupported dispatch and again after fetch. QEMU should instead stop with an obvious illegal-instruction diagnostic until exceptions are defined. |
| POP operand bit 7 escape | UNRESOLVED | The assembler deliberately maps `syscall` to opcode `0xf` with operand bit 7 set, and `POP {IRC}` can set the same bit. This may be a deliberate ISA extension mechanism due to the 16-opcode limit. |
| Host-side syscall services | COMPATIBILITY | Existing `syscall` programs use services `0`, `1`, and `63` through the Python CLI. The host queue implementation is not hardware, but the guest-visible escape encoding remains unresolved. |
| RAM/EPROM banking prose | INTENDED | Design notes describe larger RAM/EPROM banks and EPROM boot. The Python emulator allocates only flat RAM plus unused EPROM variables. Initial QEMU bring-up uses minimal RAM only. |
| Initial flat RAM for QEMU bring-up | INTENDED | Minimal RAM is enough to execute CPU tests and keeps the design open for banked memory later. |
| Byte vs word addressed architecture | UNRESOLVED | Evidence is mixed: design says 16-bit address bus and 8-bit data bus, data labels advance by bytes, `.word` advances by two, but Python instruction fetch uses one 16-bit list entry per `PC`. |

## Branch Target Compatibility Probe

A temporary program assembled with `assembler4emulator_v8.1.py`:

```asm
.text
start:
    li r0, #1
loop:
    add r0, r0, #1
    bne loop
    jmp #0xff
```

emitted:

```text
00: 1001
01: 3001
02: 7000
03: 50FF
```

The label `loop` is at instruction slot `1`, but `bne loop` encoded operand
`0x00`. In the Python emulator that becomes effective target `1` because the
branch sets `PC = 0` and the fetch loop increments it afterward.

QEMU should not reproduce the hidden post-branch increment. For old binaries,
we need either a compatibility loader/rewriter for branch operands or a
decision that old prebuilt branch binaries are not compatibility targets.

## POP Bit 7 / Syscall Evidence

- `assembler4emulator_v8.1.py` maps both `pop` and `syscall` to primary opcode
  `1111`.
- `syscall N` emits `1111 0000 (0x80 + N)`.
- `pop {IRC}` can emit the same bit-7 selector through the register-list
  syntax.
- `emulator.py` tests `operands & 0x80` before doing any stack pop. If set, the
  low seven bits are dispatched as a syscall number.
- Checked-in `tests/irc.hex` contains `F081`, `F081`, and `F080`, corresponding
  to `syscall 1`, `syscall 1`, and `syscall 0`.

The guest-visible encoding may have been deliberate. The current host services
remain compatibility behaviour until a real machine interface is designed.

## Byte vs Word Addressing Evidence

Evidence for byte addressing:

- Original design says a 16-bit address bus and 8-bit data bus.
- Data accesses read/write one byte-like RAM entry.
- `.data` labels count bytes; `.asciz` advances by one per character.
- `.word` advances addresses by two.
- Intel-HEX data records use byte records (`00`) and byte addresses.

Evidence for word/instruction-slot addressing:

- Python instruction fetch reads `ram_memory[PC]` as a complete 16-bit
  instruction word.
- `PC` increments by one per instruction.
- Assembler instruction addresses advance by one per instruction.
- Instruction records (`0x11`) store packed 16-bit words and the loader places
  each word into one RAM entry.

Current status: UNRESOLVED. The QEMU CPU/machine must keep this decision
visible and reversible during bring-up. Do not use old Python list indexing
alone as architectural proof.
