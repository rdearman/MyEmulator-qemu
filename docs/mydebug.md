# Native MyEmulator debugger

`mydebug` is the debugger for the fictional MyEmulator CPU. It does not use
the GDB remote protocol and does not require a GDB architecture plug-in. The
client speaks newline-delimited JSON over QEMU's existing QMP Unix socket.

Start a paused machine:

```sh
./tools/myasm examples/asm/arithmetic.asm -o arithmetic.bin --debug-map arithmetic.mdbg
.qemu-build/qemu-system-myemulator -M myemulator -S -nographic \
    -kernel arithmetic.bin -qmp unix:/tmp/myemulator-debug.sock,server=on,wait=off
./tools/mydebug --qmp /tmp/myemulator-debug.sock --symbols arithmetic.mdbg
```

Interactive commands include `help`/`?`, `regs`, `info registers`,
`info breakpoints`, `info locals`, `stepi`/`si`, `step`/`s`, `next`/`n`,
`finish`/`fin`, `continue`/`c`, `reset`, `run`/`r`, `list`/`l`,
`print`/`p`, `set`, `x`, `w`, `dis`, `break`/`b`, `delete`/`d`, `bt`, and
`quit`. Empty input repeats the previous command. Ctrl-C stops the guest and
returns to the prompt. Breakpoints have stable debugger-session IDs and are
QEMU execution breakpoints that stop before the instruction at their address.

`stepi` executes exactly one guest instruction. `step` advances to a
different assembler source line and steps into calls. `next` steps over a
direct `JAL` using a temporary breakpoint at the return address. `finish`
uses the current LR as a temporary return breakpoint. Source stepping falls
back to one instruction when no debug-map location is available.

`print` accepts registers, symbols, integer literals, parentheses, and safe
integer arithmetic/bitwise expressions. Formats `/x`, `/d`, `/o`, and `/t`
select hexadecimal, decimal, octal, and binary output. `set` writes only
architectural registers and enforces their widths. `info locals` reports that
locals are unavailable because the current assembler map has no variable
location metadata. `backtrace` reports the current frame reliably and marks
debugger-observed call-history frames as reconstructed.

The QMP command is `myemulator-debug` with an `op` argument. Supported
operations are `registers`, `step`, `continue`, `stop`, `read-memory`,
`write-memory`, `break`, `delete-break`, `breakpoints`, and `disassemble`.
`tools/mydebug --machine` accepts one JSON request per input line and emits one
JSON response per line. A step response contains `registers`, `flags`,
`instruction`, and state fields; register values are integers, memory data is
lower-case hexadecimal, and disassembly returns `{address,size,text}` objects.
This mode is intended for a future Emacs front end and avoids screen scraping.

The assembler metadata format is `myemulator-debug-v1` JSON. It contains a
`symbols` object and `locations` array with `address`, `source`, `line`, and
`text`. It is optional; numeric debugger addresses work without it.

The debugger exposes the real registers: R0-R3 (8-bit), A0-A3, LR, SP, and PC
(16-bit), and S0 (8-bit), including ZF/NF/CF/OF/IPL. Memory operations address
the byte-addressed 64 KiB physical space. The machine protocol now includes
structured equivalents for stepping, reset/run, breakpoints, source listing,
expressions, register writes, locals, and backtraces.

## Emacs integration

The optional package `tools/emacs/mydebug.el` talks to the machine protocol;
it does not screen-scrape the interactive console. Load it locally with:

```elisp
(add-to-list 'load-path "/path/to/MyEmulator-qemu/tools/emacs")
(require 'mydebug)
```

With an arithmetic source buffer open, use `M-x mydebug-start`, enter the QMP
socket and `.mdbg` file, then enable `M-x mydebug-mode` in the source buffer.
The package supplies `M-x mydebug-layout` for console/register/breakpoint
windows. Useful source-buffer bindings are:

| Binding | Action |
| --- | --- |
| `C-c C-s` | one-instruction `stepi` |
| `C-c C-i` | source `step` |
| `C-c C-n` | `next` |
| `C-c C-c` | continue |
| `C-c C-x` | interrupt/stop |
| `C-c C-f` | finish |
| `C-c C-r` | reset and run |
| `C-c C-0` | reset |
| `C-c C-b` | toggle source-line breakpoint |
| `C-c C-l` | source listing |
| `C-c C-p` | print expression |

`*MyEmulator Registers*` and `*MyEmulator Breakpoints*` are read-only views;
the current source line is highlighted with an overlay. `.s` is not globally
associated with the mode; `myemulator-asm-mode` must be enabled explicitly.
The Emacs package assumes QEMU is already running and does not rebuild or
launch QEMU.
