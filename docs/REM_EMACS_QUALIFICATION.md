# REM Emacs qualification

## Result

**Not qualified for flight use.** GNU Emacs 30.1 is present in the test
rootfs, configured for terminal-only use (no X, static, with terminfo), but it
does not reach terminal initialization on the tested REM tuple. Neither
interactive editing nor save/exit was validated.

## Evidence

- Existing `/usr/bin/emacs` (SHA-256
  `b5f1d6b3ad35d3931aa9d24053e9a734a99f41d7870a48150094a20177fbc709`) crashes
  even for `--version` and `--batch -Q`. The fault is at PC `0x021b1bf8`,
  address `0x460`, with FP `0x450`.
- Disassembly places the fault in `make_pure_string()`. Its caller,
  `find_string_data_in_pure()`, has a 1104-byte local frame containing
  `ptrdiff_t bm_skip[256]`; target GCC-generated code restores FP as `0x450`
  (the frame size), then the caller dereferences `FP+16`. This is evidence of
  a target GCC backend frame-pointer/callee-save code-generation defect on
  this workload, not a terminal-library failure.
- The minimal reusable reproducer is
  [`emacs-frame-regression.c`](../toolchain/tests/emacs-frame-regression.c).
  It has a 1104-byte local frame in `large_callee()` and makes `outer()` read
  its own saved local after that call. With `-O0`, the GCC binary from the
  Emacs build tree (SHA-256
  `6e784d23724aed49a99c3b9b64bdfc1d79960a2fd81a282de59affdd552d862c`)
  emits `li r12, 1168; sub sp, sp, r12; sw r12, 4(sp); ... lw r15, 4(sp)`.
  This saves the frame-size constant as the caller FP. In bare-metal QEMU,
  this build returns `1` instead of expected `0`. Current repository GCC
  (SHA-256
  `07ef3aaab8ce4ce47c0a093a4b7b2590e98c936e6e8a651d7261e0f07f0087fe`)
  emits split stack decrements and restores the saved caller FP; its same
  executable returns `0`.
- Exact QEMU register-trace results at the entry to the CRT halt loop were:
  old GCC `R1=1` (expected 0), current GCC `R1=0`, host REM chibicc `R1=0`.
  Each QEMU process was stopped by the host timeout after reaching that loop.
  No libc, syscalls, kernel image, or filesystem were involved in this
  reproducer. The regression source SHA-256 is
  `79a11d1d6df6279ad9bf42982bbeff5238f8e39e88c26f65146331f787af90d1`.
  Result ELF hashes: old GCC
  `44bc119d5b492db7de5a39db3011c5767c03dadfebef1a9a1ac9ad4f6b33df6f`,
  current GCC
  `d302f13343b4a374f79b8d0a61eee6eeda91b2c06d69e502a6c9a44756c0ef91`,
  chibicc
  `b4bbbc29400d5b8f987eae8fcb252c5116658e1c710b37795f0925ca1392a67a`.
- The host-built REM chibicc port also compiled the reproducer to REM assembly;
  assembling/linking and running it under bare-metal QEMU returned `0`. This
  stress case does not reproduce in current chibicc. The target-native chibicc
  ELF was not run in this test. Host cross-chibicc SHA-256:
  `7ae0af85b2d942b3c800748c742f011072eff9ab310a0c6c746c8a71583e6b3d`.
- A local-only experiment moved `bm_skip` to heap storage and added matching
  frees. The object compiled and a `temacs` executable linked (SHA-256
  `de54b3f97c4f4b512089815dea29b27cfb06faaf466ad0d3b646a35048032ce0`).
  Completing the build later stopped when its copied build tree tried to
  regenerate documentation through a read-only linked source directory.
- That experimental executable was substituted only into the disposable
  `/tmp/rem-make-gnu-validation-codex6/final-rootfs.ext4` test image. On the
  same kernel it failed at PC 0 during initial file-backed ELF page mapping;
  the kernel reported `MYEMU_BAD_USER_FAULT ... pc=00000000` and repeated
  `BUG: Bad page map` diagnostics for the executable. The experiment linked
  with a warning that `_start` was missing, so this does not yet isolate a
  kernel ELF-mapping defect; the experimental executable or its link recipe
  may be invalid. The workaround has not passed guest execution.

Test tuple: kernel SHA-256
`c77547060463da37cf9730e41e799c94998ad9f6e24e2c1a3282ee3299215a63`; QEMU
SHA-256 `0154db549ad01d0bcdfd79c0dde9544266fc14c75fe7825d14942ff47a75641a`.
No production flight image, kernel, compiler, Samurai, or Make artifacts were
modified.

## Rebuild assessment

The Emacs binary came from a build tree whose configured compiler is the old
GCC hash above. Recompiling the original `alloc.c` with current repository GCC
using Emacs's recorded C flags succeeds and emits the corrected large-frame
save/restore pattern. A full Emacs rebuild was not completed: the prior local
workaround build could not regenerate `../etc/DOC` through its read-only source
link, and its experimental link omitted `_start`. The original Emacs binary
must be rebuilt/relinked with the corrected compiler and valid startup objects
before terminal interaction can be assessed.

The existing `toolchain/scripts/test-gcc-large-frame.py` assembly checks also
passed before its QEMU helper was blocked by sandbox denial of its QMP Unix
socket (`PermissionError`). The independent direct QEMU register-trace runs
above provide the runtime comparison without QMP.

## Corrected-toolchain rebuild follow-up

The earlier rebuild assessment above is historical. A later isolated target
tree rebuild recompiled Emacs 30.1 with current GCC
`07ef3aaab8ce4ce47c0a093a4b7b2590e98c936e6e8a651d7261e0f07f0087fe`, then
linked with the REM assembler/linker, musl CRT/libraries, and the explicit REM
linker script. It produced a static ELF32 REM executable with `_start` at
`0x02000094`, SHA-256
`bb4d31623a70a3e314de017791202b8ef0610202c41f4ac6c33c121f627022d6`.
The disposable guest used QEMU SHA-256
`0154db549ad01d0bcdfd79c0dde9544266fc14c75fe7825d14942ff47a75641a` and
kernel SHA-256
`c77547060463da37cf9730e41e799c94998ad9f6e24e2c1a3282ee3299215a63`.

The corrected binary still does not pass startup. `/usr/bin/emacs --version`
prints no version; in a 128 MiB guest it terminates with SIGSEGV. Kernel
diagnostics record PC `0x022f8464` (`__malloc_alloc_meta`), write fault at
`0x3f7d6fe4`, SP at that address, FP `0x3f7d7068`, LR `0x022f8b04`
(`alloc_slot`), and no VMA. Initial stack top in `r11` is `0x3ffd6dbc`, so
the fault is 8,388,056 bytes below it, consistent with exhausting the default
8 MiB stack limit. A current-GCC isolation probe is in
[`startup-stack-probe.c`](../toolchain/tests/emacs-stack/startup-stack-probe.c)
(binary SHA-256
`76fa6fb9c87e51ba780b578d9217f14e1cf708ad58e04a152d97c69e6497d0cc`, built
with GCC SHA-256
`07ef3aaab8ce4ce47c0a093a4b7b2590e98c936e6e8a651d7261e0f07f0087fe`). In the
same guest tuple, its `RLIMIT_STACK` was 8,388,608 bytes; touching 7 MiB of
stack and calling `malloc(4096)` after touching 7.5 MiB both completed. The
4 KiB malloc/free case also completed. Its initial `brk` was `0x02013000`;
the small malloc raised it to `0x02015000`, while stack allocation did not
change it. This argues against ordinary malloc/free corruption or a `brk`
failure as the cause of Emacs's fault: Emacs reaches the stack limit and the
allocator's stack-frame save is where the rejected growth becomes visible.
The probe's recursive mode showed the user SP descending toward the same
boundary; the serial harness did not capture its final signal status.

One 16 MiB-limit monitor run sampled kernel PC `0x00202e0c`
(`__mmap_region`) with user SP `0x3f82a3e4`; a separate run sampled
`mas_wr_node_store` with user SP `0x3f45fffc`. Another sample was in
`arch_do_signal_or_restart`. These show Emacs progressing through stack and
VMA-related kernel paths under the raised limit, but the runs did not capture
syscall arguments or a conclusive exit status. They do not establish an mmap
failure or deadlock.

Procfs maps could not be read (`/proc/self/maps` returned `ENOENT`), and
mounting procfs failed with `Function not implemented`; kernel diagnostics
report `vma=0` after failed stack expansion, so no VMA range is available.
A 9 MiB alloca passed with a 16 MiB soft stack limit, demonstrating that the
guest allows growth beyond 8 MiB when the process limit is raised. A 15 MiB
alloca under a 32 MiB limit did not complete before harness timeout. Emacs
`--version` under a 16 MiB limit produced no output or fault before timeout;
this does not establish that the larger limit is sufficient for startup. The
immediate failure is stack-limit exhaustion at the default limit, but what
in Emacs startup consumes that stack remains unknown. Compiler-generated
stack use, ABI/runtime interaction, and application startup behavior remain
possible contributors; no fix is validated.

An initial diagnostic probe was mistakenly built with the older Emacs-project
GCC and its result was discarded; the probe results above use the repository
GCC hash recorded here. This prevents the known older-compiler frame defect
from being mistaken for current allocator behavior.

## Startup phase trace (still incomplete)

A temporary Emacs `main` instrumentation build was linked from the corrected
target objects and run as `/tmp/emacs-startup-trace --version` in the
disposable guest. The trace binary SHA-256 was
`51df22a816b39d285b25456c4e7f7ed13fde4b3fe2655a6a5bedb4cb822d4fd0`;
instrumentation existed only in `/tmp` and the disposable rootfs. It confirmed
successful return from argument sorting, `getcwd`, stack-limit handling,
`setlocale`, and `init_signals`. Earlier coarse markers in this investigation
also reached `init_alloc_once`, `init_charset_once`, `init_coding_once`,
`init_alloc`, and `init_threads`.

The finer trace reached `init_random()` after `init_threads()` and emitted no
subsequent `init_xfaces()` marker. In Emacs 30.1's `src/sysdep.c`,
`init_random()` calls `getrandom(&v, sizeof v, 0)`, so the next operation is a
blocking kernel entropy request. A standalone native REM probe printed
`GETRANDOM_BEFORE` and remained alive after three seconds without a return
marker; the harness killed it. A second probe called `getrandom()` with
`GRND_NONBLOCK` and printed `GETRANDOM_NOT_READY`, which is its explicit
`EAGAIN` branch. The source is
[`getrandom-early-boot-probe.c`](../toolchain/tests/emacs-stack/getrandom-early-boot-probe.c),
SHA-256 `5e4255aebd5ad43cf46b3de8bdc6492c2fa0256136ee2166fe05df18f6a7e85b`;
the tested native executable SHA-256 is
`a2c48db59a58f17470cdfeefd91fd82c9abb82b0a16b3c1bc9d7c111ab73c701`.

The REM kernel has the `getrandom` syscall at number 278, also declared by
musl; `GRND_NONBLOCK` returning `EAGAIN` shows the call is implemented and the
kernel's urandom source is not ready, rather than the syscall being absent.
Linux specifies that flags-zero `getrandom()` waits until this source is
initialized ([`getrandom(2)`](https://man7.org/linux/man-pages/man2/getrandom.2.html)).
The immediate startup blocker is therefore the unready entropy pool: Emacs
blocks correctly under the Linux contract. The inspected
`.rem-phone-deploy-20260925/linux-network-build/.config` has
`CONFIG_HW_RANDOM` and `CONFIG_CRYPTO_JITTERENTROPY` disabled, and the current
QEMU machine source creates console, block, and optional virtio-net devices
but no RNG device. The kernel build configuration was not matched to the exact
deployed kernel hash, so these are supporting platform evidence rather than
proof of the tested image's complete entropy setup. The exact mechanism for
initializing this guest's pool remains untested.

This trace did not produce a new Emacs stack-fault frame. The earlier
uninstrumented run still records a default-stack SIGSEGV in
`__malloc_alloc_meta` at the 8 MiB stack boundary; whether that is a separate
failure or a later consequence after entropy is ready remains unresolved.
`--version` has not reached version output, and terminal setup, interactive
startup, batch execution, and editing remain untested.

No corrected Emacs startup, `--batch`, `-nw`, edit/save, or Yash readback test
has passed. The binary is not qualified. The large-frame compiler regression
passes, but it is not sufficient coverage for this workload. See the
corresponding architecture observation in
[`REM_ARCHITECTURE64.md`](REM_ARCHITECTURE64.md#large-frame-compiler-output-must-preserve-the-caller-frame).

## Current first-stop classification

For the corrected-toolchain traced run, the last confirmed completed startup
functions include argument sorting, `getcwd`, stack-limit handling,
`setlocale`, `init_signals`, and `init_threads`. The first operation observed
not to return is `init_random()`'s flags-zero `getrandom()` call. The next
marker, after `init_random()` and before `init_xfaces()`, was not emitted.
The call path established by the markers and source is:

```text
main -> init_threads() -> init_random() -> getrandom(&v, sizeof v, 0)
```

The native nonblocking probe's `EAGAIN` result and the flags-zero probe's
continued wait identify entropy readiness as the current first startup
blocker. This is not evidence of a fault in Emacs's stack, allocator, locale,
signal, or filesystem paths: the trace did not observe a crash at this
boundary. The syscall dispatch exists, and the observed wait is consistent
with the Linux blocking contract when the urandom source is not initialized.
Thus classify the immediate result as an **application-triggered platform
readiness/integration gap**, with no demonstrated violation of the
`getrandom` syscall contract.

The earlier `__malloc_alloc_meta` write fault at the default stack boundary
is a separate observation from an uninstrumented run. It was not reproduced
in this trace, and there is no evidence yet that it lies on the call path
before the entropy wait or that it is caused by `getrandom()`. Keep its cause
open. No call stack beyond the instrumented function boundary was captured;
the trace therefore identifies the first non-returning operation, not a
post-entropy startup path or the cause of the earlier fault.
