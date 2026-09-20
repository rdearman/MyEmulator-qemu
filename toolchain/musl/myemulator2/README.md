# MyEmulator2 musl port

This directory is the tracked architecture overlay consumed by
`toolchain/scripts/build-musl.sh`.  It deliberately starts separately from
the reusable `toolchain/userspace/minilibc` runtime: the latter is a small
freestanding test library, while musl supplies the architecture-independent
libc implementation.

The port must implement the MyEmulator2 ABI rather than copy RISC-V or ARM
assembly.  The first required pieces are:

- `syscall_arch.h`: syscall number in `r1`, arguments in `r2`--`r7`, result in
  `r1`, entered with `syscall 0`;
- `crt_arch.h`: receive the initial stack in `r13`, pass it in `r1` to musl's
  `_start_c`, and maintain 16-byte stack alignment;
- `pthread_arch.h`: read the architectural TP special register;
- `atomic_arch.h`: implement the required compare-and-swap and barriers with
  MyEmulator2 atomics;
- `bits/`: exact Linux/MyEmulator2 stat, signal, setjmp, syscall and type
  layouts.

No generated musl source, archive, build directory or installed library is
stored in this repository.  The overlay is intentionally incomplete until
each ABI definition has a corresponding QEMU/Linux runtime test.
