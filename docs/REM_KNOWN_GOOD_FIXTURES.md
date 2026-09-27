# Known-good fixtures and their purpose

| Fixture | Demonstrates | Check |
|---|---|---|
| `toolchain/examples/minimal.s` | current GAS syntax, link, halt | `tests/run-assembler-qemu-test.sh` |
| `toolchain/examples/linux-echo.S` | Linux syscall entry and console I/O | userspace regression scripts |
| `toolchain/examples/linux-minilibc.c` | hosted C/minilibc smoke path | `test-linux-minilibc.py` |
| `toolchain/examples/flight-kit/hello.c` + `toolchain/flight-kit-runtime/` | native chibicc hosted-C compile/link/run tuple | `REM_C_COMPILER_STATUS.md` and `REM_VALIDATED_RUNTIME_TUPLES.md` (Level 5) |
| `toolchain/examples/linux-tty-probe.S` | tty probing from assembly | inspect/target-specific run |
| `toolchain/examples/linux-clone-wait.S` | process ABI exercise | userspace examples |
| `toolchain/examples/multi-main.s` + `multi-functions.s` | multi-object link | worked multi-file example |
| `tests/programs/*.s` | instruction/firmware fixtures | corresponding `tests/run-*` scripts |

An example is not marked tested merely because it exists. Use the named
script, record the revision, and preserve the output when preparing an
offline bundle.
