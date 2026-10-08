# REM session checkpoint — 2026-10-08 (compiler ABI, Samurai, Fold userland)

Resume from here; do not redo completed items. Canonical repo only; scratch in /tmp.
Never git clean/reset/restore; ~265 pre-existing dirty/untracked entries must be preserved.

**CURRENT STATUS: V2 COMPILER/SAMURAI/FULL QUALIFICATION PASS**

**FINAL V2 PACKAGE: QUALIFIED AND READY FOR TRANSFER**

The current continuation is in section 17 and the release/handover documents.
Earlier v1 completion statements do not qualify the corrected v2 update.

## 1. Completed (verified)

### Aggregate ABI fix (chibicc) — DONE, both directions PASS in REM QEMU
- `toolchain/chibicc-rem/codegen.c`: `gen_call` uses hidden return pointer for ALL
  struct/union returns (`hidden_return = aggregate_return`); ND_RETURN always copies
  through the hidden pointer (r1:r2 small-return path removed).
- `toolchain/chibicc-rem/parse.c` `function()`: every aggregate return gets hidden
  `new_lvar("", pointer_to(rty))` first parameter.
- `toolchain/native-userland/patches/chibicc-rem-native.patch`: regenerated (keeps
  begin_label loop headers, sign/zero widening, TOMBSTONE, neg array bound, atomics reject).
- REM ABI (GCC): r1 = hidden sret ptr, aggregate args by pointer, r1-r4 then stack,
  32-byte home/offset area (STACK_POINTER_OFFSET 16 + REG_PARM 16), SP 16-aligned.
- Evidence: "PASS full chibicc caller / GCC aggregate callees" and
  "PASS GCC aggregate caller / patched chibicc callee", exit 0, REM QEMU UAT guest.

### Draft Fold package (needs extension, see §4)
- `/tmp/REM-FOLD-native-abi-update-20261008.patch` (89,333 B,
  sha256 c5134f5dee1e34a31cf7cda3d3d5e1233845dff0f72c0700cf35078c0e7356ce)
  + `.manifest.txt`. Baseline: REM-FOLD-BUILD-SOURCE-20261007.tar.gz
  (b2aa4641cf55951080a00b2f8a9f693f541ed50958872e46843e3010343b532f), nested chibicc cd9b2ce4.
- Open question: Fold path layout (`/home/dev/rem-native-userland` vs kit root) —
  verify against tarball structure before finalising.

## 2. Fold userland findings — ROOT CAUSES (reproduced in REM QEMU, UAT image, -m 128M)

Image = REM-FLIGHT-HUMAN-UAT-FULL/rootfs.ext4 (unchanged). /bin/busybox sha 9e7acc1d…
= workstation overlay `tools/rem-flight-image/staging/distribution/workstation-20260928`
(README: "never booted or run on REM"). Built with current GCC 07ef3aaa…, -O0 style.

1. **/proc,/sys not mounted (D/config)**: dirs exist, kernel has proc/sysfs/devtmpfs,
   but `/etc/init.d/rcS` only runs `rc start 3`; `/etc/fstab` empty; rc3.d has only
   S99local (+ S40network in init.d). Nothing mounts proc/sys. Fix: init script
   `S01mountfs` (mount proc, sysfs, devtmpfs optional; /etc/mtab -> /proc/mounts).
2. **BusyBox rejects valid options (A — REM GCC backend bug)**: `grep -E/-i/-v/-F/-e`,
   `wc -l`, `ln -s`, `mount -t/-r`, `df -P`, `stat -c` → "unrecognized option".
   Proven with instrumented libbb/getopt32.c: optstring is EMPTY at getopt_long.
   Cause: vgetopt32 does `strcpy(alloca(len+1), opts)` then calls
   `getopt_long(argc, argv, opts, lo, NULL)` — 5th arg stored at sp+32, which is
   exactly where GCC's alloca returns (STACK_DYNAMIC_OFFSET = 32 only).
   **STACK_DYNAMIC_OFFSET must also include outgoing stack-argument size**
   (crtl->outgoing_args_size beyond the reg-parm area). See
   `toolchain/gcc/myemulator2.h` lines ~120-176 (`myemulator2_stack_dynamic_offset()`;
   its .cc implementation location was being searched). mkdir works only because a
   second alloca (long_options) absorbs the clobber (long_options[0].name is clobbered!).
   libc getopt/getopt_long verified correct in REM (/tmp/remqa/go.c).
   Earlier related fix: commit b4dedb3 (home-slot aliasing) — incomplete.
   Impact: every GCC-built program using alloca/VLA + calls with >4 args (BusyBox,
   possibly awk/objdump crashes, mount silently doing nothing).
3. **Exit status encoded as signal (B/C — kernel or libc)**: `sh -c 'exit 3'` and
   `bash -c 'exit 3'` → shell reports "Quit", $?=131; exit 1 → "Hangup"/129;
   exit 255 → "Unknown signal". Wait status low bits get the exit code (not <<8).
   Not yet investigated: check linux/arch/myemulator2 syscall table for
   exit/exit_group/wait4 and musl `_Exit`. Affects every script's error reporting.
4. **Config gaps (D)**: workstation busybox.config lacks FEATURE_HUMAN_READABLE (df -h/-k),
   FEATURE_STAT_FILESYSTEM (stat -f), DESKTOP (od -A), EXTRA_COMPAT, BUSYBOX applet
   (`busybox --list`), INCLUDE_SUSv2 (head -3), FEATURE_CATN, FEATURE_FIND_TYPE, EGREP,
   dmesg applet missing in this build, no tune2fs/dumpe2fs.
5. **awk crash**: busybox awk SIGSEGV pc=020ada20 addr=0000025c fp=00000270 (bad fp —
   likely same alloca/outgoing-args class; re-test after GCC fix).
   **objdump**: in this run objdump -f worked but triggered ext4 WARN
   fs/ext4/inode.c:3554/3555 + "page does not have buffers attached" (kernel page-cache
   issue, class B) — keep separate.

## 3. Other blockers recorded earlier
- Installed /usr/bin/chibicc in UAT guest faults in hashmap match() reading
  0x1000xxxx (TASK_UNMAPPED_BASE mmap region); staged samu-final-qualified also faults
  at 0x10000044 after a successful child. Class B/C (mmap/page fault), not codegen.
  Blocks native qualification (`COMPILER_REQUIRED_PORTING_TESTS_OK` NOT reached) and
  Samurai real build. Possibly related to the ext4 page/buffer warnings above.
- Samurai source not in kit; need upstream samurai (13 TUs) built with
  `-DNO_POSIX_SPAWN -DCHIBICC_REM`.

## 4. Outstanding work (priority order)
1. Fix GCC STACK_DYNAMIC_OFFSET (add outgoing args); rebuild cross GCC; add regression
   test (alloca buffer + 5+ arg call). Rebuild BusyBox with expanded config
   (§2.4 features) using /tmp/remqa/bb-dbg recipe; verify in REM.
2. Investigate exit-status encoding (kernel syscall/wait path).
3. Investigate 0x1000xxxx mmap faults → unblock native chibicc + qualification + Samurai.
4. Add S01mountfs + rc3.d link + /etc/mtab symlink; verify across normal SysV boot.
5. Extend Fold package: compiler patch + init script + rebuilt busybox (only binary
   needed) + regression tests + single BusyBox-safe apply script (no awk/objdump/
   grep -E/-C/find -type/od -A/df -h/mount -t/ln -s until replaced) + manifest + sha256.

## 5. Scratch (keep; needed for testing)
- `/tmp/remqa/run.sh NAME GUEST_SCRIPT [debugfs-extra] [mem] [timeout]` — fresh copy of
  UAT rootfs, debugfs inject, sysinit override, boots QEMU, prints `REM_QA_DONE|rc`;
  serial in `/tmp/remqa/NAME.serial`. Create symlinks via debugfs `symlink`, not ln -s.
- `/tmp/remqa/bb-dbg/` — BusyBox 1.37.0 O= build (workstation config). Build:
  MYEMU_MUSL_PREFIX=toolchain/flight-kit-runtime, MYEMU_TARGET_GCC=.toolchain-install/bin/
  myemulator2-elf-gcc, CC=toolchain/scripts/myemulator2-musl-gcc, CFLAGS with
  `-isystem $prefix/include -nostdinc -isystem $(gcc -print-file-name=include)`;
  final link done manually adding `-lc -lgcc` (busybox lib-trial drops libgcc; libm stub
  at /tmp/remqa/fakelib). busybox_unstripped / busybox_dbg (instrumented getopt32).
- `/tmp/rem-native-qualify-1pu56edm/` — earlier ABI interop logs/scripts.
- BusyBox source: `.rem-flight-build-20260924/busybox-source-cache/busybox-1.37.0` (do not edit).

## 6. Update (later 2026-10-08) — supersedes §4 items 1, 2, 4

### Done / verified in REM QEMU
- GCC STACK_DYNAMIC_OFFSET fix applied (toolchain/scripts/prepare-gcc-source.py +
  .toolchain-build tree), cross GCC rebuilt/installed; alloca now sp+48 vs 5th arg sp+32.
- **libc.a large-frame miscompile (class A, old GCC)**: old flight-kit libc.a
  (sha 30cff688…, == Fold/R4/UAT /usr/lib/libc.a) has 35 functions whose >4 KiB
  prologue uses r12 as scratch AND as saved caller-fp → caller fp := frame size
  (0x10a0 etc). Includes glob, fmt_fp (printf %f), decfloat (strtod), getaddrinfo,
  realpath, system, posix_spawn, nftw, twoway_strstr, sha512crypt, __res_send.
  Any ash glob (`for f in /etc/*`) segfaults (old AND new busybox). Current GCC is
  correct. musl 1.2.5 rebuilt with current GCC (/tmp/remqa/musl-inst; needed
  CFLAGS=-Wno-error=pointer-sign) → 0 bad prologues; same 1345 members; headers
  identical. Installed into toolchain/flight-kit-runtime/lib/{libc.a,crt1.o}
  (new sha 269b1d6b…/76fd0c9d…; old copies /tmp/remqa/oldrt/).
  Scan cmd: objdump -d X | grep -A3 'sub r13,r13,r12' | grep -c 'sw r12,'
- Kernel (linux/arch/myemulator2/kernel/syscall.c): added symlinkat(36), linkat(37),
  statfs(43)->sys_statfs64(sizeof statfs64), fstatfs(44), syslog(116).
  Exit-status fix was pre-existing uncommitted work. Built in /tmp/remqa/linux-src
  (= .linux-build/linux-6.12.1 + canonical arch, llist.c diag removed) with the phone
  network config (.rem-phone-deploy-20260925/linux-network-build/.config):
  /tmp/remqa/linux-netbuild/vmlinux. Fold runs R4 kernel 576c1f4f… (source gone).
- BusyBox rebuilt (/tmp/remqa/bb-new, link via /tmp/remqa/bb-link.sh) with new libc and
  + IFCONFIG ROUTE NC MKFIFO POWEROFF; 86 applets; 0 bad prologues.
- R4 lineage image `.r4-admin-test/rootfs.ext4` (closest Fold ancestor) has THREE
  busyboxes: /bin/busybox (9e7acc1d), /bin/rem-transfer-busybox (58f2d1f4; [ test ln wc
  ifconfig route nc tar sha256sum mv rm chmod date mkfifo printf sync poweroff),
  /bin/busybox-human-baseline (1d8ae8c3; /bin/sh, /bin/login). All old-toolchain.
- Test `/tmp/remqa/run-r4.sh r4full /dev/null r4full.extra` (R4 copy, new kernel,
  new busybox as all 3 names, new rcS, user-net): ALL PASS — /proc,/sys mounted by
  SysV boot, mtab, mount, df, df -h, stat -f, /proc/{mounts,partitions,cmdline},
  /sys/block, grep -E, wc -l, od -An, find -type, awk, ln -s, exit 3 => 3, network
  eth0 + nc TCP to host. Old kernel + new busybox: df/stat -f/ln -s fail (ENOSYS),
  exit codes wrong -> kernel update REQUIRED.
- Pre-existing harmless WARN fs/sysfs/group.c:128 at boot in both old and new kernels.

### Next
1. Retest native chibicc / samu faults with new libc (likely same libc defect).
2. Qualification + Samurai in REM. 3. Package (install.sh in /tmp/remqa/pkg/out).

## 7. Update (late 2026-10-08) — kernel, package, Fold simulation

- Native chibicc/samu faults at 0x1000xxxx under the OLD R4 kernel are class B
  (kernel). With the new kernel (`/tmp/remqa/linux-netbuild/vmlinux`, sha
  bba3c98b…) chibicc and samu dry-run/build/incremental all pass. The kernel
  update is therefore mandatory in the Fold package.
- The Fold tree is the KIT layout: compiler sources at
  `bootstrap/chibicc/{codegen.c,parse.c}`; procedure
  `bash bootstrap/rebuild-required-native.sh` -> `COMPILER_REQUIRED_SELFREBUILD_OK`.
- `toolchain/native-userland/patches/chibicc-rem-native.patch` reshaped =
  HEAD patch with only the codegen.c/parse.c hunks replaced (kit shape kept).
- Samurai = upstream 531ba700 unmodified (sources.env line added);
  `kit/bootstrap/build-samurai-native.sh` builds/self-tests it natively.
- BusyBox config with CONFIG_FALSE: `toolchain/userspace/rem-sysv/busybox-1.37.0-rem-20261008.config`.
- Fold update tooling: `android/fold-update-20261008/{install.sh,verify.sh,make-package.sh}`.
  `OUT=/tmp/rem-update-20261008 bash make-package.sh` reproduces the tested
  package tree byte-identically. Installer (debugfs, QEMU stopped) tested on a
  10 GiB simulation: install / rerun / verify / rollback / reinstall OK, e2fsck clean.
- Seen at boot with new kernel: one `WARNING fs/sysfs/group.c:128` (non-fatal).
- qual3 failed at designator-unnamed-bitfield: write_gvar_data `break` bug (see
  REM_CHIBICC_BUGLOG). Fixed in parse.c + patch; qual4 rerun started.
- qual4 kernel-panicked (double fault, misaligned SP between GCC prologue
  steps). GCC fixed (step 252), kernel rebuilt (sha c8b394e3…), package
  regenerated; qual5 + foldboot3 started.
- Resume: check `/tmp/remqa/qual.serial` and `/tmp/remqa/foldboot.serial` for
  `COMPILER_REQUIRED_PORTING_TESTS_OK`, `SAMURAI_*_OK`, `REM_UPDATE_VERIFY_OK`,
  then tar the package + manifest into /tmp.

## 8. COMPLETE (2026-10-08 ~07:00)

- qual5 (REM, new kernel c8b394e3…): STAGE1_OK, SELFBUILT_OK, all porting PASS
  incl. both aggregate interop directions, COMPILER_REQUIRED_PORTING_TESTS_OK,
  Samurai build/dry-run/native-build OK (scratch harness incremental step lacked
  `sleep 1` — mtime granularity; kit script already has it).
- foldboot3 (10 GiB Fold simulation, package installed via install.sh, normal
  SysV boot, `verify.sh full`): COMPILER_REQUIRED_PORTING_TESTS_OK,
  COMPILER_REQUIRED_SELFREBUILD_OK, all SAMURAI_*_OK, REM_UPDATE_VERIFY_OK, rc 0.
- Package: /tmp/REM-FOLD-update-20261008.tar.gz + .manifest.txt
  (regenerate: `bash android/fold-update-20261008/make-package.sh`, then
  `tar czf /tmp/REM-FOLD-update-20261008.tar.gz -C /tmp rem-update-20261008`).
- Deferred: objdump ext4 WARN; boot WARNING fs/sysfs/group.c:128 (non-fatal);
  guest-native GCC (if used on Fold) still lacks the alloca/SP-step fixes.

## 9. Real Fold ABI failure follow-up (2026-10-08; supersedes completion above)

The real Fold passed quick checks but its selfbuilt compiler produced
`aggregate-abi RC=1`, `t.first=0`, `t.last=1`. Do not ship the original archive.
The exact values have now been reproduced in REM/QEMU with a native compiler
whose codegen.c has the new hidden-result ABI but whose parser is old.
Its caller passes result in r1 and argument in r2; the callee (missing hidden
parameter) reads r1 as the argument. The zeroed result becomes `{0,1}`.

The old installer was also reproduced end-to-end: a Fold-era `break` ->
`continue` initializer fix in locally modified parse.c makes one hunk of the
combined parser diff fail. The installer updates codegen.c, leaves parse.c
unchanged, nevertheless prints INSTALL COMPLETE and VERIFY_OK. This is an
installer/compiler-source mismatch, not a libc/kernel difference.

Canonical repairs in progress:
- install.sh preflights both compiler files before any image changes, merging
  each hunk independently (including already-applied fixes); conflicts abort.
  verify no longer treats arbitrary LOCAL sources as qualified.
- codegen.c rejects an aggregate callee lacking the parser's hidden parameter.
- new aggregate-small-return.c checks source unchanged and result 11/23.
- deterministic new REM-native seed; rebuild-required-native.sh tests seed,
  stage1 and stage2 and only replaces selfbuilt after full qualification.
- v2 backups include the original compiler/Samurai and updated verifier.

Scratch (owned by this follow-up; preserve while tests run):
- /tmp/remqa/abi-followup/: variant compilers, exact reproduction logs/assembly,
  seed-build/chibicc-update-seed, original-package/, Fold-case image/backups.
- /tmp/rem-update-20261008-v2/: regenerated candidate package tree.
- /tmp/remqa/foldv2.ext4, foldv2.serial: NORMAL SYSV BOOT of v2 installed over
  the partially updated, locally modified Fold simulation. Running job foldv2.
  Require COMPILER_REQUIRED_PORTING_TESTS_OK, COMPILER_REQUIRED_SELFREBUILD_OK,
  every SAMURAI_*_OK and REM_UPDATE_VERIFY_OK, REM_QA_DONE|0.
- Full-image tests of rollback/idempotence/conflict detection still required.
- Package recipe: CHIBICC_SEED=/tmp/remqa/abi-followup/seed-build/chibicc-update-seed
  OUT=/tmp/rem-update-20261008-v2 bash android/fold-update-20261008/make-package.sh.
- Preserve old archive as evidence; final replacement must be named v2.

Later follow-up: the source merger now uses exact, unique old/new hunk
fragments in POSIX shell (sed/grep/cmp), not Termux patch; no extra download
needed. test-install.sh passed local-fix merge/preservation, idempotence,
rollback and conflicting-parser abort before either source is written.
Backups include the original compiler and Samurai for post-qualification
rollback. The kit's static libc is installed from the same corrected payload
as /usr/lib/libc.a so qualification cannot silently link an older kit runtime.
The running foldv2 used byte-identical guest sources/runtime; the final host
installer changes are separately covered by test-install.sh.

## 10. Immediate handover requested (2026-10-08 09:04 BST)

**CURRENT STATUS: QUALIFICATION STILL RUNNING**

**FINAL V2 PACKAGE: NOT YET READY FOR TRANSFER**

Implementation is paused; no new builds/tests/fixes were started for this
handover. Read `docs/REM_SESSION_HANDOVER_20261008.md` first. It contains the
exact task-only commit scope, running command, process/log paths, confirmed
results, original installer reproduction, baseline caveat and ordered next steps.

Snapshot: QEMU PID 1790915, timeout parent 1790913, tool session foldv2;
started 08:30:24 BST, elapsed 34:22 at capture, 100% of one CPU.
Live image `/tmp/remqa/foldv2.ext4`, serial `/tmp/remqa/foldv2.serial`.
Latest meaningful line: compiler qualification entered, guest log
`/home/dev/rem-native-userland/rem-update-compiler.log`.
Quick checks passed; no new compiler completion marker, user fault or panic
was observed. Leave the process alone and keep its originating CLI session alive.

Confirmed before handover:
- native Fold symptom reproduced: aggregate RC=1, FIRST=0, LAST=1;
- seed recipe PASS/0, byte-identical:
  `6b28c165cb94b7411a63b7e74932845b11b8e30ea2bd6882b9c7bf1348a2ac44`;
- installer regressions PASS/0: incomplete verification rejected; local edits
  merged/preserved/idempotent; exact rollback; conflicts abort before ABI writes.

Pending: actual required compiler markers, native Samurai markers,
REM_UPDATE_VERIFY_OK and REM_QA_DONE|0, then final v2 archive/manifest/SHA-256.
The harness exit code alone does not establish guest success.

After checkpointing, HEAD no longer holds the original source baseline:
use `BASE_PATCH=/tmp/remqa/abi-followup/base.patch` for packaging, or extract it
from pinned parent revision `e962d91f238476cc92669e5e514b12075dca0136`.
The handover documents the corresponding test-install.sh fixture caveat.

## 11. Work resumed after handover checkpoint

The user authorized continued qualification and delivery without restarting
completed or running work. Parent checkpoint:
`ab437cbb998a0143cf62607b71b84f5600cbe1cd` (19 task entries); nested compiler
checkpoint `f3a3094f16fd144719be9b4ee77e84836e4c1bfa` was published separately.
Parent HTTPS push exited 1: GitHub rejected pre-existing ancestor files
`REM-FLIGHT-DEVELOPMENT.zip` (283.23 MB) and
`docs/REM_QEMU_Debugging_Reference.pdf` (179.30 MB). Do not rewrite main or
delete recovered files to work around that. Publish a task-only checkpoint
branch based on the existing remote main, preserving the local checkpoint.

Packaging and installer-test fixtures now pin original baseline revision
`e962d91f238476cc92669e5e514b12075dca0136` and validate the original patch SHA.
The generator refuses nonempty output directories rather than deleting them.
These are host-only corrections; the running guest qualification is unchanged.
Final v2 qualification/archive remain pending actual execution markers.
At elapsed 45:44 QEMU PID 1790915 remained active, compiler qualification
entered, no completion marker in serial. Host syntax/scoped whitespace checks
passed; seed and original patch hashes remain exactly as recorded.

Safe task-only checkpoint publication PASS:
`7d12be9c0479a99de4bf97244095735ab2804edd` pushed to
`origin/rem-fold-update-20261008-v2-checkpoint`; unrelated oversized main history
was neither rewritten nor removed.
Fresh candidate `/tmp/rem-fold-final.Uzh0Cd/rem-update-20261008-v2`:
payload checksums PASS; corrected installer regression PASS/0 (all four checks);
stopped qualification launch-source verification PASS/0, including retained
local parser edits, payload identity, filesystem check and `VERIFY_OK`.
Live QEMU at elapsed 48:01 still had no authoritative completion marker.

## 12. Release gates and post-build rollback validated

Local checkpoint `87eea048afaa940ca73fd86719507ed7ef7a13ef` is preserved and
task-only publication `201846b79da0a70aad763c9c28efef67ba13c491` was pushed
successfully to `rem-fold-update-20261008-v2-checkpoint`.

The package generator now includes a checksummed `MANIFEST.txt`. Without
`QUALIFICATION_LOG` it explicitly labels the tree PENDING/not transferable.
For a release, the actual completed REM serial log must contain all 13 exact
seed/stage/compiler/Samurai/full-verification/guest-exit markers; user faults,
kernel panics and failed guest results are rejected.

New task file `android/fold-update-20261008/test-package.sh` passed all missing
marker cases, all three fault/exit rejection cases, manifest checksums, pending
label and nonempty-output preservation. Its synthetic positive log is HOST-ONLY
unit evidence, NOT native qualification; its disposable synthetic tree was
removed. No archive was released from synthetic evidence.
Installer regression passed again against the manifested candidate:
original compiler/runtime backup restored after a simulated rebuild, newly
created Samurai removed on rollback, unrelated local source/data unchanged,
plus all earlier partial-update/local-edit/idempotence/conflict checks.
Failing host regression scratch is now retained for diagnosis.

At elapsed 54:54 QEMU PID 1790915 was still running; serial remained at compiler
qualification, no completion or fault marker. Final v2 archive is still pending.
Single next action: inspect that existing qualification, not a replacement run.

## 13. Pushed release-gate checkpoint and ongoing execution

Local `c919b12f2592e1432587bb5ab5857d158863993d` and safe publication
`b16427fb4d5c3aa0dd967523276c61bb33cbe300` preserve all validated release-gate
and runtime-rollback work. Publication push PASS; unrelated work untouched.
At elapsed 01:06:38 the original QEMU PID 1790915 was still active, about one
CPU fully utilized. Serial still showed compiler qualification entered and no
completion/fault marker. No work was restarted; waiting is economical after
the independent host validation and documentation completed.

## 14. Reproducible archive and complete manifest validation

New verified task source:
`android/fold-update-20261008/archive-package.sh` (21 parent task paths total).
It requires a checksummed PASS manifest and the exact REM-tested seed/kernel,
rejects extra/unchecksummed/nonregular files and existing outputs, normalizes
archive order/time/ownership/permissions and gzip headers, compares two archives,
extracts and verifies payload hashes, then generates/verifies archive SHA-256
and an adjacent manifest with byte size. Pending packages cannot be released.

Expanded HOST-ONLY `test-package.sh` PASS/0:
13 separate missing-marker cases; user fault/panic/nonzero guest result rejection;
synthetic marker acceptance (NOT REM evidence); reproducibility despite changed
input mtimes; extracted payload and archive hashes; mismatched seed/kernel even
after resealing file checksums; complete inventory; pending release rejection;
nonempty output preservation. Synthetic archives were disposable and removed.
No final release archive or final transfer SHA-256 exists yet.

Last qualification check: elapsed 01:17:00, PID 1790915 still active, compiler
phase entered, no completion/fault marker. User now requires checks no more
often than every ten minutes unless an exit/completion/failure event occurs.
Do independent safe work between checks; do not restart the qualification.

## 15. Event-driven observation and strengthened archive regression

Archive/source/docs checkpoint local `2eb637e474922596689aef13914c301bb3e0d371`
was preserved; task-only remote `ea217004c6327e39809e03dbca69f4e8f434efb0`
was pushed successfully.

Event-only observer tool session `646` is active and verified responsive:
Linux inotify observes serial writes and pidfd observes QEMU exit; there is no
periodic QEMU/status polling. It reports completion, user fault, panic or explicit
compiler/Samurai failure and does not alter the qualification. Read its result
on completion notification, then inspect the original `foldv2` evidence.
Last actual progress snapshot remains elapsed 01:17:00; no newer completion
marker has been observed. The next action is to consume that event/result.

Archive validation additionally requires every execution marker in the sealed
manifest and checks the extracted file inventory. Host tests PASS/0 additionally
prove: an existing archive/hash cannot be overwritten; a resealed PASS manifest
missing compiler completion is rejected; unexpected symlinks are rejected.
Complete updated host package regression PASS/0 (11 PASS lines). Synthetic
fixtures are still NOT REM qualification and were removed on success.

Latest preserved checkpoint: local
`9af9f0ecaf5e7382112e5c6e87ee944773230322`, task-only remote
`15a249136c465f4a7236131c0efe48bc9c109634`, push PASS.
Archive manifest validation now also independently checks every guest payload
mapping's declared new SHA-256 against its actual file. Latest complete host-only
package regression PASS/0, 12 PASS lines, including a deliberately inconsistent
mapping with resealed package checksums. No guest or qualified build changed.
Independent release/rollback/documentation validation is complete; consume the
event-only observer's result rather than repeatedly querying QEMU.

## 16. Actual native compiler qualification PASS; Samurai running

Requested single snapshot at `2026-10-08T10:18:48+01:00`:
QEMU PID 1790915, parent timeout PID 1790913, state `Sl`, elapsed 01:48:23,
CPU time 01:48:24; original start 08:30:24 BST. Same command, kernel and live
image as documented; no restart or alteration.

Actual `/tmp/remqa/foldv2.serial` execution evidence now includes:

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

All other required compiler porting tests/rejection tests printed PASS.
The native seed/stage1/stage2 return regression checks unchanged input and
`t.first == 11`, `t.last == 23`. These are actual REM results, not host fixtures.
No user fault or kernel panic appeared in the inspected serial.

Still expected: SAMURAI_BUILD_OK, SAMURAI_DRY_RUN_OK, SAMURAI_NATIVE_BUILD_OK,
SAMURAI_INCREMENTAL_REBUILD_OK, SAMURAI_CLEAN_OK, SAMURAI_NATIVE_EXECUTION_OK,
REM_UPDATE_VERIFY_OK, REM_QA_DONE|0. Until those occur, final v2 is NOT ready.
Original tool `foldv2` and event observer `646` remain the existing run/result
sources. Codex should read their completion notification, or inspect the exact
PID/serial with the handover commands; do not start a new qualification.
Next routine check no earlier than 10:28:48 BST unless exit/failure/completion.
All validated source was already published at task-only checkpoint
`f7b8e76df738ac50a0cdf2e87de9a37d81e05239`
(local `a984e6381deccda325c0b29b3cac93fcfce432e3`).

Documentation/evidence checkpoint local
`f0cdec22b0951f3b54b2b3fdec0cf9595d716d3f`,
task-only remote `de7bc02027099dd758e110c3adb96cd0ffaaacbd`, push PASS.
Independent preservation audit PASS: all 21 task entries are byte-identical
between canonical committed HEAD and published checkpoint; task working tree
clean; no staged unrelated entries; nested compiler `f3a3094...` confirmed on
GitHub. Remaining unrelated work: exactly 272 status entries (47 modified,
one type change, 224 untracked), unchanged and not staged.

## 17. Actual full v2 qualification and final-archive tests PASS

Original `foldv2` harness exited 0, guest `REM_QA_DONE|0`; original QEMU/timeout
PIDs 1790915/1790913 were confirmed gone at 10:33:59 BST. Event observer 646
captured completion and exited. Every required actual marker passed:
seed/stage1/stage2 small return, both required compiler markers, all six Samurai
markers, REM_UPDATE_VERIFY_OK. No user fault or kernel panic in the full serial.
Durable actual logs: `docs/REM_FOLD_V2_EVIDENCE_20261008/`.

Release installer safety was strengthened without changing guest payloads:
all targets preflight before any filesystem/kernel mutation; known original
helpers/runtime hashes accepted; unknown local helpers/libraries/Samurai files
or missing required baselines rejected rather than overwritten.
Post-build reinstall no longer mistakes a newly built Samurai for an original
binary that should be restored by rollback. Actual-final-archive tests PASS/0:
corrupt/missing/wrong-hash payload, missing kernel, wrong-size/obsolete image,
missing/modified library and modified helper, incomplete ABI verification,
local edit preservation/idempotence, exact source/runtime rollback, post-build
reinstall/original-absence restoration, conflicting parser atomic abort.
Rejected installs were checked with full image+kernel SHA-256, not only sources.

Final qualified tree: `/tmp/rem-fold-final-release.koeW2P/rem-update-20261008-v2`.
Final extracted archive test tree:
`/tmp/rem-fold-final-archive-test.zuWrzf/rem-update-20261008-v2`.
Archive: `/tmp/REM-FOLD-update-20261008-v2.tar.gz`, **4,713,486 bytes**,
SHA-256 `ab19349f0fe651df40cbad970f3dce371a2850ecfee44d60c03741dd9eb95d1b`.
Two byte-identical normalized archives, extracted hashes, complete manifest,
matched REM-tested seed/kernel and final archive SHA-256 all PASS.
See `docs/REM_FOLD_V2_RELEASE_20261008.md` for exact one-archive transfer,
literal-hash Termux install block, single guest qualification command and rollback.

Further directly related release hardening may continue; do not redo the passed
native qualification or rebuild the tested compiler/kernel without a new failure.

Final source/evidence checkpoint local
`664948ab42bc5d59fd4bd0d44edb5446070829ae`,
task-only remote `18e7cdc521d89ebb092a8dd7d1de8aba79df340c`, push PASS.
Committed file set independently verified: exactly the 11 intended source/
documentation/evidence files, no unrelated files.
Raw normalized serial preserves one kernel boot line's trailing space
(`pcpu-alloc`, line 13); it is intentional evidence formatting, not source
whitespace. Do not rewrite actual logs merely to silence that diff-check warning.

Requested last exact-archive installer/rollback rerun PASS/0 (all seven groups).
`last-final-archive-install.log` is durable. Audit confirmed no unexpected
working-tree status changes, no change to existing tracked modifications
(full binary diff fingerprint), and no change to the verified release archive.
All native/package/install/rollback/transfer work is complete; no running QEMU
qualification or outstanding native blocker remains.
