# REM on a Galaxy Fold with Termux

The ready-to-transfer package is [`REM-FLIGHT-ANDROID-MVP.zip`](../REM-FLIGHT-ANDROID-MVP.zip).
Its adjacent `.sha256` file gives the archive checksum. The package contains
Android ARM64 QEMU, the matching REM32 kernel, bundled Termux runtime
libraries, a persistent 10 GiB ext4 guest disk, and the `rem` command.

## Transfer and launch

Copy both archive and checksum file to the Fold's `Download` directory. In
Termux, extract into its private home (not shared storage):

```sh
termux-setup-storage
pkg install unzip
cd "$HOME/storage/downloads"
sha256sum -c REM-FLIGHT-ANDROID-MVP.zip.sha256
mkdir "$HOME/rem"
unzip -q "$HOME/storage/downloads/REM-FLIGHT-ANDROID-MVP.zip" -d "$HOME/rem"
mv "$HOME/rem/REM-FLIGHT-ANDROID-MVP"/* "$HOME/rem/"
rmdir "$HOME/rem/REM-FLIGHT-ANDROID-MVP"
chmod 700 "$HOME/rem/rem" "$HOME/rem/scripts/launch-rem.sh" \
  "$HOME/rem/qemu/qemu-system-myemulator32"
chmod 755 "$HOME"/rem/qemu/lib/*
~/rem/rem
```

After setup, the normal workflow is to open Termux and run `~/rem/rem`. It
uses machine `myemulator32`, 128 MiB RAM, the packaged kernel, writable
`rootfs/rem-rootfs.ext4`, and an interactive serial console. The launcher
passes further QEMU options through to QEMU. Networking is not required.

## Preflight and recovery

Run `~/rem/rem check` before travel. It checks QEMU, kernel, disk, execution
permissions, writable storage, and reports free space and the QEMU hash.
Confirm the reported free space is adequate for the guest's 10 GiB disk and
normal Android use.

Shut REM down normally before backing up its disk:

```sh
~/rem/rem backup "$HOME/rem-backup-20260927"
```

The backup path must be new; the disk copy can use up to 10 GiB. Copy the
backup off the Fold. To recover, with REM shut down, run:

```sh
~/rem/rem restore "$HOME/rem-backup-20260927"
```

Restore verifies the backup disk checksum and replaces the current writable
disk. The guest disk is mutable, so its pristine-package checksum will no
longer match after guest writes. Never back it up while REM is running.

## Verification status

Host-side checks verified the archive, package manifest, AArch64 ELF
architecture, `/system/bin/linker64` interpreter, dependencies, kernel and
ext4 image formats, launch command, and scripts. The QEMU SHA-256 is
`a6da714c1643a22e6d295967704e762369ac04dc7bca05760972e0e4b0c9e3ac`.
The NDK build targets Android API 28. Execution, boot, and a write/reboot/read
persistence check on the physical Galaxy Fold remain pending.
