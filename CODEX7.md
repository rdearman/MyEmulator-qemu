# CODEX #7 handover — REM Fold readiness and persistence

**Updated:** 2026-09-27  
**Owner:** Codex #7 — persistence and filesystem reliability; most recent
task: read-only REM flight image readiness audit.  
**State:** Audit documentation complete. Fold deployment acceptance is not
complete. No code or image artifacts were changed during the readiness audit.

## Current result

The Fold Termux MVP archive passed host-side package checks, but the Android
QEMU binary has not been run on the physical Fold. A read-only inventory of
the packaged ext4 rootfs found Git, tar, Python 3.12 and OpenSSH client
commands, but no Yash, Kilo, chibicc, Samurai, or guest GCC at their expected
paths. Several of those tools passed narrower tests in other disposable
images; those tests do not prove that the tools are in the Fold package.

The actual developer workflow is therefore **BLOCKED for the packaged image**:
the image lacks its tested editor/compiler/build tools, Samurai has not passed
a real build, and an end-to-end source-build-sync-reboot-verify test on the
Fold has not run.

## Artifact identities

The persistence acceptance used this desktop host-QEMU tuple on disposable
copies:

| Artifact | Location | SHA-256 |
|---|---|---|
| QEMU | `.rem-phone-deploy-20260925/REM-FLIGHT/qemu/qemu-system-myemulator32` | `0154db549ad01d0bcdfd79c0dde9544266fc14c75fe7825d14942ff47a75641a` |
| Kernel | `.rem-phone-deploy-20260925/REM-FLIGHT/boot/vmlinux` | `c77547060463da37cf9730e41e799c94998ad9f6e24e2c1a3282ee3299215a63` |
| Persistence seed rootfs | `.rem-phone-deploy-20260925/REM-FLIGHT/disk/rem-rootfs.ext4` | `46604d9530953065df0dc74fea7eacf5ac21dfbebeb7194ba355f178cb91f4cd` |

The Fold archive's staged package README records Android QEMU SHA-256
`a6da714c1643a22e6d295967704e762369ac04dc7bca05760972e0e4b0c9e3ac` and
requires host checksum verification before extraction. Its physical-device
execution remains unverified. See
[`docs/REM_PHONE_DEPLOYMENT.md`](docs/REM_PHONE_DEPLOYMENT.md) and
[`android/TERMUX_DEPLOYMENT.md`](android/TERMUX_DEPLOYMENT.md).

## Persistence evidence and limits

Verified on disposable host-QEMU ext4 images:

- Tiny and multi-block files, source/header/assembly files, logs, object and
  executable fixtures survived synchronized shutdown/reboot and recovery
  with matching hashes. Executable mode 0755 persisted.
- A single-source chibicc program was compiled, executed, rebooted, and
  recovered with matching source and executable hashes.
- Offline image backup and hash verification passed. A restored copy booted;
  source files were extracted on the host and hash-checked.
- Hostile immediate QEMU termination **without sync** lost a recent file.
- The guest clean unmount/poweroff path is not proven. Tests synced data,
  exited the supervised shell, and stopped QEMU; journal recovery then ran on
  reboot. Treat this as synchronized reboot/recovery evidence, not a clean
  unmount guarantee.
- Near-full/ENOSPC behavior was not tested. Kilo/Samurai-to-chibicc builds,
  multi-source compile/link acceptance, and physical Fold persistence are
  unproven.

Use the offline image helper only when REM is stopped and the image is
detached from QEMU:

```sh
tools/rem-ext4-image.sh check /path/to/rem-rootfs.ext4
tools/rem-ext4-image.sh backup /path/to/rem-rootfs.ext4 /safe/path/rem-backup.ext4
tools/rem-ext4-image.sh restore /safe/path/rem-backup.ext4 /tmp/rem-restore.ext4
tools/rem-ext4-image.sh extract /safe/path/rem-backup.ext4 /root/project /tmp/project-recovered
```

Do not inspect or mutate an image behind a running QEMU process. Keep the
original package and a separately verified backup. Full test history and
known shutdown limitations are in
[`docs/REM_EXT4_PERSISTENCE.md`](docs/REM_EXT4_PERSISTENCE.md).

## Tool and application boundaries

| Component | Handover status |
|---|---|
| REM32 CPU/QEMU/kernel | Linux boot and host-QEMU guest workloads pass on documented tuples; this is not Fold execution evidence. Keep kernel, QEMU, rootfs and native tools matched. |
| ext4 | Synced data, reboot/recovery, host backup/restore and extraction pass on disposable images. Unsynced abrupt stop loses data; clean guest shutdown remains open. |
| chibicc | Matched tuple compiles/runs `hello.c`; larger application probes failed or stalled. The Fold image lacks the compiler. Chibicc self-compilation is blocked. |
| GCC | Workstation cross GCC built Samurai; no guest GCC is packaged. Native/self-hosted GCC is not accepted. |
| `as`, `ld`, `make` | Canonical staged variants have focused pass evidence; exact Fold package binary identities/overlay integration are not verified. A stale linker path is known to crash. |
| Samurai | Not present at checked package paths; `samu -n` comparison differs between GCC and chibicc builds; no full real build passes. |
| Yash/Kilo | Interactive/edit-save-reboot acceptance exists on separate disposable images. Neither is integrated in the inspected Fold rootfs. Yash compile-chain launch failed with SIGHUP. |
| Bash | Controlled build passes tested command and interactive input/exit paths, but job control is absent on its tested tuple. |
| Vim | Version command passes; editor startup hangs before a visible buffer. Save/readback is untested; syscall, thread/timer and terminal issues remain. |
| Git/tar | Present in rootfs inventory, but repository and archive workflows have not been qualified. |
| grep/sed | Legacy version commands passed on a disposable guest; semantics and exact packaged binaries are not qualified. |
| less/Python | Legacy probes include a `less` status-139 failure and provenance-mixed Python crashes. No controlled current-package acceptance. |
| SSH/OpenSSH | Client binaries are present, but networking is intentionally absent from this MVP; SSH is not usable/accepted. |

Detailed maps and source reports:

- [`docs/REM_FLIGHT_READINESS.md`](docs/REM_FLIGHT_READINESS.md) — Fold
  deployment matrix, critical path, deferrals.
- [`docs/REM_ARCHITECTURE64.md`](docs/REM_ARCHITECTURE64.md) — evidence-based
  architecture observations, including device acceptance, durable storage,
  and development-image manifests.
- [`docs/REM_VALIDATED_RUNTIME_TUPLES.md`](docs/REM_VALIDATED_RUNTIME_TUPLES.md)
  — pinned hosted-C tuple.
- [`docs/REM_CHIBICC_BUGLOG.md`](docs/REM_CHIBICC_BUGLOG.md) and
  [`docs/REM_APPLICATION_COMPATIBILITY_20260927.md`](docs/REM_APPLICATION_COMPATIBILITY_20260927.md)
  — compiler and application failures.
- [`docs/REM_SAMURAI_STATUS.md`](docs/REM_SAMURAI_STATUS.md),
  [`docs/REM_MAKE_FALLBACK_QUALIFICATION.md`](docs/REM_MAKE_FALLBACK_QUALIFICATION.md),
  [`docs/REM_YASH_STATUS.md`](docs/REM_YASH_STATUS.md), and
  [`docs/REM_VIM_COMPATIBILITY.md`](docs/REM_VIM_COMPATIBILITY.md) — build,
  shell, and editor acceptance boundaries.
- [`docs/REM_OFFLINE_BUILD_MANIFEST.md`](docs/REM_OFFLINE_BUILD_MANIFEST.md)
  — runtime contents and staging locations.

## Next actions and ownership

1. **Fold deployment (#5):** run archive checksum and `rem check` on the
   physical Fold; boot the exact package, record hashes, and validate storage
   space and writable ext4 behavior.
2. **Flight-image integration (#4):** stage a new package copy with the
   explicitly selected editor, compiler, assembler, linker, and build command.
   Compare its installed paths, SHA-256 hashes, and modes to the qualified
   manifests. Preserve the original package.
3. **Compiler/Samurai (#2):** resolve the current real build/runtime failure
   and demonstrate an actual project build, executable run, and rebuild using
   the tools staged in the candidate image.
4. **Yash (#3), if selected:** integrate and test the validated Yash/Kilo
   binaries on the same image instead of relying on older separate-image
   results.
5. **End-to-end acceptance:** on the target device, edit source, compile and
   link, run the executable, `sync`, stop and relaunch REM, verify file hashes,
   executable modes and execution, create an offline backup, verify it, and
   restore/boot a disposable copy. Keep a recovery copy off-device.

OpenSSH/networking is explicitly deferrable for this offline flight MVP. Vim,
Python, Git and tar are secondary unless the actual flight workflow requires
them. Do not mark any tool accepted from a different QEMU/kernel/rootfs tuple
without recording that boundary.
