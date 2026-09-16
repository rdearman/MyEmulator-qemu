# Architecture Decisions

QEMU is the authoritative hardware implementation. Conflicts are resolved in
this order: design intent, consistent assembler/program/test behaviour, Python
behaviour, then accidental quirks and bugs.

## Closed decisions

| Topic | Classification | Decision |
| --- | --- | --- |
| Primary opcode | ARCHITECTURAL | Bits 15..12 form 16 primary opcode groups. Unused encodings within a group remain reserved until needed. |
| Memory | ARCHITECTURAL | 16-bit byte address, 8-bit data, 64 KiB. Instructions are 16-bit and occupy two bytes; sequential `PC += 2`. |
| Endianness | ARCHITECTURAL | All 16-bit values are little endian: the low byte is stored at the lower address. |
| Registers | ARCHITECTURAL | `R0`-`R3` and `LR` are 8-bit; `PC` and `SP` are 16-bit byte addresses; flags are `ZF`, `OF`, `CF`. |
| HALT | ARCHITECTURAL | `0xf080` is the operand-free HALT instruction. It halts QEMU. `PC == 0xff` is not special. |
| Reset | ARCHITECTURAL | `SP=read16(0xfffc)` and `PC=read16(0xfffe)`. |
| PUSH/POP mask | ARCHITECTURAL | Bits 0..4 select `R0`, `R1`, `R2`, `R3`, `LR`; only selected registers transfer. |
| PUSH/POP order | ARCHITECTURAL | PUSH processes selected registers `R0` through `LR`; POP processes them in reverse. PUSH pre-decrements before each byte store; POP reads then post-increments after each byte. |
| Conditional branches | ARCHITECTURAL | `BEQ` and `BNE` use signed 8-bit PC-relative displacements in instruction units: `target = P + 2 + sign_extend(imm8) * 2`. |
| JAL | ARCHITECTURAL | Primary opcode `0x5`; writes `LR=P+2` and branches with the same signed PC-relative displacement. |
| Old compatibility | EXCLUDED | Old binaries, assembler workarounds, syscall meanings, and emulator bugs are not requirements. |
| Banked RAM/EPROM | INTENDED, DEFERRED | Preserve design room for it, but use flat RAM for bring-up. |

## Historical classifications

The Python `target + 1` branch result and unsupported-opcode double increment
are BUGS. Old PUSH/POP duplication is a BUG. The POP bit-7 syscall escape and
its services are HISTORICAL/EXCLUDED; `0xf080` is now HALT and has no syscall
meaning.

## Open decisions

No unresolved questions remain from this decision set. Future jump forms and
expanded instruction groups require separate decisions. Plain `J`, `JALR`,
`RET`, `CALL`, long immediates, and multiword instructions are not introduced.
