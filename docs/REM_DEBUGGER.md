# remdbg — GDB command language for REM32

`remdbg` is the native ptrace debugger for REM Linux. Its command language is
the GNU GDB command language (subset): GDB command names, GDB unique-prefix
abbreviation, GDB syntax and GDB semantics. The old non-GDB commands
(`regs`, `mem`, `disasm`, `load`, `write`, `symbol`, `globals`, `source`,
`locals`) no longer exist; use the GDB equivalents below.

    remdbg ./program [args]
    (remdbg) i r

Commands are resolved GDB-style: an exact command/alias wins, otherwise a
unique prefix. An ambiguous prefix reports `Ambiguous command "di": ...`;
an unknown one reports `Undefined command`. Multiword commands (`info`,
`set`, `show`) use the same prefix rule (`i r` = `info registers`,
`i b` = `info breakpoints`). An empty line repeats `stepi`/`nexti`/
`continue`/`x`.

## IMPLEMENTED

| GDB command | Abbrev. | Notes |
|---|---|---|
| run | r | start/restart the inferior |
| start, starti | | `start` needs a `main` symbol |
| continue [N] | c, cont | N = ignore this breakpoint N-1 more times |
| stepi [N] | si | exactly one machine instruction |
| nexti [N] | ni | steps over `jal`/calls |
| finish | | runs to the return address (LR) |
| until/advance *ADDR | u | run to a location |
| jump *ADDR | j | |
| kill, quit | k, q | |
| break/tbreak LOCATION | b, br | `*ADDR`, `ADDR`, `FUNC`, `FUNC+off`; GDB numbering |
| delete [N...] | d | |
| disable/enable [N...] | | `disable` alone = all; ranges `N-M` |
| ignore N COUNT | | |
| info breakpoints | i b | hit counts |
| info registers [REG] | i r | r0–r15, sp, lr, pc, sr, cause, info; hex + natural |
| print[/FMT] EXPR | p, inspect | `$r1`..`$r15`, `$sp`, `$lr`, `$fp`, `$pc`, `$sr`, `$N`, symbols, `*ptr`, C operators |
| output, echo | | |
| x/NFU ADDR | | formats `x d u o t a c s i z`; sizes `b h w g`; `x/10i`, `x/4x`, `x/8wx` |
| disassemble [START,END] | disas | REM32 decoder; function around PC by default |
| set var $reg=EXPR, set {char/short/int}ADDR=EXPR | | |
| set args | | |
| backtrace | bt, where | heuristic (LR chain) |
| frame | f | frame 0 only |
| list | l | around PC, requires source via guest tools |
| info symbol/stack/locals/source/variables | | partial |
| help, help COMMAND, help info X | h | |

## PARTIALLY IMPLEMENTED

- `step` / `next` / `until` (no arg): print an explicit
  "not implemented: no DWARF line-table support". Real GDB `step` needs
  `.debug_line` parsing, line-range stepping, and frame-aware step-over/step-out.
  Use `stepi` / `nexti`.
- `finish` uses LR only (wrong if LR was spilled); `backtrace` has no CFI/DWARF
  unwinder; `frame N` for N>0 is unsupported.
- `info locals/source/variables/symbol`: use guest `readelf`; `info symbol`
  always says `.text`.
- `break FUNC` uses the exact function address (no prologue skipping).
- `p` prints decimal as signed 32-bit.

## NOT YET IMPLEMENTED (stubbed with an honest message)

Line-number breakpoints (`break 12`), `break ... if`, conditions, watchpoints,
`display`, `ptype`/`whatis`, `call`, `file`, `load`, `attach`, `x/f` (no FPU),
`info frame`, `info line`, threads, multiple inferiors, `frame N>0`.

## Differences from GDB (deliberate)

- The inferior is held at its exec stop at startup, so `i r`, `b`, `x`,
  `disas` work before `run`. GDB would say "The program has no registers now".
  The first `run` resumes it; later `run`s restart it.
- Breakpoints persist across inferior exit/restart. A second breakpoint at an
  address already holding one is refused with a Note.
- Exit status is printed GDB-style: `[Inferior 1 (process N) exited with code 52]`
  (octal, as GDB does).

## Compact reference (offline)

    r  c  c N  si  si N  ni  finish  u *A  j *A  k  q
    b *A / b FUNC   tbreak   d N   disable N   enable N   i b
    i r   i r r1   p $pc   p/x $sp   p $r1+4   p *0x2000000
    x/4x A   x/8wx A   x/2gx A   x/s A   x/10i $pc   x/3dw $sp
    disas   disas A,B   bt   f   l   help   help break   help info registers

## Testing

Fixture: `tests/debugger/remdbg-gdb-test.S` (`_start` calls `add3`, exits 42).
Verified on real REM32 (desktop QEMU, Fold icount settings): `i r`, `b *0x20000a0`,
`i b`, `r`, `si` (PC moved into `add3`, r1–r3/lr changed), `c` (exit code 52 octal =
42), `x/4x`, `x/10i`, `disas`, `d 1`, `q`. No ptrace kernel change was required.
Build: `make REM_CC=... MUSL_PREFIX=... OUTPUT=remdbg` in `toolchain/remdbg`.
