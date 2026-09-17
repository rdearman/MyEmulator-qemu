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

Interactive commands are `regs`, `step`/`s`, `continue`/`c`, `stop`,
`x ADDRESS [LENGTH]`, `w ADDRESS HEXBYTES`, `dis ADDRESS [COUNT]`,
`b ADDRESS|SYMBOL`, `delete ADDRESS`, `bl`, and `quit`. Breakpoints are QEMU
execution breakpoints and stop before the instruction at their address.
`step` executes exactly one guest instruction.

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
the byte-addressed 64 KiB physical space. The current backend supports one
MyEmulator CPU and does not yet provide source-level stepping, watchpoints,
reverse execution, or an Emacs UI.
