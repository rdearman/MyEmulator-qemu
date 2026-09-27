# REM Bash compatibility probe — 2026-09-27

## Scope and exact test tuple

The tests ran one Bash workload at a time in disposable copies of
`.rem-flight-build-20260924/rootfs.ext4`; no kernel, Bash binary, libc, or
flight image was changed. BusyBox `ash` launched each noninteractive Bash test.
The interactive probe launched Bash from the parent shell on the serial
console.

| Artifact | SHA-256 |
|---|---|
| QEMU `.rem-phone-deploy-20260925/REM-FLIGHT/qemu/qemu-system-myemulator32` | `0154db549ad01d0bcdfd79c0dde9544266fc14c75fe7825d14942ff47a75641a` |
| Kernel `.rem-phone-deploy-20260925/REM-FLIGHT/boot/vmlinux` | `c77547060463da37cf9730e41e799c94998ad9f6e24e2c1a3282ee3299215a63` |
| Rootfs `.rem-flight-build-20260924/rootfs.ext4` | `2c608032df8fddf82240d67a4ae42790ce2a439a5ad4968f6e9e8d495594c365` |
| Guest `/usr/bin/bash` | `393c6188fea2776d0b91579c879c0bc72dacf53e34593295dab9ffc299731a43` |

The Bash binary is static ELF32 for REM machine `0xF2E2`. Its ELF `.comment`
identifies GCC 15.2.0. The repository has no Bash source checkout or build
recipe, so this session could not do a clean rebuild or establish the exact
compiler options and matching static libc provenance.

QEMU used machine `myemulator32`, 128 MiB RAM, ext4 rootfs, serial console,
and `-icount shift=0,sleep=off`. Reports and logs are under
`/tmp/rem-bash-compat-20260927/`,
`/tmp/rem-bash-echo-trace-20260927/`,
`/tmp/rem-bash-fault-20260927/`, and
`/tmp/rem-bash-interactive2-20260927/`.

## Results

| Workload | Result | Evidence |
|---|---|---|
| `bash --version` | PASS | Printed GNU Bash 5.2.37 and returned 0. |
| `bash -c 'echo hello'` | CRASH | Guest printed `Segmentation fault`; shell reported status 139. |
| `bash -c` simple command, variable, function, command substitution, redirection, child script, exported environment | CRASH | Each isolated case returned 139 with `Segmentation fault`. The tests do not establish which internal Bash path first faults. |
| Bash pipeline using `printf` and BusyBox `tr` | CRASH (repeat); FAIL (first run) | Both runs printed `Segmentation fault`; the first returned 13 and the repeat returned 139. The failing process in the pipeline is not identified. |
| Interactive Bash startup | FAIL | Bash reported `setpgid: Function not implemented`, `cannot set terminal process group`, and `no job control`; it did not reach its prompt. Kernel log then reported a Bash user fault at `PC 0x0207f214`, address `0x000040e8`, cause 7. REM `objdump` places the PC at `check_mail+0x1b4`; no source line is available. |

The `setpgid` error and the later memory fault are separate observations. The
tested tuple returned `ENOSYS` for Bash's `setpgid` request. Current tracked
REM syscall source and reference list a `setpgid` dispatcher case (number 154),
but the working-tree source does not prove that this case is in the tested
kernel binary; kernel/libc tuple skew or a syscall-number mismatch remains an
open integration question, not a proven defect in either source component.
The subsequent fault occurs
inside Bash `check_mail`; its cause remains unclassified among generated code,
ABI, static libc/runtime, and kernel/process behavior.

The Bash fault line was visible on the serial console for the interactive
case. For noninteractive cases, the test harness attempted to retrieve the
kernel ring buffer with BusyBox `dmesg`, which reported
`klogctl: Function not implemented`; that mechanism cannot currently provide
fault PCs from userspace.

The complete repeat run at `/tmp/rem-bash-complete-20260927/report.json`
reported 10 crashes and one pass. Each Bash test result records the exact
binary SHA-256, GCC provenance, command/minimal reproducer, status, and a
fault-PC field. The field is null where the noninteractive serial transcript
did not expose a PC. The first run differs for the pipeline's exit status,
which should be retained as a repeatability issue rather than normalized.

## Classification and changes

- **CPU architecture:** no evidence of an instruction-set defect.
- **Compiler/backend and ABI:** Bash was built with GCC 15.2.0 and faults in a
  low-address access, but the available evidence does not attribute the fault
  to generated code or the ABI.
- **Libc/runtime:** Bash is statically linked; matching libc build provenance
  is unknown. No libc-specific failure was isolated.
- **Syscall/kernel:** `setpgid` returned `ENOSYS` in this tested tuple;
  userspace `dmesg` also found `klogctl` unavailable. The first result may be
  stale kernel/libc integration. The current source's dispatcher entry does
  not establish behavior of the tested kernel binary.
- **Filesystem:** no filesystem error was observed in these shell workloads.
- **Application:** Bash version works; command execution and interactive
  initialization do not. The `check_mail` fault is observed inside Bash but
  not yet explained.
- **Tooling/integration:** tests now run in isolated image copies and record
  artifact hashes. Fault collection must account for the unavailable
  `klogctl` path.

This work changed only test infrastructure: added Bash cases to
`toolchain/userspace/guest-regression.sh`, added `--dump-faults` to
`toolchain/scripts/test-userspace.py`, and added the interactive serial test
`toolchain/scripts/test-bash-interactive.py`. Shell syntax and Python syntax
checks passed. No Bash, compiler, libc, kernel, or flight-package fix was
made.

## Next work

1. Obtain the Bash source and recreate a clean target build with recorded GCC,
   libc, and linker inputs.
2. Retest `setpgid` on the exact tested kernel/libc tuple and compare it with a
   newly built kernel carrying the current dispatcher source.
3. Capture the noninteractive Bash fault through a serial/QEMU diagnostic
   path; `dmesg` cannot do it while `klogctl` returns `ENOSYS`.
4. Re-run command execution and interactive startup before attributing the
   fault to the compiler, ABI, libc, or kernel.

The expanded cases are regression probes, not Bash acceptance. See the
[test fixture](../toolchain/userspace/guest-regression.sh),
[noninteractive runner](../toolchain/scripts/test-userspace.py), and
[interactive runner](../toolchain/scripts/test-bash-interactive.py).

## Controlled source build and rerun — 2026-09-27

The old binary results above remain useful historical evidence, but they do
not describe the controlled build below. Bash 5.2.37 was obtained from the
official [GNU release archive](https://ftp.gnu.org/gnu/bash/bash-5.2.37.tar.gz)
and built under `/tmp/rem-bash-controlled-20260927/`; no production image or
component source was changed.

| Input/artifact | Provenance | SHA-256 |
|---|---|---|
| Bash source archive | GNU `bash-5.2.37.tar.gz` | `9599b22ecd1d5787ad7d3b7bf0c59f312b3396d1e281175dd1f8a4014da621ff` |
| Target GCC driver | `.toolchain-install/bin/myemulator2-elf-gcc`, GCC 15.2.0, target `myemulator2-elf` | `07ef3aaab8ce4ce47c0a093a4b7b2590e98c936e6e8a651d7261e0f07f0087fe` |
| Target linker | Binutils 2.46.0.20260210 | `47686759160071e83b256c5ec35857ee8662dd2ea201aab17909716464772128` |
| Target assembler | Binutils 2.46.0.20260210 | `65fec0ac525081a9b0a741d87988ea54644d02b964660f17fa04a5930dc92fff` |
| Static musl libc | `.rem-flight-build-20260924/musl-prefix/lib/libc.a` | `312c6c393d9b879448edd28a822ec4fd56e1f40e364764c5da77a0df884ba21f` |
| Static terminfo library | `.rem-flight-build-20260924/musl-prefix/lib/libtinfo.a` | `fdbf51e3bb6bdf08855cfd82d98391e217767eee33107e3fa21fb7f860f941f0` |
| GCC runtime | `.toolchain-install/lib/gcc/myemulator2-elf/15.2.0/libgcc.a` | `4cc9ae3d89d1600d31f0cc93dca34244c9fd14f027259c6a715e8cdc4f53d1e6` |
| CRT startup objects | `crt1.o`, `crti.o`, `crtn.o` in the musl sysroot | `76fd0c9d1f3aa11e885a0610553021b7c98d456afb38141fd09da5e1181ce609`, `7fb830f63e7ba115f433b6f28253e8510bbc7b0ab0708c62a2a0ac94aeba5f37`, `5594cc2bd0fa13d22e159c78c1f3065b7f36ae217badcdeaf8e71b5c3ad64937` |
| REM user linker script | `toolchain/userspace/myemulator2-user.ld` | `6ec14dea3bda5ded2ef8b23ac1995d6b40857e2273ead8e095d57f25e8a54621` |
| Built Bash | static ELF32, REM machine `0xF2E2` | `23d8e33f62117745c7127f08128b400be2329001f135fc1f31d96f9fc4c63e1a` |

The Bash ELF has no dynamic section; its program headers contain an RWE load
segment and an executable `GNU_STACK`, matching the linker's warnings.

The build used `CFLAGS='-std=gnu17 -O2 -pipe -fno-strict-aliasing'` and
`LDFLAGS='-static'`, configured with
`--without-bash-malloc --disable-nls --enable-static-link`, then built with
`make -j2 LOCAL_LDFLAGS=`. GNU `config.sub` does not recognize the REM target
name, so `--host=mipsel-linux-musl` was used as a little-endian Linux
classifier; GCC still emitted the custom REM ELF machine. The temporary
Linux-musl compiler wrapper used the recorded GCC, musl sysroot, CRT objects,
linker script, and `-lc -lgcc`; its SHA-256 was
`261f7f8d9263cd49d72039a26a6bb5ea626f29a00b7ceab78d33d106c2707fe5`.

Build constraints observed:

- Without `-std=gnu17`, Bash's historical `extern ... list_reverse ()`
  declaration was treated as a zero-argument prototype and compilation failed
  at calls that pass one argument. GNU17 mode compiled the source without
  changing it.
- The target libc header declares `sigsetjmp`, but the selected `libc.a` has no
  `sigsetjmp` definition. Cross configure defaults that runtime probe to
  present; the build therefore supplied `bash_cv_func_sigsetjmp=missing`.
  This enables Bash's non-`sigsetjmp` path and does not repair or validate
  signal-mask semantics in this libc.
- Bash's configured `-rdynamic` link option is unsupported by the target GCC
  driver. It was removed for this static build using `LOCAL_LDFLAGS=`. The
  linker also printed `.eh_frame_hdr`, executable-stack, and RWX-segment
  diagnostics while returning success and producing the binary.

The pre-existing `vim-build/bin/myemulator2-linux-musl-gcc` and
`hosted-gcc-prefix/bin/myemulator2-elf-gcc` wrappers delegated back to each
other for ordinary invocations, so configure stalled at the compiler probe.
The build used a temporary wrapper around the installed GCC driver instead;
it did not alter either repository toolchain wrapper. GCC's default
`__STDC_VERSION__` was `202311L`; GNU17 sets it to `201710L` and avoids Bash's
old-style prototype compile error.

## Controlled runtime results

The clean test base was the rootfs above (SHA-256
`2c608032df8fddf82240d67a4ae42790ce2a439a5ad4968f6e9e8d495594c365`). The
disposable image with only the controlled Bash and matrix fixture had SHA-256
`9e476d3601da4283b13cedfdf855350158a2e53b83dd41482823249c3dd2437d` before
each run. The matrix fixture SHA-256 is
`4016e20d5a95c42699d290a5d2d86ac38a13528b6f6ae52992965dc6fb473072`.
`e2fsck -fn` reported no errors on this candidate before boot. QEMU and kernel hashes were respectively
`0154db549ad01d0bcdfd79c0dde9544266fc14c75fe7825d14942ff47a75641a` and
`c77547060463da37cf9730e41e799c94998ad9f6e24e2c1a3282ee3299215a63`; tests
used `myemulator32`, 128 MiB, serial console, and
`-icount shift=0,sleep=off`.

The corrected same-image matrix passed twice. Both reports record the same
QEMU, kernel, rootfs, and Bash hashes:
`/tmp/rem-bash-controlled-20260927/matrix-sh5/report.json` and
`/tmp/rem-bash-controlled-20260927/matrix-sh6/report.json`.

| Test | Result | Evidence |
|---|---|---|
| `bash --version` | PASS | GNU Bash 5.2.37, exit 0. |
| `bash --version` with output redirected and checked | PASS | Exit 0 and exact version string found in the output file. |
| `bash -c 'echo hello'` | PASS | Exact `hello` output; exit 0. |
| Variables, functions, command substitution | PASS | Each Bash assertion returned 0 and emitted its unique success token. |
| Pipeline | PASS | Bash piped `printf` into installed BusyBox `cat`; equality check and success token passed. |
| Redirection | PASS | Bash wrote and read an exact sentinel from a file. |
| Script | PASS | Bash executed a file that checked `BASH_VERSION`; success token observed. |
| Environment | PASS | Exported value was inherited by Bash and checked successfully. |
| Interactive startup/input/exit | PASS | Separate clean-image report `/tmp/rem-bash-controlled-20260927/interactive-clean/report.json`; prompt, host input/guest response, and clean exit passed. Bash still printed `setpgid: Function not implemented`, `cannot set terminal process group`, and `no job control`. |

The first matrix revision attempted `printf | BusyBox tr`, but this rootfs has
no `tr` applet; that fixture attempt failed and did not test Bash pipeline
compatibility. Replacing `tr` with installed `cat` produced the passing
pipeline case above. An initial combined invocation on a previously booted
and mutated disposable image printed `ext4_lookup ... inode #9807 ... checksum
invalid` and `Bad message` while reading the injected test script, and also
printed `Segmentation fault`. This was a test-image integrity/tooling failure,
not a Bash result. The exact trigger was not isolated; rebuilding the candidate
from the original rootfs and checking it with `e2fsck -fn` avoided it. The
segmentation fault did not reproduce on the clean image or the two passing
matrix reruns. Keep that failed attempt separate from the clean-tuple results.

## Process and terminal syscalls

The separately compiled probe
`toolchain/tests/bash-process-probes.c` has SHA-256
`7cba15a85b110a0c2bb8249569703393ba9b38597bb6d087d3c042b9b16680ab` and was
run on QEMU `0154db...`, kernel `c77547...`, and disposable rootfs
`c049d90b5bc90307dbcd0f299ad37e08c3c843cc08d0ba91d18c2461c78dd7d7`. Full
report: `/tmp/rem-bash-controlled-20260927/probe-clean/report.json`.

| Call | Return | errno | Observation |
|---|---:|---:|---|
| `setpgid(0, 0)` | `-1` | `38` (`ENOSYS`) | Requested process-group operation is unavailable in this tested tuple. |
| `fork()` parent | `22` | `0` | Child PID returned. |
| `fork()` child | `0` | `0` | Child path ran. |
| `execv("/bin/busybox", {"busybox", "true"})` | does not return on success | n/a | `waitpid` observed the replaced child exit normally with status 0; failure path reports `-1` and errno. |
| `waitpid(child, ...)` | `22` | `0` | Reaped the child; exit status 0. |
| `tcsetpgrp(0, getpgrp())` | `-1` | `22` (`EINVAL`) | Process-group setup was unavailable on this tested tuple, so the terminal foreground-group request could not be established. |

Classification: on this exact tested tuple, `setpgid` returned `ENOSYS`; the
underlying kernel/libc mismatch is not determined by this probe. `fork`,
`exec`, and `waitpid` work. `tcsetpgrp` failed with `EINVAL` after process-group
setup was unavailable. This is a POSIX job-control gap, not a blocker for the
tested Bash shell functionality.

## Current assessment

The controlled Bash 5.2.37 build from GNU source, static REM ELF execution,
all listed noninteractive shell features, interactive input/output, and the
`fork`/`exec`/`waitpid` probes pass. POSIX job control is the sole accepted
compatibility gap: `setpgid` returned `ENOSYS`, and `tcsetpgrp` could not set
the foreground process group on this tuple. This result is tuple-specific:
the later terminal/session probe documented in
[`REM_VIM_COMPATIBILITY.md`](REM_VIM_COMPATIBILITY.md) succeeds for
`setpgid` on a different kernel and demonstrates `tcsetpgrp` after acquiring
a controlling terminal. No Bash source or production artifact change was
made. The build required explicit language mode and a configure override for
the current libc archive. The reproducible matrix fixture is
[`bash-compat-matrix.sh`](../toolchain/tests/bash-compat-matrix.sh), and the
syscall probe is
[`bash-process-probes.c`](../toolchain/tests/bash-process-probes.c).
