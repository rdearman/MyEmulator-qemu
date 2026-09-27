# REM BusyBox compatibility retest (2026-09-27)

## Result

BusyBox 1.37.0 passes the requested smoke paths in a disposable REM guest:

| Command | Result | Check |
|---|---|---|
| `busybox --help` | PASS | Usage and applet listing printed; exit status 0 |
| `busybox ls /` | PASS | Exit status 0 |
| `busybox cp` | PASS | Copied seeded file and read back `CP_SOURCE_OK` |
| `busybox mv` | PASS | Renamed copied file and read back `CP_SOURCE_OK` |
| `busybox sh` | PASS | `sh -c 'echo BB_SH_OK'`, exit status 0 |
| `busybox printf` | PASS | Printed `BB_PRINTF:OK:42`, exit status 0 |

The first build inherited a config with `CONFIG_BUSYBOX` unset, so its
dispatcher help mode was absent. That first `--help` result was not a runtime
failure. For the retest, `CONFIG_BUSYBOX=y` and `CONFIG_PRINTF=y` were enabled
in the temporary out-of-tree build config. No source or production image was
changed.

## Artifact provenance

| Input/output | SHA-256 |
|---|---|
| BusyBox source archive, 1.37.0 (pinned by build script) | `3311dff32e746499f4df0d5df04d7eb396382d7e108bb9250e7b519b837043a4` |
| REM GCC `.toolchain-install/bin/myemulator2-elf-gcc` | `07ef3aaab8ce4ce47c0a093a4b7b2590e98c936e6e8a651d7261e0f07f0087fe` |
| staged `libc.a` | `30cff68855d3793dedd63020928e65dfc084a5aa6667ef0114b25d6d92bd0924` |
| staged `libgcc.a` | `759fb011a7d4dfba4da8a96d17cc978485f2daf62c2ba4545180d4de40773044` |
| rebuilt BusyBox, `/tmp/rem-busybox-compat-20260927/build/busybox` | `5b8e0e01ef6e673543ea5e2f3569063f412acbba0193bf9dc74dfc8a33a4ca85` |
| disposable ext4 image, `/tmp/rem-busybox-compat-20260927/rootfs-help.ext4` | `8944d370431795271c33b674352627537131994b42aaa9d92c6f6fe59a1b857e` |
| kernel `/tmp/rem-yash-unified-build/vmlinux` | `5bd96e5d7e9163d95e8470eb550504357d84e78d38cd269781dfb096a511638a` |
| QEMU `REM-FLIGHT-TTY/qemu/desktop/qemu-system-myemulator32` | `0154db549ad01d0bcdfd79c0dde9544266fc14c75fe7825d14942ff47a75641a` |

The rebuilt executable is a statically linked little-endian ELF32 for REM's
machine value `0xf2e2`. The ext4 image was created with the normal rootfs
builder and passed `e2fsck -fn` before boot.

## Historical comparison and fault classification

`docs/REM_APPLICATION_AUDIT.md` records earlier `cp`/`mv` low-address
`MYEMU_BAD_USER_FAULT` failures (`0x30c`) from older BusyBox artifacts. The
handover identifies a stale pre-fix compiler artifact and the GCC alloca/VLA
stack fix (`b4dedb3`) as the historical compiler fault. In this retest both
filesystem applets complete and there are no `MYEMU_BAD_USER_FAULT` or ext4
errors. Thus that historical compiler fault did not reproduce; no new
compiler defect or minimal reproducer was found.

The only recorded anomaly is a `MYEMU llist bad add` kernel diagnostic in the
console log. It did not coincide with a failing command or stop this run; it
is classified as a kernel diagnostic requiring kernel-owner investigation,
not as evidence of a BusyBox/compiler, libc, syscall, or filesystem failure.

| Layer | Retest finding |
|---|---|
| Compiler | No new defect observed; historical `cp`/`mv` crash not reproduced |
| libc | No failure observed on tested paths |
| syscall | No user-fault or command failure observed |
| filesystem | ext4 check clean; `cp`/`mv` content checks pass |
| kernel | Non-fatal `MYEMU llist bad add` diagnostic; no kernel changes made |

## Reproduction locations

All test material is disposable under `/tmp/rem-busybox-compat-20260927/`.
`run2.py` records the six command results in `result.json`; `console-help.log`
contains the guest console trace. The test used the QEMU and kernel hashes
listed above and did not alter `REM-FLIGHT-TTY` contents.
