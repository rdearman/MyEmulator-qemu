# Fold deployment checklist

## Host package checks

- [x] Android QEMU is AArch64 PIE with `/system/bin/linker64` interpreter.
- [x] QEMU hash matches the recorded Android ARM64 artifact.
- [x] REM32 kernel and 10 GiB ext4 root disk are included.
- [x] Bundled libraries, relative launcher paths, executable modes, and
      QEMU arguments were inspected.
- [x] Package SHA256SUMS and archive checksum verify.
- [x] Launch, `check`, backup, and restore shell syntax verified.
- [x] Archive compressed data verified; archive size is approximately 201 MiB.

## On the Fold

- [ ] Copy the archive and `.sha256` file to the Fold.
- [ ] Extract it under Termux private `$HOME/rem`, not shared `Download`.
- [ ] Run `~/rem/rem check`; confirm free space is adequate.
- [ ] Run `~/rem/rem` and reach the REM interactive shell.
- [ ] Create a file, shut down normally, relaunch, and read it back.
- [ ] Run `~/rem/rem backup "$HOME/rem-backup-YYYYMMDD"` and copy the
      backup off the phone.

Detailed steps and recovery instructions are in
[`TERMUX_DEPLOYMENT.md`](TERMUX_DEPLOYMENT.md).
