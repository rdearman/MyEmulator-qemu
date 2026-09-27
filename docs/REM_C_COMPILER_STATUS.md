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
