# REM Linux Userspace Debugging — Handoff

Status date: 2026-09-22 (integration-regression session)
Handling agent: GitHub Copilot CLI

**SUPERSEDED — see `docs/CODEX_HANDOVER.md` for the current, authoritative
state.** That document covers the signal-handling implementation, the r14
link-register fix, the active IRQ-enable panic regression, and the
diagnosed (pre-existing, non-kernel) BusyBox SIGBUS issue. Read it first.
This file and `docs/REM_LINUX_DEBUG_HANDOVER.md` are kept for their earlier
history (Bug #6 root cause/fix, the original integration-regression
bisection) which remains accurate.

## Summary — integration regression found and fixed; shell + ext4 persistence re-verified

While preparing to integrate `emacs-cross` and `android-arm64` into
`main`, the previously-reported "verified working" BusyBox shell + ext4
persistence state was found to be **broken** at the start of this
session (immediate `Kernel panic: Attempted to kill init!`, SIGSEGV).
Root-caused via controlled bisection (see
`docs/REM_LINUX_DEBUG_HANDOVER.md`'s "UPDATE (integration regression
investigation)" section for full detail) to **two independent,
pre-existing bugs**, both now fixed and verified:

1. **`myemulator2_defconfig` had a duplicated/contradictory
   `CONFIG_BLOCK` line** — a later `# CONFIG_BLOCK is not set` silently
   overrode the earlier `CONFIG_BLOCK=y`, disabling the block/ext4
   driver on any freshly-regenerated `.config`. Fixed by deleting the
   stray line.
2. **`sync(2)` was never implemented** in
   `linux/arch/myemulator2/kernel/syscall.c` (only `fsync(fd)` existed),
   and BusyBox's `SYNC` applet wasn't even enabled in
   `toolchain/scripts/build-busybox.sh`. This meant `sync` at the shell
   was a silent no-op / "not found", so ext4 writes never actually
   reached the block device before a guest restart, even though they
   read back fine within the same boot. Fixed both.

**Verified this session** (fresh disposable images, not the previous
Bug #6 session's artifacts):
- BusyBox reaches an interactive shell via the real ext4-root boot path
  (`root=/dev/myemu0 rw init=/sbin/init`, not initramfs).
- `echo`, `pwd`, `ls`, `cat`, `mkdir -p`, file redirection all work with
  no `MYEMU_USER_FAULT`/panic.
- `sync` now succeeds; a file written+synced is confirmed via `debugfs`
  to be present in the **host-side** ext4 image immediately afterward.
- The same disk image rebooted **twice**, with two separately-created
  files, shows both intact both times — genuine, demonstrated
  cross-reboot persistence.
- `make spec-test`: PASS (6/6). `make cpu32-test`: PASS (all 3 suites).

**Not yet done:**
- The `emacs-cross`/`android-arm64` integration itself has **not
  started** — this session stopped at "prove the shell/persistence
  baseline is reliable again," per explicit instruction, before
  proceeding to signal-handling/Emacs/Android work.
- Native/self-hosted GCC compilation inside REM Linux not re-tested
  against these fixes.
- `toolchain/scripts/prepare-gcc-source.py` still uncommitted, still
  needs a hunk-split review pass (mixes verified Bug #6 patches with
  older, unrelated uncommitted edits).

## Git state

`main` branch, HEAD is `8cc3399` "Fix defconfig CONFIG_BLOCK conflict
and add missing sync(2) syscall", on top of `3b9c244` "Preserve verified
working baseline: BusyBox shell + ext4 persistence" (commits this
session's — and the prior session's leftover — kernel/QEMU/minilibc
diagnostics), on top of `b4dedb3` "Fix Bug #6" (GCC alloca/VLA fix).

Tag `baseline-busybox-working-v1` marks `3b9c244`, for rollback if the
emacs-cross/android-arm64 integration needs to be abandoned.

`origin/emacs-cross` and `origin/android-arm64` are fetched as
remote-tracking branches in this repo, ready for integration but not yet
touched. Both diverge from `main` at `e2c32b0` (one commit before the
Bug #6 fix), so integrating either will also need to bring in the Bug #6
fix and this session's two additional fixes.

**Still uncommitted** (preserved, not evaluated further this session):
- `toolchain/scripts/prepare-gcc-source.py` — see above.
- Untracked: `.busybox-build/` (Bug #6-verified busybox build dir from
  the prior session — NOT the sync-enabled one from this session, which
  lives at `/tmp/busybox-build-sync` instead, kept separate
  intentionally), `boot.bin`, `myemulator.img`, `rikmon/boot.s`
  (protected, untouched), `toolchain/examples/linux-busybox-{direct,shell-direct}.c`,
  `toolchain/scripts/test-linux-selfhosted-gcc.py`.

Protected files (`boot.bin`, `myemulator.img`, `rikmon/boot.s`) were not
touched.

## Artifacts (current, this session)

- `/tmp/rem-verified-rootfs.ext4` — **replaced** this session with a
  newly-built, fully re-verified image, SHA-256
  `6d795c31a3511b754a529e1ec27cec341146f8dd192201a6016fc6508bfa5d25`.
  The previous artifact at this path predated the `sync`/BLOCK fixes and
  is superseded.
- Kernel: `.linux-build/build/vmlinux`, Linux 6.12.1, SHA-256
  `3ce254a26180f94cf523ca403b2c485c3d9371618909958b81112603261437a6`.
  Build recipe: `rm -rf .linux-build/build && bash
  toolchain/scripts/configure-linux.sh`, then apply
  `toolchain/scripts/run-linux-ext4.sh`'s `.config` overrides
  (`INITRAMFS_SOURCE=''`, `EXT4_FS` enabled, `CMDLINE='console=ttyMY0,115200
  earlycon=myemulator2,0xf0000000 root=/dev/myemu0 rw init=/sbin/init'`),
  then `bash toolchain/scripts/build-linux.sh`.
- QEMU: `.qemu-build/qemu-system-myemulator32`, QEMU 9.2.0, SHA-256
  `5fab1e7f85e288f20eceddbe36d4b06f08c54d5e8781d26447a8ba3328010901`
  (unchanged from prior session).
- BusyBox: freshly rebuilt 1.37.0 with `SYNC` applet, linked against
  `/tmp/rem-musl-fixed`, SHA-256
  `464f7682599ebd0b5408bdadde2f3fdf8aea3741b88caba9c9146a9e9ea4e54f`
  (`/tmp/busybox-build-sync/busybox`). Build:
  `MYEMU_MUSL_PREFIX=/tmp/rem-musl-fixed MYEMU_BUSYBOX_BUILD=/tmp/busybox-build-sync
  bash toolchain/scripts/build-busybox.sh`.
- Reusable non-interactive test harness: `/tmp/rem_ext4_test.py`
  (pexpect; usage: `python3 /tmp/rem_ext4_test.py <ext4-image> "<cmd1>"
  "<cmd2>" ...`; boots with `-append` matching
  `run-linux-ext4.sh`'s cmdline directly, for scriptability).

## Recommended next action

1. Re-read `docs/REM_LINUX_DEBUG_HANDOVER.md`'s latest section for full
   bisection detail and reasoning.
2. Proceed with the `emacs-cross`/`android-arm64` integration now that
   the shell + persistence baseline is genuinely reliable again: create
   a dedicated integration branch off `3b9c244`/`8cc3399`, merge
   `origin/emacs-cross` first (replacing the temporary
   get_signal()-drop workaround in `traps.c` with the proper
   `arch_do_signal_or_restart()`/`signal.c` implementation from commit
   `0a1e979`), rebuild, and re-run this exact shell+persistence sequence
   before merging `origin/android-arm64`.
3. Split and commit `toolchain/scripts/prepare-gcc-source.py`.
4. Re-verify native/self-hosted GCC compilation with the current fixes.
