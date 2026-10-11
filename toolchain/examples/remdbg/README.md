# remdbg examples

Build the portable fixture `debug_test.c` with the selected REM C compiler and
symbols enabled where supported. Keep `remdbg`, its matching `readelf` helper,
kernel ptrace support, and fixture from a qualified tuple.

Start an ordinary run:

```sh
remdbg ./debug_test
```

At the prompt, enter the commands from `basic.commands`. The fixture calls
`add_values`, includes a compiler-visible global, and contains an explicit
REM breakpoint in builds configured for that fixture. `fault.commands` is a
separate deliberate invalid-memory-access run; launch it with
`remdbg ./debug_test fault`.

For memory inspection, stop at `main`, read the `sp` or argument pointer from
`regs`, then run `mem ADDRESS LENGTH` with that mapped address. Do not copy an
arbitrary address: memory reads target the debugged process and unmapped
addresses are expected to fail.

Expected behavior is a breakpoint at `main`, a register/memory/disassembly
inspection, one instruction step, then normal exit. The fault session should
stop on SIGSEGV and show the faulting PC and symbol where symbol data permits.
Source rows and locals depend on the compiler's DWARF output. remdbg does not
evaluate arbitrary C expressions, and stack-local locations may be reported
unavailable rather than guessed. See `/usr/share/doc/rem/REM_DEBUGGER.md`.
