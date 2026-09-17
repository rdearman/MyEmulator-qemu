# MyEmulator assembler manual

The assembler is invoked with `./tools/myasm program.asm -o program.bin`.
Add `--listing program.lst` for a source listing and `--symbols` or
`--symbol-file program.sym` for the symbol table. `--flat-64k` emits a complete
64 KiB image; otherwise the raw binary runs from address zero through the last
emitted byte. `.org` creates zero-filled gaps by default.

Source is case-insensitive for mnemonics and registers. A semicolon starts a
comment unless it is inside a double-quoted string. Labels use `name:` and
constants use `name = expression`. The supported registers are R0-R3, A0-A3,
LR, SP, PC, and S0. S0 is not a data-register operand; PC is not an MVA
operand.

Numbers may be decimal, hexadecimal (`0x2a`), binary (`0b101010`), octal
(`0o52`), or a character literal such as `'A'`; underscores are allowed.
Expressions support symbols, `+`, `-`, parentheses, and `hi(x)`/`lo(x)`, with
`#` used to mark immediate operands. Undefined symbols, overflow, and cyclic
constants are errors.

The confirmed current instruction forms are LD/ST with `[An+signed8]`, LI,
immediate and register-form ADD/SUB/AND/OR/XOR/SHL/SHR, register CMP, JAL, BR/BEQ/BNE/BLT/BGE/BLTU/BGEU,
LDA/GTA, MVA, ADA, GF/SF, PUSH/POP lists, RET, RTI, and HALT. Branches are
resolved in two passes using the actual instruction-unit displacement formula.
PUSH/POP accept R0-R3 and LR, reject duplicates, and emit the architectural
mask without changing CPU transfer order.

Directives are `.byte`, `.word`, `.ascii`, `.asciz`, and `.org`. Words are
little-endian. Strings support `\n`, `\r`, `\t`, `\\`, `\"`, and `\0`.
Output regions are checked for overlap. The `la` pseudo-instruction takes
`la A0,#value,R2,R3` and expands deterministically to `LI R2,#hi(value)`,
`LI R3,#lo(value)`, and architectural `LDA A0,R2,R3`; scratch registers are
always explicit.

The register forms are destructive two-operand operations, for example
`and r0,r1` means `r0 = r0 & r1`; immediate forms retain all 256 values, for
example `and r0,r1,#0xff`. No historical
assembler syntax or encoding workaround is used.

`--debug-map file.mdbg` writes JSON metadata for `mydebug`, including resolved
symbols and source line locations for emitted bytes.
