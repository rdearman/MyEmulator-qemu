# MyEmulator assembler

`./tools/myasm source.asm -o image.bin` assembles a raw binary. `--listing
file.lst` writes address/bytes/source lines, and `--symbols` prints symbols;
`--symbol-file file.sym` writes them. `--flat-64k` emits all 65536 bytes.

Raw output starts at address zero. `.org` gaps are filled with `0x00` (use
`--fill 0xff` to select another byte), so an image beginning at `.org 0x100`
contains 0x100 leading fill bytes. This is also the format expected by the
current flat-memory QEMU machine.

Registers are `R0`-`R3`, `A0`-`A3`, `LR`, `SP`, `PC`, and `S0`; PC is not a
general operand and S0 is only accessed through `GF`/`SF`. Numeric literals are
decimal, `0x` hexadecimal, `0b` binary, `0o` octal, or character literals.
Underscores are accepted. Expressions use symbols, numbers, `+`, `-`,
parentheses, and `hi(expr)`/`lo(expr)`. `#` marks an immediate.

Labels end in `:` and constants use `NAME = expression`. Comments begin with
`;` outside strings. `.byte`, `.word`, `.ascii`, `.asciz`, and `.org` are
implemented. Strings support `\n`, `\r`, `\t`, `\\`, `\"`, and `\0`.

Confirmed instruction syntax is:

```
ld/st r,[a+disp]       li r,#imm       add/sub rd,rn,#imm
cmp rd,rn              jal label       br/beq/bne/blt/bge/bltu/bgeu label
lda an,rx,ry           gta rx,ry,an     mva dst,src       ada an,#imm
gf r                   sf r             push/pop {r0,r1,lr}
ret                    rti              halt
```

`la an,#value,r_hi,r_lo` is a six-byte pseudo-instruction that explicitly
uses the two named scratch data registers and expands to `LI`, `LI`, `LDA`.
Architectural three-register `LDA` remains available. PUSH/POP lists accept
R0-R3 and LR, with no duplicates.

Branches use `target = PC + 2 + sign_extend(disp8)*2`; targets must be even and
the instruction-unit displacement must fit signed eight bits. `.word` is
little-endian. Assembly errors include filename and line and do not leave an
output file.

ADD, SUB, AND, OR, XOR, SHL, and SHR support immediate three-operand forms and
destructive two-operand register forms. Register forms use the `0x77xx`
extended ALU family; immediate forms retain all 256 immediate values.
