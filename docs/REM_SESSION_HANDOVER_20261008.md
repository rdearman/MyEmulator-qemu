# REM32 Fold ABI/update handover -- 2026-10-08

**CURRENT STATUS: EXACT FOLD BASELINE IDENTIFIED; FULL QUALIFICATION RUNNING**

**NATIVE COMPILER / SAMURAI / FULL UPDATE: PRIOR SIMULATED BASELINE PASS**

**OLD V2 ARCHIVE: REJECTED FOR THIS FOLD; CORRECTED CANDIDATE: NOT YET QUALIFIED**

The old v2 archive was rejected by the actual Fold at the private bootstrap library:
`/home/dev/rem-native-userland/bootstrap/musl/native/libc.a`, SHA-256
`0bc5770f3d1b84a97a1030f5c2911b164a375d89c2db9e4a89a4f51c4f2b3186`.
It reported no guest or kernel files changed. The byte-for-byte archive is now
available as `/tmp/libc.a.fold` and matches the reported hash. Occurrence-aware
comparison identifies all 33 changed objects with the recovered bootstrap
recipe and verifies the added syscall object exactly; see
`docs/REM_FOLD_REAL_LIBC_BASELINE_20261008.md`. The candidate accepts that exact
hash only at the private bootstrap path and replaces the full archive. Exact
baseline host install/verify/reinstall/rollback tests pass. Full REM
qualification is running from a fresh copy at
`/tmp/rem-fold-real-fold-qualification.20261008`; do not start another run.
No corrected release archive is transferable until every required guest
marker passes. The release document distinguishes the rejected archive from
this candidate and any eventual corrected release.

## Current exact-baseline run snapshot — 2026-10-08 12:19 BST

Qualification started at `2026-10-08T12:00:23+01:00`. Runner tool session is
`15780`; its QEMU PID is `2954`. Keep that session alive and do not restart it.
The disposable image is
`/tmp/rem-fold-real-fold-qualification.20261008/rootfs.ext4`; the input archive
is `/tmp/libc.a.fold`. The launcher reproduced rejection by the prior archive
with zero image/kernel mutation, installed and verified the corrected
candidate, reinstalled it idempotently, and confirmed the exact old and new
library hashes in the guest.

Actual serial output passed the system checks and entered compiler
qualification at 12:02:24 BST. At 12:19 BST the serial was unchanged and there
was no completion, failure,
user-fault, panic or OOM marker; neither `qualification-exit.txt` nor
`qemu-exit.txt` exists yet. Do not inspect or mutate the live ext4 image. The
runner watches for actual fault/completion markers and applies an 18,000
second timeout. Next routine observation is no earlier than 12:29 BST unless
the runner reports a completion or failure event.

The host installer regression passed against a disposable image carrying the
exact Fold archive, including install, verify, reinstall, rollback, reinstall
after rollback, local-source preservation and zero-mutation rejection cases.
The host package gate regression is retained at
`docs/REM_FOLD_V2_EVIDENCE_20261008/real-fold-host-package-test.log`; it is
explicitly synthetic host evidence, not native qualification.

## Previous continuation: unknown actual Fold libc, investigation stopped

The requested SHA-256 was not found in the available artifacts. No installer
allowlist or compiler/runtime source was changed, and no replacement release or
native rebuild was started. The prior archive is retained unchanged as evidence,
not as a supported update for this Fold.

Read-only search covered canonical standalone libraries and bootstrap outputs,
retained REM scratch/backups, the original committed runtime, 244 standalone
image candidates, and ten rootfs images inside nine older distribution archives.
The 244 candidates included one 78-byte placeholder, not a filesystem; the old
`bash-rebuilt.ext4` had a bitmap checksum error and was inspected successfully
with read-only `debugfs -c`, without repairing it. An additional retained
`/tmp/rem-abi-base.ext4` was checked. Extracted guest libraries had only the
known `30cff688...` and `269b1d6b...` hashes. No reference to the requested full
hash was found in searched repository documentation/scripts/manifests.
The known divergent recovery directory no longer exists; it was not recreated.
The original `REM-FOLD-BUILD-SOURCE-20261007.tar.gz` was not found among retained
release/source archives.

The known old/new libraries are respectively 2,835,504 and 2,861,972 bytes,
with 1,345 members each and identical member-name sets; `clone.o` changes order.
Their representative objects are ELF32 little-endian REM machine `0xf2e2`.
The target-aware host disassembler again found 35 bad large-frame prologues in
the old library and zero in the corrected library. The old exact bytes are in
Git commit `25ab091504e0c1027c7e5efcf332aecd2e072432` and the userspace import;
the corrected exact bytes match `/tmp/remqa/musl-inst/lib/libc.a` and the already
qualified payload. None of this identifies the unknown Fold archive.

The recovered bootstrap musl recipe, in
`toolchain/native-userland/patches/musl-rem-native.patch` (recovery commit
`f89a89d15df50f2eda340fbc76a22fbef78ad0df`), copies `/usr/lib/libc.a`, then
compiles 33 listed C files, updates their members, and adds `native_syscall.o`.
This establishes that the
private bootstrap archive need not equal `/usr/lib/libc.a`; it does **not**
establish that SHA `0bc5770f...` is this recipe's legitimate output.
No retained `librem-runtime.a` was found, and the retained Fold kit's native
musl directory contains only the corrected `269b1d6b...` archive.

Disposable inventories and known-library disassembly are in
`/tmp/rem-libc-identify.g59nt0n9/`; no extracted rootfs remains there.
Authoritative blocked-state findings are also in the release document.

**Historical next action before the archive arrived:** obtain a byte-for-byte copy of the actual Fold
`/home/dev/rem-native-userland/bootstrap/musl/native/libc.a`, verify the stated
SHA-256, and compare its archive members/objects with the known libraries and
bootstrap recipe. Until then, do not whitelist, bypass checks, rebuild, or
resend the rejected release. If provenance is established, reproduce that
exact starting archive and require install/rollback, native compiler
self-rebuild/porting, Samurai, `REM_UPDATE_VERIFY_OK`, and `REM_QA_DONE|0`
before producing a distinctly versioned replacement and new SHA-256.

## Objective and workspace boundary

Preserve the Fold's existing compiler sources and data; qualify a REM-native
C compiler, build and execute Samurai with it, and deliver one small incremental
v2 update installed from Termux while QEMU is stopped. The real active image is
`~/rem/rootfs.ext4`, a 10 GiB ext4 image. `~/remvm/rootfs.ext4` is obsolete.
Do not resize, format, replace or retransmit the rootfs.

All persistent work must stay under
`/home/rick/Development/Active/MyEmulator-qemu`. Only disposable scratch belongs
in `/tmp`. Preserve unrelated dirty/untracked recovered work. No reset, clean,
blind checkout/restore, broad staging, or new persistent source tree.

## Real Fold failure and established reproduction

After installing the original update, quick system checks passed. The first
full compiler runtime test, `native-tests/porting/aggregate-abi`, returned 1.
With `/home/dev/rem-native-userland/bootstrap/native/chibicc-selfbuilt`:

- caller input `s.last` remained 22;
- expected returned struct: `{11,23}`;
- observed returned struct: `{0,1}` (`FIRST=0`, `LAST=1`).

The exact values were reproduced with a REM-native compiler in QEMU, not just
in host assembly analysis:

```text
FOLD_NATIVE_AGGREGATE_RC=1
FOLD_NATIVE_FIRST=0
FOLD_NATIVE_LAST=1
REM_QA_DONE|0
```

Evidence: `/tmp/remqa/fold-abi-repro.serial`,
`/tmp/remqa/abi-followup/fold-native-values.s`,
`/tmp/remqa/abi-followup/fold-native-probe.sh`.
The probe's own result 0 means the diagnostic script finished; the program
failure is the explicitly printed aggregate result 1.

The reproduced compiler had the updated backend and the old parser. Its caller
passed the zeroed hidden result buffer in r1 and the aggregate argument pointer
in r2. The callee lacked the parser-added hidden parameter and treated r1 as
its argument, incrementing the initially zero result buffer to `{0,1}`.
This matches the Fold symptoms exactly. We have not retrieved the Fold sources;
do not pretend their precise bytes have been compared with the desktop.

The original installer was reproduced end-to-end too. A Fold-era initializer
fix (`break` -> `continue`) already present in a locally edited parse.c caused
one hunk of the combined parser patch to fail. The installer updated codegen.c,
left the entire parser unchanged, printed INSTALL COMPLETE, and its verifier
treated LOCAL parse.c as VERIFY_OK. Evidence:
`/tmp/remqa/abi-followup/fold-case/v1-install.log` and `v1-verify.log`.
The prior successful simulation used exact-baseline sources and missed this
partial-update case. This is an ABI/source-update defect, not evidence of a
libc, kernel, or Android-vs-desktop execution difference.

## Changes already made

### Compiler and regressions

- All struct/union returns, including 8-byte structs, use a hidden result
  pointer in r1; aggregate argument pointers follow it in r2, matching REM GCC.
- parse.c creates the hidden first parameter for every aggregate return.
- codegen.c rejects an aggregate callee whose parser did not supply that
  hidden parameter, rather than silently emitting a mismatched function.
- The earlier static initializer fix is retained: an uninitialized/unnamed
  bitfield does not stop emission of later members; masks use uint64_t.
- `aggregate-small-return.c` checks unchanged caller input, `t.first == 11`
  and `t.last == 23`, with distinct failure codes.
- Required porting tests run this regression first and retain both GCC/chibicc
  interoperability directions.
- `rebuild-required-native.sh` uses an explicit tested seed, checks the small
  return on seed/stage1/stage2, and runs the entire final candidate suite before
  replacing `chibicc-selfbuilt`. It retains the prior binary.
- The generated `chibicc-rem-native.patch` matches canonical codegen.c/parse.c
  while retaining the original kit layout.

The compiler is a nested Git repository at `toolchain/chibicc-rem`, tracked in
the parent as a gitlink. Its original HEAD was
`cd9b2ce433dc82b635581c68e22142e8d04b89fb`; only codegen.c and parse.c are changed.
The nested origin is upstream rui314/chibicc, not Rick's fork. The checkpoint
compiler commit is **`f3a3094f16fd144719be9b4ee77e84836e4c1bfa`**, published
successfully to Rick's repository on the dedicated branch
`rem-chibicc-fold-update-20261008`. Upstream origin is unchanged; the parent
checkpoint records this gitlink. If its object needs fetching in the nested
repository, use Rick's HTTPS remote, not upstream:

```sh
git -C toolchain/chibicc-rem fetch \
  https://github.com/rdearman/MyEmulator-qemu.git rem-chibicc-fold-update-20261008
```

Do not checkout/restore over any dirty compiler worktree.

### Installer/package

- Both compiler source merges are preflighted before any image files change.
- Each hunk is accepted only as a unique exact old/new fragment. Already
  applied Fold fixes are preserved independently; conflicts abort instead of
  installing half the ABI change. POSIX shell/sed/grep/cmp are used, with no
  external patch, awk, Python or native objdump dependency.
- Verification now returns nonzero for incomplete source updates.
- v2 has separate `update-20261008-v2-backup` and log paths; v1 backups remain.
- Backups include the existing compiler and Samurai, since later in-guest
  qualification replaces them. Original modes/owners and local edits survive.
- The verifier is a backed-up managed file, not an untracked overwrite.
- A missing kernel path is an explicit error; `REM_KERNEL` overrides it.
- The deterministic seed, staged rebuild script and small-return regression
  are included in the candidate. The kit's static libc uses the corrected
  payload too, avoiding accidental linkage against an older kit runtime.
- Previously required BusyBox/SysV/libc/kernel payloads remain in the same
  incremental update. No full rootfs or complete source distribution is added.

## Already-qualified results -- do not repeat

### Seed rebuild: PASS, exit 0

```sh
bash android/fold-update-20261008/build-seed.sh \
  /tmp/remqa/abi-followup/seed-rebuild
```

```text
SEED_REBUILD_BYTE_IDENTICAL
6b28c165cb94b7411a63b7e74932845b11b8e30ea2bd6882b9c7bf1348a2ac44
```

This cross-build uses the fixed canonical GCC and flight-kit-runtime musl,
`-DCHIBICC_REM`, and the ten canonical compiler translation units.
The seed was executed in REM and its aggregate test returned 0
(`/tmp/remqa/abi-seed2.serial`, `QUALIFIED_SEED_AGGREGATE_RC=0`).
The recipe reproduced that tested binary byte-for-byte.

### Installer/package regression: PASS, exit 0

```sh
bash android/fold-update-20261008/test-install.sh \
  /tmp/remqa/foldsim/rootfs.ext4 /tmp/rem-update-20261008-v2
```

```text
PASS verification rejects an incomplete local ABI update
PASS local parser fix merged, preserved, verified and idempotent
PASS rollback restores exact pre-update compiler sources
PASS conflicting parser aborts before either ABI source is written
```

Direct simulation tests also restored the exact pre-update locally edited
parser and old compiler binary, and confirmed conflicts produce no partial
compiler-source writes. A host-generated compiler with the new backend and old
parser rejects the small-return source with
`REM aggregate return requires a hidden result parameter`.

## Prior simulated-baseline qualification snapshot — later completed

Launch command (already running; do not run it again):

```sh
cd /tmp/remqa &&
KERNEL=/tmp/remqa/abi-followup/fold-case/vmlinux \
./run-r4.sh foldv2 foldsim.dummy abi-followup/qualification.extra \
  128M 18000 /tmp/remqa/abi-followup/fold-case/rootfs.ext4
```

At the 09:04 BST snapshot:

- QEMU PID **1790915**, parent timeout PID **1790913**.
- Started **2026-10-08 08:30:24 BST**, elapsed **34:22**, about 100% of one CPU.
- Tool shell/session ID: **foldv2**.
- Actual running guest image: **`/tmp/remqa/foldv2.ext4`**. The Fold-case image
  in the launch command is the source copied by the harness, not the live image.
- Serial/console log: **`/tmp/remqa/foldv2.serial`**.
- Image injection log/commands: `/tmp/remqa/foldv2.dbglog`, `foldv2.dbg`.
- Guest compiler log:
  `/home/dev/rem-native-userland/rem-update-compiler.log`.
- Eventual guest Samurai log:
  `/home/dev/rem-native-userland/rem-update-samurai.log`.
- Guest quick-check log: `/tmp/rem-update-verify.log`.
- Kernel: `/tmp/remqa/abi-followup/fold-case/vmlinux`, SHA-256
  `c8b394e333fdc8ad3d399e10a3723daad194dfbfbb56e3be74a573fa3d179782`.

Exact QEMU command line:

```sh
/home/rick/Development/Active/MyEmulator-qemu/REM-FLIGHT-HUMAN-UAT-FULL/qemu-system-myemulator32 \
  -M myemulator32 -m 128M \
  -kernel /tmp/remqa/abi-followup/fold-case/vmlinux \
  -append "console=ttyMY0,115200 earlycon=myemulator2,0xf0000000 root=/dev/myemu0 rw init=/sbin/init virtio_mmio.device=0x200@0xf0200000:5 ip=off" \
  -drive file=/tmp/remqa/foldv2.ext4,format=raw,if=none,id=myemulator2-disk \
  -nographic -monitor none -serial stdio -netdev user,id=net0 \
  -icount shift=0,sleep=off -no-reboot
```

Latest meaningful output:

```text
PASS exit status 3
--- root filesystem
Filesystem                Size      Used Available Use% Mounted on
/dev/root                 9.8G    517.9M      8.8G   5% /
/dev/root / ext4 rw,relatime 0 0
--- compiler qualification (log: /home/dev/rem-native-userland/rem-update-compiler.log)
```

All quick system checks passed. No compiler completion marker, user fault or
kernel panic was observed. The known non-fatal boot-time sysfs warning is not
compiler qualification success or failure.

Check/resume without starting another job:

```sh
ps -p 1790915 -o pid,ppid,lstart,etime,pcpu,args
tail -n 20 /tmp/remqa/foldv2.serial
grep -a 'REM_QA_DONE|' /tmp/remqa/foldv2.serial
grep -a 'COMPILER_REQUIRED\|SAMURAI_\|REM_UPDATE_VERIFY\|BAD_USER\|panic' \
  /tmp/remqa/foldv2.serial
```

These are desktop commands, not commands for Rick to type on the Fold.
If the original tool session is accessible, read **foldv2** without restarting
it. The process is attached to that CLI session: keep the originating session
alive; ending it may terminate attached jobs. Nothing has been killed or
detached for this handover.

The harness shell can exit 0 even when the guest failed. The authoritative
guest result is **`REM_QA_DONE|N`** in the serial log, printed after guest sync.
No marker means unknown/incomplete, not success. The timeout is 18000 seconds.
After QEMU has stopped, guest files can be extracted read-only using debugfs:

```sh
debugfs -R 'cat /home/dev/rem-native-userland/rem-update-compiler.log' \
  /tmp/remqa/foldv2.ext4
debugfs -R 'cat /home/dev/rem-native-userland/COMPILER-REQUIRED-RESULT' \
  /tmp/remqa/foldv2.ext4
debugfs -R 'cat /home/dev/rem-native-userland/rem-update-samurai.log' \
  /tmp/remqa/foldv2.ext4
```

Do not mutate or fsck the live image. If extraction after shutdown shows
unflushed metadata, make a disposable copy before any recovery; preserve the
original test image/log.

Required future success evidence:

```text
PASS aggregate-small-return (seed)
PASS aggregate-small-return (stage1)
PASS aggregate-small-return (stage2)
COMPILER_REQUIRED_PORTING_TESTS_OK
COMPILER_REQUIRED_SELFREBUILD_OK
SAMURAI_BUILD_OK
SAMURAI_DRY_RUN_OK
SAMURAI_NATIVE_BUILD_OK
SAMURAI_INCREMENTAL_REBUILD_OK
SAMURAI_CLEAN_OK
SAMURAI_NATIVE_EXECUTION_OK
REM_UPDATE_VERIFY_OK
REM_QA_DONE|0
```

Per-stage smoke messages are in the compiler log, which is printed/filtered
by the verifier only after that phase completes.

## Explicit checkpoint scope

Parent-repository task files selected for this checkpoint:

1. `android/fold-update-20261008/README.txt`
2. `android/fold-update-20261008/build-seed.sh`
3. `android/fold-update-20261008/install.sh`
4. `android/fold-update-20261008/make-package.sh`
5. `android/fold-update-20261008/test-install.sh`
6. `android/fold-update-20261008/verify.sh`
7. `docs/REM_CHIBICC_BUGLOG.md`
8. `docs/REM_SESSION_CHECKPOINT_20261008.md`
9. `docs/REM_SESSION_HANDOVER_20261008.md`
10. `toolchain/native-userland/patches/chibicc-rem-native.patch`
11. `toolchain/native-userland/kit/bootstrap/rebuild-required-native.sh`
12. `toolchain/native-userland/kit/bootstrap/build-samurai-native.sh`
13. `toolchain/native-userland/sources.env`
14. `toolchain/tests/native-userland/compiler-porting-native.sh`
15. `toolchain/tests/native-userland/aggregate-small-return.c`
16. `toolchain/scripts/prepare-gcc-source.py`
17. `toolchain/userspace/rem-sysv/etc/init.d/rcS`
18. `toolchain/userspace/rem-sysv/busybox-1.37.0-rem-20261008.config`
19. `toolchain/chibicc-rem` (gitlink to the separate compiler commit).

Nested compiler commit: only `codegen.c` and `parse.c`. The generated patch
also preserves their changes in the parent repository.

The GCC generator changes are exclusively this task's earlier alloca/outgoing
area fix and aligned 252-byte prologue stepping. The selected SysV/config
files are this update's source inputs, not generic build outputs.
Do not stage the rest of rem-sysv, linux, QEMU, build trees or recovered docs.
Earlier runtime payloads `toolchain/flight-kit-runtime/lib/{libc.a,crt1.o}`
remain modified and unstaged; kernel syscall.c mixes pre-existing recovered
work with earlier update changes and remains unstaged too. Preserve them.
They and the scratch kernel/BusyBox outputs are package dependencies already
present in the canonical workspace, not new v2 compiler implementation.

The requested full `git status --short --untracked-files=all`, diff stat and
diff check were captured. Expanded status was huge; tool output is
`/tmp/1791446690344-copilot-tool-output-1472699-92da3a47-9e4d-4a33-adb8-43e3cbad5c85.txt`.
Before checkpoint staging, ordinary status was 276 entries: 53 modified,
one type change, one dirty nested repository, and 221 untracked entries.
Full diff: 55 entries, 5904 insertions, 789 deletions.
`git diff --check` returned 2 solely for a blank unified-diff context line
in `chibicc-rem-native.patch` (line 3534 contains its required leading space).
Compiler source and scoped checks excluding the patch were clean. Do not
rewrite the source patch merely to suppress this metadata whitespace warning.

## Prior simulated-baseline scratch artifacts and delivery state

Keep these; do not delete anything still needed by the running job:

- `/tmp/remqa/foldv2.ext4`, `foldv2.serial`, `.dbg`, `.dbglog`.
- `/tmp/remqa/run-r4.sh`, `foldsim.dummy`.
- `/tmp/remqa/abi-followup/qualification-rc.local`, `qualification.extra`.
- `/tmp/remqa/abi-followup/fold-case/`: 10 GiB source test image, kernel,
  v1/v2 install/verify/rollback/conflict logs and exact original backups.
- `/tmp/remqa/abi-followup/seed-build/` and `seed-rebuild/`: tested seed,
  objects and recipe log; seed-build binary is the packaging input.
- `/tmp/remqa/abi-followup/`: host/native variant compilers, diagnostic sources,
  assembly, original installer reproduction and baseline `base.patch`.
- `/tmp/remqa/abi-followup/original-package/`: unpacked old archive evidence.
- `/tmp/rem-update-20261008-v2/`: candidate package directory; no final v2 tarball
  or v2 manifest has yet been produced.
- `/tmp/remqa/bb-new/busybox`, `busybox.links`, `/tmp/remqa/linux-netbuild/vmlinux`.
- `/tmp/rem-native-userland-downloads/samurai-531ba700.tar.gz`.
- `/tmp/REM-FOLD-update-20261008.tar.gz` and original manifest: OLD delivery,
  retained as evidence, not qualified for further transfer.

The final v2 archive must contain the checked installer/README/checksums,
compiler source diffs and generated kit patch, deterministic seed, staged
rebuild and regression scripts, Samurai source/build test, and the existing
BusyBox, rcS, corrected libc/crt1 and kernel payloads. No rootfs, build trees,
object caches or duplicate full source archive.

Important baseline detail after the checkpoint commit:
the scripts currently obtain the original source patch from **git HEAD**.
Before this commit HEAD was
`e962d91f238476cc92669e5e514b12075dca0136`. After committing, HEAD's patch is
the NEW patch, not the original baseline. Do not use that new patch as baseline.
The original patch is already saved at `/tmp/remqa/abi-followup/base.patch`,
SHA-256 `b235638b86fb58fcbc9f192b4d460c9ec281f19212dbff1585f4639f6d133f97`.
The packaging script supports an explicit BASE_PATCH; use it:

```sh
BASE_PATCH=/tmp/remqa/abi-followup/base.patch \
CHIBICC_SEED=/tmp/remqa/abi-followup/seed-build/chibicc-update-seed \
OUT=/tmp/rem-update-20261008-v2 \
bash android/fold-update-20261008/make-package.sh
```

If that disposable baseline file is missing, regenerate it from the pinned
`e962d91...` commit, not from new HEAD. test-install.sh also currently reads HEAD
for its original fixture: it passed before this checkpoint, but its fixture
must be pinned to the old revision before any future rerun. No implementation
change to those defaults was made during this handover.

## Historical next actions before the prior simulation finished

1. **Inspect the already-running foldv2 job and its serial log.** Do not restart,
   kill it, or wait for Rick to supply phone diagnostics.
2. On completion, check the authoritative guest status and every required
   compiler/Samurai/full-verification marker. If it failed, obtain its logs
   after shutdown and diagnose only that concrete blocker.
3. If it passed, preserve those actual REM execution results in this handover
   and checkpoint. Do not infer success from quick checks or harness exit 0.
4. Assemble the final candidate with the explicit original BASE_PATCH above.
   Host installer refinements made after the running image was cloned are
   separately covered by passing installer regressions; guest compiler,
   rebuild script, seed and runtime inputs are unchanged. Confirm final guest
   payloads match the qualified ones before delivery.
5. Create `/tmp/REM-FOLD-update-20261008-v2.tar.gz` from the candidate directory,
   then an adjacent plain-text manifest with byte size, archive SHA-256,
   changed/added file purposes, baselines, backup/rollback and exact Termux/REM
   application/verification commands. That final archive/hash is still pending.
6. Report only final qualification results and one compact transfer/install
   procedure once the evidence exists. Preserve the Fold's existing image.

Do not redo the ABI investigation, seed rebuild, original installer failure
reproduction, or already-passed installer regressions merely because a new
agent resumes. Do not resurrect the accidental external native-userland tree,
discard Fold-era source changes, weaken the GCC interoperability tests, or
label the old desktop-only delivery as real-Fold-qualified.

## Historical authorization and observer context for the prior simulation

The user subsequently authorized resuming this task through final qualification
and packaging. The earlier implementation pause is superseded; do not restart
the already-running qualification or redo the qualified seed.

Parent checkpoint is `ab437cbb998a0143cf62607b71b84f5600cbe1cd`, with exactly
the 19 entries listed above. Its HTTPS push exited 1: GitHub rejected unrelated
pre-existing ancestor files `REM-FLIGHT-DEVELOPMENT.zip` (283.23 MB) and
`docs/REM_QEMU_Debugging_Reference.pdf` (179.30 MB). Do not rewrite main or delete
recovered files. A task-only publication branch based on remote main is the
safe workaround; keep the original local checkpoint and working tree intact.
Nested compiler checkpoint remains `f3a3094f16fd144719be9b4ee77e84836e4c1bfa`.
Remaining Git status at capture: 47 modified, one type change, 224 untracked
entries, no staged changes; unrelated recovered work remains intact.
Full ordinary status snapshot: `/tmp/rem-fold-handover-status.9ld4pK`.

The baseline caveat above is now fixed in both host scripts: defaults are pinned
to `e962d91f238476cc92669e5e514b12075dca0136`; installer regressions also accept
the verified `BASE_PATCH` override. Packaging refuses a nonempty output directory
instead of deleting evidence. Use a fresh owned scratch directory for the final
candidate. These host-only changes do not alter the running guest payload.

At the last completed process snapshot QEMU PID 1790915 was still running,
elapsed 42:08, with quick checks passed and compiler qualification entered.
At a later snapshot (elapsed 45:44) the same QEMU remained active with the same
compiler phase. Host syntax/scoped whitespace checks passed; both seed and
original baseline hashes were reconfirmed. No completion/fault markers were
present in serial. Final package remains
unqualified and not ready for transfer until all required markers are observed.

Task-only publication succeeded, without rewriting local main:
`7d12be9c0479a99de4bf97244095735ab2804edd` on
`origin/rem-fold-update-20261008-v2-checkpoint`. It contains the same 19 task
entries as the local checkpoint, based on remote main
`e2c32b0b51ab247a739d0e18bd6e93872aa60442`. Nested compiler publication remains
on `rem-chibicc-fold-update-20261008`. Future coherent checkpoints should update
the safe publication branch rather than retrying oversized main ancestry.

Fresh final candidate directory:
`/tmp/rem-fold-final.Uzh0Cd/rem-update-20261008-v2`.
All payload checksums passed. Corrected `test-install.sh` against this directory
and stopped `/tmp/remqa/foldsim/rootfs.ext4` passed all four installer regressions,
exit 0. Verification of this candidate against the stopped launch-source image
`/tmp/remqa/abi-followup/fold-case/rootfs.ext4` also passed, exit 0:
all kernel/runtime/guest-script/seed/Samurai source payloads matched; merged
local parser edits were retained; filesystem check and `VERIFY_OK` passed.
This proves candidate/qualification-input identity, not compiler completion.
The live QEMU remained active at elapsed 48:01. A read-only raw-image compiler
log snapshot yielded no text and cannot establish progress or failure because
the live guest's cached/journaled writes may not yet be visible.

### Subsequent validated release/rollback changes

Local checkpoint `87eea048afaa940ca73fd86719507ed7ef7a13ef`, task-only remote
checkpoint `201846b79da0a70aad763c9c28efef67ba13c491`: push PASS.
Current task scope additionally includes
`android/fold-update-20261008/test-package.sh` (20 parent task paths total).

`make-package.sh` now writes a checksummed `MANIFEST.txt` with baselines, seed/
kernel SHA-256, payload mapping, install/verify/rollback commands and qualification
status. Without `QUALIFICATION_LOG`, output is explicitly PENDING/not ready.
With the actual completed serial log, every one of the 13 exact markers is
required and any user fault, kernel panic or failed guest exit aborts packaging.

Host-only `test-package.sh` PASS/0: each individual missing marker, three fault/
exit cases, synthetic positive marker validation, manifest checksums, pending
label and output preservation. Synthetic logs/trees are test fixtures, NOT REM
execution evidence, and were removed on success. Never use one for delivery.

Expanded `test-install.sh` PASS/0 against
`/tmp/rem-fold-package-gates.E4xfZ2/rem-update-20261008-v2`:
all earlier checks, restored existing compiler after a simulated native rebuild,
removed newly built Samurai on rollback, preserved unrelated local source/data.
Failed host regression work directories are retained with an explicit diagnostic
path rather than silently cleaned. These changes remain host-only.

At elapsed 54:54, QEMU PID 1790915 was active (one CPU fully used); compiler phase
still entered, no completion/fault marker in serial. Do not restart the job.
After actual completion, use a fresh output and the actual serial:

```sh
release_parent=$(mktemp -d /tmp/rem-fold-release.XXXXXX)
CHIBICC_SEED=/tmp/remqa/abi-followup/seed-build/chibicc-update-seed \
QUALIFICATION_LOG=/tmp/remqa/foldv2.serial \
OUT="$release_parent/rem-update-20261008-v2" \
bash android/fold-update-20261008/make-package.sh
```

Then validate that exact release directory against the stopped image and run
installer regression, check all payload hashes, archive only
`rem-update-20261008-v2/`, and write the adjacent release report with byte size
and archive SHA-256. Do not overwrite an existing archive/evidence path blindly.
The current manifested scratch candidate is still PENDING, not for transfer.

Release-gate/runtime-rollback checkpoint was preserved:
local `c919b12f2592e1432587bb5ab5857d158863993d`;
remote `b16427fb4d5c3aa0dd967523276c61bb33cbe300`
on `rem-fold-update-20261008-v2-checkpoint`, push PASS.
The original QEMU PID 1790915 remained active at elapsed 01:06:38, one CPU
fully utilized, serial still at compiler qualification with no completion/fault
marker. Independent host validation is complete; wait economically for the
existing execution rather than spending credits repeating qualified work.

### Reproducible release tooling and exact takeover

Additional verified task source:
`android/fold-update-20261008/archive-package.sh` (21 parent task paths total).
Archive creation now checks the complete checksum inventory, rejects pending
manifests, unexpected files, changed REM-tested seed/kernel and existing output
paths. Order/time/ownership/permissions/gzip headers are normalized. Two archives
must match byte-for-byte; extracted payload and final archive checksums are
verified; adjacent `.sha256` and `.manifest.txt` record hash and byte size.

Expanded HOST-ONLY package tests PASS/0: all earlier marker/fault cases plus
reproducibility after changing source mtimes, extracted/hash verification,
mismatched seed/kernel despite resealed checksums, uncovered-file rejection,
pending-package release rejection and output preservation. Synthetic proof is
NOT native execution; all synthetic archives were removed. No final release
archive/hash has yet been produced.

Last observed QEMU state: PID 1790915, elapsed 01:17:00, compiler phase entered;
no completion/fault markers. New user rule: check at most once per ten minutes,
except an exit/completion/failure event. No restart or qualified rebuild.

Codex's first action, on the desktop, not on Rick's phone:

```sh
cd /home/rick/Development/Active/MyEmulator-qemu
git status --short
ps -p 1790915 -o pid,ppid,stat,lstart,etime,time,args
tail -n 35 /tmp/remqa/foldv2.serial
grep -E 'COMPILER_REQUIRED_|SAMURAI_.*OK|REM_UPDATE_VERIFY_OK|REM_QA_DONE|MYEMU_BAD_USER_FAULT|Kernel panic|FAIL compiler|FAIL Samurai' /tmp/remqa/foldv2.serial
```

If unfinished, leave it alone and honor the ten-minute interval. If it failed,
after QEMU stops obtain the compiler/Samurai logs from the guest image and fix
only the first actual failure. If all actual success markers are present, use
the fresh release recipe above with the actual serial log; do not reuse a
PENDING or synthetic tree. Against the exact fresh release, run:

```sh
bash android/fold-update-20261008/test-install.sh \
  /tmp/remqa/foldsim/rootfs.ext4 "$release_parent/rem-update-20261008-v2"
TMPDIR=/tmp sh "$release_parent/rem-update-20261008-v2/install.sh" verify \
  /tmp/remqa/abi-followup/fold-case/rootfs.ext4
bash android/fold-update-20261008/archive-package.sh \
  "$release_parent/rem-update-20261008-v2" \
  /tmp/REM-FOLD-update-20261008-v2.tar.gz
```

Record actual markers/exit state, final size/hash and exact transfer/install
commands in both docs. Provide one archive download and a literal checksum
verification/install block, not a guessed transfer URL, another rootfs or phone
diagnostics. Preserve the actual image `~/rem/rootfs.ext4`; stop Fold QEMU before
Termux installation/rollback. Guest verification is one script invocation after
normal boot, not manual editing.

### Latest preserved checkpoint and event observer

Local source/documentation checkpoint:
`2eb637e474922596689aef13914c301bb3e0d371`.
Task-only remote checkpoint:
`ea217004c6327e39809e03dbca69f4e8f434efb0`,
branch `rem-fold-update-20261008-v2-checkpoint`, push PASS.

Original qualification tool session: `foldv2`; original QEMU PID: 1790915.
Additional event-only observer tool session: `646`, verified active/responsive.
It uses Linux inotify for serial writes and pidfd for process exit, not periodic
progress/status polling. It cannot stop or modify QEMU. Upon its completion
notification, read session 646 once and capture actual original execution
results; do not launch a replacement qualification. The last recorded progress
snapshot remains elapsed 01:17:00 with compiler phase entered and no marker.

Further archive checks require all expected execution markers in a checksummed
PASS manifest and compare the extracted file inventory. Latest full host-only
package regression PASS/0, 11 PASS lines, including preservation of existing
archive/hash, rejection of a resealed PASS manifest lacking compiler completion,
and rejection of unexpected symlinks. Synthetic fixtures were removed; no final
REM-qualified v2 archive/hash has been generated.

Current single next action: consume the existing qualification's completion/
failure event and authoritative serial markers. Until then do not restart QEMU,
repeat the seed build, or transfer a candidate/synthetic archive.

Latest preserved checkpoint before this update:
local `9af9f0ecaf5e7382112e5c6e87ee944773230322`;
task-only remote `15a249136c465f4a7236131c0efe48bc9c109634`, push PASS.
All guest payload mapping hashes are now independently checked before archiving.
The complete latest host package regression PASS/0, 12 PASS lines, includes
rejection of an inconsistent mapping even after its checksums were resealed.
This remains host-only evidence; the running guest and qualified seed were not
rebuilt or changed. Independent release validation and takeover documentation
are complete; the remaining critical-path action is the existing event/result.

## Prior simulated-baseline compiler snapshot -- PASS

At `2026-10-08T10:18:48+01:00`, requested single current-state check:
QEMU PID **1790915**, parent timeout **1790913**, process state **Sl**,
elapsed **01:48:23**, CPU time **01:48:24**, started **08:30:24 BST**.
The exact process command/kernel/live image are unchanged from above.
Serial: `/tmp/remqa/foldv2.serial`.

Actual REM execution now proves compiler qualification/self-rebuild PASS:

```text
PASS aggregate-small-return (seed)
PASS aggregate-small-return (stage1)
PASS aggregate-small-return (stage2)
PASS aggregate-small-return
PASS aggregate-abi
PASS chibicc caller / GCC aggregate callee
PASS GCC caller / chibicc aggregate callee
COMPILER_REQUIRED_PORTING_TESTS_OK
COMPILER_REQUIRED_SELFREBUILD_OK
--- Samurai (log: /home/dev/rem-native-userland/rem-update-samurai.log)
```

All other required porting/rejection tests printed PASS. Each stage's small
return checks unchanged caller input and returned `{11,23}`. This is real REM
execution, not synthetic host gate testing. No user fault or panic was observed.
Latest meaningful output is the Samurai phase entry, guest log
`/home/dev/rem-native-userland/rem-update-samurai.log`.

Remaining expected actual markers:
SAMURAI_BUILD_OK, SAMURAI_DRY_RUN_OK, SAMURAI_NATIVE_BUILD_OK,
SAMURAI_INCREMENTAL_REBUILD_OK, SAMURAI_CLEAN_OK, SAMURAI_NATIVE_EXECUTION_OK,
REM_UPDATE_VERIFY_OK, REM_QA_DONE|0. No final v2 release archive/hash yet.

Source checkpoint local `a984e6381deccda325c0b29b3cac93fcfce432e3`,
task-only remote `f7b8e76df738ac50a0cdf2e87de9a37d81e05239`, push PASS.
All current-task source/scripts/tests are preserved there; unrelated recovered
work remains untouched. Only these final evidence/documentation changes are new.

**Single next action for Codex:** consume existing observer session `646` /
original `foldv2` completion, then apply the exact release commands above if
every actual marker passed. Never restart or repeat native compiler rebuilds.
To inspect without restarting, use the documented `ps`/`tail`/`grep` block.
Next routine observation no earlier than **10:28:48 BST**, unless an exit,
completion or failure event occurs. Keep the originating CLI alive; do not
kill QEMU, mutate/fsck its live image, or transfer a PENDING/synthetic archive.

Final independent preservation audit after that snapshot:
local documentation/evidence checkpoint
`f0cdec22b0951f3b54b2b3fdec0cf9595d716d3f`;
task-only remote `de7bc02027099dd758e110c3adb96cd0ffaaacbd`, push PASS.
All 21 task entries matched byte-for-byte between committed canonical HEAD and
remote checkpoint; all task files clean and no unrelated staged entries.
Nested compiler commit `f3a3094f16fd144719be9b4ee77e84836e4c1bfa` was independently
confirmed on its GitHub publication branch. Unrelated remaining Git status:
272 entries, consisting of 47 modified, one type change, 224 untracked.
These are preserved, not cleaned or included in task commits.

## Prior simulated-baseline qualification and release record -- rejected for actual Fold

Original `foldv2` completed, harness exit 0 and authoritative guest
`REM_QA_DONE|0`. Observer 646 captured the actual completion; both original QEMU/
timeout PIDs 1790915/1790913 were gone at 10:33:59 BST. No job was restarted.
Actual seed/stage1/stage2 small-return, compiler porting/self-rebuild, all six
Samurai, full verification and guest-success markers were observed. No user
fault or panic appeared. Actual logs are durable:
`docs/REM_FOLD_V2_EVIDENCE_20261008/{qualification.serial,compiler.log,samurai.log}`.
Compiler result file in the guest was explicitly read as 0.

New host-only release safety: every target is preflighted, accepting tested
old-helper/library baselines but rejecting missing/unknown noncompiler files
before image/kernel mutation. Compiler local hunks still merge/preserve edits.
This also avoids overwriting unknown local Samurai/helper sources.
New post-build reinstall fix: a runtime recorded absent before the first install
is never reclassified as original merely because native qualification built it.
Rollback still removes that new runtime after reinstall.

Exact final archive's installer suite PASS/0, seven PASS groups: corrupted/
missing/wrong-hash payload, missing kernel and obsolete/wrong-size images;
missing/modified required library and local helper conflicts; incomplete ABI
verification rejected; local parser merge/preservation/idempotence; exact source
rollback; post-build reinstall/runtime restoration/removal/local data; parser
conflict abort. Negative preflights compare complete image+kernel SHA-256.
Logs: `release-input-verify.log`, `final-archive-install.log` in the evidence dir.

Final release directory:
`/tmp/rem-fold-final-release.koeW2P/rem-update-20261008-v2`.
Actual archive extraction used for tests:
`/tmp/rem-fold-final-archive-test.zuWrzf/rem-update-20261008-v2`.
Archive: `/tmp/REM-FOLD-update-20261008-v2.tar.gz`.
Size: **4,713,486 bytes**.
SHA-256:
`ab19349f0fe651df40cbad970f3dce371a2850ecfee44d60c03741dd9eb95d1b`.
Adjacent `.sha256` and `.manifest.txt`; internal checksummed `MANIFEST.txt`.
Reproducibility (two archives), extraction/hash checks, complete manifest and
matched tested seed/kernel all passed. Do not overwrite this verified artifact.

`docs/REM_FOLD_V2_RELEASE_20261008.md` contains the literal-hash install block,
one-archive transfer procedure, original backup/log paths and rollback.
Further related hardening may continue, but there is no native qualification
blocker. Next agent should verify preserved release artifacts/checkpoints and
only pursue an actual remaining release-validation gap; do not redo native builds.

Final source/docs/evidence checkpoint:
local `664948ab42bc5d59fd4bd0d44edb5446070829ae`;
published `18e7cdc521d89ebb092a8dd7d1de8aba79df340c`,
task-only branch `rem-fold-update-20261008-v2-checkpoint`, push PASS.
The committed set was independently verified as exactly 11 intended task files.
One trailing space in the kernel's `pcpu-alloc` serial line is deliberately
retained as actual evidence; source/script/docs whitespace is otherwise clean.

Requested final repeat from the exact immutable archive PASS/0: all seven
install/reinstall/post-build rollback/safety groups. Durable log:
`docs/REM_FOLD_V2_EVIDENCE_20261008/last-final-archive-install.log`.
Audit proved existing tracked binary-diff fingerprint and unrelated Git status
unchanged, and archive still exactly the published hash. Only intended evidence
was added. No qualifications are still running; no native or packaging blocker.

Historical next action for that old-baseline checkpoint: verify its preserved
archive SHA-256. That archive is now rejected for the actual Fold; follow the
current status at the top of this handover instead.
