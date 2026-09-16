# Boot Process

This document distinguishes the intended boot process from the behaviour
implemented by the current Python emulator.

## Implemented Boot

There is no autonomous hardware boot sequence.

Actual startup flow in `new/emulator_v8.py`:

1. Construct `Emulator`.
2. Construct `CommandLineInterface`.
3. Link them to each other.
4. Start `emulator.run()` in a background thread.
5. Enter CLI command loop.

The CPU starts with `interrupt_flag = true`, so `fetch_and_execute()` does not
fetch instructions. The machine is effectively paused until the CLI clears the
flag.

## Program Launch

The normal launch path is CLI-driven:

1. User runs `load <filename>`.
2. CLI parses the Intel-HEX-like file.
3. Instruction records are placed in the first empty RAM slot.
4. Data records are placed at their encoded addresses.
5. Loader returns `0`.
6. If `auto_run` is true, CLI sets `PC = 0` and `interrupt_flag = false`.
7. The emulator thread fetches from RAM address `PC`.

Programs commonly halt by jumping to `0xff`, because
`END_MARKER_ADDRESS = 0xff` in the current emulator and CLI.

`fetch_and_execute()` also halts when `PC > 0xff`, setting `interrupt_flag =
true`. The `run()` loop separately breaks when `PC == 0xff`.

## Documented Boot Intent

`MyFictional8BitComputer.txt` describes:

- Hardware initialization.
- Loading boot code and initial instructions from EPROM.
- Executing boot code from EPROM.
- Simulating a hard drive as a host folder.

This is not implemented in the inspected Python code. EPROM is allocated but
not read during fetch or reset.

## QEMU Boot Direction

For the first QEMU implementation, do not invent a ROM boot protocol.

Recommended staged approach:

1. Support `-kernel` or `-bios` loading of a raw or converted program image.
2. Reset `PC = 0`, `SP = 0xffff`, flags clear.
3. Execute from RAM or ROM according to the selected image option.
4. Add an explicit ROM region only when a real boot ROM image or specification
   exists.
5. Later reproduce the Python CLI loader's `.hex` format in a host-side tool or
   QEMU loader path for compatibility testing.

## Compatibility Notes

The original assembler emits files that the Python CLI loads, not raw binaries.
QEMU should eventually accept either:

- A converted flat binary suitable for QEMU memory loading.
- The old `.hex` format through a helper tool in this repository.
- A QEMU machine loader that directly understands the old record type `0x11`.

The first milestone should use a deliberately tiny handcrafted raw program so
CPU reset/fetch/decode can be proven before reproducing the loader.

## Open Questions

- Should reset fetch from RAM address 0, ROM address 0, or a fixed high ROM
  vector?
- Is `0xff` an architectural halt address or merely a Python emulator testing
  convention?
- Should there be a real `HALT` instruction? None exists in the current 16-op
  ISA.
