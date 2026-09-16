# Devices and Peripherals

The current Python emulator has very limited hardware devices. Most peripheral
behaviour is implemented as CLI/host services rather than memory-mapped devices.

## Console and Syscalls

Console output is implemented through a syscall queue between the emulator and
CLI.

Guest-visible mechanism:

- Execute `POP`/`SYSCALL` instruction with operand bit 7 set.
- Low 7 operand bits become the syscall number.
- `R0`-`R3` are passed to `syscall_dispatcher()`.

Implemented host-side calls:

| Number | Name | Host behaviour |
| --- | --- | --- |
| `0` | `SYS_EXIT` | Set `PC = 0xff`; exit CLI process. |
| `1` | `SYS_PRINT` | Treat `R0` as an address of a NUL-terminated string in RAM; print it. |
| `63` | `SYS_UNAME` | Print `Rick's Amazing Emulator version: 8.0.0`. |

Declared but not implemented:

- `SYS_OPEN = 2`
- `SYS_READ = 3`
- `SYS_WRITE = 4`
- `SYS_CLOSE = 5`
- `SYS_SEEK = 8`
- `SYS_SLEEP = 11`
- `SYS_UNAME = 63`
- `SYS_MKDIR = 83`
- `SYS_RMDIR = 84`
- `SYS_RENAME = 82`
- `SYS_MALLOC = 92`
- `SYS_FREE = 93`
- `SYS_GETTIMEOFDAY = 96`

## Storage

The CLI creates a host directory named `harddrive` if it does not exist.

Implemented commands:

- `load <filename>` reads a guest program from the current host directory.
- `ls` lists host files below the current directory.
- `cd <directory>` attempts to change the CLI's current directory, with a guard
  intended to prevent leaving `./harddrive`.

The current implementation initializes `current_directory = "./"` and changes
the process working directory to it. This conflicts with comments saying the
root is `./harddrive`. The path guard in `change_directory()` checks for paths
starting with `./harddrive`, so storage semantics need experimental cleanup
before they become hardware.

No disk block device, filesystem format, controller registers, DMA, or sector
layout is implemented.

## Memory Inspection and Loading

CLI monitor commands are host debugger facilities, not guest devices:

- `mem <start> <end>`
- `store <address> <data...>`
- `registers`
- `sysinfo`
- `log`
- `auto`
- `start` / `run`
- `shutdown` / `exit`

`store` writes directly to RAM.

## Interrupt Controller and Timers

No timer device is implemented.

No interrupt controller is implemented. The emulator has an `interrupt_flag`,
but it acts as a run/halt latch:

- Reset value is `true`, preventing fetch.
- CLI `start`, `run`, and successful auto-load set it `false`.
- Some halt paths set it `true`.

## QEMU Device Direction

The initial QEMU machine should avoid modelling host syscalls as CPU internals.
Recommended path:

1. Implement CPU and RAM first.
2. Add a tiny memory-mapped debug/console device for output.
3. Add an optional compatibility semihosting/syscall path for existing
   `syscall`/`POP bit7` programs.
4. Design storage as a simple block or byte-stream device only after old
   software requirements are known.

Potential QEMU mappings:

- Console: `Chardev`-backed MMIO transmit register, later bridged from legacy
  syscall `SYS_PRINT` if needed.
- Exit: QEMU semihosting-style trap for tests, or an MMIO debug-exit device for
  automated regression.
- Storage: QEMU block backend with a small custom controller, not the host
  folder API from the Python CLI.

## Open Questions

- Is the syscall ABI part of the architecture or just an emulator monitor
  convenience?
- Should the initial QEMU target support `SYS_PRINT` for compatibility before a
  real serial device exists?
- What existing software, if any, depends on the host `harddrive` directory
  behaviour?
