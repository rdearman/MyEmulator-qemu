# chibicc-rem port status

## Verified

- Upstream source is pinned to `90d1f7f199cc55b13c7fdb5839d1409806633fdb`.
- REM data model is ILP32: `char` 1, `short` 2, `int`/`long`/pointer 4;
  `long long` remains 8 in the parser but is rejected by the initial backend.
- Host chibicc builds with the REM backend.
- Integer/pointer examples covering constants, locals, arithmetic, comparisons,
  conditionals, loops, and direct calls generate current REM GAS.
- Generated objects assemble with `myemulator2-elf-as` and identify as ELF32
  little-endian MyEmulator2.
- A generated `main` object links with the repository user linker script and
  staged musl startup/libc plus libgcc into an ELF32 MyEmulator2 executable.
- The REM driver can perform compile → assemble → link when its tool and sysroot
  paths are supplied through `CHIBICC_REM_*` environment variables.
- All chibicc source files cross-compile with the existing REM GCC, and a
  statically linked REM ELF compiler was produced in `/tmp/chibicc-rem-stage/`.

## Not yet verified

- Execution of the native compiler on REM (left pending to avoid the active
  kernel/userspace/QEMU work).
- Native `chibicc hello.c -o hello` on a booted REM rootfs.
- Self-hosting (chibicc compiling chibicc).
- Struct/union ABI, variadic calls, stack arguments beyond four, atomics,
  floating point, VLA, TLS, and full initializer relocation support.

The existing REM GCC backend and `docs/MYEMULATOR_2_ABI.md` remain authoritative
for details beyond this initial subset.
