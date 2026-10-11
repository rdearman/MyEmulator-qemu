# REM C compiler status

This document records the state of the isolated chibicc port in
`toolchain/chibicc-rem`. It describes the practical REM compiler path; it is
not a GCC replacement plan.

## Source and current levels

- Upstream chibicc revision: `90d1f7f199cc55b13c7fdb5839d1409806633fdb`.
- REM source tree revision used for the validated tuple:
  `d463af9f6298446ca94ef0710efb42ee21ac2f7c`, plus the working-tree chibicc
  port changes.
- Level 1: host chibicc emits current REM GAS assembly — **PASS**.
- Level 2: generated assembly assembles and links — **PASS**.
- Level 3: generated REM programs execute — **PASS**.
- Level 4: a static REM chibicc executable runs on REM — **PASS**.
- Level 5: native chibicc compiles and runs `hello.c` — **PASS**.
- Level 6: chibicc compiles its own source — **BLOCKED**.
- Level 7: a self-hosted compiler rebuild workflow — **NOT REACHED**.

The Level 5 runtime tuple, including hashes for QEMU, kernel, rootfs, native
binutils, CRT objects, libc, libgcc, and chibicc, is recorded in
`docs/REM_VALIDATED_RUNTIME_TUPLES.md`.

## Current compiler capabilities

The REM backend currently handles the integer and pointer subset needed by the
validated programs:

- 32-bit ILP32 integers and pointers;
- constants, locals, globals, strings, address-of and dereference;
- integer arithmetic, bitwise operations, comparisons, shifts and boolean
  operations;
- conditional branches, loops, direct function calls and returns;
- current REM GAS syntax and ELF32 MyEmulator2 relocations;
- four-register argument passing and the current stack-frame convention;
- static linking through the REM linker script, CRT objects, musl `libc.a`,
  and the matching `libgcc.a`.

The port is intentionally small and understandable. It does not claim full
C11 or GCC compatibility.

## Exact Level 5 reproduction

Use one matching QEMU/kernel/rootfs tuple. The rootfs must contain the native
`as`, `ld`, CRT objects, musl `libc.a`, and the matching `libgcc.a`. The base
flight rootfs used during bring-up omitted `libgcc.a`; adding it to a
disposable copy is part of the test setup.

Inside the guest, with `/root/chibicc`, `/root/hello.c`, and
`/usr/lib/libgcc.a` installed:

```sh
env \
  CHIBICC_REM_AS=/root/native/bin/as \
  CHIBICC_REM_LD=/root/native/bin/ld \
  CHIBICC_REM_LINKER_SCRIPT=/root/native/lib/myemulator2-user.ld \
  CHIBICC_REM_CRTDIR=/usr/lib \
  CHIBICC_REM_GCCLIB=/usr/lib \
  /root/chibicc -o /root/hello /root/hello.c

/root/hello
```

The validated result was:

```text
CCLINK:0
Hello from REM
RUN:0
```

The chibicc driver places `libc.a` before `libgcc.a` in its archive group.
This is required by the native linker for libc references such as
`__muldi3`. Linking without `libgcc.a` fails at `memset.o` with an undefined
reference to `__muldi3`.

## Self-compilation audit

The first audit used the host REM chibicc binary to compile each compiler
source to REM assembly. The normal musl headers expose
`typedef __builtin_va_list va_list`, which the current parser cannot consume.
A disposable copy of the headers with those two typedefs represented as a
pointer allowed the audit to proceed far enough to expose backend blockers.
That shim is diagnostic only and is not a valid variadic ABI implementation.

Observed first blockers:

| Source | First observed blocker |
|---|---|
| `codegen.c` | `__builtin_ctz` is not recognized by the REM parser/backend. |
| `hashmap.c` | aggregate or 64-bit local rejected by the REM backend. |
| `main.c` | aggregate or 64-bit local rejected by the REM backend. |
| `parse.c` | aggregate or 64-bit local rejected by the REM backend. |
| `preprocess.c` | aggregate or 64-bit local rejected by the REM backend. |
| `strings.c` | aggregate or 64-bit local rejected by the REM backend. |
| `tokenize.c` | aggregate or 64-bit local rejected by the REM backend. |
| `type.c` | `switch` is explicitly unsupported by the REM backend. |
| `unicode.c` | reached successful assembly generation in the diagnostic audit. |

The compiler sources also contain variadic functions (`format`, `error`,
`error_at`, `error_tok`, `warn_tok`, and diagnostic helpers), `long double`
and `int64_t` fields, structs passed and returned internally, `switch`
statements, and compiler builtins. These are normal requirements for building
chibicc itself, not optional test features.

## Level 6 blockers

To compile chibicc with itself, the REM backend needs at least:

1. A defined variadic function ABI and implementation of `va_list`,
   `va_start`, `va_arg`, and `va_end` compatible with the REM libc headers.
2. Aggregate local objects and accesses to struct members, including correct
   size, alignment, copies, and address calculations.
3. The existing 64-bit integer and `long double` data used by compiler
   structures, or a carefully justified source-level reduction that does not
   change the compiler's language contract.
4. `switch` lowering, including case labels and default handling.
5. The builtins used by the compiler itself, beginning with `__builtin_ctz`.
6. Reliable calls to the required libc functions and compiler-runtime helpers
   when the resulting compiler is linked statically.

These are backend and ABI features. Adding a special self-build mode that
silently changes the compiler sources would not demonstrate self-hosting.

## Level 7 requirements

After Level 6, a practical self-hosted workflow needs:

- a native copy of chibicc installed on REM;
- native `as`, `ld`, `ar`, and `ranlib`;
- CRT objects, musl headers and `libc.a`, and the matching `libgcc.a`;
- a persistent source tree and a simple explicit build script;
- enough shell/file utilities to create objects, link, inspect, and replace the
  compiler executable safely.

GNU Make is useful but not a prerequisite. An explicit script listing the
compiler source files and invoking chibicc, `as`, and `ld` is preferable for
the first bootstrap because it avoids implicit-rule and timestamp problems.
Once that script can build generation 2 and generation 2 can compile the
Level 5 hello program, the target reaches Level 7 for the supported language
subset.

## Recommended path

1. Implement and test the REM variadic ABI in a small standalone C fixture.
2. Add aggregate local/parameter/return support, guided by the existing REM
   GCC output and ABI documentation.
3. Add `switch` lowering and the small set of compiler builtins used by
   chibicc.
4. Re-run the per-source self-compilation audit until all compiler sources
   produce valid REM objects.
5. Link a generation-1 native compiler with the pinned runtime tuple.
6. Use an explicit native build script to compile generation 2, then run the
   Level 5 regression through generation 2.

Floating point, atomics, TLS, VLA, full aggregate ABI coverage, and broad C11
library coverage should remain separately tracked. They are not required to
claim the initial integer/pointer self-hosting milestone, but they are needed
before calling the compiler generally C11-capable.

### Nested loop/switch control-flow regression

The REM backend now assigns each loop an explicit parser-generated header label,
separate from its `continue` and `break` labels. This avoids relying on the
code-generation counter when lowering nested option-parser shapes such as
`for (;;)` containing a `for`/`switch` pair. The focused regression is
`toolchain/chibicc-rem/test/rem-nested-loop-switch.c`; it assembles and links
with the REM toolchain and checks that a switch `break` does not terminate the
surrounding option loop.

## Level 7 bounded result (2026-09-29)

A clean, generation-consistent stage-2 build compiled and assembled all nine
chibicc translation units with the qualified no-TLS assembler
`1a437fd5bfe7564e61ac14ceacc40f06bc932bca2099b74a39e773010bf300ba`.
Native ld linked stage-3 successfully. Stage-3 passed `--help`, hosted
preprocessing and code generation, native assembly/link, and execution of a
hosted hello program (`stage3 works`, exit 0). The preserved artifact is
`/tmp/chibicc-stage3-qualified`, SHA-256
`54e1e52678f5a71cb4107d4af6afb7fa194ca5ca80e05a7440d1e5c0e549d396`,
ELF32 little-endian MyEmulator2, 706868 bytes.

Using stage-3 to compile `strings.c` remains deferred: it selects musl's
`NULL nullptr` branch while stage-2 does not. This is a non-blocking Level-7
bootstrap-equivalence issue for REM 0.1; the stage-3 artifact is suitable for
optional inclusion and ordinary hosted C use demonstrated above.

### Differential reducer checkpoint

The stage-2/stage-3 reducer was preserved after testing four identical inputs:
standalone `__cplusplus` conditionals, `<unistd.h>`, and the standard-header
set. Both generations reported `CPP_NOT_DEFINED`; output differed only in
`restrict` qualifiers. The attempted `chibicc.h` probe lacked the required
include path and is not valid evidence. Full stage-3 self-hosting remains
unresolved and is deferred.
### Wide integer comparison fix (stage-3 bootstrap)

The REM backend previously lowered 64-bit `<`/`<=` by OR'ing independent
high-word and low-word results.  That is incorrect when the high words differ.
It caused a native stage-3 preprocessor to evaluate `#if __cplusplus >=
201103L` incorrectly.  `gen_wide_binary()` now gives high-word ordering
precedence and only consults low words when high words are equal.  Regression
coverage is in `toolchain/chibicc-rem/test/rem-pp-int64-compare.c`.

The corrected stage-3 artifact is `/tmp/chibicc-stage3-wide` (ELF32
MyEmulator2; SHA-256 `efbe27e60ab0bc2dc02601ed523c5ff1a9b2a64d8be8411334e2acddc4220c57`).
It passes `--help` and compiles `strings.c` with the full hosted options.  A
complete stage-4 native rebuild remains to be rerun in a persistent guest;
the disposable QEMU runner used for this pass terminated during a long native
invocation, so no stage-4 claim is made.

### Stage-3 translation-unit boundary

With direct QEMU control and one translation unit per clean guest, stage-3
successfully compiled and assembled `strings.c`, `hashmap.c`, and
`tokenize.c`.  It successfully preprocesses `main.c` and `parse.c`, but the
full `-S` operation for those larger units terminates with `Hangup` (exit 129)
before producing assembly.  Raising the guest shell stack limit did not
change the result.  No mixed-generation stage-4 object set has been used.
This is the current Level-7 blocker; its ownership (large-TU compiler
execution versus REM runtime/resource handling) remains to be isolated.

### Controlled signal isolation (2026-09-30)

The boundary was reproduced with a one-shot init and filesystem completion
markers. `main.c` compilation exits with `RC:129`; redirecting compiler stderr
captures `Hangup`, while an init-level HUP trap does not fire. The same result
occurs with guest `ulimit -s 262144`, `trap '' HUP`, and with `-serial null`
plus stdout/stderr redirected away from the tty. A small `setsid`/`exec` helper
built by stage-3 changed the shell status to 240 and captured `Unknown signal`,
but did not produce assembly. These runs cleanly terminated their recorded
QEMU PID and restored the image init. The failure is deterministic in the
large-TU stage-3 execution path, but its owning layer is still unresolved; no
compiler source change is justified.

### Large-TU reduction checkpoint (2026-09-30)

A disposable `main.c` variant with the complete `main()` body replaced by
`return 0` still terminates stage-3 with `RC:129` before assembly. The failure
is therefore outside the `main()` body, in another helper, translation-unit
workload, or the runtime boundary. An all-function-body variant failed to
produce a completion marker and is not considered valid evidence. No compiler
change was made.

### Stage-3 global-initializer reducer (2026-09-30)

remdbg reports `SIGSEGV` at `fnv_hash+0x1a4` (`PC 0x02001eb0`) with fault
address `0x10ab3000`. The smallest valid source boundary currently preserved
is:

```c
#include "chibicc.h"
int x;       /* Stage-3 PASS */
int x = 1;   /* Stage-3 SIGSEGV */
```

Removing the body of `main()` does not change the result. Header-only and
header-plus-enum inputs pass. This is now a concrete Stage-3 self-hosting
reproducer for differential debugger analysis; no compiler change has been
made.
### REM double alignment correction

The REM target data model uses 8-byte `double` values with 4-byte alignment.
A probe compiled with the REM GCC ABI reports `sizeof(struct { char c; double
d; }) == 12`.  chibicc's target table had retained 8-byte alignment, which
made compiler-internal structures such as `Token` differ between generations
(68 bytes versus 72 bytes) during bootstrap.  `type.c` now records
`TY_DOUBLE` as `(8, 4)` and `test/rem-double-alignment.c` guards the rule.

The corrected cross-built compiler (SHA256
`788be1ecab756fcd381e3cd4748de5ad2aeac30ddae61aa5f6ca446441878178`) was
verified inside a fresh, fsck-clean disposable guest: a header-free C file
compiled successfully and produced assembly.  A hosted `strings.c` compile
was still running at the 180-second bounded run limit without a kernel user
fault or completion marker; this does not qualify the full self-hosting
translation and remains an open performance/large-TU validation step.

### Stage-3 to stage-4 native bootstrap (2026-09-30)

Using the same corrected compiler in one REM guest, all nine chibicc
translation units (`strings`, `tokenize`, `parse`, `preprocess`, `type`,
`codegen`, `unicode`, `hashmap`, and `main`) reached the output phase.  The
qualified no-TLS assembler (`1a437fd5bfe7564e61ac14ceacc40f06bc932bca2099b74a39e773010bf300ba`)
assembled every object.  The qualified native linker
(`edd83609045df123c8a261cf615f70de1bd3e249082a9fe39f392ef0f6ee3b65`) linked
stage-4; the stale packaged linker bus-errored on the same full object set.

The resulting stage-4 ELF32 MyEmulator2 compiler is
`/tmp/chibicc-stage4-rem` (SHA256
`07b0c33cdba6c2838d391f7b4d929bfd7e7c5e6d89dafb7480bb6f656a881d45`).  It
ran natively, `--help` passed, a header-free program compiled, and hosted
`strings.c` reached successful assembly output.  Full stage-5 regeneration
and bootstrap equivalence remain unqualified because subsequent long native
guest runs were externally terminated before their object outputs committed.

## Final Stage-7/Samurai qualification (2026-10-04)

The original Stage-7 artifact `/tmp/chibicc-stage7` (SHA256
`72bf5c9b519eb2a33ab5eb2e014a48d21ef661049e240b3a42000f39f423eba0`) passed
its nine-TU, link, help, hosted return-42 and Samurai rebuild qualification,
but is superseded and must not be shipped. The subsequent Samurai fault exposed
an ILP32 preprocessor defect: `eval_const_expr()` returned `long`, truncating
`UINT64_MAX`; 64-bit hexadecimal `U` literals were also typed as 32-bit
`unsigned long`. Musl therefore selected `PRId64="ld"`, and Samurai's 64-bit
log timestamp left its high word to be consumed as the following `%s` pointer.
The fixes use `int64_t` evaluation and target-aware `unsigned long long`
literal typing. Regression: `toolchain/chibicc-rem/test/rem-pp-uintptr-width.c`.
Corrected compiler: `/tmp/REM-R3-HANDOFF/codex-2/artifacts/chibicc`, SHA256
`9aa9782dfeeac64e2ffa22d222f88afb3e2293cd19dc34d857bbf721a05ac761`.
Corrected Samurai: `/tmp/REM-R3-HANDOFF/codex-2/artifacts/samu`, SHA256
`1bee2e3c9aa19fe0ece0af1eb333b41e63ba7382bff993b68d297515d9d1aa93`.
No-op, `samu -n`, real build, target execution, changed-input rebuild and
clean exit all passed.
