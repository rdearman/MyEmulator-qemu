# Samurai status on REM

This is an application-level probe of the native REM C toolchain. It does not
change the kernel, QEMU, flight rootfs, shells, or editors.

## Source and tuple

Samurai source revision: `531ba700c5fbbfd3d8e3ed91226d0c57134a6fb3`.
The staged source archive is SHA-256
`b782ef526d6d05b8236b5720295f6499332de7ca99894f360b3dc35b3705a9a3`.
The guest compiler used for the original build was `/tmp/chibicc-native-new`,
SHA-256 `a273c75bc88410cfc37a263f3164d441125bd6d73aa9d1dbd3e246d4e96540d3`.
The later clean rebuild used the corrected native compiler containing the
variadic save-area change; its host source/backend was rebuilt from the
current `toolchain/chibicc-rem` tree.
All experiments used disposable copies of the REM rootfs; `REM-FLIGHT-TTY`
was not modified.

The guest used the matching native tools, CRT and musl library, with the
REM `libgcc.a` staged at `/root/native/lib/libgcc.a`:

```text
CHIBICC_REM_LINKER_SCRIPT=/root/native/lib/myemulator2-user.ld
CHIBICC_REM_CRTDIR=/usr/lib
CHIBICC_REM_GCCLIB=/root/native/lib
```

The guest needed 64 MiB. With the 16 MiB default image, preprocessing the
large source was killed by the kernel OOM handler. This was a memory limit,
not a C-language failure.

## In-guest compilation and link

The unmodified upstream sources were compiled inside the running REM guest:

```text
for f in build deps env graph htab log os-posix parse samu scan tool tree util; do
    /root/chibicc -c -o /root/$f.o /root/samurai/$f.c || exit
done
```

The clean loop returned `COMPILE:0`: all thirteen unmodified Samurai translation
units compiled inside the guest and native `as` produced valid objects.

The resulting objects were then tested with the native linker.  The linker
failed before producing an output ELF:

```text
/root/chibicc -o /root/samu /root/build.o /root/deps.o /root/env.o \
  /root/graph.o /root/htab.o /root/log.o /root/os-posix.o /root/parse.o \
  /root/samu.o /root/scan.o /root/tool.o /root/tree.o /root/util.o
```

The native link returned `139` (SIGSEGV).  The same extracted objects link with
the host REM linker, so this is a native-binutils/runtime failure rather than
a Samurai or chibicc compilation failure.

The smallest reproduction is:

```text
native as hello.S -o hello.o       -> 0
native ld --version                -> starts
native ld -o hello hello.o         -> SIGSEGV (139)
native ld -T user.ld hello.o ...   -> SIGSEGV (139)
```

The fault is in the native linker's generated `yyparse`/linker runtime path.
An isolated native-binutils rebuild was attempted but did not complete because
the Canadian build still needs target-runtime/libtool configuration. No
resulting replacement linker was installed.

The host-linked comparison confirms the objects themselves are valid, but it
does not satisfy the native-link acceptance requirement:

```text
REM chibicc -> REM assembly -> native REM as -> native REM ld -> REM ELF
```

## Runtime result

`/root/samu -h` cannot yet be tested from the required native-link path because
no native-linked Samurai executable is produced.

A minimal Ninja file was then supplied and the following was attempted:

```text
/root/samu -n -f /root/build.ninja
```

`samu -n`, real builds, and incremental rebuilds remain pending until native
`ld` can link even the minimal program above.

## Compatibility and remaining issue

The port now handles the C features needed to compile all thirteen Samurai
translation units, including variadic APIs, switch statements, structures,
64-bit integer operations, function pointers and the required declarations.
No Samurai source was changed.

The remaining blocker is native `ld`; Samurai is no longer the first failing
component. Until that linker is corrected, Samurai cannot replace GNU Make for
REM workflows.

## Acceptance levels

```text
unmodified Samurai compiles in REM guest             PASS
native REM objects link to samu                       FAIL (native ld SIGSEGV)
samu -h executes                                      NOT REACHED
samu -n parses a build graph                          NOT REACHED
samu invokes REM chibicc for a C build               NOT REACHED
```

The next valid step is to repair or rebuild native `ld`, rerun the minimal
assemble/link test, and then resume the Samurai acceptance sequence. Do not
modify the Samurai source to hide the linker failure.

## Runtime debugging update

The fault was captured from the REM kernel:

```text
pc=020359b8 addr=00000000 lr=0201e5ec
r1=10000420 r2=00000000 r3=00000072
```

The PC is in the REM musl `memcpy` implementation; the link register maps to
`scankeyword` in Samurai. This showed that a scanner string copy was receiving
a null source pointer, rather than a failure to execute the program entry
point. A minimal REM program exercising global-struct `realloc` and indexed
writes passes, so the allocator and basic struct layout are not independently
broken.

The first concrete compiler defect found was variadic-local overlap. REM
variadic functions save r1-r4 in stack slots 16..28. chibicc previously placed
locals beginning immediately after named parameters, allowing a local
`va_list` to overwrite the saved variadic arguments. `assign_lvar_offsets` now
reserves that 16-byte save area for variadic functions. The REM stdarg header
also now uses the REM builtin va-list operations instead of the old x86-64
register-save structure.

The clean in-guest rebuild was then repeated: all thirteen translation units were
compiled again, assembled by native `as`, and linked by native `ld`. `samu -h`
again printed its usage text, but `samu -n` still faulted. The remaining fault
is not the original scanner-only symptom: the kernel now reports a fault in
musl's `printf_core`, reached through `vfprintf`, with a null/invalid format
path during graph parsing.

A smaller hosted test isolates the ABI mismatch without Samurai:

```c
void w(const char *fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    vfprintf(stderr, fmt, ap);
    va_end(ap);
}
int main(void) { w("x=%d\\n", 42); }
```

The same source generated by chibicc prints `x=33554684` (the format-string
address in the vararg position), while the equivalent REM-GCC-generated
caller prints `x=42`. A chibicc caller to a GCC-compiled variadic wrapper and
direct pointer inspection both work, so this is specifically an incompatibility
between chibicc's variadic call representation and the GCC-compiled musl
`vfprintf`/`va_copy` boundary. The Samurai source, musl sources, kernel and
QEMU were not changed. `samu -n` therefore remains **FAIL** pending that
compiler ABI fix in the backend is now identified and corrected in the host
source tree. The existing native chibicc binary still predates that change, so
no successful in-guest Samurai rebuild is claimed yet.

The isolated checks narrow the boundary further. A chibicc caller can pass a
`va_list` to a GCC-compiled variadic wrapper, and a chibicc function can
dereference its own variadic save area and recover `42`. The failure appears
only when the chibicc variadic caller invokes the GCC-compiled musl
`vfprintf` interface directly. This rules out a general allocator,
pointer-load, or filesystem failure, but the final compiler-side representation
mismatch remains unresolved.

The boundary investigation then identified the concrete rule: the REM ABI
requires the stack pointer to be 16-byte aligned at every call boundary,
including calls with arguments temporarily pushed by the caller. chibicc's
`gen_call` left `sp` eight bytes off alignment for a two-argument call such as
`printf("%d", 42)`. REM-GCC keeps the aligned frame and places those arguments
directly in `r1`/`r2`. The misalignment was tolerated by simple variadic test
functions but broke musl's GCC-compiled `vfprintf` frame/`va_copy` path.

The backend now reserves a padding amount before temporary argument pushes and
releases it after the call. Host-generated REM assembly and linked guest tests
show:

```text
chibicc printf("%d",42)       -> 42, exit 0
chibicc vfprintf wrapper      -> x=42, exit 0
REM-GCC printf("%d",42)       -> 42, exit 0
```

The native compiler was rebuilt from the corrected source and the clean,
verified REM image compiled all thirteen unmodified Samurai translation units.
The first apparent assembler failure was not reproducible after rebuilding the
image with ordinary ext4 staging; each object was produced successfully.

The current blocker is the native REM linker, before Samurai can be executed.
The native `ld` faults even on a minimal object assembled by the native `as`:

```text
as /root/hello.S -o /root/h.o       -> 0
ld -T myemulator2-user.ld ...       -> SIGSEGV (139)
```

The same extracted objects link successfully with the host REM linker, so this
is not a Samurai source or chibicc compilation failure.  Native-link fault
locations vary between the two staged native binutils builds (including
`get_meta`/`free.c` in one build), which points to a native-binutils/runtime
build defect.  No permanent binutils, kernel, libc, rootfs, or Samurai source
was changed while isolating it.  Consequently `samu -h`, `samu -n`, and the
real/incremental build acceptance tests remain pending until a corrected native
linker is available.

Temporary workaround for disposable validation only: keep the objects produced
inside REM, extract them to the host, link with the x86 host
`myemulator2-elf-ld`, and stage the resulting REM ELF back into a test image.
This is useful for separating Samurai runtime failures from the native-linker
failure, but it is not a self-hosted development path and is not suitable for
the flight package.

The correctly staged cross-bootstrap used for this run is
`/tmp/chibicc-native-fixed2/chibicc`, SHA-256
`12490f4733b3401f810f866b389e88e46d043c916313def47480fbf9dfaa8ad4`.
It was installed into a normally-created ext4 image and verified before
boot. Samurai source and the flight artifacts were not modified.

## Host-linker fallback boundary

The host `myemulator2-elf-ld` is a safe **preflight** fallback: it links
REM objects produced by the native guest assembler and emits a valid
ELF32-MyEmulator2 file. The same object set and linker script that crash the
native guest `ld` link successfully with the host linker, and the resulting
ELF can be staged into a REM image for execution testing.

This fallback is not a native flight solution. The host linker is an x86-64
program and cannot execute inside REM. It therefore supports only this
workflow:

```text
REM chibicc -> REM as -> copy objects to host
host myemulator2-elf-ld -> REM ELF -> copy ELF back to REM
```

It cannot support an offline self-hosted build where REM runs the linker.
The native-link boundary remains an explicit flight blocker until `ld -r
hello.o` succeeds on REM.

An additional isolated repair attempt forced the existing native-binutils
`ld` sources to rebuild with `-O0 -g`.  The rebuild reached `ldelf.c` but did
not complete within the available execution window; no replacement `ld` was
installed and the existing staged linker was left unchanged.

## Current native-link and call-alignment follow-up

The stale-linker condition described above has since been superseded by the
validated native linker tuple: `ld -r hello.o` and a complete Samurai link
both return zero.  With the current REM chibicc backend, all thirteen unmodified
Samurai translation units also compile inside REM and assemble successfully.

The first linked Samurai image trapped at a call boundary because the backend
left padding for the complete temporary argument list below register-only
calls.  The backend now computes padding from outgoing stack words after
register arguments have been popped, preserving the 16-byte REM ABI boundary.
The change is covered by host assembly-generation and GAS tests; a fresh
native chibicc bootstrap has ELF32/MyEmulator2 identity.  A clean guest
`samu -h`/`samu -n` run with this replacement is still pending a clean boot of
the matching runtime tuple, so this note does not claim Samurai acceptance.

## Follow-up: variadic string-argument reduction

A clean mixed variadic probe reproduced the remaining `samu -n` failure without
Samurai or musl changes:

```c
static void fatal(const char *fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    vfprintf(stderr, fmt, ap);
    va_end(ap);
}
int main(void) { fatal("format %s %d\\n", "string", 42); }
```

The REM chibicc caller emitted a string-typed argument as two words because
`gen_call` used `ty->size > 4` without accounting for array-to-pointer decay.
That shifted later arguments at the call boundary.  The backend now uses an
`arg_words` helper: arrays and function types occupy one pointer word, while
actual 64-bit scalar values occupy two words.  The generated call changed from
`sw r2`/`sw r1` pair staging to a single pointer push for the string literal.

The corrected probe was rebuilt with the current host REM backend, native GAS,
and the validated REM linker tuple.  A separate three-argument non-variadic
`write(1, "string\\n", 7)` probe prints `string`, confirming ordinary argument
staging.  A GCC-built variadic control prints `format string 42`; the corrected
chibicc probe still requires one further clean native-image validation of the
GCC/musl `va_list` boundary before `samu -n` can be called PASS.  No Samurai,
musl, kernel, QEMU, or flight artifact was modified.

## Follow-up: variadic save-area placement and Samurai parser probe

The remaining mixed chibicc/GCC variadic failure had a second ABI cause.  The
backend saved incoming `r1`--`r4` at offsets 16--28 from its active stack frame.
Those addresses are inside the caller's current stack area, so `vfprintf`'s
prologue overwrote them while constructing its own frame.  The backend now
saves the register area at old-SP+16 (`frame_size+16` from its frame base), and
`va_start` points into that area.  The minimal `vfprintf` probe now prints
`format string 42` under the candidate REM image.

The same clean Samurai rebuild exposed an independent static-data issue: global
pointer relocations were emitted as zero bytes, leaving the keyword table's
names null.  The backend now emits 32-bit `.word` relocations for `Obj.rel`;
a standalone static-struct pointer probe returns zero under REM.  A rebuilt
Samurai binary reaches `samu -n` but still faults in its parser's `strcmp` path
(`pc=0x02035a0c`, `strcmp+0x38`, `r2=0`).  The same fault is present in the
earlier staged Samurai binary, so it is not introduced by the variadic fix.
The exact Samurai-specific parser/runtime cause is not yet isolated; Samurai
acceptance remains pending and no source workaround has been made.

## Follow-up: current compiler rebuild and 64-bit store regression

The previous `strcmp(NULL, ...)` result came from an older Samurai build.  A
fresh rebuild of all thirteen unmodified translation units with the current host
chibicc (including static-pointer relocations) no longer faults in
`scankeyword`; the standalone keyword binary and `samu -h` both pass.

That rebuild exposed a separate backend defect in plain 64-bit assignments.
`store()` recovered the destination address into `r2`, then used `r3` for the
two stores of an `r1:r2` value.  The destination register was therefore stale.
This occurred in Samurai's 64-bit hash path.  The backend now recovers the
destination into `r3` for every 64-bit type, and
`test/rem-u64-store.c` records the regression.

With that fix, `samu -n` proceeds beyond the parser and reaches the graph/file
diagnostic path.  A complete real-build/incremental-build acceptance run is
still pending; no Samurai source or runtime component has been changed.

### Control-flow follow-up

The compiler's loop lowering now carries an explicit header label on each
`for`/`while`/`do` node. This makes nested `for`/`switch`/`break` control flow
unambiguous and is covered by `rem-nested-loop-switch.c`. Samurai was rebuilt
from all thirteen unmodified sources with this backend and the resulting
assembly/link steps still complete; guest `samu -n` execution remains the next
runtime acceptance check.

## Runtime reduction: stat/mtime path and launch-directory requirement (2026-09-27)

The missing-output metadata path was reduced independently of Samurai. A clean
REM probe implementing `osmtime()` with `stat()`, `st_mtim.tv_sec`, nanosecond
conversion, and the `MTIME_MISSING == -2` sentinel returned the expected
sentinel for a missing file and a positive timestamp for an existing file.
The direct stat/fstatat/statx probes also returned `-1`, `errno == ENOENT`, and
`strerror(errno) == "No such file or directory"`. No kernel or musl change is
indicated by these tests.

The acceptance invocation must run from the directory containing the manifest:
`cd /root; /root/samu -n` when the manifest is `/root/build.ninja`. Running
`/root/samu -n` from `/` fails to open the relative `build.ninja`; its fatal
`perror()` text was misleading (`Hangup`) because the diagnostic path does not
preserve `errno` across formatting.

When run from `/root`, the current Samurai binary advances past parsing but
still faults in the graph/dependency path. A disposable instrumented build
showed the first invalid state is not the mtime sentinel: after argument
processing and manifest parsing, the `argc` local contained a text/code address
instead of zero, before default-target traversal. This is retained as a
compiler-generated main/argument-state investigation; Samurai source, kernel,
and musl were not changed.

## GCC cross-compiler comparison (2026-09-27)

To separate compiler output from Samurai/runtime behavior, the unmodified
Samurai revision `531ba700c5fbbfd3d8e3ed91226d0c57134a6fb3` was exported from
its clean git commit and all thirteen C translation units were built with
`/tmp/rem-cross-gcc-clean/bin/myemulator2-linux-musl-gcc` (GCC 15.2.0, SHA-256
`abb88fb6cbd255641f68529c99ca1b2c389007fcd91cca5bc5caa9fa2bbd3fb4`). The
GCC driver used the configured REM `as` and `ld`, CRT, static musl and libgcc.
The static REM ELF `samu` was linked successfully despite linker diagnostics
about missing `.eh_frame_hdr` and an RWX load segment. Its SHA-256 is
`f84dad8dad2777ad0aad8e0472bf3cc5967119ff53293fc519b8dd6b2f4cf3ce`.

Both this binary and the current chibicc-built binary were staged in
separate copies of the same clean ext4 image and booted with the same tuple:
QEMU `0154db549ad01d0bcdfd79c0dde9544266fc14c75fe7825d14942ff47a75641a`,
kernel `5bd96e5d7e9163d95e8470eb550504357d84e78d38cd269781dfb096a511638a`,
and base image `ac7c839c1c1c7fac93cdcc6a681099574b92b786f41100f3f0b095176e0ff88d`.
The comparison chibicc-built `/root/samu` has SHA-256
`f183fbae5c76f5aa686e5e5a06cdc2fdc85b8025acd657395b096bb82dcfbffe`.

| Check | GCC-built `samu` | chibicc-built `samu` |
|---|---|---|
| `-h` | Prints usage; upstream Samurai has no `-h` case and exits 2 | Same usage and raw exit 2 |
| `cd /root; /root/samu -n` | PASS, exit 0; prints `[1/1] /root/chibicc -o /root/hello /root/hello.c` | FAIL, shell reports 139 after `MYEMU_BAD_USER_FAULT` |

The chibicc fault is `pc=0x020359b8`, `addr=0`, `r2=0`, `cause=7`;
`nm` maps that address to `strcmp+0x38` in this exact binary (the symbol starts
at `0x02035980`). This matched-source, matched-image result makes chibicc
code generation/build output the demonstrated cause of this `-n` failure;
the Samurai source and common guest runtime are not required to explain the
GCC/chibicc difference. The `-n` pass is a useful compiler comparison, not
full Samurai acceptance: no real build or incremental rebuild was tested.

The help invocation also exposed a separate syscall/kernel wait-status defect.
Both binaries print usage because `-h` is unsupported; the kernel trace shows
the process calling `exit(2)`, but BusyBox ash reports `$?` as 130. A GCC-built
static wait probe calling `fork`, child `_exit(2)`, and parent `waitpid`
observed `status=0x0002`, `WIFEXITED=0`, and `WIFSIGNALED=1` (`SIGINT`). A
child `_exit(1)` similarly appeared to ash as 129. This is a wait-status ABI
failure, separate from the Samurai compiler comparison; no kernel, libc,
Samurai, or chibicc source was changed. Test harnesses, logs, and binaries
remain under `/tmp/rem-samu-gcc-compare-20260927/`.

## Clean same-image comparison (2026-09-27, current artifacts)

The preceding comparison was invalidated by a reboot and stale/disposable artifacts. It was recreated from the unmodified Samurai commit `531ba700c5fbbfd3d8e3ed91226d0c57134a6fb3d` (thirteen translation units) and two fresh builds. Compiler hashes were:

```text
myemulator2-elf-gcc  07ef3aaab8ce4ce47c0a093a4b7b2590e98c936e6e8a651d7261e0f07f0087fe
chibicc              1d42607078ad852e3f852cb7639b6fc0f0ff033fcba9a6bd2036a3e14fca1631
```

Both assembly sets assembled and linked to ELF32 MyEmulator2 binaries. Fresh `samu` hashes were `0bb1e309558ee94f723ab7a0c4d6c02ca904d65f261ce73d6ce927f3652eabe2` (GCC) and `6206ad86d57e4a75586ae329c5570e95e48768eb21e46336316a82339b354a0b` (chibicc). Both were placed in the same newly-built disposable ext4 image and run with the same QEMU/kernel tuple.

GCC `samu -h` and `cd /root; samu -n` passed. Chibicc `samu -h` passed but fresh `cd /root; samu -n` faulted in `strcmp`. The old fault address is not reused: in the fresh chibicc ELF, `strcmp` starts at `0x02036094`. Its caller loads `buf.data` into `r1` and `keywords[mid].name` into `r2` immediately before `jal strcmp`; no compiler change is claimed from that observation.

An isolated static-buffer/reallocation reducer was built twice from the same source. Both the GCC-built and chibicc-built reducer faulted in the disposable guest, so that reducer does not demonstrate a chibicc-only defect. Real and incremental Samurai build checks were not run because the chibicc `-n` gate remains failed; the GCC real-build path is separately blocked on the tuple's `posix_spawn`/`ENOSYS` runtime limitation.

## Minimal strcmp/runtime probes (2026-09-27)

The fresh comparison was followed by isolated probes built with both REM-GCC and
chibicc and run in the same disposable guest. These all returned zero for both
compilers: direct `strcmp("abc", "abc")`; an array of `char *`; a struct
containing a pointer; a six-entry static keyword table; and a realloc-growing
buffer with a terminating NUL. The original first buffer reducer was invalid
because it omitted the terminating NUL before calling `strcmp`; the corrected
version passes with both compilers.

This rules out a generic `strcmp`, pointer-array, struct-pointer, realloc, or
static-keyword-table failure. No chibicc change is justified by these probes.
The remaining difference is specific to the full scanner/build-graph path and
needs a smaller scanner reproducer or a valid guest fault trace.

## Current clean qualification after compiler fixes

The clean unmodified Samurai sources were rebuilt with the current chibicc
backend after fixing indirect function-pointer calls and REM `not` operand
order. The resulting binary is `/tmp/samurai-clean-final2/samu`, SHA-256
`26128967b0cd60506321534617af359cc21d795a59633aa99fa05d13c04e936b`.

On the pinned QEMU/kernel/runtime tuple, `samu -n` completed successfully and
printed the expected `[1/1] echo built > out.txt`. The same image and command
using the GCC-built Samurai also completed `-n`, establishing that the next
failure is outside the compiler comparison.

A real build then reached Samurai's process-launch path and failed for both
binaries with:

```text
samu: posix_spawn /bin/sh: Function not implemented
samu: job failed to start
```

This is `ENOSYS` from the common REM process-creation/runtime tuple, not a
Samurai source or chibicc failure. Consequently real, incremental, and clean
command execution remain blocked until `posix_spawn`/`clone`/`exec` support is
available. No Samurai source was changed.

## Disposable `NO_POSIX_SPAWN` qualification

Samurai's upstream source provides a `fork`/`execvp` fallback selected by
`-DNO_POSIX_SPAWN`. A disposable clean build of all twelve translation units
with the current chibicc and this flag produced `/tmp/samurai-nospawn/samu`
(SHA-256 `98aec69f3b5810d299045010bd5a5569bae1e3102032ddf77f2c89cf1064e15f`).
On a freshly built ext4 image with the same pinned tuple it passed:

```text
samu -n                         -> DRYRC=0
samu                           -> REALRC=0, OUT=built
modified manifest; samu        -> INCRC=0, INCOUT=changed
samu -t clean                  -> CLEANRC=0, CLEANED=1
samu                           -> REBUILD_RC=0, REBUILT=changed
```

This proves the chibicc-built Samurai graph, filesystem, incremental logic,
native assembler/linker output, and the REM `fork`/`execvp` path work together.
The default POSIX-spawn build remains blocked by the missing `posix_spawn`
implementation; `NO_POSIX_SPAWN` is an upstream-supported build configuration,
not a Samurai source modification. A flight package must either build Samurai
with this flag or add and validate the `posix_spawn`/clone path.

## Stage-7 compiler rebuild comparison (2026-10-04)

The unmodified 13-file source set from `/tmp/samurai-clean-compare` was rebuilt inside REM using Stage-7 chibicc (`-DNO_POSIX_SPAWN -DCHIBICC_REM`), native `as`, and the established `ld -r` plus final-link workaround. The extracted binary `/tmp/samu-stage7` has SHA256 `623cb3ff64ea04ca0526fd1bdf2f63384fb3afacc450594e22bb293b9daa345a`; all translation units, assembly, partial link and final link passed. `-n` returned 0. The real tiny build produced and ran its target (exit 42), then samu itself faulted after child completion at PC `0x02035eb4` (musl `memchr`/`strnlen`, bad address `0x183`). The prior reference `/tmp/samurai-nospawn/samu` reproduced the same post-build fault in the same disposable image, so this is not a demonstrated Stage-7 compiler-only regression. Earlier clean-tuple NO_POSIX_SPAWN qualification remains the authoritative full real/incremental/clean PASS.
## Stage-7 post-build fault investigation (2026-10-04)

Stage-7 chibicc `/tmp/chibicc-stage7` (SHA256 `72bf5c9b519eb2a33ab5eb2e014a48d21ef661049e240b3a42000f39f423eba0`) is qualified: all nine compiler translation units, partial/final link, `--help`, and a hosted compile/link/run returning 42 pass. All 13 unmodified Samurai translation units rebuilt with it, linked, and passed `samu -n`; a real one-target build produced and ran its target. The remaining fault occurs after the child succeeds, in the Samurai post-build logging/metadata path (musl `memchr`/`strnlen`). The reference Samurai binary reproduces the same fault on the same guest, so this evidence does not implicate Stage-7 chibicc.

The smallest current reproducer is a one-rule build whose command is `touch hello`: the target is created successfully, then `samu` faults with a small invalid string address. A standalone high-value 64-bit `printf` probe was also reduced and passes when built from the current Stage-7 compiler, so the Samurai fault is not yet proven to be the generic variadic ABI.

Further reduction identified the concrete shared cause: Stage-7's
`preprocess.c:eval_const_expr()` returned `long`. On REM32 this truncated
`UINT64_MAX`, making musl's `#if UINTPTR_MAX == UINT64_MAX` branch select
`PRId64="ld"` instead of `"lld"`. Samurai's `PRId64` log call then passed a
64-bit value while musl consumed one 32-bit word, using the high word as the
following `%s` pointer. The source fix is `int64_t` evaluation plus
`test/rem-pp-uintptr-width.c`; rebuilding a corrected native compiler and
requalifying Samurai is the next required step.

The targeted fixes were rebuilt into `/tmp/chibicc-fix-build/chibicc`
(SHA256 `9aa9782dfeeac64e2ffa22d222f88afb3e2293cd19dc34d857bbf721a05ac761`).
It emits `PRId64="lld"` for the REM32 headers. Rebuilding only Samurai's
`log.c` with this compiler and relinking produced `/tmp/samu-fix` (SHA256
`1bee2e3c9aa19fe0ece0af1eb333b41e63ba7382bff993b68d297515d9d1aa93`). On a
cleaned disposable guest, `samu-fix` completed the real `touch hello` build
with exit 0, a second no-op build with exit 0, `-n` with exit 0, and a rebuild
after changing the input with exit 0. The prior post-build memchr/strnlen
fault did not recur.
