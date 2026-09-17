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
| Registers | ARCHITECTURAL | `R0`-`R3` and `S0` are 8-bit; `A0`-`A3`, `LR`, `PC`, and `SP` are 16-bit. S0 is not a general-purpose operand register. |
| S0 status register | ARCHITECTURAL | S0 bits 0..3 are `ZF`, `NF`, `CF`, and `OF`; bits 4..6 are IPL; bit 7 is reserved; reset sets S0 to zero. GF/SF expose the complete byte. |
| HALT | ARCHITECTURAL | `0xf080` is the operand-free HALT instruction. It halts QEMU. `PC == 0xff` is not special. |
| Reset | ARCHITECTURAL | `SP=read16(0xfffc)` and `PC=read16(0xfffe)`. |
| PUSH/POP mask | ARCHITECTURAL | Bits 0..4 select `R0`, `R1`, `R2`, `R3`, `LR`; only selected registers transfer. |
| PUSH/POP order | ARCHITECTURAL | PUSH processes selected registers `R0` through `LR`; POP processes them in reverse. R0-R3 transfer one byte; LR transfers two little-endian bytes. PUSH pre-decrements before each byte store; POP reads then post-increments after each byte. |
| CMP | ARCHITECTURAL | `CMP rA,rB` performs 8-bit `rA-rB`, discards the result, and sets `ZF`, `NF`, `CF` (no borrow), and signed-subtraction `OF`. |
| Ordinary ALU | ARCHITECTURAL | ADD/SUB/AND/OR/XOR/SHL/SHR have full-range immediate forms on primary opcodes `0x3`, `0x4`, and `0x9-0xd`; destructive register-register forms use `0x77oo`, with operation selectors 0-6. Zero-count shifts preserve CF. |
| Conditional/unconditional branches | ARCHITECTURAL | Opcode `0x6` uses condition bits 11..8: `0=BEQ`, `1=BNE`, `2=BLT`, `3=BGE`, `4=BLTU`, `5=BGEU`, `6=BR`; all use signed 8-bit PC-relative instruction-unit displacements. |
| Opcode `0x7` | ARCHITECTURAL | Address/status-operation family containing `LDA`, `GTA`, `MVA`, `ADA`, `GF`, and `SF`; unused subencodings remain reserved. |
| JAL | ARCHITECTURAL | Primary opcode `0x5`; writes `LR=P+2` and branches with the same signed PC-relative displacement. |
| Address registers | ARCHITECTURAL | `A0`-`A3` are 16-bit general address registers. `LD`/`ST` use `A[n] + sign_extend(disp8)`. |
| Address operations | ARCHITECTURAL | Primary opcode `0x7` contains `LDA`, `GTA`, `MVA`, `ADA`, `GF`, and `SF` in disjoint subgroups; they do not modify S0 except SF. Revised MVA selectors cover A0-A3, LR, and SP, never PC. |
| GF/SF | ARCHITECTURAL | `GF Rn` is `0x7600 | (Rn << 2)` and `SF Rn` is `0x7601 | (Rn << 2)`; only R0-R3 are valid. |
| RET | ARCHITECTURAL | `0xf040` sets `PC=LR` without changing any other register. |
| RTI | ARCHITECTURAL | `0xf060` restores S0 and PC from the descending hardware interrupt frame. |
| Hardware interrupts | ARCHITECTURAL | IRQ1-IRQ7 are level-sensitive priority inputs. The highest asserted level greater than S0.IPL is accepted between instructions; entry sets IPL to that level and vectors through `0xffee + 2*N`. |
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
