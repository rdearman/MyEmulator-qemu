# REM QEMU on Termux (Android ARM64)

This directory contains the Android-specific build and launch workflow. It
does not alter the normal Linux Mint QEMU build. The Android runtime target is
`myemulator32-softmmu`; its executable is `qemu-system-myemulator32` and its
machine name is `myemulator32`. This is the current 32-bit REM CPU used by the
Linux port. The older firmware target remains `myemulator-softmmu` /
`myemulator`.

## Build on the Galaxy Fold

Install Termux from F-Droid or GitHub, then install the build dependencies:

```sh
pkg update
pkg install clang git make meson ninja python pkg-config glib
```

Copy this repository (or an archive containing it) into Termux storage. The
build script clones the pinned upstream QEMU source into a separate temporary
directory, applies the tracked REM overlay, and writes the build tree under
`$PREFIX/var/tmp/rem-qemu-build` by default:

```sh
cd /path/to/REM-android
JOBS=4 ./android/build-termux-qemu.sh
```

The script builds only the two REM system targets, without graphical/audio,
network, documentation, or plugin features. It prints the resulting executable
and a `file`/`readelf` dependency report. Termux supplies the ARM64 dynamic
linker and GLib shared library; no root access is required.

## Deployment layout

Place these files in one Termux directory (the launch script can use another
directory via `REM_HOME`):

```text
rem/
  qemu-system-myemulator32
  vmlinux
  rootfs.ext4
  launch-rem.sh
```

`vmlinux` must be the current REM Linux `myemulator2` kernel, and
`rootfs.ext4` must be an ext4 image containing `/sbin/init` and the BusyBox
interactive shell. The image is opened read-write and is intentionally not
copied on launch, so files created by the guest persist across runs.

```sh
chmod 700 launch-rem.sh qemu-system-myemulator32
./launch-rem.sh
```

The launcher uses `-nographic`, disables the QEMU monitor, attaches the guest
serial console to the Termux terminal, and connects the disk through the
REM-specific `myemulator2-disk` backend. Exit the guest using its normal halt
command or the QEMU escape sequence shown by QEMU; do not use Android’s
back-button force-stop while the guest is writing the filesystem.

## Cross-build from Linux

The same target can be cross-compiled with the Android NDK and ARM64 Termux
runtime packages:

```sh
ANDROID_NDK=/home/rick/Android/sdk/ndk/26.1.10909125 \
  QEMU_SOURCE=/tmp/rem-android-qemu-v9.2.0 \
  ./android/build-ndk-qemu.sh
```

The script downloads pinned ARM64 Termux packages into the ignored
`.android-build/termux-sysroot`, applies `qemu-android.patch`, and builds a
PIE ELF whose interpreter is `/system/bin/linker64`. The package intentionally
disables QEMU graphics, audio, networking, and optional host backends; the
runtime dependencies are GLib and zlib from Termux. The host cannot execute
this ARM64 binary, so final execution still requires the Galaxy Fold.

## Verification status

Host-side REM CPU, Linux process, ext4 persistence, and interactive ext4 shell
tests are documented in `docs/MYEMULATOR2_LINUX_PROGRESS.md`. A binary built by
this workflow is not considered Android-verified until it has been copied to a
real ARM64 device and `launch-rem.sh` has produced the guest prompt and a
create/read persistence check. The Android build and phone test results must be
reported separately.
