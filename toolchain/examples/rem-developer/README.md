# REM developer sample map

This directory collects the reusable sample locations and identifies the
library/archive starter. `tools/userspace/rem-doc/install-docs.sh` stages the
samples under `/usr/share/doc/rem/examples/`; the hash-pinned acceptance
overlay is prepared for flight-image integration.

| Example | Source |
|---|---|
| C hello and compiler stages | `../flight-kit/hello.c`, `../flight-kit/build.sh` |
| assembly syscall hello | `../flight-kit/asm-hello.S` |
| command-line arguments | `../flight-kit/args.c` |
| file, directory, memory, clock, signal, process, pipe | `../flight-kit/` |
| C calling assembly | `../flight-kit/call-asm.c`, `call-asm.S` |
| Makefile and build graph | `../developer-build/Makefile`, `build.ninja` |
| remdbg break/step/fault fixture | `../debugger/debug_test.c` |
| remdbg command transcripts | `../remdbg/` |
| static library/archive | `library/` |
| Larger project starters | `../flight-project-pack/` |

`asm-hello.S` demonstrates Linux `write` and `exit_group` syscalls. Direct
user-mode MMIO is not an application interface on REM; access hardware through
documented syscalls and device nodes.
