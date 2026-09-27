# Offline flight build manifest

The workstation source tree is useful for offline reference and rebuilding,
but it is not itself the guest compiler sysroot. Carry these groups on the
device and include the runtime tuple as a matched set.

## Reading and navigation

Carry `REM_OFFLINE_INDEX.md`, the current target/syscall/ABI/compiler/runtime
tuple documents, linker guide, terminal matrix, known-good fixtures, the
assembly and C manuals, architecture/instruction and ELF/GAS/LD/POSIX/QEMU
references, and `REM_OFFLINE_ENGINEERING_NOTES.pdf` (named in index as
`REM_Offline_Engineering_Notes.pdf`). Include `REM_SAMURAI_STATUS.md` and
`REM_YASH_STATUS.md` for tool acceptance boundaries.

## Source projects

Carry `toolchain/examples/flight-kit/`, `toolchain/examples/`,
`toolchain/userspace/minilibc/`, `toolchain/userspace/myemulator2-user.ld`,
`toolchain/chibicc-rem/` and `docs/REM_C_COMPILER_STATUS.md`. Carry the
syscall-reference generator with the kernel dispatcher source if the syscall
table may need regeneration.

## Guest files required for hosted C

Put these into the image as one matched, validated runtime tuple:

- Native REM chibicc executable.
- Native REM `as`, `ld`, and recommended `ar`, `ranlib`, `nm`, `objdump`,
  `readelf`, and `objcopy`.
- musl target headers and generated `bits` headers.
- `crt1.o`, `crti.o`, and `crtn.o`.
- musl `libc.a`, matching `libgcc.a`, and `myemulator2-user.ld`.

The workspace now contains a separate integration staging copy at
`toolchain/flight-kit-runtime/`. It contains the native chibicc executable,
its builtin headers, native `as`/`ld`, musl headers, CRT objects, `libc.a`,
`libgcc.a`, and linker script. Hashes for the compiler, tools, CRT, libc,
libgcc, and script match the validated tuple in
`REM_VALIDATED_RUNTIME_TUPLES.md`. The staged files are copies; they do not
modify a flight image. See the staging README for guest destination paths and
the precise validated limits. `.musl-install/` remains absent; `.musl-build/`
is an incomplete build tree and is not the source of the staged runtime.

`.toolchain-install/` also contains host-executable cross binutils/GCC
artifacts and compiler-runtime archives. Those are distinct from the native
REM tools staged above. The flight image integration must copy the staged
matched set into its rootfs and preserve the recorded paths/permissions.

The source tree also has no native guest build of Samurai (`samu`) or a
guaranteed in-guest Ninja binary. The files `build.ninja.*` are offline
examples and require the acceptance described in `REM_SAMURAI_STATUS.md`.
The flight image's shell/editor inventory is image-specific; `REM_YASH_STATUS.md`
records Yash/Kilo staging and acceptance constraints.

## Workstation-only build resources

For rebuilding REM itself, additionally keep `toolchain/scripts/`, the Linux
and QEMU sources, cached compiler/binutils/musl source archives, build
configuration and adequate persistent storage. This is not the recommended
12-hour guest programming workflow.

## Preflight

Run `tools/check-offline-flight-kit.sh` against the assembled source bundle.
Then verify the **guest image** has its compiler, sysroot, tools, editors and
starter project files; a workstation check cannot prove image contents. Keep
QEMU, kernel, rootfs, toolchain, CRT, libc, libgcc and compiler together using
the hashes recorded in `REM_VALIDATED_RUNTIME_TUPLES.md`.
