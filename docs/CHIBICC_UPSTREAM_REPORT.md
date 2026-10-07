# chibicc REM Port / Bootstrap Findings

## Native userland investigation: confirmed upstream findings (2026-10-06)

The otherwise-unmodified upstream revision
`90d1f7f199cc55b13c7fdb5839d1409806633fdb` was cloned into the isolated
`/home/rick/rem-native-userland/upstream-chibicc` workspace and built on its
supported x86-64 Linux target. No issue, patch or message was sent upstream.

**Confirmed upstream header defect:** upstream `include/float.h` describes
`long double` using binary64 constants (`LDBL_MANT_DIG=53`,
`LDBL_MAX_EXP=1024`), although its x86-64 type/backend uses the extended
long-double representation. `toolchain/tests/native-userland/long-double-abi.c`
returns 1 with pristine upstream and 0 with system GCC; preprocessing confirms
the upstream header's 53/1024 constants. The stronger numeric reducer
`long-double-mantissa.c` checks the advertised precision against adding
`0x1p-63L` to a volatile `1.0L`: pristine upstream returns 1, whereas system
GCC returns 0. This numerically confirms the inaccurate precision macro,
rather than inferring precision solely from storage size. The header/layout
disagreement is upstream evidence, NOT evidence
that REM should adopt the x86-64 long-double ABI. REM's actual GCC/musl ABI is
binary64, so the REM backend's 16-byte long-double type is a separate
REM-specific defect documented in `REM_CHIBICC_BUGLOG.md`.

The same pristine upstream header is missing mandatory C11
`FLT_DECIMAL_DIG`, `DBL_DECIMAL_DIG`, `LDBL_DECIMAL_DIG` and the three
`*_HAS_SUBNORM` macros. `toolchain/tests/native-userland/float-header-macros.c`
fails compilation on pristine upstream with its explicit missing-macro
diagnostic (exit 1), while system GCC compiles and runs it with exit 0.
The log is retained as `upstream-results/float-header-macros.upstream.log` in
the isolated workspace. The REM source header adds these macros with values
for REM's binary32/binary64 types; its combined `DECIMAL_DIG` is 17, whereas
the upstream x86-64 extended type requires a different combined value.

All these other minimal probes compile and return 0 with pristine upstream:
`float-arithmetic.c`, `float-cast.c`, `float-literal.c`,
`libc-float-printf.c`, `va64.c`, `va-stack-named.c`,
`wide-compare.c`, `wide-negate.c`, `wide-split-argument.c`,
`wide-subtract.c`, `wide-truth.c`, `float-global.c`,
`indirect-call-depth.c`, `assignment-call-alignment.c`,
`qsort-large-frame.c`, `va-large-frame.c`, `local-alignment.c`,
`large-zero-initializer.c`.
`unsupported-diagnostic.c` compiles and returns its intended 42 (upstream
supports this VLA); REM currently lacks that backend support. Those failures
are therefore NOT reported as architecture-independent upstream defects.
Complete per-probe build/run logs are retained in
`/home/rick/rem-native-userland/upstream-results/`.

### Multiple function declarators in an ordinary C declaration

Minimal reducer: `toolchain/tests/native-userland/comma-function-declarations.c`.
The valid declaration `int first(int), second(int), value;` is rejected by
pristine upstream with **parameter name omitted** (compiler exit 1).
System GCC compiles and runs the reducer with exit 0. This is a generic parser
defect, not an REM argument-layout issue: the function parser expects a
semicolon or a body after the first declarator, and mistakes a comma-separated
prototype for a definition. Musl's `__syscall_ret` declaration exposed it during
runtime-source rebuilding.

The staged source-kit parser now handles comma-separated function prototypes
and mixed function/object declarations, including block-scope extern
declarations. Native acceptance remains pending; a host-emitting REM check
generates assembly successfully. The upstream failure log is
`/home/rick/rem-native-userland/upstream-results/comma-functions.upstream.log`.

### Postfix operations on a compound literal

Minimal reducer: `toolchain/tests/native-userland/compound-literal-member.c`.
Pristine upstream rejects `(union { int value; char byte; }){.value = 42}.value`
with **expected ')'** (compiler exit 1); system GCC compiles and runs with
exit 0. The reducer also checks struct-member access and an array subscript
after a compound literal. Musl's ordinary endianness probe in `crypt_blowfish`
exposed the first shape.

The generic `postfix` parser returns immediately after constructing a compound
literal, without consuming its following postfix operators. The staged repair
routes that expression through the existing common postfix loop. Native
acceptance remains pending; host-emitting REM assembly generation succeeds.
The upstream failure log is
`/home/rick/rem-native-userland/upstream-results/compound-literal.upstream.log`.
No report, patch, or message has been sent upstream for either finding.

### NaN converted to `_Bool` becomes false

Minimal reducer: `toolchain/tests/native-userland/nan-bool.c`. It constructs
an IEEE binary64 quiet NaN using a union, stores it in a volatile double, and
converts it to `_Bool`. C requires a true result because NaN does not compare
equal to zero. Pristine upstream on supported x86-64 compiles the reducer but
returns 1; system GCC returns 0. The independent log is retained as
`/home/rick/rem-native-userland/upstream-results/nan-bool.upstream.log`.

The upstream x86-64 cast uses `ucomisd` followed only by `setne`. An unordered
comparison sets ZF as well as PF, so this incorrectly converts NaN to false.
This is an independently confirmed upstream supported-target defect, separate
from REM's pointer-to-Boolean byte truncation (NU-20). The staged REM backend
already tests floating magnitude bits rather than unordered comparison flags;
native verification of NaN truth is included in `ieee-soft-float.c`.
No upstream source tree was patched and no report was submitted.

### A brace-enclosed string initializes only the first array element

Minimal reducer: `toolchain/tests/native-userland/brace-string-initializer.c`.
The ordinary declaration `static const char digits[4] = {"0123"};` must
initialize four characters. Pristine upstream chibicc on supported x86-64
compiles the reducer but returns 1; GCC returns 0. This was encountered
because musl's `vfprintf` hexadecimal digit table uses precisely this valid
C initializer, so hexadecimal and pointer formatting cannot work correctly
in the source-built runtime.

The array parser treats the brace-enclosed string as a scalar initializer
for the first character, instead of using its existing string initializer
for the complete array. The staged source-kit repair routes a brace-enclosed
string through that existing path when its element type matches the string
type, preserving ordinary arrays of string pointers and accepting the
optional trailing comma. Native repaired-runtime verification is pending.
This is a genuinely architecture-independent upstream frontend defect,
not a REM frame-pointer defect. No upstream contact or submission occurred.

### A true `_Bool` bitfield is read as -1

Minimal reducer: `toolchain/tests/native-userland/bool-bitfield.c`.
Setting a Boolean bitfield to 1 and clearing its neighbour must preserve
values 1 and 0. GCC returns 0; pristine upstream chibicc on supported x86-64
returns 1. In contrast, `bitfield-neighbours.c`, containing ordinary unsigned
and signed integer fields, passes both compilers.

Upstream's member extraction selects arithmetic right shift because its
`TY_BOOL` type does not carry the unsigned flag. A one-bit true value is
therefore sign-extended to -1 instead of yielding the required Boolean 1.
Curl's `BIT` state fields directly require correct Boolean bitfield support.
The staged REM lowering selects logical extraction for `TY_BOOL`, separately
from restoring the entirely missing REM bitfield load/merge lowering.
Native repaired verification is pending. No upstream report was submitted.

## 1. Executive Summary

REM is a 32-bit little-endian Linux architecture with an ILP32 C data model:
`int`, `long`, pointers, and `size_t` are 32 bits; `long long` is 64 bits.
Its function ABI passes the first four arguments in `r1`–`r4`, returns scalar
values in `r1` (or `r1:r2` for 64-bit values), uses `r13` as the stack pointer
and `r14` as the link register, and requires 16-byte call-boundary alignment.

The port is based on upstream chibicc revision:

    90d1f7f199cc55b13c7fdb5839d1409806633fdb

The REM port was developed in the `toolchain/chibicc-rem` repository. Its
current REM changes are recorded in commit `81fbf49` (the nested repository may
also contain uncommitted regression fixtures from the qualification work).

The port progressed through a native Stage-7 qualification. The corrected
compiler built all nine chibicc translation units, linked and ran on REM, and
rebuilt all thirteen unmodified Samurai translation units. Samurai passed
dry-run, real-build, no-op, changed-input and clean-exit tests. Complete
generation-to-generation identity is reported separately from this functional
qualification. This report separates target/backend implementation from
generic or portability findings; it does not present REM ABI work as an
upstream chibicc defect.

## 2. Test / Bootstrap Environment

The test environment consisted of:

- MyEmulator2/REM ELF32 little-endian Linux;
- REM musl headers and static `libc.a`;
- REM CRT objects and `libgcc.a`;
- native REM GNU assembler and linker;
- a no-TLS native assembler used for the REM 0.1 qualification because the
  TLS-enabled assembler faulted in emulated TLS during BFD error handling.

Terminology used here:

- **stage-1:** the bootstrap compiler used to produce the first native REM
  compiler artifact;
- **stage-2:** a native REM chibicc that can execute on REM and was used to
  compile the chibicc source set;
- **stage-3:** the compiler linked from those stage-2-generated objects;
- **stage-4 and later:** successive native generations produced during the
  qualification runs.

Preserved stage-3 artifact:

    /tmp/chibicc-stage3-qualified
    SHA-256: 54e1e52678f5a71cb4107d4af6afb7fa194ca5ca80e05a7440d1e5c0e549d396

The artifact is ELF32 little-endian MyEmulator2, 706,868 bytes. It demonstrated
native startup, `--help`, hosted preprocessing and code generation, assembly
and linking, and execution of a hosted hello program. It is retained as an
historical intermediate, not the final release artifact.

The final corrected compiler is:

    /tmp/REM-R3-HANDOFF/codex-2/artifacts/chibicc
    SHA-256: 9aa9782dfeeac64e2ffa22d222f88afb3e2293cd19dc34d857bbf721a05ac761

The earlier Stage-7 `/tmp/chibicc-stage7` (SHA-256
`72bf5c9b519eb2a33ab5eb2e014a48d21ef661049e240b3a42000f39f423eba0`) is
superseded and must not be shipped.

## 3. Consolidated Issue Table

| ID | Issue | Classification | Status | Minimal reproducer | Fix | Upstream relevance |
|---|---|---|---|---|---|---|
| 1 | Call-boundary stack alignment | REM-SPECIFIC ISSUE | Fixed | Yes, call probes | Yes | Target-specific |
| 2 | Variadic register-save area / `va_start` | REM-SPECIFIC ISSUE | Fixed | Yes | Yes | Target-specific |
| 3 | Array/function argument decay and word count | REM-SPECIFIC ISSUE | Fixed | Yes | Yes | Target-specific |
| 4 | Static pointer relocations | REM-SPECIFIC ISSUE | Fixed | Yes | Yes | Target-specific |
| 5 | 64-bit local stores on ILP32 | REM-SPECIFIC ISSUE | Fixed | `rem-u64-store.c` | Yes | Target-specific |
| 6 | Nested loop/switch/break labels | REM-SPECIFIC ISSUE | Fixed | `rem-nested-loop-switch.c` | Yes | Target-specific |
| 7 | Narrow-to-wide return extension | REM-SPECIFIC ISSUE | Fixed | `rem-wide-return.c` | Yes | Target-specific |
| 8 | Indirect-call target clobbered an argument | REM-SPECIFIC ISSUE | Fixed | `rem-indirect-call.c` | Yes | Target-specific |
| 9 | REM `not` operand order | REM-SPECIFIC ISSUE | Fixed | `rem-bitnot.c` | Yes | Target-specific |
| 10 | GNU declaration attributes after parameters | POSSIBLE UPSTREAM ISSUE | Fixed | `rem-gnu-attribute.c` | Yes | Potentially upstream-relevant, but needs pristine-upstream reproduction |
| 11 | ILP32 integer-width assumptions (`strtoul`, predefined sizes) | UPSTREAM PORTABILITY ISSUE | Fixed in REM port | `rem-ilp32-predefined.c` and literal probes | Yes | Potentially upstream-relevant if reproduced on another 32-bit target |
| 12 | `__builtin_ctz` use in native bootstrap | TEST/REGRESSION RECOMMENDATION | Fixed locally | No standalone preserved | Yes | Possible portability improvement, not yet an upstream bug |
| 13 | Old-style function parameters | REM-SPECIFIC ISSUE | Implemented in REM port | `rem-oldstyle-main.c` | Yes | Unknown; needs pristine-upstream reproduction |
| 14 | 32-bit FNV/hashmap behavior | REM-SPECIFIC ISSUE | Fixed in REM port | Hashmap tests | Yes | Unknown; no upstream-generic failure established |
| 15 | Stage-3 hosted preprocessing divergence | POSSIBLE UPSTREAM ISSUE | Open | Not yet validly reduced | None | Not suitable for upstream claim |
| 16 | Large-frame/frame-restoration faults | POSSIBLE UPSTREAM ISSUE | Open | No standalone reproducer preserved | None | Unknown |
| 17 | 32-bit lowering of 64-bit comparisons | REM-SPECIFIC ISSUE | Fixed | `rem-pp-int64-compare.c` | Yes | Backend only |
| 18 | Preprocessor conditional evaluation stored in `long` | UPSTREAM PORTABILITY ISSUE | Fixed | `rem-pp-uintptr-width.c` | Yes | Relevant to 32-bit hosts/targets |
| 19 | 64-bit preprocessor literals typed as 32-bit `unsigned long` on ILP32 | UPSTREAM PORTABILITY ISSUE | Fixed | `rem-pp-uintptr-width.c` plus `UINTPTR_MAX` probe | Yes | Relevant to 32-bit ports |
| 20 | Samurai post-build varargs corruption from wrong `PRId64` branch | REM-SPECIFIC ISSUE (symptom of 18/19) | Fixed | one-rule `touch hello` build | Yes | Integration evidence, not a separate runtime bug |
| 21 | Incoming stack-argument offset | REM-SPECIFIC ISSUE | Fixed | `rem-six-argument-call.c` | Yes | Target ABI |
| 22 | REM32 `double` alignment in aggregate layout | REM-SPECIFIC ISSUE | Fixed | `rem-double-alignment.c` | Yes | Target data model |
| 23 | Empty parameter list emitted as variadic | REM-SPECIFIC ISSUE | Fixed | `rem-empty-params.c` | Yes | Target frontend/ABI |
| 24 | `-g` accepted but no DWARF emitted | TEST/REGRESSION RECOMMENDATION | Open | No standalone reducer | No | Feature gap, not an upstream bug claim |

Classification in this table is deliberately conservative: the REM ABI,
instruction encoding, relocation, frame, and calling-convention entries are
**REM-SPECIFIC ISSUE**; the integer-width entries are **UPSTREAM PORTABILITY
ISSUE** candidates; unresolved observations remain **POSSIBLE UPSTREAM ISSUE**
or **TEST/REGRESSION RECOMMENDATION** until reproduced against pristine
upstream. No item here is labelled **UPSTREAM CHIBICC BUG** without a
reproducer independent of the REM backend.

## 4. Detailed Findings

### 4.1 Call-boundary stack alignment

**Symptom:** calls into libc or mixed compiler-generated code could enter with
incorrect stack alignment.

**Minimal reproducer:** small integer call and `printf("%d", 42)` probes; no
single upstream-independent source file was preserved beyond the REM tests.

**Expected:** `r13` is 16-byte aligned at every call boundary.

**Actual:** padding was calculated from all temporary arguments even after
register arguments had been removed from the outgoing stack calculation.

**Root cause:** confirmed in REM `gen_call` lowering.

**Fix:** `toolchain/chibicc-rem/codegen.c` calculates padding from arguments
remaining on the outgoing stack while preserving the REM register home area.

**Verification:** integer-call probes, `printf("%d",42)`, and Samurai builds
passed through REM assembly/link and execution.

**Upstream relevance:** REM backend/ABI-specific. Not an upstream chibicc bug.

### 4.2 Variadic register-save area and `va_start`

**Symptom:** a chibicc caller into GCC/musl `vfprintf` delivered the format
pointer as the first variadic value.

**Minimal reproducer:** chibicc caller to a GCC/musl variadic wrapper and
`vfprintf` probe.

**Expected:** the callee's `va_list` must address saved incoming `r1`–`r4`
and subsequent stack arguments according to the REM ABI.

**Actual:** saved argument registers were inside the active frame and were
overwritten by nested calls.

**Root cause:** confirmed REM variadic-frame/home-area violation.

**Fix:** `codegen.c` saves incoming registers at old-SP+16, outside the active
frame, and initializes the va-list cursor there.

**Verification:** internal variadic tests and the mixed chibicc-to-GCC/musl
`vfprintf` regression passed.

**Upstream relevance:** target-specific ABI implementation, not upstream.

### 4.3 Array/function argument decay and word count

**Symptom:** string literals and function arguments were staged as multiple
words, shifting later call arguments.

**Minimal reproducer:** string/variadic argument probes.

**Expected:** arrays and functions decay to one pointer word at a call boundary.

**Actual:** call lowering used raw `ty->size > 4` and treated array/function
objects as multiword values.

**Root cause:** confirmed in REM argument lowering.

**Fix:** `arg_words`/`gen_call` apply C array/function decay; only genuine
64-bit scalar values consume two words.

**Verification:** pointer/string and mixed variadic probes passed against
GCC/musl behavior.

**Upstream relevance:** REM ABI-specific.

### 4.4 Static pointer relocations

**Symptom:** Samurai's static keyword table contained null pointers, eventually
causing a `strcmp(NULL, ...)` fault.

**Minimal reproducer:** static struct/pointer table fixture.

**Expected:** aggregate data containing addresses must emit relocations.

**Actual:** data bytes were emitted, but `Obj.rel` entries were dropped.

**Root cause:** confirmed in REM `emit_data`.

**Fix:** `codegen.c` emits recorded 32-bit pointer relocations as `.word`
expressions accepted by the REM ELF toolchain.

**Verification:** static-pointer fixture and fresh Samurai assembly/link passed;
`samu -h` ran successfully.

**Upstream relevance:** REM relocation/backend-specific.

### 4.5 64-bit local stores on a 32-bit target

**Symptom:** a 64-bit local assignment corrupted the destination address or
value high word in Samurai's hash path.

**Minimal reproducer:** `toolchain/chibicc-rem/test/rem-u64-store.c`.

**Expected:** an `r1:r2` value pair remains intact while the destination is
recovered for the store.

**Actual:** `store()` recovered the destination into `r2`, clobbering the
64-bit value pair.

**Root cause:** confirmed REM register-allocation mistake in `codegen.c`.

**Fix:** destination recovery uses `r3`, preserving `r1:r2`.

**Verification:** the regression assembled/linked and the fresh Samurai
translation-unit build passed.

**Upstream relevance:** REM ILP32 backend-specific.

### 4.6 Nested loop/switch/break lowering

**Symptom:** nested ARGBEGIN/EARGF-style option-parser control flow lost or
obscured the loop back-edge.

**Minimal reproducer:** `test/rem-nested-loop-switch.c`.

**Expected:** `break` exits the switch or loop selected by the parser's nesting,
while the option loop returns to its explicit header.

**Root cause:** loop-header labels were generated separately during codegen
rather than being part of the parser-owned loop contract.

**Fix:** loop nodes carry parser-generated `begin_label`, separate from
`cont_label` and `brk_label`; changes are in `chibicc.h`, `parse.c`, and
`codegen.c`.

**Verification:** regression assembles/links with REM tools; Samurai's option
path progressed beyond the earlier control-flow failure.

**Upstream relevance:** target backend control-flow implementation.

### 4.7 Narrow-to-wide return sign extension

**Symptom:** a function returning an `int64_t` sentinel from a narrow signed
expression returned a valid low word but stale high word. Samurai's `osmtime`
missing-file path then compared garbage.

**Minimal reproducer:** `test/rem-wide-return.c` and the `osmtime` sentinel
probe.

**Expected:** signed widening sign-extends into `r1:r2`; unsigned widening
clears the high word.

**Root cause:** confirmed REM `ND_CAST`/64-bit return lowering defect.

**Fix:** `codegen.c` emits signed extension with `srai` and clears `r2` for
unsigned widening.

**Verification:** wide-return regression and Samurai osmtime path passed.

**Upstream relevance:** REM ABI-specific.

### 4.8 Indirect-call target clobber

**Symptom:** Samurai graph traversal received a corrupted node pointer after
an indirect call.

**Minimal reproducer:** `test/rem-indirect-call.c`.

**Expected:** evaluating an indirect callee must not overwrite already prepared
argument registers.

**Root cause:** `gen_call` evaluated the function-pointer expression after
placing arguments in `r1`–`r4`; the expression reused `r1`.

**Fix:** save the indirect target in a temporary stack slot before argument
evaluation, account for it in padding, then load it into `r15` and emit `jalr`.

**Verification:** regression passed and clean chibicc Samurai completed `samu
-n` on the pinned tuple.

**Upstream relevance:** REM calling convention/backend-specific.

### 4.9 REM `not` operand order

**Symptom:** QEMU trapped on an illegal instruction during graph traversal.

**Minimal reproducer:** `test/rem-bitnot.c`.

**Expected:** emitted operands must match the REM decoder's `not` encoding.

**Actual:** backend emitted `not r1, r0, r1`; REM requires `not r1, r1, r0`.

**Fix:** corrected operand order in `codegen.c`.

**Verification:** bit-not regression and Samurai `samu -n` passed.

**Upstream relevance:** REM ISA/backend-specific.

### 4.10 GNU declaration attributes after parameters

**Symptom:** canonical Kilo source failed at
`int value __attribute__((unused))` with `expected ','`.

**Minimal reproducer:**

```c
static int f(int value __attribute__((unused))) { return 42; }
int main(void) { return f(7) == 42 ? 0 : 1; }
```

**Expected:** an ignored GNU declaration attribute should be accepted after a
named parameter when it does not affect code generation.

**Root cause:** parser support existed for some attribute positions but not for
declaration attributes after a named declarator/parameter.

**Fix:** `parse.c` consumes balanced `__attribute__((...))` lists in that
position. Regression: `test/rem-gnu-attribute.c`.

**Verification:** Kilo preprocesses, compiles, assembles, and links with the
REM tuple.

**Upstream relevance:** potentially generic parser portability work, but it
has not been reproduced against pristine upstream in this report.

### 4.11 ILP32 integer-width and predefined-macro assumptions

**Symptom:** host-oriented assumptions about `_LP64`, predefined sizes, and
integer literal conversion caused incorrect target choices; using `strtoul`
for preprocessing literals truncated values on ILP32.

**Minimal reproducer:** `test/rem-ilp32-predefined.c` and 64-bit literal probes.

**Expected:** `long`, pointers and size-related predefined macros reflect ILP32;
64-bit literals are parsed with a conversion type wide enough for their value.

**Root cause:** confirmed portability problem in the REM port: `strtoul` is
32-bit because `unsigned long` is 32-bit.

**Fix:** target predefined macros were corrected and integer-token conversion
uses `strtoull`; REM type sizes are explicitly represented.

**Verification:** ILP32 predefined-size and wide-literal tests passed, and
64-bit helper paths could be generated correctly.

**Upstream relevance:** potentially relevant to any 32-bit host/target port,
but the exact changes are target configuration rather than a demonstrated
upstream bug.

### 4.12 `__builtin_ctz` in the native bootstrap source

**Symptom:** the REM compiler could not compile chibicc's use of
`__builtin_ctz` while bootstrapping its own `codegen.c`.

**Minimal reproducer:** no standalone fixture was preserved.

**Expected:** the compiler source should use a builtin supported by its target
compiler or provide a portable equivalent.

**Actual:** REM chibicc treated the builtin as an undeclared function.

**Fix:** `codegen.c` uses an ordinary `rem_ctz` helper for the identified use.

**Verification:** host-generated REM assembly for `codegen.c` assembled.

**Upstream relevance:** possible portability improvement in chibicc source,
not a confirmed upstream compiler defect.

### 4.13 Old-style function parameters

**Symptom:** old-style declarations used by some C sources required explicit
REM parser/ABI handling during qualification.

**Minimal reproducer:** `test/rem-oldstyle-main.c`.

**Expected:** supported old-style parameter declarations should lower using the
same established function ABI.

**Actual/root cause:** the REM port lacked the required compatibility path.

**Fix/verification:** implemented and covered in the REM regression set.

**Upstream relevance:** classification remains uncertain; it needs a pristine
upstream reproduction before an upstream report is appropriate.

### 4.14 32-bit FNV/hashmap behavior

**Symptom:** early native bootstrap and application tests exposed incorrect
hash-table behavior under REM's 32-bit integer/pointer model.

**Minimal reproducer:** hashmap self-tests and the REM hashmap regression set.

**Root cause:** REM port assumptions about integer width and pointer/value
storage; the exact failures were fixed in the REM `hashmap.c`/support changes.

**Fix/verification:** REM hashmap tests and stage-1 bootstrap tests passed.

**Upstream relevance:** not established as upstream-generic; treat as REM
portability/backend work unless reproduced on pristine chibicc on another
32-bit target.

### 4.15 Large-frame and frame-restoration observations

**Symptom:** Emacs/coreutils probes reported frame-relative load faults and
frame-restoration failures.

**Minimal reproducer:** no standalone reproducer was preserved.

**Expected:** generated frame setup, local offsets, and restoration must remain
valid for large frames and nested calls.

**Root cause:** not conclusively established.

**Fix:** none; no application workaround was accepted.

**Upstream relevance:** unknown; do not submit as an upstream bug without a
small target-independent or REM-only reproducer.

### 4.16 Native process creation limitation

A real Samurai build reached `posix_spawn` and received `ENOSYS`. Both GCC and
chibicc Samurai binaries shared this behavior. This is a REM kernel/libc
capability boundary, not a chibicc issue, and is included here only to prevent
misclassification.

### 4.17 REM backend wide-comparison lowering

**Classification:** REM-SPECIFIC ISSUE.

**Symptom:** 64-bit preprocessor comparisons on the 32-bit REM target chose
incorrect branches. A comparison of a high and low 32-bit word could let the
low word override a non-equal high word.

**Fix:** `gen_wide_binary()` now orders the high-word comparison before the
low-word tie case. Regression: `test/rem-pp-int64-compare.c`.

**Verification:** hosted `strings.c` and the Stage-7 compiler qualification
passed. This is target/backend work and should not be submitted as a generic
chibicc bug.

### 4.18 Preprocessor conditional evaluation must not use host-width `long`

**Classification:** UPSTREAM PORTABILITY ISSUE.

**Symptom:** `#if UINTPTR_MAX == UINT64_MAX` evaluated incorrectly on REM32.
`eval_const_expr()` returned `long`, truncating the 64-bit operand.

**Minimal reproducer:**

```c
#include <stdint.h>
#if UINTPTR_MAX == UINT64_MAX
#error wrong ILP32 branch
#endif
int main(void) { return 0; }
```

**Fix:** `toolchain/chibicc-rem/preprocess.c` evaluates and stores the result
as `int64_t`. Regression: `test/rem-pp-uintptr-width.c`.

**Verification:** the corrected native compiler selected the ILP32 branch and
Samurai completed its post-build logging without the prior fault. This should
be considered for upstream because a 32-bit host can expose the same mistake;
reproduction against pristine upstream remains to be done.

### 4.19 Preprocessor integer literal typing on ILP32

**Classification:** UPSTREAM PORTABILITY ISSUE.

**Symptom:** `convert_pp_int()` represented a large hexadecimal `U` literal as
32-bit `unsigned long` when `long` was 32 bits. This compounded the conditional
comparison problem.

**Fix:** `toolchain/chibicc-rem/tokenize.c` chooses `unsigned long long` for
values requiring 64 bits when target `unsigned long` is narrower.

**Verification:** `UINTPTR_MAX`/`UINT64_MAX` regression passed; musl expanded
`PRId64` to `"lld"`; corrected Samurai real/no-op/changed-input/clean tests
all exited 0. This is a portability candidate, not a REM ABI issue.


### 4.20 Incoming stack-argument offset

**Classification:** REM-SPECIFIC ISSUE.

**Symptom:** functions with more than four arguments loaded a stack argument
from its local home slot rather than from the caller's stack area. The
self-hosted compiler then corrupted global-initializer output.

**Fix:** `emit_text` tracks incoming stack words independently and loads them
from `frame_size + 32 + stack_word * 4`, with 64-bit arguments consuming two
words. Regressions: `rem-six-argument-call.c` and
`rem-global-initializer.c`.

**Verification:** the corrected native compiler passed the six-argument and
global-initializer reducers and later Stage-7 qualification. This is wholly
REM ABI-specific.

### 4.21 REM32 aggregate `double` alignment

**Classification:** REM-SPECIFIC ISSUE.

**Symptom:** a self-hosted `Token` layout differed between generations because
`double` was aligned to 8 bytes instead of the REM ILP32 ABI's 4-byte rule.
The resulting field offsets corrupted token pointers in large preprocessing
runs.

**Fix:** target type alignment was corrected from 8 to 4; regression:
`rem-double-alignment.c`. The corrected layout is 68 bytes where the bad
layout was 72 bytes.

**Verification:** Stage-3/Stage-7 hosted translation and token-chain tests
passed after the fix. This is target data-model work, not an upstream generic
bug.

### 4.22 Empty parameter lists

**Classification:** REM-SPECIFIC ISSUE.

**Symptom:** `int main()` was emitted with variadic-frame behavior, causing
native bootstrap failures. In C, an empty parameter list is an old-style
unspecified prototype, not a declaration of a variadic function.

**Fix:** parser/type handling distinguishes empty old-style parameter lists
from `...`; regression `rem-empty-params.c`.

**Verification:** host-generated REM startup code and the native compiler
smoke tests passed. Target ABI/frontend issue.

### 4.23 Debug information option

**Classification:** TEST/REGRESSION RECOMMENDATION.

**Symptom:** `-g` is accepted by the driver but ignored; generated binaries
contain no chibicc-produced DWARF. remdbg can inspect symbols and addresses,
but source-level debugging is not provided by chibicc.

**Status:** open feature gap. No patch was made and no upstream bug is claimed.
A future portable test should assert either documented rejection or actual
DWARF emission rather than silently accepting the option.

## 5. Stage-7 bootstrap and Samurai findings

The original report called the hosted `strings.c` result an unresolved
stage-3 divergence. Later qualification completed the relevant native stages:
all nine chibicc translation units compiled and assembled, the next compiler
linked and ran, and the qualified Stage-7 compiler rebuilt all thirteen
unmodified Samurai translation units. The original Stage-7 binary is now
superseded by the corrected artifact listed in Section 2.

A REM-backend defect was found first: 32-bit lowering of 64-bit `<` and `<=`
allowed a low-word comparison to override a non-equal high-word comparison.
This made a stage-3 preprocessor conditional such as
`#if __cplusplus >= 201103L` choose the wrong branch. The fix is in
`gen_wide_binary()` and the regression is `test/rem-pp-int64-compare.c`.
This is a REM backend issue, not a generic preprocessor claim.

The later Samurai post-build fault was a separate, upstream-relevant ILP32
portability failure. The following header-level test is the essential
reproducer:

```c
#include <stdint.h>
#if UINTPTR_MAX == UINT64_MAX
#error ILP32 preprocessor selected the LP64 inttypes branch
#endif
int main(void) { return 0; }
```

On REM, `preprocess.c:eval_const_expr()` returned `long`; because `long` is
32 bits, evaluating `UINT64_MAX` truncated the value. In addition,
`tokenize.c:convert_pp_int()` typed a sufficiently large hexadecimal `U`
literal as 32-bit `unsigned long` instead of `unsigned long long` when
`long` was 32 bits. Musl therefore selected `PRId64="ld"` rather than
`"lld"`. Samurai's logging call passed an `int64_t` timestamp, but musl's
`vfprintf` consumed only one 32-bit word and interpreted the high word as the
following `%s` pointer. The observable result was a post-build fault in
musl `memchr`/`strnlen` with a small invalid address. The reference Samurai
binary reproduced the same symptom on the same guest, but the corrected
compiler removed it.

The fix was to use `int64_t` for preprocessor conditional evaluation and to
make preprocessor integer-literal typing target-aware, preserving 64-bit
values as `unsigned long long` on ILP32. Regression:
`toolchain/chibicc-rem/test/rem-pp-uintptr-width.c`. The corrected compiler
emits `PRId64="lld"`, and the corrected Samurai passed `samu -n`, a real
build, no-op rebuild, changed-input rebuild, clean exit, and target execution.
The corrected artifacts are `/tmp/REM-R3-HANDOFF/codex-2/artifacts/chibicc`
(SHA-256 `9aa9782dfeeac64e2ffa22d222f88afb3e2293cd19dc34d857bbf721a05ac761`)
and `/tmp/REM-R3-HANDOFF/codex-2/artifacts/samu` (SHA-256
`1bee2e3c9aa19fe0ece0af1eb333b41e63ba7382bff993b68d297515d9d1aa93`).
This is relevant upstream because the source assumes host `long` can represent
all C preprocessor integer results; that is false for 32-bit hosts/targets.
The exact fix has not been tested against pristine upstream, so classify it as
an upstream portability issue rather than a proven upstream regression.

The earlier `#include "chibicc.h"` differential probe that appeared to show a
`NULL`/`nullptr` divergence was invalid because it omitted
`-I/root/chibicc-src`; no upstream claim is based on that run. The valid
header and integer-width tests, followed by the corrected Samurai run, are the
binding evidence.

## 6. Patches / Changes


### A. REM target backend implementation

The REM port adds code generation for REM registers, instructions, ELF/GAS
syntax, ILP32 data layout, calls, returns, relocations, 64-bit pairs, and the
REM variadic ABI. These changes are target-specific and should not be sent to
upstream as generic fixes.

### B. Generic or portability candidates

The following may merit separate upstream discussion only after reproduction
against pristine upstream:

- accepting GNU declaration attributes after named parameters;
- avoiding an unsupported `__builtin_ctz` in portable compiler source;
- making preprocessor conditional evaluation independent of host `long`
  width;
- preserving 64-bit preprocessor literal types on ILP32 instead of selecting
  32-bit `unsigned long`;
- old-style parameter compatibility, if upstream behavior is incomplete.

### C. Regression additions

REM-specific fixtures include:

- `rem-call-home-area.c`;
- `rem-variadic-register-home.c`;
- `rem-u64-store.c`;
- `rem-wide-return.c`;
- `rem-nested-loop-switch.c`;
- `rem-indirect-call.c`;
- `rem-bitnot.c`;
- `rem-gnu-attribute.c`;
- `rem-oldstyle-main.c`;
- `rem-ilp32-predefined.c`;
- `rem-pp-int64-compare.c`;
- `rem-pp-uintptr-width.c`;
- `rem-six-argument-call.c`;
- `rem-global-initializer.c`.

### D. Experimental/non-upstream changes

Trace instrumentation, no-TLS assembler qualification, REM-specific predefined
macros, ABI home-area layout, relocation emission, and target runtime work are
REM integration changes. They should not be submitted as upstream chibicc
patches without a separate generic justification.

The reliable source-level REM port commit is `81fbf49`; upstream base is
`90d1f7f199cc55b13c7fdb5839d1409806633fdb`.

## 7. Suggested Upstream Submission Set

1. **“Accept declaration attributes after named parameters”** — potentially
   generic parser improvement; a minimal reproducer and tested REM patch exist,
   but pristine-upstream reproduction should be added first.
2. **“Avoid host-width `strtoul` when parsing integer literals”** — potentially
   relevant to 32-bit ports; submit only as a portability improvement with a
   host-independent test.
3. **“Make preprocessor conditional evaluation independent of host `long` width”** —
   tested REM32 regression; likely relevant to every 32-bit host/target, but
   still needs a pristine-upstream reproduction.
4. **“Preserve 64-bit preprocessor literal types on ILP32”** — tested with the
   `UINTPTR_MAX`/`UINT64_MAX` regression and Samurai's `PRId64` failure; a
   plausible upstream portability patch pending pristine reproduction.
5. **“Replace unsupported `__builtin_ctz` in bootstrap-sensitive source”** —
   possible portability cleanup; no standalone reducer was preserved.

Do not submit the REM call ABI, register-save area, relocation, instruction,
stack-frame, pointer-word, or control-flow changes as upstream bugs. They are
REM backend implementation.

The earlier `NULL`/`nullptr` observation was not submitted as an upstream
claim: one `chibicc.h` differential probe omitted its include path. The valid
integer-width regressions and the Samurai varargs symptom are documented in
Sections 4.18–4.19.

## 8. Reproduction Information

The broad REM qualification chain is:

```text
REM-native chibicc
    -> REM GAS assembler
    -> REM ld with CRT, musl libc.a and libgcc.a
    -> ELF32 MyEmulator2 executable
    -> REM Linux execution
```

For a minimal backend regression, build the relevant fixture with the REM
compiler, assemble with the qualified REM assembler, and link with the REM
linker and matching CRT/libc/libgcc tuple. The exact stage-3 artifact listed
above is suitable for inspecting the observed bootstrap behavior but requires
the REM guest to execute.

The no-TLS assembler used for the REM 0.1 qualification is:

    /tmp/rc3-qualified-as
    SHA-256: 1a437fd5bfe7564e61ac14ceacc40f06bc932bca2099b74a39e773010bf300ba

## 9. Current Bootstrap Status

| Stage / capability | Result | Evidence |
|---|---|---|
| Host chibicc emits REM assembly | PASS | REM fixtures and translation units assemble |
| Generated REM programs assemble/link/execute | PASS | hosted hello and application probes |
| Native chibicc executes on REM | PASS | qualified native compiler |
| Stage-7 chibicc qualification | PASS | nine TUs, partial/final link, `--help`, hosted return-42 |
| Corrected chibicc artifact | PASS | `/tmp/REM-R3-HANDOFF/codex-2/artifacts/chibicc` |
| All 13 unmodified Samurai TUs with corrected chibicc | PASS | native compile, assemble and link |
| `samu -n` | PASS | corrected Samurai |
| Samurai real/no-op/changed-input/clean builds | PASS | corrected Samurai, target executes |
| Complete generation-to-generation identity | NOT DEMONSTRATED | functional Stage-7 qualification is complete; exact stage identity is not claimed |

**Native compiler usable:** YES. The qualified corrected compiler is suitable
for the demonstrated hosted C workflow and Samurai rebuild.

**Complete self-hosting bootstrap equivalence:** not claimed as an exact
bit-for-bit generation identity. The report distinguishes that from the
functional Stage-7 and application qualification above.
