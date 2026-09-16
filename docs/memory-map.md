# Memory Map

This document records the memory behaviour actually implemented by
`rdearman/MyEmulator` at commit `b6141f978fc44537a7a5b3ea53c9250cf7699e3d`.

## Implemented Memory

| Region | Address range | Backing | Access |
| --- | --- | --- | --- |
| RAM | `0x0000`-`0xffff` | `ram_memory`, 65,536 Python list entries | Instruction fetch, data load/store, stack, loaded programs, loaded data. |
| EPROM | no effective mapped range | `eprom_memory`, 4,096 Python list entries | Allocated but unused by fetch/load/reset. |

The current emulator uses one RAM array for both instruction words and data
bytes. An instruction is a 16-bit integer stored in one RAM entry. Data writes
mask values to 8 bits and store them in one RAM entry.

`PC` addresses instruction slots, not byte offsets. `SP` also indexes the same
RAM array.

## Reset Contents

All RAM entries initialize to `0x0000`. All EPROM entries initialize to `0x00`.
No ROM image is loaded automatically.

## Program Loading

The CLI command `load <filename>` reads an Intel-HEX-like file from the host
filesystem.

For instruction records:

- Record type `0x11` is treated as an instruction record.
- The data payload is split into 4-hex-digit chunks.
- Each chunk is parsed as a 16-bit instruction word.
- Words are loaded into RAM starting at the first empty slot found by
  `find_empty_memory_slot(64)`.
- The loader returns `0`; the CLI sets `PC = 0` on successful auto-run.

For data records:

- Record type `0x00` is treated as byte data.
- Bytes are stored at the record address.

EOF:

- Record type `0x01` stops loading.

The loader does not validate Intel HEX checksums.

## Data Placement by Assembler

The Python assembler places `.data` labels starting at `0xad`.

Supported data directives in the assembler:

- `.byte`
- `.word`
- `.asciz`

`.asciz` currently emits the characters of the string, but does not append an
explicit NUL terminator in the inspected implementation. This conflicts with
`SYS_PRINT`, which reads until `0x00`; it relies on zero-filled RAM after the
string.

## Stack

`SP` resets to `0xffff`. Push decrements `SP` before writing. Pop reads at `SP`
and then increments.

The stack shares the same RAM as code and data. There is no guard page, stack
limit, or wrap handling.

## Documented But Not Implemented

`MyFictional8BitComputer.txt` describes:

- Three RAM banks, each 256 KB.
- EPROM bank 0 with 256 MB.
- Bank selection, including `current_ram_bank` and `boot_eprom_bank`.

Only `current_ram_bank = 0` and `boot_eprom_bank = 60` exist as variables in
the emulator. They do not affect reads, writes, fetch, or boot.

## Proposed QEMU Memory Model

Start with the implemented architecture:

| QEMU region | Proposed range | Notes |
| --- | --- | --- |
| RAM | `0x0000`-`0xffff` | Main system memory, byte-addressed in QEMU. |
| ROM/EPROM | unresolved | Add only after boot/ROM semantics are defined. |
| MMIO console/syscall | unresolved | Prefer real MMIO device rather than preserving host syscall queue forever. |

For compatibility with old `.hex` files, the loader can map each 16-bit
instruction word to two bytes in guest memory while keeping architectural `PC`
as an instruction index, or it can define `PC` as a byte address from the start.
This is unresolved and should be settled before writing the translator.

Compatibility recommendation for milestone 1:

- Define instruction fetch as 16-bit little-endian words from byte address
  `PC * 2`.
- Keep architectural `PC` reported in instruction slots.
- Provide a loader/converter for old `.hex` instruction records.

This matches QEMU's byte-addressed memory while preserving the old emulator's
visible PC arithmetic.
