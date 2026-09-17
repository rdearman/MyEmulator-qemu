# Memory Map

MyEmulator has a 16-bit byte address bus and an 8-bit data bus.

| Range | Meaning | Status |
| --- | --- | --- |
| `0x0000-0xefff` | RAM: program, data, and stack | writable RAM |
| `0xf000-0xf00f` | MyEmulator floppy-controller MMIO | device |
| `0xf010-0xf01f` | MyEmulator console MMIO | device |
| `0xf020-0xf0ff` | Reserved future MMIO | unmapped/reserved |
| `0xf100-0xffff` | External firmware ROM | read-only |
| `0xffec-0xffed` | Instruction-alignment exception vector | architectural |
| `0xffee-0xffef` | IRQ1 vector | architectural |
| `0xfff0-0xfff1` | IRQ2 vector | architectural |
| `0xfff2-0xfff3` | IRQ3 vector | architectural |
| `0xfff4-0xfff5` | IRQ4 vector | architectural |
| `0xfff6-0xfff7` | IRQ5 vector | architectural |
| `0xfff8-0xfff9` | IRQ6 vector | architectural |
| `0xfffa-0xfffb` | IRQ7 vector | architectural |
| `0xfffc-0xfffd` | Initial SP reset vector | architectural |
| `0xfffe-0xffff` | Reset PC vector | architectural |

Every address identifies one byte. Data `LD` and `ST` access one byte. A 16-bit
instruction occupies `PC` and `PC+1`; ordinary execution advances `PC` by 2.
`LD Rd,[An+disp8]` and `ST Rd,[An+disp8]` calculate a 16-bit effective address
from an address register and signed 8-bit displacement, with 16-bit wrapping.
Stack addresses are byte addresses. PUSH pre-decrements before each byte store;
POP reads before each post-increment. PUSH order is ascending mask order and POP
order is reverse mask order.

Reset vectors and all future 16-bit values are little endian. Firmware is an
external binary loaded at `0xf100`, padded to 3840 bytes with `0xff`; its final
20 bytes contain the alignment vector, seven IRQ vectors, initial SP, and reset
PC. The vectors
are therefore physically in ROM and are never manufactured in writable RAM.
Writes to the ROM are ignored by the memory-region implementation. A floppy
or console attachment does not alter reset vectors or reset PC.

Development `-kernel` images are loaded into RAM. A normal short `-kernel`
image receives a compatibility ROM with SP=`0xf000` and PC=`0x0000`; a legacy
64 KiB raw image supplies its `0xf100-0xffff` ROM window while only its
`0x0000-0xefff` portion is copied to RAM. Use `-bios` for real firmware.

The Python list model, which stored a complete instruction in one entry while
treating data entries as bytes, is a historical defect. Older built-in floppy
bootstrap files are retained as historical test material and are not part of
normal machine reset.
