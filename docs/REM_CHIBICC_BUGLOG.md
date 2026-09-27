# REM chibicc backend bug log

This log records defects found while bringing `toolchain/chibicc-rem` through real REM userspace programs. These are REM-specific backend/ABI failures, not claims about upstream chibicc.

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
