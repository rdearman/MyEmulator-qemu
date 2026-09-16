# Memory Map

MyEmulator has a 16-bit byte address bus and an 8-bit data bus.

| Range | Meaning | Status |
| --- | --- | --- |
| `0x0000-0xfffb` | Program, data, stack, and future device space | architectural address space |
| `0xfffc-0xfffd` | Initial SP reset vector | architectural |
| `0xfffe-0xffff` | Reset PC vector | architectural |

Every address identifies one byte. Data `LD` and `ST` access one byte. A 16-bit
instruction occupies `PC` and `PC+1`; ordinary execution advances `PC` by 2.
Stack addresses are byte addresses. PUSH pre-decrements before each byte store;
POP reads before each post-increment. PUSH order is ascending mask order and POP
order is reverse mask order.

Reset vectors and all future 16-bit values are little endian. Initial QEMU
maps the full space as flat RAM, then writes vectors in the top four bytes. ROM,
EPROM, MMIO overlays, and bank switching are future work.

The Python list model, which stored a complete instruction in one entry while
treating data entries as bytes, is a historical defect. Its unused EPROM and
large banked RAM/EPROM prose are future design material, not initial mappings.
