# Codex #4 handover

## Current state

The latest task compared unmodified Samurai built with the validated REM GCC
cross compiler against the current chibicc-built Samurai. The comparison
isolates a compiler-output difference on `samu -n`: GCC-built Samurai passes;
the chibicc-built binary faults. This is not full Samurai acceptance: a real
build and incremental rebuild were not run.

No Samurai, chibicc, kernel, libc, production image, or production artifact
was modified. Test outputs are disposable under
`/tmp/rem-samu-gcc-compare-20260927/`.

## Samurai comparison

### Inputs and provenance

- Unmodified Samurai git revision:
  `531ba700c5fbbfd3d8e3ed91226d0c57134a6fb3`.
- GCC 15.2.0 driver:
  `/tmp/rem-cross-gcc-clean/bin/myemulator2-linux-musl-gcc`
  SHA-256 `abb88fb6cbd255641f68529c99ca1b2c389007fcd91cca5bc5caa9fa2bbd3fb4`.
- REM assembler and linker used by the GCC driver:
  `.toolchain-install/bin/myemulator2-elf-as` and
  `.toolchain-install/bin/myemulator2-elf-ld`.
- GCC-built static REM ELF:
  `/tmp/rem-samu-gcc-compare-20260927/samu`
  SHA-256 `f84dad8dad2777ad0aad8e0472bf3cc5967119ff53293fc519b8dd6b2f4cf3ce`.
- Comparison chibicc-built binary:
  `/tmp/rem-samu-current-root/root/samu`
  SHA-256 `f183fbae5c76f5aa686e5e5a06cdc2fdc85b8025acd657395b096bb82dcfbffe`.
- Clean base ext4 image before guest runs:
  `/tmp/rem-samu-gcc-compare-20260927/candidate.ext4`
  SHA-256 `ac7c839c1c1c7fac93cdcc6a681099574b92b786f41100f3f0b095176e0ff88d`.
- QEMU SHA-256 `0154db549ad01d0bcdfd79c0dde9544266fc14c75fe7825d14942ff47a75641a`.
- Kernel SHA-256 `5bd96e5d7e9163d95e8470eb550504357d84e78d38cd269781dfb096a511638a`.

The image used a new ext4 filesystem assembled from a temporary directory with
normal filesystem operations and passed `e2fsck -fn`. GCC and chibicc binaries
were tested in separate copies of that same image, using the same kernel and
QEMU tuple. `guest-test.py` and the per-variant console logs are in the
temporary directory above.

### Build recipe

All thirteen source files were exported from the pinned clean git revision and
compiled separately, without source edits:

```sh
CC=/tmp/rem-cross-gcc-clean/bin/myemulator2-linux-musl-gcc
SRC=/tmp/rem-samu-gcc-compare-20260927/src
OBJ=/tmp/rem-samu-gcc-compare-20260927/obj
for f in build deps env graph htab log parse samu scan tool tree util os-posix; do
    "$CC" -O2 -std=c99 -D_POSIX_C_SOURCE=200809L \
      -Wall -Wextra -Wshadow -Wmissing-prototypes -Wpedantic \
      -Wno-unused-parameter -c "$SRC/$f.c" -o "$OBJ/$f.o"
done
"$CC" -static -o /tmp/rem-samu-gcc-compare-20260927/samu "$OBJ"/*.o -lrt
```

The link returned success and produced a static ELF32 for REM machine
`0xf2e2`. The REM linker emitted diagnostics about `.eh_frame_hdr` and an RWX
load segment; these did not prevent the executable from being produced or
run.

### Results

| Test | GCC-built binary | chibicc-built binary |
|---|---|---|
| `/root/samu -h` | Prints usage; raw process exit 2 | Same |
| `cd /root; /root/samu -n` | PASS, exit 0, prints the expected dry-run edge | FAIL, `MYEMU_BAD_USER_FAULT`, shell exit 139 |

Samurai's source has no `-h` case; the usage output and raw exit 2 are its
response to an unsupported option. The guest shell reported 130 for that
external process because of the separate wait-status issue below.

For the failed chibicc dry run, the fault is at `pc=0x020359b8`,
`addr=0`, `r2=0`, `cause=7`. In the exact chibicc binary, `strcmp` starts at
`0x02035980`, so the PC is `strcmp+0x38`. The matching-source and
same-runtime comparison points to chibicc-generated output for this specific
`-n` failure. It does not establish that Samurai is fully usable; real and
incremental build tests remain open.

### Separate wait-status finding

A static GCC-built probe is at `waitprobe.c` (binary SHA-256
`c7a20cfed25a7c30134bc41efcd5695c9450ce92692a880275a30f4745708ff8`). A child
called `_exit(2)` and its parent collected status using `waitpid`. The raw
status was `0x0002`; `WIFEXITED` was false and `WIFSIGNALED` was true with
signal 2. Ash similarly reported exit 1 as 129 and exit 2 as 130. This is a
Linux wait-status syscall/kernel ABI issue, separate from the chibicc
comparison. No implementation fix was made.

## Previous BusyBox work

The separate BusyBox 1.37.0 retest passed `--help`, `ls`, `cp`, `mv`, `sh`, and
`printf` in a clean disposable REM image. `CONFIG_BUSYBOX` and `CONFIG_PRINTF`
had to be enabled in that temporary build config. The resulting binary hash
is `5b8e0e01ef6e673543ea5e2f3569063f412acbba0193bf9dc74dfc8a33a4ca85`;
the report is [REM_BUSYBOX_COMPATIBILITY_20260927.md](docs/REM_BUSYBOX_COMPATIBILITY_20260927.md).
There were no user faults or ext4 errors; a non-fatal `MYEMU llist bad add`
kernel diagnostic appeared in that test's console log.

## Documentation and follow-up

- [Samurai status and GCC comparison](docs/REM_SAMURAI_STATUS.md#GCC-cross-compiler-comparison-2026-09-27)
- [REM64 wait-status implication](docs/REM_ARCHITECTURE64.md#wait-status-must-use-the-linux-process-abi-encoding)
- [BusyBox compatibility report](docs/REM_BUSYBOX_COMPATIBILITY_20260927.md)

Next engineering steps belong with the relevant owners: isolate/fix the
chibicc-generated Samurai fault, correct Linux wait-status encoding in the
kernel, then rerun `samu -n`, a real build, and an incremental rebuild on one
pinned candidate. Codex #4 made no changes to those implementations.
