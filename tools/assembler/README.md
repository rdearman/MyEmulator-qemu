# MyEmulator assembler

The assembler is invoked as:

```sh
./tools/myasm program.s -o program.bin
```

The complete language reference is in [docs/assembler.md](../../docs/assembler.md).
It is a flat two-pass assembler with GNU-as-inspired `.equ`, `.set`,
`.include`, local labels, expressions, sections-as-annotations, data/layout
directives, and explicit `.org` placement. It preserves the legacy
`NAME = expression` syntax and all current MyEmulator instruction encodings.
