% REM documentation verification ledger
% REM project
% Revision 1.2 — 2026-09-22

# Purpose

This ledger separates implementation evidence from documentation claims. A
claim is **verified** only when the cited repository test or source directly
demonstrates it. **Built** means an artifact exists; it is not evidence that
the artifact runs in the guest or on the Fold. **Unverified** means the
repository does not currently establish the claim. The manuals must use these
labels instead of converting an incomplete feature into a promise.

# Current evidence

| Area | Status | Evidence and boundary |
|---|---|---|
| ISA encoding and reserved fields | Verified | `docs/MYEMULATOR_2_ARCHITECTURE.md`; `docs/myemulator2-encoding.json`; CPU regression scripts |
| `ADC`/`SBC` flags and operand overlap | Verified by architecture source | `docs/MYEMULATOR_2_ARCHITECTURE.md`: operands are read before `rd`; `CF` is an input and then replaced |
| `CAS` alignment, returned value, ordering | Verified by architecture source | `docs/MYEMULATOR_2_ARCHITECTURE.md`: `CAS Rd,Rs,[Ra]`, naturally aligned, strongly ordered, no `CF`/`OF` update |
| Subtraction carry/borrow convention | Verified and regression-tested | `qemu/target/myemulator32/helper.c` sets `CF` when an unsigned borrow occurs; `tests/run-myemulator32-integer-tests.py` now covers a low-word borrow feeding a high-word `SBC`. |
| CAS QEMU translation and existing CPU regression | Verified for compare/replace behavior | `qemu/target/myemulator32/translate.c`; `tests/run-myemulator32-cpu-tests.py` covers a successful replacement and a failed comparison. A looping source-level increment test and explicit fault cases remain required. |
| Published manual assembly listings | Assembler syntax checked on host | All 27 fenced `asm` blocks in the two manuals assemble with `/tmp/myemu-binutils-install/bin/myemulator2-elf-as`; this does not verify linking or execution. |
| Relocation spelling in published examples | Corrected to implementation syntax | The installed GAS port emits HI20/LO12 relocations when the symbol itself is supplied to `lui` and `ori` (for example, `lui r3, message` followed by `ori r3, r3, message`). The previously documented modifier spellings are not usable with this port. |
| GNU assembler/linker/objdump/readelf | Verified on host when binutils is built | `toolchain/README.md`; `toolchain/scripts/test-binutils.sh` |
| GAS macros and conditional assembly | Target support documented; regression evidence required | Uses normal GAS facilities; target-specific behavior must be checked by the installed binutils test |
| Bare-metal ELF32 static ABI | Verified by toolchain sources/tests | `docs/MYEMULATOR_2_ABI.md`; `toolchain/scripts/test-binutils.sh` |
| Hosted C libc regression | Verified in the host-QEMU fixture when prerequisites exist | `toolchain/examples/linux-libc-regression.c`; this does not verify assembly variadic calls |
| Assembly calling `puts` through hosted `main` | ABI-documented, not independently regression-tested | Uses the documented `r1` first-argument convention and musl crt0 |
| Direct assembly calling `printf` | Unverified / ABI intentionally incomplete | Variadic register-save-area rules are not frozen; do not invent a convention |
| Native MyEmulator2 GCC inside the guest | Required release capability; current repository fixture is not sufficient | `toolchain/scripts/test-linux-native-gcc.py` is host-driven cross compilation plus guest execution, not self-hosting |
| Native GNU Make inside the guest | Required release capability; current repository fixture is not sufficient | `toolchain/scripts/test-linux-native-make.py`; historical progress records timeout/non-self-hosted states |
| Native GAS, linker, objdump, readelf, editor in the Android guest | Required release capability; current package evidence is incomplete | The final rootfs must include and execute each tool inside REM |
| Android ARM64 QEMU package | Built, device test pending | `android/package-rem.sh`, `android/validate-package.sh` |
| Galaxy Fold boot and persistence | Unverified | `android/DEPLOYMENT_CHECKLIST.md` explicitly leaves the physical test open |
| Desktop-to-guest file transfer | Unverified | `android/TERMUX_DEPLOYMENT.md` guarantees archive transfer to Termux, not a guest shared folder or network protocol |
| Standalone `_start` argc/argv contract | Unverified | `toolchain/musl/myemulator2/crt_arch.h` passes the initial stack pointer in `r1` to musl `_start_c`; no stable standalone user-program stack contract is documented or covered by an executable regression. |
| Direct assembly calls to variadic libc | Unverified / release-blocking for direct `printf`/`snprintf` examples | The C regression proves formatted output through C, not the register-save-area and promotion rules needed by a direct assembly call. |
| Complete syscall/data-structure reference | Incomplete | `linux/arch/myemulator2/kernel/syscall.c` is the implementation source, but the manuals do not yet reproduce every supported structure, flag, and error contract. |
| Complete exercise solution set | Incomplete | `docs/assembly-programming-manual/exercise-solutions/` contains only a scaffold; no claim of twenty verified solutions is permitted. |

# Acceptance record for the offline programming workflow

The workflow requested for an offline programming session is not complete
until each row has a dated command transcript and artifact hash:

| Step | Required evidence | Current state |
|---|---|---|
| Boot REM on the Fold | Termux launch transcript reaching guest shell | Pending physical test |
| Open an editor in REM | `vi --version` or `emacs --version` from the guest | Not established |
| Write source in REM | Source file created on the persistent ext4 image | Pending physical test |
| Assemble with native GAS | `myemulator2-elf-as --version` and successful `.S` build in guest | Not established |
| Link with native linker | Successful guest `ld` link and ELF inspection | Not established |
| Inspect executable | Guest `objdump` and `readelf` output | Not established |
| Execute program | Program output and exit status from guest shell | Not established |
| Modify/debug program | Repeat build plus fault/register inspection transcript | Not established |
| Preserve source | Clean exit, relaunch, and source hash match | Pending physical test |
| Transfer backups | A tested, documented path in both directions | Not established |

The final program is required to satisfy every row. Until the transcripts and
hashes exist, the guide must call the native Fold workflow a
**release-blocking verification item**, not a completed feature.

# Example inventory

The reference manual may reproduce and explain existing complete examples,
but the examples remain independently testable source artifacts:

| Topic | Source |
|---|---|
| Freestanding Linux exit | `toolchain/examples/linux-init.S` |
| Direct Linux echo | `toolchain/examples/linux-echo.S` |
| Terminal loop and probe | `toolchain/examples/linux-tty-loop.S`, `linux-tty-probe.S` |
| Process creation and wait | `toolchain/examples/linux-clone-wait.S` |
| Multi-function and multi-file linkage | `toolchain/examples/multi-functions.s`, `multi-main.s` |
| Relocation stress | `toolchain/examples/relocation-stress.s` |
| Hosted libc regression | `toolchain/examples/linux-libc-regression.c` |
| Persistent filesystem probe | `toolchain/examples/linux-ext4-persist.c` |
| Bare-metal minimal and end-to-end images | `toolchain/examples/minimal.s`, `e2e.s` |

No example should be described as “tested on the Fold” until the physical
deployment transcript is added to the release record.

# Release-blocking corrective actions

| Requirement | Missing work | Required acceptance test | Blocks offline programming |
|---|---|---|---|
| Atomic increment documentation | Add a source-level CAS loop test with success, failure, retry, overlap, alignment, and access-fault cases | Assemble, link, run under the CPU/Linux harness, and assert returned values, retry count, final word, and expected fault codes | Yes for reliable concurrent examples |
| Relocation syntax agreement | Decide whether to add explicit modifier support, or retain direct-symbol HI20/LO12 syntax and update all ABI prose | Assemble and link a symbol-address fixture and run the relocation stress test with the chosen syntax | Yes for self-contained address-loading examples |
| Native toolchain | Package and boot GAS, ld, objdump, readelf, GCC, Make, headers, libc, and an editor in the guest | Run the complete acceptance table above after a clean Fold restart | Yes |
| File transfer and backup | Implement or verify a bidirectional offline path (shared block image, serial protocol, or networking) | Import/export a source file, executable, and filesystem backup; restore and compare hashes | Yes |
| Direct Linux example execution | Not run in this checkout | Assemble, link with the installed REM user linker script, execute under REM QEMU, and assert output plus exit status | Yes for claiming a tested walkthrough |
| Argument startup | Establish and test the standalone `_start` initial-stack contract, or require libc `main` for argument examples | Run programs with zero, one, and two arguments and environment variables; assert exact strings | Yes for command-line exercises |
