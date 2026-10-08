REM Fold update 20261008 v2
=========================

This replaces the earlier update, not the rootfs. Stop QEMU before installation.
Target: $HOME/rem/rootfs.ext4 (10 GiB); never the obsolete remvm image.
Kernel: vmlinux beside that image; REM_KERNEL can override its exact location.
Termux needs the e2fsprogs/debugfs already used for the first update.
No patch, awk, Python, native objdump, resize or formatting is required.

From Termux, after checking the archive SHA-256 against the adjacent manifest:
  cd ~/rem &&
  tar xzf REM-FOLD-update-20261008-v2.tar.gz &&
  sh rem-update-20261008-v2/install.sh install &&
  sh rem-update-20261008-v2/install.sh verify

The installer preflights BOTH compiler files before changing the image.
It also preflights every other target against its installed/new/known-old baseline.
Missing required files or unknown local helper/library/Samurai changes abort
before any image or kernel changes; local source is never silently overwritten.
It merges exact hunks independently, accepting already-applied Fold fixes and
preserving other local source edits. Ambiguous or conflicting changes abort
without applying half an ABI update. Backups and log:
  ~/rem/update-20261008-v2-backup
  ~/rem/update-20261008-v2.log
The original compiler and Samurai are backed up too: qualification later
replaces them inside REM. The original update's backups remain untouched.
Reinstalling after native qualification keeps the original pre-update backups,
including the recorded absence of a Samurai binary that was first built later.

Boot REM normally, log in as root, then run:
  sh /home/dev/rem-update-20261008/verify.sh full
This tests the new explicit bootstrap seed, rebuilds the compiler twice, tests
each stage's 8-byte struct return, runs the whole required compiler suite, then
builds/runs Samurai. It takes hours. The existing selfbuilt compiler is only
replaced after the final candidate passes the complete compiler suite.
Success: COMPILER_REQUIRED_PORTING_TESTS_OK,
COMPILER_REQUIRED_SELFREBUILD_OK, SAMURAI_NATIVE_EXECUTION_OK and
REM_UPDATE_VERIFY_OK. Logs: rem-update-compiler.log and rem-update-samurai.log
under /home/dev/rem-native-userland.

Rollback, from Termux with QEMU stopped:
  cd ~/rem
  sh rem-update-20261008-v2/install.sh rollback
This restores the exact pre-v2 source files, compiler, Samurai and kernel,
and removes installer-created files. Other sources and guest data are kept.

Host regression command (uses a disposable copy, never the supplied image):
  bash android/fold-update-20261008/test-install.sh IMAGE PACKAGE_DIRECTORY
This covers exact source rollback, preservation of local source/data, restoration
of rebuilt existing native runtimes and removal of newly built runtimes, even
after a post-build reinstall. Corrupt/missing/wrong-hash payloads, missing kernel,
obsolete/small images and unsupported installed baselines are rejected without
changing image or kernel bytes.
Host-only package marker/checksum tests (not native qualification):
  bash android/fold-update-20261008/test-package.sh VERIFIED_REM_COMPILER

Host package recipe (canonical repository only):
  bash android/fold-update-20261008/build-seed.sh /tmp/rem-fold-update-seed
  CHIBICC_SEED=VERIFIED_REM_COMPILER OUT=/tmp/rem-update-20261008-v2 \
    bash android/fold-update-20261008/make-package.sh
Build the seed with the fixed cross-GCC and flight-kit-runtime musl, using
-DCHIBICC_REM for all ten compiler translation units; execute it in REM before
packaging. The package does not rely on whichever old compiler the Fold has.
The package and installer regression use the pinned original kit baseline,
not the current Git HEAD. BASE_PATCH can supply that same verified baseline.
OUT must be absent or empty; existing package/evidence directories are preserved.
The package contains a checksummed MANIFEST.txt with baselines and payload paths.
Set QUALIFICATION_LOG to the completed full REM serial log for the final release:
all seed/stage, compiler, Samurai, full verification and guest exit markers are
required, and fault/failed-exit logs are rejected. Without it the manifest clearly
labels the output as an unqualified candidate, not ready for transfer.
After actual REM qualification and final installer regression, create the release:
  bash android/fold-update-20261008/archive-package.sh PACKAGE_DIRECTORY \
    /tmp/REM-FOLD-update-20261008-v2.tar.gz
This refuses pending packages, changed tested seed/kernel binaries and existing
outputs. Two normalized archives must be byte-identical; extracted payload hashes
and the final archive SHA-256 are verified. Adjacent .sha256 and .manifest.txt
files contain the transfer checksum, byte size and qualification report.
The final delivery must provide the literal archive SHA-256 in a ready-to-paste
verification/install block; a single archive download is sufficient. Do not use
an old archive hash, an unqualified candidate, or a synthetic host-test archive.
