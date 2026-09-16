# MyEmulator Architecture

This document describes the architecture evidence found in
`rdearman/MyEmulator` as inspected on 2026-09-16 at commit
`b6141f978fc44537a7a5b3ea53c9250cf7699e3d`.

The main executable evidence is currently `new/emulator/emulator.py`,
`new/cli/cli.py`, and `assembler4emulator_v8.1.py`. The older prose files are
important design notes, but they disagree with the executable implementation in
several places.

For QEMU work, the Python emulator is a behavioural reference rather than an
automatic architectural authority. Discrepancies are resolved using
`docs/architecture-decisions.md`.

## Related Repositories

`rdearman/MyEmulator`

- Purpose: current Python emulator, Python assembler, CLI, example programs,
  and historical archived versions.
- Status for this project: behavioural reference and architecture evidence.
- Default branch: `main`.
- Relevant files:
  - `new/emulator/emulator.py`
  - `new/cli/cli.py`
  - `new/emulator_v8.py`
  - `assembler4emulator_v8.1.py`
  - `MyFictional8BitComputer.txt`
  - `Reference.txt`
  - `tests/*.s`, `tests/*.hex`, `tests/sp/*`

`rdearman/myassembler`

- Purpose: Rust assembler for an 8-bit breadboard CPU.
- Status for this project: related background, not the executable reference for
  the Python MyEmulator ISA.
- It describes a different-looking assembly language and microcode-oriented
  implementation: `r1`-`r4`, `MOV`, `LDR`, `STR`, `INC`, `DEC`, `BL`, `BLT`,
  `BGT`, `CCF`, and EEPROM/micro-op concepts.
- Use only as historical context unless later software proves compatibility is
  required.

## Machine Model

The current emulator behaves as a simple single-CPU machine:

- One CPU.
- Four general purpose registers, `R0` through `R3`.
- One link register, `LR`, stored as `registers[4]`.
- Separate `SP` and `PC` registers.
- Three arithmetic/status flags: zero, overflow, carry.
- One execution gate named `interrupt_flag`.
- A 64 Ki-entry RAM array, where each entry may hold an instruction word or a
  byte-like data value.
- A 4 Ki-entry EPROM array exists but is not used by fetch, reset, or boot.
- No implemented MMU, privilege model, timer, interrupt controller, or true
  device bus.

## Register State

Implemented CPU state:

| Register | Width implied by implementation | Reset value | Notes |
| --- | --- | --- | --- |
| `R0` | 8-bit observable after arithmetic/data writes | `0` | Also syscall argument 0. |
| `R1` | 8-bit observable after arithmetic/data writes | `0` | Also syscall argument 1. |
| `R2` | 8-bit observable after arithmetic/data writes | `0` | Also syscall argument 2. |
| `R3` | 8-bit observable after arithmetic/data writes | `0` | Also syscall argument 3. |
| `LR` | effectively 16-bit possible, not consistently masked | `0` | Set by `JMP`; used by `POP` with LR bit. |
| `SP` | 16-bit address | `0xffff` | Stack grows down. |
| `PC` | 16-bit-ish address, but practical program halt at `0xff` | `0x0000` | Counts instruction slots, not bytes. |

The Python list allows unmasked register assignments in some paths, but most
arithmetic results and memory writes are explicitly masked to 8 bits. QEMU
should model registers as 8-bit for architectural values, with `PC` and `SP`
as 16-bit.

## Flags

Implemented flags:

| Flag | Meaning in implementation |
| --- | --- |
| `ZF` | Set when certain results equal zero. Used by `BEQ` and `BNE`. |
| `OF` | Set by arithmetic and shifts using inconsistent rules. |
| `CF` | Set by add carry and subtract/compare borrow-like logic. |
| `interrupt_flag` | When true, fetch is suppressed. It is reset true and cleared by CLI `start`, `run`, or auto-load. |

There is no implemented architectural interrupt controller. The name
`interrupt_flag` is closer to a halted/running latch in the current code.

## Instruction Format

Fetched instructions are 16-bit words held in `ram_memory[PC]`.

```
15            12 11       8 7                 0
+---------------+----------+-------------------+
| opcode[3:0]   | regs[3:0]| operand[7:0]      |
+---------------+----------+-------------------+

regs[3:2] = Rd
regs[1:0] = Rn
```

`PC` increments by one instruction slot after `execute_instruction()` returns in
the Python emulator. Branches and jumps assign `PC` to their target and are
then still followed by the unconditional post-execute increment. This means the
effective next fetch is `target + 1` for branch/jump targets in the current
implementation.

This is classified as a Python emulator bug with compatibility consequences,
not as intended architecture. The assembler compensates for labels by recording
them as `address - 1`; a branch probe confirmed this behaviour. QEMU should use
normal branch/label semantics.

## Reset

Constructing `Emulator` initializes:

- RAM: 65,536 entries of `0x0000`.
- EPROM: 4,096 entries of `0x00`.
- `current_ram_bank = 0`.
- `boot_eprom_bank = 60`.
- `R0`-`R3`, `LR = 0`.
- `SP = 0xffff`.
- `PC = 0`.
- `ZF = OF = CF = false`.
- `interrupt_flag = true`.

No ROM code is loaded or executed during reset.

## Exceptions and Interrupts

No real exceptions or interrupts are implemented.

Unsupported opcodes are logged and then the PC is incremented in
`execute_instruction()`, after which `fetch_and_execute()` increments it again.
The separate `handle_unsupported_opcode()` method would halt the CPU, but it is
not used by the current dispatch path. QEMU should not preserve this double
increment; unsupported opcodes should halt/diagnose obviously until a real
illegal-instruction exception is defined.

The `POP` instruction has a syscall escape when operand bit 7 is set. This is
not a hardware interrupt in the Python emulator; it queues a host-side request
to the CLI. Its guest-visible encoding is deliberately emitted by the assembler,
so it is currently classified UNRESOLVED rather than discarded as an artefact.

## Discrepancies from Prose Documentation

- `MyFictional8BitComputer.txt` describes three 256 KB RAM banks and 256 MB
  EPROM. The executable emulator implements one 65,536-entry RAM array and one
  unused 4 Ki-entry EPROM array.
- The prose says boot code comes from EPROM. The executable emulator loads
  Intel HEX files from the CLI into RAM and starts at address zero.
- `Reference.txt` describes `PUSH`/`POP` as operating on register sets. The
  implementation also pushes `Rd` unconditionally and has duplicate register
  handling.
- Several test files use syntax that does not match the current assembler
  (`LI #0`, three-operand logical instructions, `r4`, immediate `PUSH`/`POP`).

## Current Decisions and Open Questions

- Branch/jump should use normal target semantics in QEMU. Existing assembled
  label branches may need compatibility handling because the assembler encoded
  `target - 1`.
- `POP` bit 7 / syscall is UNRESOLVED. The host CLI services are compatibility
  behaviour, but the encoding may be an intentional ISA extension.
- Larger RAM/EPROM banking is INTENDED architecture, but not required for
  initial CPU bring-up.
- Byte vs word addressing is UNRESOLVED and must be decided from evidence
  before it becomes a stable QEMU architectural contract.
