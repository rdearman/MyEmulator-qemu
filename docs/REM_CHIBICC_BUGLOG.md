# REM chibicc backend bug log

This log records defects found while bringing `toolchain/chibicc-rem` through real REM userspace programs. These are REM-specific backend/ABI failures, not claims about upstream chibicc.

## Native userland source-kit investigation (2026-10-06)

All items below are classified **REM-specific backend/ABI/runtime defects**.
Minimal C reducers are in `toolchain/tests/native-userland/`. They were also
compiled/run using otherwise-unmodified upstream chibicc revision
`90d1f7f199cc55b13c7fdb5839d1409806633fdb` on its supported x86-64 Linux target.
The float/varargs/integer probes pass there. The separately confirmed
upstream long-double *header* issue is recorded in the EXISTING
`CHIBICC_UPSTREAM_REPORT.md`; it is not conflated with REM's ABI.

The source-kit compiler repair is currently staged in
`/home/rick/rem-native-userland/kit/bootstrap/chibicc`, NOT yet qualified or
installed. Source inspection and host syntax checks are not native PASS.
No compiler replacement, Fold change, original-rootfs edit, kernel change,
or QEMU source change has been made for this task.

| ID | Defect / minimal C reproducer | Classification and current evidence |
|---|---|---|
| NU-01 | Floating arithmetic: `float-arithmetic.c` | REM backend dispatches double arithmetic to integer pair operations. Native `1.5+2.25==3.75` probe actually returned failure (42 in initial probe). Soft-float helper lowering staged; native fix verification pending. |
| NU-02 | Floating casts: `float-cast.c` | REM backend clears the high double word even on a double-to-double cast and treats integer-to-double as integer widening. Reducer passes pristine upstream; native regression pending. Proper signed/unsigned soft-float conversions staged. |
| NU-03 | Floating literals: `float-literal.c` | REM-native tokenizer's decimal loop itself uses unsupported floating operations; codegen serializes through invalid casts. Initial native emitted constants were incorrect. Pointer/raw-bit serialization and integer-only bootstrap conversion staged; native verification pending. Final compiler must use repaired libc conversion, not retain a limited decimal-parser workaround. |
| NU-04 | Wide variadic values: `va64.c` | REM `ND_VA_ARG` only handles widths through 4; actual native compiler faults compiling the probe. Cursor/pair lowering staged; native verification pending. Pristine upstream probe passes. |
| NU-05 | Unsupported-type diagnostic dereferences NULL: `unsupported-diagnostic.c` | REM `unsupported()` passes NULL to `error_tok` for tokenless ABI errors. VLA reducer must produce an explicit diagnostic, not a signal. Null-safe error path staged; native verification pending. Upstream supports the VLA and returns 42. |
| NU-06 | Wide argument split at r4/stack boundary: `wide-split-argument.c` | Existing REM call/callee lowering spills the entire pair when one word still fits, disagreeing with GCC's partial-register argument ABI. Word-wise split staged; mixed-toolchain native regression pending. Pure same-compiler calls alone are insufficient ABI proof. |
| NU-07 | Stack-passed last named variadic parameter: `va-stack-named.c` | REM lowering explicitly rejects `va_start` after the fourth named word. Cursor calculation now accounts for named word widths; native verification pending. |
| NU-08 | 64-bit subtraction borrow: `wide-subtract.c` | REM code compares old low lhs against result rather than low rhs; 0x100000010-0xffffffff exposes the wrong borrow. Source correction staged; native verification pending. |
| NU-09 | 64-bit signed comparisons and <= equality: `wide-compare.c` | REM compares low words as signed and overwrites rhs before computing <= equality. Source correction staged; native verification pending. This extends prior comparison entries rather than implying upstream failure. |
| NU-10 | 64-bit negation carry: `wide-negate.c` | REM high-word carry for two's-complement negation is reversed. Source correction staged; native verification pending. |
| NU-11 | Scalar truth of a nonzero high word: `wide-truth.c` | REM branch/logical lowering tests only r1. Source correction includes high-word truth and floating signed-zero handling; native verification pending. |
| NU-12 | Long-double target layout: `long-double-abi.c` | REM type uses size/alignment16/16 despite target GCC/musl binary64 ABI8/4. Type correction staged; native/mixed ABI verification pending. Upstream header inconsistency is a separate defect, not this target-layout repair. |
| NU-13 | Libc floating formatter restores frame-size constant: `libc-float-printf.c` | Native linked program faults at printf_core PC02004378, VA254, FP2a0. Linked old fmt_fp stores r12=672 into its saved-FP slot. Requires native libc repair, not suppressing formatting or modifying the kernel. No fixed-libc PASS yet. |
| NU-14 | Indirect-call expression-stack depth decremented twice: `indirect-call-depth.c` | REM `gen_call` decrements once in `pop_reg` and once explicitly for a register-only indirect target, corrupting subsequent helper padding calculation. Redundant decrement removed in staged source; native verification pending. |
| NU-15 | Call alignment ignores enclosing expression temporaries: `assignment-call-alignment.c` | REM padding ignored `eval_depth`, so saving an assignment destination before a call misaligns the callee stack. Padding must also be tracked while evaluating nested arguments. Source correction staged; native verification pending. |
| NU-16 | Callee-saved registers not preserved, including r15 on large frames: `qsort-large-frame.c` | REM C backend uses r6-r9 and r15 as scratch but only saves r5/LR. A GCC/libc caller requires its preserved registers/frame pointer intact across callbacks. Large stack adjustment must use caller-saved r12, and all used preserved registers must be saved/restored. Source correction staged; mixed libc/native verification pending. |
| NU-17 | Large variadic register-save addressing clobbers incoming arguments: `va-large-frame.c` | `store_fp(r1, large_offset)` materialized its address in r2 before saving incoming r2. Address scratch moved to caller-saved r12. Source correction staged; native verification pending. |
| NU-18 | Explicit local alignment ignored: `local-alignment.c` | REM local layout used only the type's natural alignment, not the object's `_Alignas` requirement. Source correction takes the maximum; native verification pending. |
| NU-19 | Large zero-initializer offsets exceed REM immediate range: `large-zero-initializer.c` | REM `ND_MEMZERO` emitted thousands of byte stores with direct offsets past the signed immediate limit. A counted pointer loop removes out-of-range offsets without omitting initialization. Source correction staged; native verification pending. |
| NU-20 | Scalar conversion to `_Bool` does not normalize: `bool-normalization.c` | The inherited REM `ND_CAST` has no `_Bool` case. Storing a pointer or integer in a one-byte Boolean retains its low byte rather than `(value != 0)`. A nonzero value with a zero low byte becomes false. Corrected lowering uses the complete scalar truth test, including wide and floating values. Pristine upstream and system GCC reducers return 0. Native request 024 proves the inherited compiler's reducer returns 1 and the repaired integer-stage compiler's reducer returns 0. |
| NU-21 | REM driver discards linker options: `driver-library-search.c`, with `driver-library-provider.c` | The REM branch of `run_linker` ignores the parsed `ld_extra_args`, including `-L`. The upstream x86-64 driver builds and runs this library-search probe successfully. The REM branch now forwards those arguments and accepts an explicit private libc archive through `CHIBICC_REM_LIBC`; native driver verification is pending. |
| NU-22 | Installed soft-float runtime uses non-IEEE encoding: `soft-float-encoding.c` | Native request 037 converts integer-derived 1.5 to raw bits `00000000800007fe`, not IEEE binary64 `3ff8000000000000`, and returns 1. Pristine upstream chibicc on x86-64 and system GCC produce the expected bits and return 0. This is a REM runtime/ABI artifact defect, not an upstream chibicc defect. Source-built IEEE helpers are staged; native verification is pending. |

NU-22 explains why request 030's integer-derived arithmetic checks passed
while request 033's literal arithmetic failed: the old helpers were internally
consistent with their own incorrect representation. That arithmetic result
does **not** establish IEEE compatibility. Request 035 verifies that the
repaired compiler emits the correct IEEE literal words and documented
low/high argument words. The baseline `__eqdf2` disassembly instead extracts
the sign from r1 bit 0 and the exponent from r1 bits 1..11.

The cached GCC source used for the old artifact selects `__BIG_ENDIAN` in
`libgcc/config/myemulator2/sfp-machine.h`, despite REM being little endian.
The current repository's `toolchain/gcc/sfp-machine.h` already selects little
endian; no change to that current header is warranted by this evidence.
The retained baseline disassembly and native request results are in
`/home/rick/rem-native-userland`.

The source bootstrap now includes compiler-rt 15.0.7's scalar IEEE helpers
(official source SHA256
`353832c66cce60931ea0413b3c071faad59eefa70d02c97daa8978b15e4b25b7`),
preserving its Apache-2.0-with-LLVM-exception license. No CMake is required:
the native script compiles 29 C files and links a relocatable object explicitly
before the legacy archives. REM selects software-only implementations;
zero returns are constructed through integer union members so the first
integer-only compiler stage needs no floating literal parser. The default
rounding mode is nearest-even, without a hardware exception environment.
Host assembly generation is not native runtime acceptance.

The integer-only host-emitting REM compiler generates assembly successfully
for all 29 selected compiler-rt sources with `__SOFTFP__=1`; results are retained
in `bootstrap-host-syntax/ieee-integer/results.json`. Native revision 4 built
successfully (785712-byte compiler); the source-only IEEE bundle was checked
against SHA256 `a26cc7fe4b626659d98425e0247da825872e52cf4ce6c7ede6f4a1afab1ac8dd`
before extraction into the disposable guest. Its native IEEE build is ongoing,
not yet an execution PASS. `ieee-soft-float.c` extends NU-22 acceptance to
literal arithmetic, binary32, wide conversions, NaN, signed zero and subnormals.
The system GCC test passes; pristine upstream exposes a separate NaN-to-Boolean
defect now documented, with its own minimal reducer, in the existing upstream
report.

Native request 044 verifies the corrected IEEE encoding of 1.5. Request 048
then passes the scalar `ieee-soft-float.c` acceptance after relinking the
native compiler itself against the corrected helpers. The previously linked
compiler still used the old conversion helper when serializing binary32
literals; changing only the compiled application's helper object was not
enough. Full compiler/libc/bootstrap and package acceptance remain pending.

The native linker's default `ld -r` script also injects executable image
boundary symbols (`__text_start`, `__image_end`, etc.). Combining two partial
objects consequently failed with duplicate definitions (request 047).
Bootstrap partial links now use an explicit relocatable-section script with
no executable image symbols; request 048 proves this fixes the blocking link
without suppressing duplicate-definition errors or changing QEMU/kernel.

### Hostname-resolution failure reported on the Fold

The reported `nc bbc.co.uk 80` fault has r15/fp=`0x570` (1392), while raw
`nc 1.1.1.1 80` succeeds. The retained baseline libc disassembly shows exactly
this frame size in `__lookup_serv`: `addi r12,r0,1392`, then
`sw r12,4(r13)` in the saved-frame-pointer slot. This is the already established
NU-13 libc construction defect on a resolver path, not evidence of a kernel
or QEMU network failure. Exact Fold PC-to-symbol correspondence has not yet
been verified. `__lookup_serv` and the remaining audited resolver members are
already included in the source rebuild.

The source kit's `tests/dns-tcp.c` separately exercises raw outbound TCP,
`getaddrinfo` hostname resolution, TCP by hostname, and a second resolver call.
A small source-built `nc HOST PORT` client links against the repaired runtime,
because changing a private archive cannot repair the existing static BusyBox
nc executable. BusyBox itself, vi and boot programs are not replaced. Native
DNS acceptance remains pending.

`float-global.c` is the additional static-initializer reducer for NU-01/03;
runtime literals and global constant evaluation must both pass before the
floating compiler is accepted.

NU-13 is not limited to floating printf. A native HTTP-transfer helper also
faulted after `strstr` restored a corrupted frame pointer:
`pc=02002a0c`, `fp=00000480`, `address=0000046c`. Its long-needle path enters
`twoway_strstr`, whose baseline prologue stores the 1152-byte frame size in the
saved-FP slot. `libc-strstr-frame.c` is the additional minimal runtime reducer.
A read-only audit of the original `/usr/lib/libc.a` identifies 35 affected
functions across 32 archive members, including name resolution, `strstr`,
`memmem`, floating parsing and formatting. The same audit finds no matching
bad prologue in the original `libgcc.a`. Rebuilding just floating printf is
therefore insufficient: the source bootstrap must repair every affected libc
member. Audit output is retained in the isolated workspace's
`large-frame-audit.json`; no original filesystem was changed.

### First native bootstrap and Boolean normalization

The initial integer-only compiler built natively (698856 bytes), but is not
accepted as a self-hosted toolchain. Its attempt to build the floating stage
stopped with an implicit declaration of `parse_hex`; a complete preprocessing
probe was interrupted after it failed to finish within the command deadline.
The simple conditional-macro probe succeeded. These results are retained as
requests 018 through 023 in the workspace, not recorded as bootstrap PASS.

NU-20 also affects the bootstrap compiler's own `#ifdef`/`#ifndef` handling:
`bool defined = find_macro(...)` is compiled by the inherited compiler before
the repaired backend can take over. The source now spells these conversions
as `find_macro(...) != NULL`, and similarly normalizes the keyword-map result.
This preserves C semantics while making the first source-built stage usable
with the defective installed compiler. The backend `_Bool` correction remains
required and has its own reducer; these source adaptations do not substitute
for native regression and self-rebuild acceptance.

### Runtime-source portability work following the first stage

The retained libc source build has been expanded from three C files to all
32 audited defective archive members, plus `strtod` and assembly syscall/TLS/
atomic bridges. The private runtime archive is constructed from the installed
archive plus source-rebuilt replacements; installation does not overwrite the
original archive. No rebuilt-runtime execution PASS is claimed yet.

Musl's extended-inline-assembly operations now have equivalent external REM
ABI leaf functions for TLS reads and compare-and-swap, alongside the syscall
bridges. The compiler parser now preserves GNU weak/alias declarations rather
than silently discarding them, with `gnu-weak-alias.c` as a focused extension
probe; these are source-port features, not upstream C-conformance allegations.

The runtime source check also exposed two genuine architecture-independent
parser defects. `comma-function-declarations.c` and
`compound-literal-member.c` both fail with otherwise-unmodified upstream on
x86-64, while system GCC compiles and runs them successfully. Their detailed
upstream classification is in the existing `CHIBICC_UPSTREAM_REPORT.md`.
The staged parser fixes let every audited runtime C source generate REM
assembly in a host-emitting check (33 sources, including `strtod`). This is
useful frontend/backend evidence, but is not a native runtime build or ABI
execution acceptance.

Request 030 separately proves arithmetic lowering without relying on the
unfinished floating-literal parser stage: `float-integer-bootstrap.c` compiles
and runs with the repaired native integer-stage compiler, both exits 0. It
tests double division/addition/multiplication, integer conversions, floating
comparisons and negative-zero conversion to `_Bool`. The same ordinary C
probe passes pristine upstream and system GCC. This is genuine native
execution evidence for NU-01/02 and the scalar-truth path, but not acceptance
of decimal parsing, repaired libc formatting, or the complete bootstrap.

The REM source-kit `float.h` now also matches the binary64 target's
`DECIMAL_DIG=17` and includes C11 decimal-round-trip/subnormal macros. The
existing upstream header report records the independently reproduced missing
C11 macros using `float-header-macros.c`; that is separate from REM's required
binary64 macro values.

### Existing curl crash: binary-level causal resolution

Existing `curl --version` reproduced the recorded userspace fault at
PC0203698c (`curl_msnprintf+0x4c`), VA13f8, FP1400. In the retained failing ELF,
`formatf` materializes its frame size in r12 at 02036a30/34, subtracts SP,
then stores r12 at 4(SP) at **02036a40** as if it were the saved caller frame
pointer. Its epilogue at **02037af8** restores that slot into r15. The caller's
`lw r2,-8(r15)` at **0203698c** then faults at exactly 1400-8=13f8.
This is old REM compiler-generated frame corruption, not TLS/networking and
not the separate repaired kernel nested-IRQ issue. The exact old compiler
source/link tuple was not retained; the current cross-GCC source must not be
blamed without matching provenance. Upstream native curl rebuilding against
the repaired source-built compiler/runtime is still required.

### Preserved evidence and safety boundary

Initial native probe logs and generated assembly are retained under
`/home/rick/rem-native-userland/requests/`; the failing float executable and
old curl ELF disassemblies resolve the actual frame failures. Testing used an
owned disposable image derived from `fold-flight/final.ext4`; all QEMUs
started for initial probes were stopped, including clean guest poweroff of
the final initial session. Sources were fetched from public upstream release
sites, without uploading repository data or contacting maintainers.
No source archive, HTTPS test, userland build or replacement libc has yet
received acceptance. Source-extraction slowness is not a proven tar defect.

## 1. Call-boundary stack alignment

**Exposed by:** mixed chibicc/musl calls in Samurai and hosted call probes.

**Symptoms:** register-only calls entered library code with an incorrectly aligned stack and could trap.

**Root cause:** padding was calculated from the complete temporary argument list even though register arguments are popped before the call.

**Changed:** `toolchain/chibicc-rem/codegen.c`, `gen_call`.

**Fix:** calculate padding from arguments remaining on the outgoing stack while preserving the REM ABI home area.

**Regression/validation:** integer calls, `printf("%d", 42)`, and the Samurai translation-unit build pass the host REM backend, native assembler, and linker pipeline.

## 2. Variadic register-save area and `va_start`

**Exposed by:** a chibicc caller into GCC/musl `vfprintf`.

**Symptoms:** `vfprintf` read the format pointer as the first variadic argument.

**Root cause:** incoming `r1`--`r4` were saved inside the active callee frame, where a nested callee could overwrite them.

**Changed:** `toolchain/chibicc-rem/codegen.c`, variadic frame emission and `ND_VA_START` lowering.

**Fix:** save incoming registers at old-SP+16, outside the active frame, and initialise the va-list cursor there.

**Regression/validation:** internal variadic tests and the mixed chibicc-to-GCC/musl `vfprintf` probe pass.

## 3. Array/function argument pointer sizing

**Exposed by:** Samurai diagnostics and variadic string-argument probes.

**Symptoms:** a string literal was staged as multiple argument words, shifting following arguments.

**Root cause:** call lowering used `ty->size > 4` without applying C array/function decay at the ABI boundary.

**Changed:** `toolchain/chibicc-rem/codegen.c`, `arg_words` and `gen_call`.

**Fix:** arrays and function types consume one pointer word; actual two-word scalar values consume two words.

**Regression/validation:** pointer/string calls and the mixed variadic probe agree with the GCC/musl result.

## 4. Static pointer relocation emission

**Exposed by:** Samurai's static keyword table.

**Symptoms:** names in a global table were null at runtime, leading to a `strcmp(NULL, ...)` parser fault.

**Root cause:** aggregate data bytes were emitted but recorded pointer relocations in `Obj.rel` were dropped.

**Changed:** `toolchain/chibicc-rem/codegen.c`, `emit_data`.

**Fix:** emit recorded 32-bit pointer relocations as `.word` expressions for the REM ELF toolchain.

**Regression/validation:** the isolated static-struct pointer fixture and a fresh Samurai build pass assembly/link validation; `samu -h` reaches usage successfully.

## 5. Plain 64-bit local-store corruption

**Exposed by:** Samurai's 64-bit hash path and `test/rem-u64-store.c`.

**Symptoms:** assigning a 64-bit local could store through a stale address register because the high value word occupied `r2`.

**Root cause:** `store()` recovered the destination into `r2` and then used `r3` while preserving an `r1:r2` value.

**Changed:** `toolchain/chibicc-rem/codegen.c`, `store`.

**Fix:** recover 64-bit destinations into `r3`, leaving `r1:r2` available for the value pair.

**Regression/validation:** `test/rem-u64-store.c` assembles and links; the fresh 12-file Samurai build completes.

## 6. Nested loop/switch/break control-flow lowering

**Exposed by:** Samurai's ARGBEGIN/EARGF option parser shape.

**Symptoms:** generated code was difficult to audit around nested option loops and switches; earlier builds appeared to lose the loop back-edge while processing `break` targets.

**Root cause:** loop-header labels were generated separately during codegen from parser-owned `break`/`continue` labels, making the control-flow contract implicit for nested constructs.

**Changed:** `toolchain/chibicc-rem/chibicc.h`, `parse.c`, and `codegen.c`.

**Fix:** each loop node now carries an explicit parser-generated `begin_label`; codegen branches to it while retaining distinct `cont_label` and `brk_label` targets.

**Regression/validation:** `test/rem-nested-loop-switch.c` produces valid REM assembly and links with native REM tools. Guest `samu -n` still requires a clean valid disposable runtime image for final execution acceptance.

## Open investigations

## 8. Incoming stack-argument offset

**Exposed by:** Stage-3 self-compilation reducer:

```c
int x = 1;
```

Stage-2 completed this translation unit; Stage-3 faulted while serialising
the global initializer.  remdbg on the ptrace-enabled diagnostic kernel
stopped in `write_buf`; the caller chain included `write_gvar_data`, whose
five-argument call path had loaded its stack parameter from the wrong address.

**Root cause:** parameter homes (the compiler's local copies beginning at
`v->offset`) were incorrectly reused as incoming stack-argument offsets.  The
REM ABI places stack arguments above the caller's 32-byte register home area.

**Changed:** `toolchain/chibicc-rem/codegen.c`, `emit_text`.

**Fix:** track stack words independently and load stack-passed parameters from
`frame_size + 32 + stack_word * 4`; 64-bit parameters advance by two words.

**Regression:** `test/rem-six-argument-call.c` and
`test/rem-global-initializer.c`.

**Status:** source fix applied.  A fresh cross-built diagnostic compiler
`/tmp/chibicc-stackfix-cross/chibicc-stackfix` (SHA-256
`509676edf8c9b32c9d37011e2239df63259f68aa353e4a31aa4b9250f8db2279`) compiled
the reducer `int x = 1;` natively inside the ptrace-enabled REM guest with
`STAGEFIX_RC:0`.  The same compiler progressed further on `strings.c` but
then exposed a separate invalid macro-table key in `get_or_insert_entry`; that
remaining Stage-3 divergence is still under investigation.  This is a
confirmed REM backend/ABI defect, not a Samurai or libc workaround.

## 7. Narrow-to-wide return sign extension

**Exposed by:** Samurai's `osmtime()` missing-file sentinel path and a minimal
function returning `(int64_t)-1` from a 32-bit expression.

**Symptoms:** the callee returned a valid low word but left the high return
word stale.  Callers comparing the 64-bit sentinel then observed garbage;
diagnostic output could become misleading even though the underlying `stat`
and `errno` calls were correct.

**Root cause:** integer casts to a 64-bit return type generated the low word
only.  The REM ABI returns 64-bit scalars in `r1:r2`.

**Changed:** `toolchain/chibicc-rem/codegen.c`, `ND_CAST` and 64-bit return
handling.

**Fix:** sign-extend signed narrow values into `r2` with `srai`, and clear
`r2` for unsigned widening.

**Regression/validation:** `test/rem-wide-return.c` and the minimal
`osmtime`/sentinel reproducer now generate the required high return word;
Samurai was rebuilt through native assembly and linking.

### Stack/frame handling in large applications

Large applications exposed faults involving frame restoration, frame-relative loads, and large local frames (including Emacs and coreutils probes). These remain under investigation; no application source workaround is accepted as a compiler fix. The next step is a minimal reproducer for each distinct failure, followed by comparison with REM GCC output.

### Remaining Samurai runtime validation

The compiler and native binutils produce a linked Samurai executable and `samu -h` works. A previous disposable image emitted `MYEMU llist bad add` before reaching the shell, so its `samu -n` result is invalid. Validation must be repeated with a clean image and a pinned QEMU/kernel/rootfs/toolchain tuple.

## 8. Main argument diagnostic corruption (reclassified)

**Reproducer:** unmodified Samurai, clean REM image, `cd /root; samu -n`.

**Symptom:** after manifest parsing, the `argc` local used to select default
nodes contains a code/data address rather than zero; graph traversal then faults.
The independent `osmtime()`/stat probes pass, so this is not an errno or mtime
sentinel defect.

**Status:** the temporary caller-home-area hypothesis was disproved by GCC
assembly comparison and reverted. The current failure has moved to the
`xasprintf`/`getbuilddir` path; the malformed `.ninja_log.tmp` prefix still
requires a minimal string/variadic reproducer before another compiler change.

## 9. Fresh GCC/chibicc Samurai comparison (not yet a compiler defect)

The earlier Samurai binary comparison was invalidated by a reboot and stale artifacts. A clean rebuild from Samurai commit `531ba700c5fbbfd3d8e3ed91226d0c57134a6fb3d` produced fresh GCC and chibicc ELFs from the same thirteen sources. GCC's `samu -n` passed in the same clean guest while fresh chibicc `samu -n` faulted in `strcmp`.

The fresh chibicc call site loads `buf.data` into `r1` and the selected keyword pointer into `r2` immediately before `jal strcmp`; the old fault address does not apply. A smaller static-buffer/reallocation reproducer faulted for both GCC and chibicc in the same disposable guest. Therefore this comparison alone does not prove a chibicc defect, and no compiler source was changed. A reducer that reproduces only with chibicc is required before changing the backend.

### Minimal probe results

Direct string comparison, `char *` arrays, pointer-containing structs, a
six-entry keyword table, and a corrected realloc-growing NUL-terminated buffer
all pass with both REM-GCC and chibicc. The initial buffer reducer was invalid
because it passed a non-NUL-terminated string to `strcmp`. These results do
not identify a generic chibicc pointer or string-codegen defect.

## 10. Indirect function-pointer call clobbered an argument

**Exposed by:** the unmodified Samurai graph path, specifically
`defaultnodes(buildadd)`, after parsing a minimal manifest.

**Symptom:** `samu -n` reached graph traversal with a corrupted node pointer and
faulted while loading a graph node. Direct calls and small pointer/string
reducers passed.

**Root cause:** REM `gen_call` evaluated the indirect callee expression after
placing register arguments in `r1`--`r4`. Evaluating the function-pointer
expression reused `r1`, so the callee received the function address instead of
the first argument.

**Changed:** `toolchain/chibicc-rem/codegen.c`.

**Fix:** save an indirect call target in a temporary stack slot before argument
evaluation, account for that temporary in call padding, then load it into
`r15` and emit `jalr r15` after argument setup.

**Regression/validation:** `test/rem-indirect-call.c`; the clean chibicc-built
Samurai now completes `samu -n` on the pinned tuple.

## 11. REM NOT operand order emitted an illegal instruction

**Exposed by:** Samurai graph traversal after the indirect-call fix allowed the
next code path to execute.

**Symptom:** QEMU trapped on an instruction decoded as `not` before the graph
dry-run completed.

**Root cause:** the backend emitted `not r1, r0, r1`. The REM decoder requires
the second source operand of `not` to be the zero register; the valid form is
`not r1, r1, r0`.

**Changed:** `toolchain/chibicc-rem/codegen.c`.

**Fix:** emit the operand order required by the current REM encoding and
decoder.

**Regression/validation:** `test/rem-bitnot.c`; the clean Samurai binary
completed `samu -n` after this fix.

## 12. Native Samurai command execution is blocked by process creation

The fixed chibicc-built and GCC-built Samurai binaries both parse the same
manifest and complete `samu -n` on the same clean guest. A real build reaches
`osspawn()` and reports `posix_spawn /bin/sh: Function not implemented`;
`posix_spawn` receives `ENOSYS` from the common REM runtime tuple. This is an
OS/libc process-creation limitation, not a chibicc or Samurai-source failure.
The required platform work is a validated clone/spawn/exec path; no Samurai
workaround was applied.

## 13. GNU declaration attributes after parameters

**Exposed by:** the canonical Kilo source, whose signal handler declares an
unused parameter with `__attribute__((unused))`.

**Minimal reproducer:**

```c
static int f(int value __attribute__((unused))) { return 42; }
int main(void) { return f(7) == 42 ? 0 : 1; }
```

**Symptom:** chibicc stopped at the attribute token while parsing the
declaration (`expected ','`).

**Root cause:** the parser consumed GNU attributes on some type forms but did
not consume declaration attributes following a named declarator or parameter.

**Changed:** `toolchain/chibicc-rem/parse.c`.

**Fix:** consume balanced `__attribute__((...))` lists after declarators when
the attributes do not affect REM code generation.

**Regression:** `test/rem-gnu-attribute.c`.

**Validation:** Kilo now preprocesses, compiles, assembles, and links with the
REM chibicc tuple. Guest startup reached Kilo's terminal initialization; its
`TIOCGWINSZ` call returned zero rows/columns and the cursor-position fallback
waited for a terminal response, so interactive editing remains a terminal
qualification issue rather than a compiler failure.
## 14. Native bootstrap source used an unsupported host builtin

**Exposed by:** the first REM-native attempt to compile chibicc's own
`codegen.c`.

**Symptom:** the source used `__builtin_ctz` to encode an alignment exponent;
REM chibicc treated it as an undeclared function and could not produce a valid
self-hosting translation unit.

**Fix:** `toolchain/chibicc-rem/codegen.c` now uses a small ordinary C
`rem_ctz` helper. The host chibicc-generated assembly for `codegen.c` assembles
successfully, and the source remains target-independent.

**Status:** fixed for the identified source use. A complete REM-native
self-rebuild remains open: the current native bootstrap attempt did not finish
compiling `codegen.c` within several minutes, so no Level 7 claim is made.

## 15. Native self-hosting phase trace remains incomplete

The compiler was instrumented with `CHIBICC_TRACE=1` around tokenization,
preprocessing, parsing, code generation, and output. The host chibicc reaches
all phases for `codegen.c` and its output assembles. A clean guest image
staging attempt reached the boot test but the image was not mountable by the
kernel, so that run cannot provide a valid native phase result. Earlier valid
guest runs showed the native compiler entering the `codegen.c` compile and
making no phase-progress output before the timeout; this remains an open
bootstrap/resource investigation rather than a proven backend defect.

## 15. `-g` is accepted but emits no DWARF

**Exposed by:** integrating chibicc-built programs with the REM native
debugger's source and variable inspection.

**Observed:** `main.c` accepts options beginning with `-g` in the ignored
option list. The resulting object/executable has no usable `.debug_info` or
`.debug_line` compilation-unit data, so `remdbg source`, `locals`, and `globals`
cannot provide source-level information for this build. Native symbols and
instruction stepping remain available.

**Classification:** compiler feature gap / integration expectation, not a
code-generation regression. This log records the observed behavior; adding
DWARF emission belongs to the chibicc owner and is not implied by accepting
`-g`.

**Validation:** inspected the option handling in `toolchain/chibicc-rem/main.c`
and the debugger qualification's chibicc-produced ELF. GCC's separate DWARF
local-location mismatch is documented in `docs/REM_DEBUGGER.md` and is not
evidence about chibicc code generation.
## 16. `-g` is accepted but does not emit debug information

The REM chibicc driver accepts `-g` for compatibility, but its option parser
currently ignores the flag and the backend emits no DWARF sections. This is a
known tooling limitation: remdbg can consume symbols from existing debug
fixtures, but chibicc-built programs do not yet receive compiler-generated
source-line information. Debug-info generation remains future work for native
compiler qualification.

## 17. Native bootstrap preprocessing is the current large-file boundary

With a refreshed source tree containing the `rem_ctz` fix, a valid clean guest
ran the instrumented native compiler on `codegen.c` and emitted:

```text
TRACE cc1 start codegen.c
TRACE tokenize complete
```

Preprocessing did not complete during the observed multi-minute run. Host
chibicc completes the same file. This is now localized before parsing and code
generation, but ownership between native compiler performance, generated
preprocessor code, and guest runtime remains open; no workaround or self-hosted
stage-2 binary is claimed.
## 18. Preprocessor phase isolation for native self-hosting

Added `CHIBICC_TRACE_PP=1` tracing around include-file reads and periodic
preprocessed-token progress. The first valid run did not reach the completion
marker within the observation window, but a longer run on the corrected musl
sysroot advanced through the headers and reached `TRACE preprocess complete`.
The result is a severe native-QEMU performance boundary rather than evidence
of a token-loop at one location. The complete stage-2 translation unit still
has not finished, so no self-hosted compiler claim is made.

## 19. Initial bootstrap stalls were partly invalid image tests

The first refreshed disposable image omitted `/usr/include`, so preprocessing
stopped at `assert.h`; that result was an image-staging error, not a compiler
defect. A corrected image with the REM musl headers and `-I/root/musl/include`
read the complete `chibicc.h` include tree and emitted periodic progress through
the preprocessor. The trace advanced from the musl headers into `chibicc.h`
instead of looping at one token. This establishes that the earlier boundary
was not a proven tokenizer or file-I/O hang. The remaining native bootstrap cost
is very high under the current QEMU tuple (the include-heavy translation unit
requires many minutes); Level 7 is still unproven and needs a timed, complete
translation-unit run before any further backend change is justified.

## 20. REM predefined macros incorrectly described the host LP64 model

**Reproducer:** `toolchain/chibicc-rem/test/rem-ilp32-predefined.c`, exposed
while preprocessing the hosted `strings.c` translation unit during native
self-hosting.

**Symptom:** the target preprocessor defined `__LP64__`, reported 8-byte
`long`, pointer and `size_t` widths, and advertised x86-64 macros even though
REM is an ILP32 target. This made musl headers select host-oriented
conditional declarations and made the native compiler disagree with its type
model.

**Root cause:** `init_macros()` retained upstream host chibicc predefined
macros after the REM backend changed the type sizes.

**Fix:** removed LP64/x86-64 markers, set REM widths to 4, set
`__SIZE_TYPE__` to `unsigned int`, and added `__myemulator2__`.

**Validation:** the host REM compiler's preprocessor now selects the REM
branch and rejects `__LP64__`; the corrected seed links as an ELF32
MyEmulator2 executable. Native full-header validation remains in progress
under the slow QEMU tuple; no Level 7 claim is made yet.

## 21. Empty function parameter lists were emitted as variadic functions

**Reproducer:** `toolchain/chibicc-rem/test/rem-oldstyle-main.c`, reduced from
a native-stage test whose generated `main` wrote `r1`--`r4` at offsets beyond
its frame and then terminated with a guest `Hangup`.

**Root cause:** `func_params()` treated an empty `()` as `is_variadic`, even
though C's empty parameter list is an old-style unspecified prototype. The
backend consequently emitted a variadic register-save area for ordinary
functions such as `int main()`.

**Fix:** added `Type.has_prototype`; empty lists are unspecified and do not
allocate a `va_list` save area, while explicit `(void)` and `(...)` retain
prototype/variadic semantics. Calls to old-style declarations remain
permitted. Type compatibility includes the distinction.

**Validation:** host-generated REM assembly for `int main()` no longer emits
the save-area stores. A native guest link/run of a nonzero-return program is
still being separated from the independent guest exit-status/Hangup path.

## 22. Native self-hosting reducer: hosted NULL plus pointer-member store

**Reproducer:** disposable native guest sources `probe9.c`/`probe10.c` during
the Level 7 run. `#include "chibicc.h"` plus
`arr->data[0] = NULL` terminates after preprocessing, before `TRACE parse
complete`, while the same expression with literal `0` completes. A
header-free equivalent struct with `#define NULL ((void *)0)` also completes.

**Current evidence:** the failure is therefore not a general `for` parser
failure, pointer-array failure, or `NULL` cast failure in isolation. It
requires the hosted chibicc header/macro environment and the pointer-member
store combination. The native run took about 225 seconds before status 129;
no source fix is claimed yet. This remains an open compiler/runtime reducer
for Level 7.

The bounded targeted follow-up confirmed that replacing `NULL` with literal
`0` reaches parse, code generation, and output successfully. The failing
variant does not reach the preprocess-complete marker in the native run, so
the current phase classification is **preprocessor/macro expansion or its
generated runtime**, not the REM assembler or linker. A targeted source fix
has not been justified; Level 7 remains blocked pending a smaller macro-table
or token-expansion reproducer.

## 23. Bounded include-guard/hash follow-up did not clear Level 7

**Investigation:** instrumented the native preprocessor around the hosted
`probe9.c` reducer and observed repeated processing of musl `features.h` and
`bits/alltypes.h` while macro-event counts continued to rise. This is
consistent with include-guard lookup or macro-table state failing before the
`NULL` expression is reached.

**Attempted fix:** a disposable build changed the FNV table hash to 32-bit to
avoid REM32 multiword hash arithmetic. The same hosted reducer still failed
before the preprocess-complete marker, so that change was reverted. No
compiler fix is claimed.

**Status:** Level 7 remains blocked at the hosted preprocessor boundary. The
next valid fix requires a smaller reproducible include-guard/macro-table
failure; broad application reduction is deliberately stopped here.

## 24. REM32 preprocessor hash width corrupted hosted include guards

**Reproducer:** `toolchain/chibicc-rem/test/rem-hosted-null-member.c`, run by
native chibicc with the musl include tree. The source includes `chibicc.h` and
stores `NULL` through `StringArray->data[0]`. Header-free and reduced-header
variants passed; the full hosted case stalled while repeatedly processing
musl include guards.

**Root cause:** `hashmap.c` used the 64-bit FNV-1a hash on REM32. The native
compiler's multiword arithmetic path produced unreliable macro/include-guard
lookups in the large hosted header table.

**Fix:** use the 32-bit FNV-1a constants for the REM32 hashmap. This keeps the
preprocessor hash in the target word size and removes the failing multiword
operation from this hot path.

**Validation:** a clean disposable REM image using the rebuilt native
compiler now reaches `preprocess complete`, `parse complete`, `codegen
complete`, `output complete`, and returns `NORMAL9_RC:0` for the reducer. The
QEMU instance was terminated and verified absent afterward. Full chibicc
self-compilation is the next validation stage.

**Follow-up validation:** with the 32-bit hash fix, clean native REM runs now
complete `strings.c`, `hashmap.c`, and `main.c` through preprocessing, parsing,
code generation, and assembly output. Stage-2 linking has not yet been
attempted.

## 25. Level 7 batch reached codegen.c performance boundary

**Validation:** after the hashmap fix, native REM chibicc completed and
assembled `tokenize.c`, `parse.c`, and `type.c` in one clean guest. The next
translation unit, `codegen.c`, reached `TRACE tokenize complete` but did not
reach `TRACE preprocess complete` within the 600-second bounded runner limit.
The guest and QEMU process were then cleaned up by the runner.

**Status:** this is now a measured bootstrap performance boundary, not a new
confirmed semantic defect. Stage-2 linking was not reached. Further progress
requires a longer controlled run or a separately scoped performance
investigation; no speculative compiler change is claimed.

## 26. Stage-2 batch reaches native assembler throughput boundary

**Validation:** with the optimized (`-O2`) native chibicc, the clean batch
completed compilation and assembly for `strings.c`, `hashmap.c`, `main.c`, and
`tokenize.c`, and completed compilation/code generation for `parse.c`.
`parse.c` reached `TRACE output complete` and then printed `ASSEMBLE:parse`,
but the native assembler did not complete within the bounded 2400-second guest
run. The QEMU guest was cleaned up.

**Status:** this is a native assembler/tool throughput boundary, not a new
chibicc semantic failure. Stage-2 linking remains untested. The next bounded
step is to qualify the native `as` path on the generated `parse.s` separately
and compare it with host REM `as`.

## 27. Native assembler qualification and stage-2 link result

**Native assembler:** the identical 793,886-byte REM `parse.s` assembled with
native REM `as` to a 212,432-byte `parse.o` in a fresh guest. This disproves an
assembler correctness failure for that input; the earlier batch timeout was
cumulative guest execution/resource pressure.

**Stage-2 link:** the persistent batch reached `unicode.o`; native `ld`
reported `STAGE2_RC:0` with the validated CRT, musl `libc.a`, and `libgcc.a`
tuple. A clean, persistent stage-2 executable run was not completed because
subsequent disposable image handling lost or invalidated the linked artifact.
No Level 7 claim is made.

## 28. Stage-2 self-rebuild stops after tokenization on hosted strings.c

**Validation:** the native-linked stage-2 compiler was recovered as a
751,184-byte ELF32 MyEmulator2 executable (SHA-256
`ce1b1e19ca759ef2d113b8a6a6cee3caf0b728655d8a0a3136735a6b353b49d1`) and
included before boot in a clean image. Stage-2 `--help` and a header-free
translation both passed.

**Failure:** compiling hosted `strings.c` with stage 2 reached
`TRACE tokenize complete` but terminated with guest status 129 before
preprocessing completed. This is a stage-2 bootstrap/runtime boundary; no
stage-3 claim is made. The stage-1 compiler remains able to translate the same
unit.

## 29. Stage-2 hashmap self-test diverges from stage 1

**Comparison:** stage 1 native chibicc completes `-hashmap-test` in a clean
REM guest. The recovered stage-2 executable starts but does not complete the
same test within the bounded run. Stage 2 can still print `--help` and compile
`int main(void){return 0;}`.

**Status:** this is the smallest current stage-generation divergence and
blocks stage 3. It points to a code-generation/runtime correctness issue in
the stage-2 output for the larger hashmap/format allocation path; no source
change has been made without a smaller reproducer.

## 30. ILP32 numeric-literal conversion truncated 64-bit constants

**Reproducer:** a native stage-2 translation containing 64-bit FNV constants
materialized `1099511628211ULL` and the expected hash value as `0xffffffff`
followed by zero. The stage-1 compiler accepted the same source.

**Root cause:** REM32 `strtoul` returns an unsigned long, which is 32 bits
under ILP32. Numeric-token conversion used it for all integer literals,
silently truncating `ULL` values before code generation.

**Fix:** `tokenize.c` now converts integer tokens with `strtoull` and casts the
result to `int64_t`.

**Validation:** a rebuilt stage-1 compiler emits correct 64-bit immediates; a
mixed stage-2 executable built with corrected `hashmap.o`, `tokenize.o`, and
`codegen.o` passes `-hashmap-test` in a clean guest. The focused 64-bit
multiply test remains blocked separately by the REM `__muldi3` runtime path.

## 31. REM libgcc `__muldi3` remains unqualified for 64-bit execution

**Reproducer:** a four-instruction REM assembly caller and an equivalent
GCC-generated C program both call `__muldi3` with `2 * 3`. The C/CRT forms
report a bus error or fail to reach their normal exit marker. A bare `_start`
caller reaches the instruction after `jal __muldi3`; its subsequent `halt`
then correctly traps as a privileged instruction. Thus the helper itself is
not yet proven to be the faulting operation.

**Comparison:** the failure is present with GCC-generated code as well as
chibicc-generated code, so it is not currently attributed to chibicc lowering.
The helper object uses the expected `r1:r2`/`r3:r4` convention, but runtime
execution still needs an isolated qualification/fix.

**Status:** open caller/CRT/runtime investigation. 64-bit helper calls still
need a passing hosted regression before Level 7 bootstrap is claimed.

## 32. Mixed-generation stage-2 objects are not a valid bootstrap

**Evidence:** the stage-2 executable linked from a mixture of native-generated
and host-generated objects passes `--help` and the hashmap self-test, but
hosted preprocessing either stalls after `TRACE tokenize complete` or raises a
user alignment trap in `preprocess2`. Replacing one object at a time changes
the failure boundary rather than producing a stable compiler.

**Interpretation:** this is an artifact-generation qualification failure, not
yet a new front-end semantic defect. A stage-2 compiler must be relinked from
a single consistent set of objects produced with the same REM target defines,
headers, and runtime tuple. No source workaround is justified by these mixed
object experiments.

## 33. Stage-2 hosted translation boundary revalidated

**Validation:** the intact native stage-2 artifact
`/tmp/chibicc-native-ilp32-new/chibicc` (ELF32 MyEmulator2) was staged into a
fresh guest with the matching musl headers and runtime. It translated a
`#include <stdio.h>` unit through preprocessing, parsing, code generation,
and assembly output, returning `D1_RC:0` and producing the output assembly.

This supersedes the earlier invalid/stale-image observations in §§28–29 for
that binary. A full all-source stage-2 rebuild and stage-3 execution remain
outstanding; mixed-object experiments are not accepted as bootstrap evidence.

## Stage-3 hosted preprocessor divergence (deferred)

**Exposed by:** the stage-3 self-rebuild attempt. Stage-2 compiled all nine
chibicc translation units; stage-3 was then used on `strings.c`.

**Symptoms:** stage-3 starts, handles `--help`, preprocesses/compiles hosted
hello, and links/runs it. On `strings.c`, it reaches the musl conditional in
`/usr/include/unistd.h` and reports `#define NULL nullptr`; stage-2 handles
the same source. A tiny stage-3 `0 >= 201103L` probe generated the expected
comparison assembly, so the header/macro-state divergence was not reduced to a
single expression in the bounded investigation.

**Status:** deferred and non-blocking for the REM 0.1 flight. The working
stage-3 artifact is preserved at `/tmp/chibicc-stage3-qualified`; full
stage-3 self-rebuild equivalence is not claimed. No source workaround was
made for this bounded investigation.

## Differential preprocessor investigation (2026-09-29, saved)

Stage-2 and stage-3 were run on identical disposable REM inputs. They agreed
on standalone `#ifdef __cplusplus`, `defined(__cplusplus)`, `<unistd.h>`, and
the combined standard-header probe; both emitted `CPP_NOT_DEFINED`. Their
first observed output difference was removal of `restrict` qualifiers, which
is unrelated to the reported NULL branch. The attempted `#include "chibicc.h"`
probe was invalid because the disposable working directory did not include
`-I/root/chibicc-src`; both compilers correctly reported the missing header.
The exact chibicc.h-dependent divergence therefore remains unresolved. No
source workaround or header modification was made.
## Confirmed REM backend defect: wide integer comparison lowering

**Symptom:** the stage-3 compiler evaluated `#if __cplusplus >= 201103L` as true
even though `__cplusplus` was not defined (and therefore had value zero in the
preprocessor expression).  This caused musl's headers to select the `nullptr`
branch and made the native bootstrap diverge while compiling `strings.c`.

**Root cause:** `gen_wide_binary()` combined the high-word and low-word
comparisons incorrectly for signed/unsigned `<` and `<=`.  A low-word result
could override a non-equal high-word result.

**Fix:** compare the high words first, gate the low-word comparison on high-word
equality, and combine the result explicitly.  The `<=` equality case is gated
by the same high-word equality condition.

**Regression:** `test/rem-pp-int64-compare.c` covers both an undefined macro
comparison and `0 >= 201103L`.

**Validation:** the rebuilt native stage-3 compiler now preprocesses and
compiles `strings.c` with `-DCHIBICC_REM -I/root/chibicc-src`.

**Classification:** REM backend correctness defect exposed by compiler
self-hosting; not an upstream preprocessor defect.
## Stage-3 large translation-unit bootstrap boundary (open)

The corrected stage-3 compiler runs and can compile/assemble `strings.c`,
`hashmap.c`, and `tokenize.c` natively.  It preprocesses `main.c` and
`parse.c`, but their full `-S` invocations terminate with `Hangup` (status
129) before emitting assembly.  Raising the guest stack limit did not alter
the result.  No root cause or minimal compiler reproducer has been
established; this remains an unresolved stage-3 bootstrap/runtime boundary,
not a confirmed new backend defect.

Repeated clean guests reproduce the same boundary: preprocessing `main.c` and
`parse.c` completes, but `-S` exits 129 (`SIGHUP`/reported as `Hangup`) before
assembly output exists.  The smaller `tokenize.c` translation unit passes
through assembly.  Attempts to persist post-failure `dmesg` were interrupted
when the disposable QEMU instance terminated before the shell could sync it,
so no PC/register claim is made.

### Signal isolation evidence (2026-09-30)

The failure was reproduced with a one-shot guest init writing result files.
The compiler's stderr contains `Hangup`; the shell's own HUP trap is not run,
so this is not merely the shell formatting a normal compiler exit. Raising the
guest stack limit, ignoring HUP, removing the serial device, and running
through a `setsid`/`exec` helper did not yield `main.s`. The no-TTY run still
returned 129. This is evidence of a deterministic signal/runtime boundary for
the large stage-3 translation unit, not a proven new chibicc defect.

### Large-TU reduction checkpoint (2026-09-30)

A disposable source variant preserving all declarations and helper functions but
replacing `main()` with `return 0` still causes the same stage-3 `RC:129`
`Hangup` before assembly. This rules out the body of `main()` as the sole
trigger. A follow-up all-function-body reduction did not produce a guest
completion marker, so it is not treated as valid evidence. No code change was
made.

### Stage-3 global-initializer reducer (2026-09-30)

remdbg maps the failure to `fnv_hash+0x1a4` at `0x02001eb0`, with a bad
string pointer and fault address `0x10ab3000`. A native Stage-3 reduction gives
the first valid PASS/FAIL boundary:

```c
#include "chibicc.h"
int x;       /* PASS */
int x = 1;   /* SIGSEGV in fnv_hash+0x1a4 */
```

The complete `main()` body can be removed and the fault remains. Include-only
and include-plus-enum sources pass; later global declarations are not needed.
This points to Stage-3 generated handling of global initializers or the parser
path they activate. No source fix has been applied; Stage-2/Stage-3 differential
execution and remdbg caller/register capture remain the next diagnostic step.

### Stage-2/Stage-3 differential reducer (2026-09-30)

The reducer was rerun in the same disposable REM image with the same source,
headers, runtime and command line. Stage-2 completed it and emitted assembly;
Stage-3 terminated with status 129 (`Hangup`) and emitted no assembly:

```c
#include "chibicc.h"
int x = 1;       /* Stage-2: PASS; Stage-3: SIGSEGV/Hangup */
```

Changing the declaration to `int x;` makes Stage-3 pass. The Stage-3 failure
is therefore compiler-generation/self-hosting specific and is triggered by
the parser path for a global initializer. It is not evidence of a generic
`fnv_hash`, libc, stat, or errno failure. The larger translation-unit fault
captured by remdbg remains `fnv_hash+0x1a4`; this reducer does not yet identify
the exact miscompiled function or data object. No compiler source change was
made from this result.

Static disassembly narrows the failing path to `parse.c:is_typename()`:
Stage-3 loads `tok->loc` at the normal `Token` offset, passes it through
`hashmap_get2()`/`get_entry()` to `fnv_hash()`, and the remdbg fault occurs in
the byte load inside that hash loop. The Stage-3 keyword pointer table itself
is present at the referenced address, so the current evidence does not justify
calling this a static-table relocation defect. The bad value is more likely a
corrupted token pointer/location or an earlier self-hosted parser-state write;
that distinction remains open.

The Stage-3 disassembly shows an affected caller in `primary()`: it follows
three `Token->next` links and passes that token to `is_typename()` immediately
before the failing hash lookup. The immediate corruption boundary is
therefore compatible with a damaged token chain (`tok->next` or a copied
token), rather than with FNV arithmetic itself. This narrows the trace point
but does not yet prove the repair location.

An earlier attempt to run remdbg on this reducer used a candidate image whose
kernel returned `PTRACE_TRACEME: Function not implemented`; that was an image-
specific debugger limitation at the time, not a compiler result. It has been
superseded as the current debugger status by the ptrace implementation and
guest qualification documented in [REM_DEBUGGER.md](REM_DEBUGGER.md). This
does not resolve the compiler reducer: its exact failing write or corrupted
object remains under investigation.
## REM ABI data-model defect: `double` alignment in self-hosted layout

**Symptom:** Stage-3 generated `copy_token` allocated and copied 72 bytes for
`Token`, while the known-good Stage-2 compiler generated 68 bytes.  The
resulting over-copy corrupted token state and later presented an invalid key
to `fnv_hash` during hosted preprocessing.

**Root cause:** the REM target C ABI uses 4-byte alignment for an 8-byte
`double` (`sizeof(struct { char c; double d; }) == 12`), but the chibicc REM
type table declared `ty_double` with 8-byte alignment.  This made struct
layout depend on which compiler generation produced the compiler.

**Fix:** `toolchain/chibicc-rem/type.c` now declares `ty_double` as
`(TY_DOUBLE, 8, 4)`.

**Regression:** `test/rem-double-alignment.c` checks the 12-byte struct size
and 4-byte alignment; the host REM backend emits the expected constants.

**Classification:** confirmed REM backend/data-model portability defect.

The corrected host-built REM compiler emits `copy_token` with both allocation
and copy length 68; the pre-fix Stage-3 artifact emitted 72.  A clean native
guest rerun is still required to complete the bootstrap acceptance after this
source correction; the disposable images used for the diagnostic run became
unusable due filesystem/image lifecycle failures and are not qualification
artifacts.

### Corrected compiler native smoke run (2026-09-30)

The corrected cross-built compiler (`788be1ec...`) was staged into a fresh
filesystem that passed `e2fsck -fn`.  Inside the REM guest it compiled the
header-free `simple.c` and returned `SIMPLE_RC:0`, producing assembly.  The
same guest then started the hosted `strings.c` translation with the corrected
headers and compiler, but no completion or fault marker was observed within
the explicit 180-second host timeout (`QEMU_RC:124`).  Kernel output showed no
user fault for that run.  This is a bounded performance/long-running test,
not a strings.c correctness PASS; full Stage-3 translation remains
unqualified.

A subsequent clean-image run used the same compiler and tracked QEMU image.
It again completed the header-free smoke test and entered hosted `strings.c`,
but produced no completion or kernel fault marker during the multi-minute
bounded run. The run was stopped by its recorded session and left no visible
QEMU process. This strengthens the classification as an unresolved large-TU
runtime/performance boundary; it does not qualify the hosted translation.

### Large hosted workload / hashmap corruption (2026-10-01, unresolved)

The corrected compiler (`788be1ec...`) compiles and links a header-free
allocator stress program that allocates 4096 blocks of 8-12 KiB, but the
resulting REM program exits with status 148. The identical source built by
the REM GCC cross compiler (`/tmp/ap-gcc`, SHA256
`1eef2cb6faf7cd162f46ea9b000a09f3135994e7f2221c375ef3957288e34b08`)
returns 0. This is a compiler/runtime differential, but the exact owner is
not established; some larger runs also suffered invalid native-linker/image
failures and are not evidence.

An instrumented cross-GCC build of chibicc reaches the combined musl-header
preprocessor workload and records a `hashmap_put2` call with a valid-looking
key pointer but an implausible key length (`272769296`), before the eventual
`fnv_hash` fault. The first corrupting write has not been identified. No
canonical source change has been made for this issue. **Status: under
investigation; not a confirmed upstream defect.**
## Stage-7/Samurai differential result (2026-10-04)

`/tmp/chibicc-stage7` (SHA256 `72bf5c9b519eb2a33ab5eb2e014a48d21ef661049e240b3a42000f39f423eba0`) passes the complete recorded Stage-7 qualification and rebuilds all 13 unmodified Samurai sources. A real Samurai target build succeeds and the target returns 42, but `samu` then faults in musl `memchr`/`strnlen`. The same post-build fault is reproduced by the reference Samurai binary on the same image. This is therefore currently classified as shared runtime/application-path evidence, not a confirmed chibicc defect.

Standalone Stage-7 reducers for `printf`/`fprintf` with high 64-bit values and `%s` pass, so no new compiler fix is justified by the Samurai fault alone.

### Preprocessor `long` truncation in `#if` (newly confirmed)

The shared Samurai fault was reduced to the REM musl `inttypes.h` selection:
`#if UINTPTR_MAX == UINT64_MAX` incorrectly evaluated true in Stage-7, so
`PRId64` expanded to `"ld"` although REM is ILP32 (`long` is 32-bit). A
`fprintf` then consumed only the low word of an `int64_t`; the high word was
used as the following `%s` pointer, producing the observed `memchr/strnlen`
fault. The cause is `preprocess.c:eval_const_expr()` returning `long` on a
32-bit target. The targeted source fix is to return/store `int64_t`.
Regression: `test/rem-pp-uintptr-width.c`. A corrected compiler rebuild and
clean Samurai run remain outstanding.

The follow-up fix also corrected integer-literal type selection in
`tokenize.c`: on ILP32 a suffixed 64-bit hexadecimal constant must become
`unsigned long long`, not 32-bit `unsigned long`. The corrected compiler is
`/tmp/chibicc-fix-build/chibicc` (SHA256
`9aa9782dfeeac64e2ffa22d222f88afb3e2293cd19dc34d857bbf721a05ac761`). The
`rem-pp-uintptr-width.c` regression now passes natively and the corrected
`PRId64` expansion removes Samurai's post-build fault. Corrected Samurai is
`/tmp/samu-fix` (SHA256
`1bee2e3c9aa19fe0ece0af1eb333b41e63ba7382bff993b68d297515d9d1aa93`).
