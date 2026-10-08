# REM32 Fold update 20261008 v2

**Qualified in actual REM/QEMU; final archive installation and rollback PASS.**

Archive: `/tmp/REM-FOLD-update-20261008-v2.tar.gz`

Size: **4,713,486 bytes**

SHA-256:
`ab19349f0fe651df40cbad970f3dce371a2850ecfee44d60c03741dd9eb95d1b`

Sidecars: the archive path plus `.sha256` and `.manifest.txt`. The archive
contains its own checksummed `MANIFEST.txt`, installer and guest verifier.
Only this one small archive is needed; do not transfer another rootfs/source tree.

## Actual execution and installation evidence

`docs/REM_FOLD_V2_EVIDENCE_20261008/` contains the full normalized serial,
compiler/Samurai logs, release-input verification and final-archive installer
regression. Original run `foldv2` exited 0; guest result was `REM_QA_DONE|0`.
The harness stopped QEMU PID 1790915 and timeout PID 1790913 after guest success.
Their individual wait statuses were suppressed by the original harness; the
guest result and actual markers, not a guessed QEMU exit code, prove success.

Observed actual markers include all three seed/stage small-return checks,
`COMPILER_REQUIRED_PORTING_TESTS_OK`, `COMPILER_REQUIRED_SELFREBUILD_OK`,
all six Samurai markers, and `REM_UPDATE_VERIFY_OK`. Both GCC/chibicc aggregate
interop directions passed. No user fault or kernel panic occurred.

Two normalized archives were byte-identical; extracted payload hashes and the
archive SHA-256 verified. Installation tests ran against the **actual final
archive's extracted contents**, on a disposable copy of the stopped 10 GiB
Fold simulation. PASS: local parser changes preserved; idempotent install;
post-build reinstall; exact original compiler/source rollback; removal of
newly built Samurai; local source/data preserved; compiler conflicts, unknown
library/helper baselines, corruption, missing payload/kernel and wrong hashes
rejected without image/kernel changes.

The requested last independent test repeated the full install/reinstall/
post-build rollback/safety suite from this exact archive and passed, exit 0.
The existing tracked-diff fingerprint, unrelated Git status entries and release
archive bytes were unchanged; only the intentional last-test evidence was added.

The update uses the verified seed
`6b28c165cb94b7411a63b7e74932845b11b8e30ea2bd6882b9c7bf1348a2ac44`
and kernel
`c8b394e333fdc8ad3d399e10a3723daad194dfbfbb56e3be74a573fa3d179782`.
The full run used the installed image with the deliberately retained local
parser edit, not an exact-clean-source-only baseline.

## Transfer and install

Copy/download the single archive above into Termux's `~/rem`. Use the existing
transfer route; no new download URL or full distribution is required.
With Fold QEMU stopped, paste:

```sh
cd ~/rem &&
printf '%s\n' 'ab19349f0fe651df40cbad970f3dce371a2850ecfee44d60c03741dd9eb95d1b  REM-FOLD-update-20261008-v2.tar.gz' | sha256sum -c - &&
tar xzf REM-FOLD-update-20261008-v2.tar.gz &&
sh rem-update-20261008-v2/install.sh install &&
sh rem-update-20261008-v2/install.sh verify
```

Target is the existing **10 GiB `~/rem/rootfs.ext4`**, never `~/remvm/rootfs.ext4`.
No resizing, formatting or replacement occurs. The kernel defaults to
`~/rem/vmlinux`; `REM_KERNEL` can specify a different actual kernel path.
Termux requires the existing e2fsprogs/debugfs tools.

Boot REM normally, log in as root and run the single verification/build command:

```sh
sh /home/dev/rem-update-20261008/verify.sh full
```

This rebuilds from the Fold's preserved sources using the qualified seed,
qualifies both native stages, then installs the final compiler and builds/runs
Samurai. It is not a request for manual diagnostics or source editing.
Final success is `REM_UPDATE_VERIFY_OK`; detailed logs are under
`/home/dev/rem-native-userland/`.

## Recovery

Backups: `~/rem/update-20261008-v2-backup`; log: `~/rem/update-20261008-v2.log`.
The original update's backup directory is not reused or removed.
If preflight rejects an unsupported baseline, image/kernel bytes were not
changed: preserve the log and do not force an overwrite or replace the image.
If a later install/build fails, stop QEMU and restore the pre-v2 state:

```sh
cd ~/rem && sh rem-update-20261008-v2/install.sh rollback
```

Rollback restores originals and their metadata, including the original compiler/
Samurai or their original absence, even after post-build reinstall. Other guest
data and local sources remain intact. Rollback returns the previous state,
not a newly qualified compiler. The log/archived backup directory is retained.

Source/evidence checkpoint: local
`664948ab42bc5d59fd4bd0d44edb5446070829ae`,
published `18e7cdc521d89ebb092a8dd7d1de8aba79df340c` on
`rem-fold-update-20261008-v2-checkpoint`. Original main ancestry cannot be pushed
because it contains unrelated oversized recovered files; it was not rewritten.
The task-only branch preserves the verified source/docs without those ancestors.
