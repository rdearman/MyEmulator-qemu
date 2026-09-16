# Architecture Decisions

QEMU is the authoritative hardware implementation. Conflicts are resolved in
this order: design intent, consistent assembler/program/test behaviour, Python
behaviour, then accidental quirks and bugs.

## Closed decisions

| Topic | Classification | Decision |
| --- | --- | --- |
| Primary opcode | ARCHITECTURAL | Bits 15..12 form 16 primary opcode groups. Unused encodings within a group remain reserved until needed. |
| Memory | ARCHITECTURAL | 16-bit byte address, 8-bit data, 64 KiB. Instructions are 16-bit and occupy two bytes; sequential `PC += 2`. |
| Registers | ARCHITECTURAL | `R0`-`R3` and `LR` are 8-bit; `PC` and `SP` are 16-bit byte addresses; flags are `ZF`, `OF`, `CF`. |
| HALT | ARCHITECTURAL | `0xf080` is the operand-free HALT instruction. It halts QEMU. `PC == 0xff` is not special. |
| Reset | ARCHITECTURAL | `SP=read16(0xfffc)` and `PC=read16(0xfffe)`. |
| PUSH/POP mask | ARCHITECTURAL | Bits 0..4 select `R0`, `R1`, `R2`, `R3`, `LR`; only selected registers transfer. |
| Old compatibility | EXCLUDED | Old binaries, assembler workarounds, syscall meanings, and emulator bugs are not requirements. |
| Banked RAM/EPROM | INTENDED, DEFERRED | Preserve design room for it, but use flat RAM for bring-up. |

## Historical classifications

The Python `target + 1` branch result and unsupported-opcode double increment
are BUGS. Old PUSH/POP duplication is a BUG. The POP bit-7 syscall escape and
its services are HISTORICAL/EXCLUDED; `0xf080` is now HALT and has no syscall
meaning.

## Open decisions

### Instruction byte order

The design establishes byte addressing but does not establish order. Existing
bring-up bytes use little endian (`02 10` for `0x1002`), while historical text
and HEX formats are not convincing evidence for hardware order. QEMU currently
uses little endian provisionally. Choices are little endian, big endian, or an
explicit instruction-only order; this affects fetch, vectors, images, and the
assembler.

### Branch encoding

An 8-bit operand cannot directly represent every 16-bit byte address. Choices
are an absolute page-local byte address, a signed PC-relative byte offset, or a
long-form instruction using reserved encodings. PC-relative is compact and
relocatable; page-local is simple but limited; long form has reach but consumes
encodings/bytes. No choice is made yet, and QEMU must not infer the old
`target+1` behaviour.

### PUSH/POP ordering

Mask semantics are closed, but historical code is too inconsistent to establish
ordering. Choices are ascending mask order, descending order, or an explicit
ABI order. This determines stack layout and call conventions. No choice is made.
