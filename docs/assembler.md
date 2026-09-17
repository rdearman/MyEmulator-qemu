# MyEmulator assembler language

`./tools/myasm program.s -o program.bin` produces a flat MyEmulator binary.
The assembler is two-pass and uses a GNU-as-inspired source spelling, but it
does not produce ELF objects or perform linking. `.org` is therefore the
authoritative placement mechanism.

## Invocation and images

Useful options are:

```text
--firmware, --rom       emit the compact 0xF100-0xFFFF (3840-byte) ROM image
--flat-64k              emit a complete 64 KiB flat image
-I DIRECTORY            search DIRECTORY for .include files (repeatable)
--listing FILE          write address/bytes/source listing
--symbols               print symbols
--symbol-file FILE      write symbols to FILE
--debug-map FILE        write JSON source/debug metadata
--fill BYTE             fill gaps (RAM defaults to 00, firmware to FF)
```

For a firmware image:

```sh
./tools/myasm -I include rikmon.s -o rikmon.bin --firmware \
    --debug-map rikmon.debug.json
```

The output contains bytes from guest address `0xF100`; it does not contain a
64 KiB leading gap. Emitted firmware must remain within `0xF100-0xFFFF`.

## Lexical syntax

Mnemonics, registers and symbol references are case-insensitive. A semicolon
starts a comment outside a quoted string. `#` is deliberately not a comment:
it marks instruction immediates such as `li r0,#7`. Source files conventionally
use `.s`.

Labels can stand alone or precede an instruction/directive:

```asm
reset:  li r0,#'R'
        jal print_banner
print_banner:
        halt
```

Numeric local labels may be reused. `1f` means the next `1:` and `1b` means the
previous one.

## Constants and expressions

Use `.equ` for a constant which cannot be redefined and `.set` for a mutable
assembly symbol. The old `NAME = expression` spelling remains a `.set`
compatibility form.

```asm
.equ CONSOLE_BASE,   0xF010
.equ CONSOLE_DATA,   CONSOLE_BASE
.set COUNT, 1
.set COUNT, COUNT + 1
```

Numbers can be decimal, `0x` hexadecimal, `0b` binary, or `0o` octal, with
optional underscores. Expressions support symbols, the current address `.`,
parentheses, `+ - * / % << >> & | ^ ~`, and `hi(expr)`/`lo(expr)`. Division is
integer division truncated toward zero. Character constants are one byte:
`'A'`, `'\n'`, `'\r'`, `'\t'`, `'\0'`, `'\\'`, `\'`, and `'\x41'`.

## Sections and visibility

`.section .text`, `.section .rodata`, `.section .data`, `.section .bss`, and
the shorthand `.text`/`.data` are accepted as source-organisation annotations.
They do not switch output buffers or move the location counter; use `.org`
when placement matters. There is no linker.

`.global` and `.globl` mark an exported symbol; `.local` records a local
symbol. They do not change the flat binary. Symbol/debug output includes
symbol kind and visibility metadata.

## Data and layout directives

| Directive | Meaning |
|---|---|
| `.org EXPR` | set the absolute location counter |
| `.byte EXPR, ...` | emit 8-bit bytes |
| `.word EXPR, ...` | emit little-endian 16-bit words |
| `.ascii "..."` | emit bytes without a terminator |
| `.asciz "..."` | emit bytes followed by zero |
| `.space COUNT[, FILL]` | emit `COUNT` fill bytes, default zero |
| `.fill COUNT, SIZE, VALUE` | repeat a little-endian `SIZE`-byte value |
| `.align N` | pad to an address divisible by N |
| `.balign N` | same unambiguous byte-boundary rule as `.align` |
| `.include "file"` | include a source file |

Strings support `\n`, `\r`, `\t`, `\0`, `\\`, `\"`, and `\xNN`. `.word` is
little-endian. `.align 2` means byte-address alignment to a multiple of two,
not a power-of-two exponent. Padding directives default to zero; firmware
image gaps default to `0xFF` at output time.

Include lookup tries the directory containing the including file first, then
each `-I` directory. Recursive includes and missing files are errors with
filename/line diagnostics.

## Instructions

The current MyEmulator 1.0 forms are:

```text
ld/st r,[a+disp8]       li r,#imm8
add/sub/and/or/xor/shl/shr rd,rn,#imm8
add/sub/and/or/xor/shl/shr rd,rn
cmp rd,rn                jal label
beq/bne/blt/bge/bltu/bgeu/br label
lda an,rx,ry             gta rx,ry,an       mva dst,src
ada an,#imm8             gf r                sf r
push/pop {r0,r1,lr}     ret                rti                halt
```

The register ALU forms are destructive two-operand operations: `and r0,r1`
means `r0 = r0 & r1`. Immediate forms retain all 256 values. The `la`
pseudo-instruction remains `la an,#value,r_hi,r_lo` and expands to `LI`, `LI`,
and `LDA` using the explicitly named scratch registers.

Macros and conditional assembly are intentionally not implemented. Use
`.include`, constants and ordinary labels for reusable firmware definitions.

## Debug metadata and diagnostics

`--debug-map` writes absolute source locations, resolved symbols, and symbol
kind/visibility information for `mydebug` and the Emacs integration. Included
source locations retain their source filename. Numeric local labels are used
for resolution but are not exported as ordinary symbols.

Errors are reported as `filename:line: explanation`, including undefined
symbols, duplicate labels, illegal `.equ` redefinitions, bad operands,
overflow, malformed strings, recursive includes, and firmware placement errors.
