# MyEmulator console

The first-generation console is a byte-oriented MMIO device at
`0xf010-0xf01f`. It is deliberately not a UART, terminal emulator, or line
editor. It passes raw host bytes to and from guest software.

| Address | Register | Access | Meaning |
| --- | --- | --- | --- |
| `0xf010` | DATA | read/write | Read/pop the next input byte; write/transmit one byte |
| `0xf011` | STATUS | read | bit 0 `RX_READY`, bit 1 `TX_READY`; other bits zero |
| `0xf012` | CONTROL | read/write | bit 0 `RX_IRQ_ENABLE`; other bits reserved |
| `0xf013` | IRQ_STATUS | read | bit 0 `RX_PENDING`; other bits zero |
| `0xf014-0xf01f` | reserved | — | reserved |

The receive FIFO holds 16 bytes. Bytes are queued in arrival order, and a
DATA read removes one byte. If the FIFO is full, newly arriving bytes are
dropped and a trace event records the overflow. Traffic is raw 8-bit data;
CR, LF, backspace, and printable ASCII have no special hardware meaning.

TX_READY is currently always one. A DATA write is sent to the attached QEMU
character backend; when no backend is attached, it is discarded. This keeps
polling software compatible with a future asynchronous transmitter.

## Interrupts

Console receive uses IRQ4. When CONTROL.RX_IRQ_ENABLE is one and the FIFO is
non-empty, IRQ4 is asserted. IRQ_STATUS is derived from that same condition;
there is no separate command acknowledge. Reading DATA drains the FIFO and
naturally deasserts IRQ4 when it becomes empty. The level remains asserted if
more input is waiting, so an interrupt handler must consume the available
bytes it intends to service before RTI.

## QEMU backends

The machine automatically attaches a chardev named `console` if one is
provided. A separate QEMU monitor socket keeps monitor commands away from the
guest console:

```sh
qemu-system-myemulator -M myemulator -kernel console-echo.bin \
  -chardev stdio,id=console \
  -monitor unix:/tmp/myemulator-monitor.sock,server=on,wait=off \
  -nographic -serial none
```

For a PTY backend, QEMU prints the slave path:

```sh
qemu-system-myemulator -M myemulator -kernel console-echo.bin \
  -chardev pty,id=console -monitor none -nographic -serial none
```

For a separate socket client:

```sh
qemu-system-myemulator -M myemulator -kernel console-echo.bin \
  -chardev socket,id=console,path=/tmp/myemulator-console.sock,server=on,wait=off \
  -monitor unix:/tmp/myemulator-monitor.sock,server=on,wait=off \
  -nographic -serial none
```

Assemble the hardware tests with:

```sh
./tools/myasm examples/asm/console-output.asm -o console-output.bin
./tools/myasm examples/asm/console-echo.asm -o console-echo.bin
./tools/myasm examples/asm/console-interrupt.asm -o console-interrupt.bin --flat-64k
```

The output test transmits `RIK` and halts. The polling test echoes each
received byte. The interrupt test enables IRQ4, halts, and stores the next
received byte at `0x0200` in its handler. These are hardware tests only, not
monitor firmware. No prompt, echo policy, editing, or command parsing is
implemented by the device; those belong to the future RIKMON software.
