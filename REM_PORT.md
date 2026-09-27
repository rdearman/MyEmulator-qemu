# REM port status

This directory is an isolated port of upstream chibicc to 32-bit
MyEmulator2/REM. Upstream revision:

```
90d1f7f199cc55b13c7fdb5839d1409806633fdb
```

The parser and preprocessor remain close to upstream. The x86-64 code generator
has been replaced with a REM GAS backend. The initial backend covers integer and
pointer C, local/global objects, control flow, direct calls, and the current
REM ELF/ABI conventions. Unsupported floating-point, variadic, atomic,
aggregate, and >4-argument cases fail explicitly.

`make` builds a host compiler with `-DCHIBICC_REM`. `./test-rem.sh` generates
current REM assembly and verifies it assembles as ELF32 little-endian
MyEmulator2. The driver accepts `CHIBICC_REM_*` environment variables for
host-side testing with a disposable REM toolchain/sysroot; a native build uses
ordinary `as` and `ld` plus local paths.

## Verified

- Host chibicc build.
- Constants, locals, arithmetic, comparisons, conditionals, loops, and direct
  calls generated as current REM GAS.
- Generated objects assembled by `myemulator2-elf-as`.
- Generated `main` object linked with the repository linker script and staged
  musl startup/libc plus libgcc into an ELF32 MyEmulator2 executable.
- All source files cross-compiled with the existing REM GCC.
- A static REM ELF compiler produced at `/tmp/chibicc-rem-stage/chibicc`.
- On the pinned runtime tuple in `docs/REM_VALIDATED_RUNTIME_TUPLES.md`, the
  native compiler generated `hello.s`, native GAS produced `hello.o`, and
  native `ld` linked and ran a hosted `Hello from REM` program.

## Pending

Self-hosting remains pending. Struct/union ABI, variadic calls, stack
arguments beyond four, atomics, floating point, VLA, TLS, and full initializer
relocation support remain outside this initial backend.

## Native execution results

The staged REM-native compiler was booted in an isolated copy of the pinned
repository flight rootfs. It successfully ran:

```text
/tmp/chibicc -E /tmp/hello.c   -> exit 0
/tmp/chibicc -S -o /tmp/hello.s /tmp/hello.c -> exit 0
```

During this test two portability defects were found and corrected: unavailable
`localtime()` data now has deterministic `__DATE__`/`__TIME__` fallbacks, and
backend-generated functions are aligned to four-byte instruction boundaries.

The native `/root/native/bin/as` accepted the generated assembly. Linking first
failed because the base rootfs omitted `libgcc.a`, while musl `libc.a` refers
to `__muldi3`. Adding the matching REM GCC runtime archive to a disposable
rootfs made the native link complete and the program print `Hello from REM`
with exit status zero. The permanent flight image was not modified.
