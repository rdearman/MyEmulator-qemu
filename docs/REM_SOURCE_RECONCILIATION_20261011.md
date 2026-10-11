# REM source reconciliation against canonical checkpoint

Audited filesystem state against commit `4c09a49cbea87ded229f009e68887aa1f019e4d8`
on 2026-10-11. No files were deleted, cleaned, or overwritten. No build or
package was run.

## Source promoted into Git

- `toolchain/native-userland/kit/packages/`: the 15 package entry scripts
  required by the already-tracked `kit/build-all-native.sh`, plus the BearSSL
  and bzip2 Ninja descriptions. The top-level runner directly invokes each
  versioned `packages/*/build-native.sh`; omitting these files made the tracked
  runner incomplete. They consume the central tracked `plans/` tree.
- `tests/kernel/check-dfsp-initialized.py`: focused regression for the
  tracked early DFSP setup in `linux/arch/myemulator2/kernel/head.S`.
- `tests/network/`: the task-work/TCP reproducer, checksum patch and captured
  qualification notes. The patch's checksum changes are already present in
  tracked `linux/arch/myemulator2/lib/checksum.c`; the README now says so and
  preserves the limit that checksum correlation does not prove the separate
  task-work fault was fixed.
- `tests/debugger/`, `tests/developer/`, `tests/qemu/`: regression and
  qualification sources for tracked `remdbg`, native binutils and QEMU/runtime
  behavior. Python bytecode and logs were excluded.
- `toolchain/tests/` source probes: targeted syscall, clock, entropy, mmap,
  OpenSSH runtime and invalid-load reproducers; these are investigation tests,
  not claims that every probed behavior passed.
- `toolchain/examples/rem-developer/` and `toolchain/examples/remdbg/`:
  buildable developer examples for the tracked toolchain and debugger.
- `toolchain/userspace/rem-admin/`, `toolchain/userspace/rem-account-base/`,
  and `tools/userspace/rem-admin/r4-rootfs-audit.sh`: the R4 dedicated-sudo,
  account and rootfs-audit source described by the retained R4 qualification
  handoff. These are historical R4 inputs; their test credentials are not the
  locked account defaults in the current `distribution/` profile.
- `tools/rem-transfer/` source and its desktop/REM qualification scripts,
  excluding its staged rsync executable; `tools/rem-make-diagnostics/` bounded
  Make/OOM capture scripts and current instructions; and `tools/remfetch/`
  source. The old allocation tracing discriminator and R4 toolchain installer
  remain excluded as superseded/historical.

The tracked package plans, REM compiler tests, Linux/QEMU overlays, Android
ARM64 build scripts and distribution tree were already present at the
checkpoint. Android build scripts include slirp/user networking. No unique
kernel or QEMU fix was found only in the older copied source trees examined.
The authoritative physical Fold launcher capture was added after the initial
inventory and is reconciled in the section below.

## Classification of remaining untracked material

Remaining untracked files are retained in place and classified below. This
inventory is by path family; nested trees can contain both source copies and
generated output, so the family-specific notes identify which material was
compared or promoted.

### A — required source

Promoted and tracked in this reconciliation as listed above. Existing tracked
`distribution/`, `linux/`, `qemu/`, `toolchain/native-userland/plans/`,
`toolchain/chibicc-rem`, and `toolchain/remdbg` remain canonical.

### B — recoverable source copies/forks

- `.rem-phone-deploy-20260925/qemu-android-src/`: older QEMU v9.2.0 overlay
  copy. Compared machine/block and target CPU files with tracked `qemu/`; it
  lacks later block flush, clock/boot metadata and runtime fixes. Not promoted.
- `.linux-build/emacs-r15-readv-trace/qemu-src/` and `.qemu-upstream/`:
  QEMU v9.2.0 copies with diagnostic/build-local deltas. Tracked QEMU source
  is the current integration point; no required later fix was found in these
  copies.
- `.rem-memory-scale-20260925/linux-6.12.1/` and Linux copies under
  `.rem-phone-deploy-20260925/`: older upstream-plus-overlay snapshots.
  Compared MyEmulator2 head, setup, syscall, time, process and MM source with
  tracked `linux/`; they predate the tracked integration and were not copied.
- `.rem-native-assembly-build/source/make-4.4.1-rem-fixed/`: divergent GNU Make
  experiment. It has a direct directory-enumeration change absent from the
  tracked patch but has reverted the tracked heap-backed glob fix; no single
  coherent, fully qualified source revision was established. It is retained
  as an unverified fork and was not promoted. The tracked
  `toolchain/native-userland/patches/make-rem.patch` keeps the identified
  config, glob, implicit-rule, prototype and spawn fixes.
- `.rem-native-assembly-build/` logs and other dated `.rem-*` roots can contain
  source snapshots alongside build state; no whole directory is suitable for
  wholesale Git addition.
- `android/bearssl-*` and `android/native-userland-*` contain older corrective
  package payloads/installers. Their package scripts were compared with the
  canonical native-userland runner and central plans; they are superseded
  candidates. Earlier `android/r4/` launcher copies use `128M` and
  `-icount`; the separate `fold-active-20261011` capture is authoritative
  and is reconciled below.
- `toolchain/native-userland/kit/bootstrap/rem-object-resume.sh`, the
  alternate musl build script, and `build-userspace-samu.sh` are unintegrated
  older mechanisms. The first two are superseded custom resume attempts; the
  last duplicates the tracked top-level package sequence. They were not
  promoted.
- `tools/rempkg/` and `tests/rempkg/` are an unintegrated package-manager
  prototype/test suite. The current package format and assembler are tracked
  under `docs/REM-PACKAGE-FORMAT.md` and `distribution/`; the prototype is not
  used by the active image definition.
- `toolchain/userspace/rem-sysv/`, BusyBox user fragments and
  `tests/userspace/test-rem-sysv.sh` are the old BusyBox-backed SysV image
  prototype. They conflict with the tracked BusyBox-free `distribution/` and
  are intentionally not promoted.
- `tools/rem-flight-image/` untracked manifests, overlays, tuples, staging and
  old packaging helpers are the September flight-image candidate. Their
  source/image definition is superseded by tracked `distribution/`; staged
  binaries and rootfs overlays are not canonical source.
- `docs/REM_SAMURAI_HANDOVER.md` and the Samurai handoff history describe
  earlier desktop/guest states; the current tracked status and first-edge
  update now include the later physical Fold failures. The remaining untracked
  `docs/CODEX-{1,2,6}-HANDOFF.md` and `CODEX-6-REM32-FIX-HANDOFF.md` are
  session handoffs whose relevant source recovery is consolidated in tracked
  `REM_SOURCE_RECOVERY_20261011.md` and this audit.
- Untracked `docs/REM_*` guides not promoted here are September flight/UAT
  instructions, old RC3/R4 deployment handoffs, or duplicate platform,
  workstation, package, network, user-management and Yash documentation.
  Specifically: `REM-BUILD-ARTEFACT-INVENTORY`, `REM-CLEANUP-REPORT`,
  `REM_BACKUP_WORKFLOW`, `REM_BUILD_PROVENANCE`, `REM_BUILD_WORKFLOW`,
  `REM_CHEATSHEET`, `REM_CONFIGURATION`, `REM_C_PROGRAMMING`,
  `REM_DEBUGGER_USER_GUIDE`, `REM_DEVELOPER_ENVIRONMENT`, `REM_DEVELOPER_GUIDE`,
  `REM_DOCUMENTATION`, `REM_FILESYSTEM`, `REM_FLIGHT_CANDIDATE_20260928_QUALIFICATION`,
  `REM_FLIGHT_UAT_CHECKLIST`, `REM_INIT_SYSTEM`,
  `REM_KILO_TEXT_INSERTION_INVESTIGATION`, `REM_NETWORK_CLIENTS`,
  `REM_OFFLINE_WORKFLOW`, `REM_PLATFORM`, `REM_PLATFORM_AUDIT`,
  `REM_R4_ADMIN_HANDOFF`, `REM_R4_TERMUX_STABILITY`,
  `REM_RC3_ASSEMBLER_HANDOFF`, `REM_RC3_INTEGRATION_PREP`,
  `REM_RC3_UAT_RUNBOOK`, `REM_SOFTWARE_INVENTORY`, `REM_SOURCE_BACKUP`,
  `REM_TERMINAL_USABILITY`, `REM_UAT_PLAN`, `REM_USER_MANAGEMENT`,
  `REM_WORKSTATION_OVERLAY`, and `REM_YASH_PROFILE`. Their candidate-specific
  facts were compared with the tracked architecture, deployment, package,
  debugging and source-recovery documents; no additional current source fix
  was found that should be copied from them.
- `docs/PLATFORM-VALIDATION.md` is an older multi-platform package validation
  procedure for the pre-distribution candidate and does not specify a newer
  source change.
- `docs/REM_Debugger_User_Guide.pdf` and `docs/REM_remdbg_User_Guide.pdf` are
  generated PDF copies of documentation; source examples and current debugger
  integration notes are tracked separately.
- `.gdbinit` references the temporary top-level `scripts/` symlink into
  `/tmp/rem-android-qemu-current-src`; it is not a portable repository
  configuration. `Makefile.prereqs` is configure-generated.
- `tools/userspace/install-rem-sysv.sh`, BusyBox fragment mergers and
  `tools/userspace/write-rem-release.py` are old BusyBox/release-candidate
  staging helpers. The active desktop rootfs definition is the tracked
  `distribution/` assembler. Untracked user manuals are promoted;
  release-only helper copies are not.

### C — build output, cache, or generated material

- Top-level Meson/Ninja output: `build.ninja`, `build.ninja.stamp`,
  `compile_commands.json`, `config-host.h`, `config-host.mak`,
  `config-meson.*`, `config.log`, `config.status`, `meson-info/`,
  `meson-logs/`, `meson-private/`, generated `myemulator32-*config*`,
  `linux-headers/`, `openbsd-compat/`, `pc-bios/`, `subprojects/`, generated
  `scripts`/`python` links, `pyvenv/`, Python caches, `free.o`, `boot.bin`,
  `myemulator.img`, and `idle-wakeup-stress-qemu.log`.
- `.rem-flight-build-20260924/`, `.rem-memory-scale-20260925/`,
  `.rem-native-assembly-build/`, `.rem-native-assembly-test*`,
  `.rem-phone-deploy-20260925/`, `.rem-tty-20260925/`, and `.r4-admin-test/`
  are retained scratch/build/qualification roots. They are narrowly ignored;
  nothing was removed.
- `tools/rem-flight-image/staging/` contains extracted application trees,
  generated filesystem overlays and binaries; only source-level overlay
  definitions already covered by `distribution/` are canonical.
- top-level `_tmp_rem-r4-*` files are extracted image files, not templates.
- `docs/config` and `tests/{avocado,data}` are symlinks into a `/tmp` QEMU
  source snapshot and are not portable repository inputs.
- `openbsd-compat/`, `pc-bios/`, `subprojects/`, `pyvenv/`,
  `tests/qemu-iotests/`, and `tests/tcg/` are configure-created copies,
  symlinks, dependency builds or generated configuration from the temporary
  QEMU build. Canonical upstream inputs are under tracked `qemu/`.
- `meson_1.3.2-1ubuntu1_all.deb` and
  `pkg-config_1.8.1-2build1_amd64.deb` are downloaded host packages.

### D — release or deployment artifacts

- Top-level `REM-FLIGHT*` trees/archives/checksums and `REM-0.1-UAT-RELEASE/`.
- `android/*20261009*`, `android/*20261010*`, `android/r4/` package payloads,
  extracted payload trees, staged QEMU/Samurai/compiler binaries and checksums.
- `tools/rem-flight-image/staging/**` binaries/archives, the staged rsync
  executable, downloaded `.deb` files, and copied rootfs/QEMU/kernel images.
- `zivA8YFf` is an unidentified zip archive retained for manual provenance
  review; it was not treated as source.

### E — duplicate, obsolete, or superseded

- `toolchain/userspace/rem-sysv/` and BusyBox configs conflict with the
  canonical BusyBox-free distribution.
- Earlier `android/r4/` launcher copies use stale 128 MiB/`-icount`
  arguments. The later authoritative physical capture at
  `android/r4/fold-active-20261011/` supersedes the earlier audit finding;
  the canonical implementation is `android/fold-remctl/`.
- Old handoff docs under `docs/CODEX-*`, old candidate/UAT instructions and
  duplicated flight manuals were retained untracked where they were historical
  rather than current source-of-truth documentation. Relevant recovery and
  qualification findings are captured in tracked docs, including this audit.
- The standalone `Makefile.prereqs` and root object/test scratch files are
  experiment residue, not release definitions.

## Physical Fold launcher reconciliation (2026-10-11)

The authoritative physical capture is retained byte-for-byte at
`android/r4/fold-active-20261011/remctl` and `rem`. The adjacent
`fold-active-20261011.txt` is deployment provenance (capture time, status,
and artifact hashes), not executable source. Its active QEMU path is
`$HOME/rem/diag/qemu-system-myemulator32.r15-diag`; the captured launcher
also checks the base QEMU at `$HOME/rem/qemu-system-myemulator32`.

`android/fold-remctl/remctl` and `rem` are the canonical source reconciled
from that capture. Compared with the tracked generic launchers, the Fold
launcher adds managed-process identity, single-writer/rootfs checks, console
socket management, safe stale-state recovery, and base/active QEMU hash gates.
It preserves `512M`, the current block device, user/slirp host forwarding on
127.0.0.1:2304, socket console handling, and no `-icount`. The exact active
launcher behavior was retained; the only behavior-level source adjustment is
that `REM_STATE` uses Bash parameter expansion rather than an external
`dirname`.

The captured QEMU hashes remain evidence only. Canonical `remctl` obtains its
expected hashes from `$HOME/rem/.remctl/qemu-sha256`, parses the two named
records strictly, and fails closed when the file is missing or malformed.
`android/fold-remctl/write-qemu-sha256.sh` generates that file from the exact
release base and active QEMU binaries using atomic replacement. Thus release
artifact hashes are set by deployment/package generation, while launch and
recovery continue to enforce integrity.

## Current source verification

- The failed wall-clock kernel's PID 1 exit was traced in the tracked recovery
  record to a test initramfs configuration leaking into the shared build tree;
  tracked kernel test harnesses restore temporary configuration and the normal
  Linux build rejects an embedded test initramfs unless explicitly opted in.
- Current syscall/process/exec, memory, block, serial, fault/ptrace, QEMU r15,
  RAM/boot metadata, and wall-clock changes are present in tracked overlays.
- The chibicc submodule is pinned by the tracked gitlink and the checkout is
  at that commit. Small aggregate-return and caller/ABI regressions are in the
  tracked native-userland tests.
- The current Android ARM64 scripts are tracked and enable slirp. The old
  candidate archives are not evidence of the current build definition.
- `docs/REM_SAMURAI_STATUS.md` and the Fold-first-edge handoff now distinguish
  desktop success from the later physical Fold fault report; no Samurai binary
  hash is represented as Fold-qualified.

## Ignore policy

`.gitignore` adds anchored patterns for known top-level generated Meson/Ninja
files and the exact dated scratch/release roots found in this audit. It does
not ignore general `tests/`, `toolchain/`, `tools/`, `android/`, or `docs/`
source trees. All ignored artifacts remain present on disk.
