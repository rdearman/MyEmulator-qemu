# REM32 Fold update 20261008 v2

**CORRECTED REAL-FOLD BASELINE QUALIFICATION: RUNNING; NOT TRANSFERABLE YET**

The prior archive below is rejected for this Fold and must not be installed.
The exact Fold input `/tmp/libc.a.fold` is present and verifies as
`0bc5770f3d1b84a97a1030f5c2911b164a375d89c2db9e4a89a4f51c4f2b3186`.
Occurrence-aware object evidence identifies the 33 changed recipe members and
the exact added syscall object; the corrected installer accepts this hash only
at the private bootstrap path and replaces the entire archive. See
`docs/REM_FOLD_REAL_LIBC_BASELINE_20261008.md`.

The exact-baseline installer tests pass. Full REM qualification is running on
a fresh stopped-image copy in `/tmp/rem-fold-real-fold-qualification.20261008`.
There is no corrected release archive or new transfer SHA-256 yet. Do not call
this candidate qualified or transfer it until all full REM markers pass and
the exact final archive passes extraction/install/rollback tests.

## Prior v2 archive — rejected for the actual Fold

The actual Fold rejected the installer for
`/home/dev/rem-native-userland/bootstrap/musl/native/libc.a`, SHA-256
`0bc5770f3d1b84a97a1030f5c2911b164a375d89c2db9e4a89a4f51c4f2b3186`.
It reported no guest or kernel files changed. The archive below passed native
REM/QEMU qualification and installation/rollback for the previous simulated
baseline, but that does not establish support for this different library.
Baseline validation must not be bypassed. Retain the archive as prior evidence.

Archive: `/tmp/REM-FOLD-update-20261008-v2.tar.gz`

Size: **4,713,486 bytes**

SHA-256:
`ab19349f0fe651df40cbad970f3dce371a2850ecfee44d60c03741dd9eb95d1b`

Sidecars: the archive path plus `.sha256` and `.manifest.txt`. The archive
contains its own checksummed `MANIFEST.txt`, installer and guest verifier.
Only this one small archive is needed; do not transfer another rootfs/source tree.

## Actual Fold rejection: identification result

The reported full `0bc5770f...` SHA-256 was not found in the available canonical
libraries, bootstrap outputs, original committed runtime, REM scratch/backups,
244 standalone image candidates, ten rootfs images inside nine distribution
archives, or the extra retained `/tmp/rem-abi-base.ext4`. One candidate was a
78-byte placeholder rather than a filesystem; one older checksum-damaged image
was inspected read-only with `debugfs -c`. No image was modified. Archive
rootfs images were extracted only to disposable sparse scratch and removed
after inspection. The existing divergent recovery directory is absent.

The known library comparison, not a comparison of the unknown Fold bytes:

| Archive | SHA-256 | Bytes | Members | Bad large-frame prologues |
| --- | --- | ---: | ---: | ---: |
| Original flight runtime | `30cff68855d3793dedd63020928e65dfc084a5aa6667ef0114b25d6d92bd0924` | 2,835,504 | 1,345 | 35 |
| Corrected qualified runtime | `269b1d6b5a15a3e2b3c4f8fe48dc0f9f8b5922e2c90937586ec7167d22a1c7c2` | 2,861,972 | 1,345 | 0 |

Both have identical member-name sets (`clone.o` changes position) and
representative ELF32 little-endian REM objects, machine `0xf2e2`. Original
bytes are retained in commit `25ab091504e0c1027c7e5efcf332aecd2e072432` and the
userspace import. Corrected bytes match the existing musl install output and
qualified payload. The recovered native musl bootstrap recipe copies the
system archive and updates members from 33 listed C files plus `native_syscall.o`; that recipe
does not identify the unknown archive or prove its provenance/compatibility.

**Required next input:** the actual Fold archive bytes, not another hash.
Its size, member inventory, object contents and provenance remain unknown.
No hash was whitelisted, no validation was relaxed, and no new package or
native qualification was started. After identification, an exact-baseline
installation/rollback and full native qualification must precede any
replacement release.

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

## Prior transfer and install procedure (blocked for the actual Fold)

Do not run the following procedure again until the unknown library is identified
and an exact-baseline replacement update is qualified. These are historical
instructions for the prior artifact, not instructions to work around rejection.

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
