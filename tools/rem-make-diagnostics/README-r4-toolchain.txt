R4 corrected REM chibicc + Samurai update (staged, not Fold-tested)
===================================================================

This package contains the corrected REM-native chibicc and the qualified
NO_POSIX_SPAWN samu artifact. It does not contain or modify a kernel, QEMU,
rootfs, or user project. Do not install on the Fold until Termux is stable.

From inside REM, unpack the archive in a writable directory and run:

    bash ./install-r4-toolchain-update.sh

The installer verifies the exact hashes before changing anything, saves
existing ~/.local/bin/chibicc and ~/.local/bin/samu under
~/.local/share/rem/toolchain-backups/, then installs the pair in ~/.local/bin.
The existing PATH already places this directory before system tools.

Artifact hashes and qualification details are in PROVENANCE.txt. Qualification
was performed by #2 on a disposable guest; this package itself has only been
host-validated and extracted/hash-checked. `samu -h` follows upstream behavior
and returns 2; the recorded actual/dry/incremental build checks returned 0.
