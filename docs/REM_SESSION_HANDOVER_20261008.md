# REM32 Fold ABI/update handover -- 2026-10-08

**CURRENT STATUS: QUALIFICATION STILL RUNNING**

**FINAL V2 PACKAGE: NOT YET READY FOR TRANSFER**

Implementation resumed at Rick's subsequent request. This is a code/fix checkpoint,
not a claim that the new full qualification has passed. Do not restart the
already-running job or send the original archive to the Fold again.

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

## Running qualification -- snapshot, not completion

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

## Scratch artifacts and unfinished delivery

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

## Exact next actions

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

## Authorized continuation after checkpoint

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
