# REM Android Deployment Handoff

## Workspace and current state

- Workspace: `/home/rick/Development/Active/REM-android`
- Branch: `android-arm64`
- Latest commit: `4f9c169 Harden Android deployment validation`
- Android execution on a physical Galaxy Fold remains unverified.
- No release ZIP has been produced from the failing root filesystem.
- Do not modify `/home/rick/Development/Active/MyEmulator-qemu`,
  `/home/rick/Development/Active/REM-emacs`, or the CPU, Linux kernel, GCC,
  musl, or BusyBox implementations.
- Do not push commits unless explicitly requested.

The latest documentation-only commits also include:

```text
2ab1e34 Document MyEmulator2 32-bit assembly programming
b123e38 Add REM programmer guide for Termux users
```

## Android executable and required inputs

The existing Android ARM64 QEMU executable is:

```text
.android-build/ndk-test/qemu-system-myemulator32
```

It is an ARM64 PIE using `/system/bin/linker64`. Its identified runtime
dependencies are `libz.so.1`, `libm.so`, `libglib-2.0.so.0`, and `libc.so`.
The Android build script is `android/build-ndk-qemu.sh`; rebuild only if the
existing executable is unavailable or fails validation.

The required guest kernel is:

```text
.android-build/deployment-input/vmlinux
```

The required corrected root filesystem must come from the GCC/Linux agent:

```text
/tmp/rem-verified-rootfs.ext4
```

After the GCC/Linux agent confirms its shell and persistence tests, copy it to:

```text
.android-build/deployment-input/rootfs.ext4
```

The currently staged rootfs has previously failed the BusyBox userspace boot
test and must not be treated as a working release artifact.

## Build, validate, and package commands

Run from the repository root:

```bash
cd /home/rick/Development/Active/REM-android
git rev-parse --show-toplevel
```

If the Android executable must be rebuilt:

```bash
ANDROID_NDK=/path/to/android-ndk \
  QEMU_SOURCE=/tmp/rem-android-qemu-v9.2.0 \
  ./android/build-ndk-qemu.sh
```

Stage the verified GCC/Linux artifacts:

```bash
install -m 0644 /tmp/rem-verified-rootfs.ext4 \
  .android-build/deployment-input/rootfs.ext4
test -x .android-build/ndk-test/qemu-system-myemulator32
test -r .android-build/deployment-input/vmlinux
test -r .android-build/deployment-input/rootfs.ext4
```

Build a deployment directory and validate it:

```bash
rm -rf .android-build/rem-deployment
ANDROID_TERMUX_SYSROOT=/tmp/rem-termux-sysroot \
  ./android/package-rem.sh \
  .android-build/ndk-test/qemu-system-myemulator32 \
  .android-build/deployment-input/vmlinux \
  .android-build/deployment-input/rootfs.ext4 \
  .android-build/rem-deployment
./android/validate-package.sh .android-build/rem-deployment
```

Create the final ZIP only after the host shell and persistence tests pass:

```bash
rm -f REM-android-arm64.zip
(
  cd .android-build
  zip -qr ../REM-android-arm64.zip rem-deployment
)
unzip -l REM-android-arm64.zip
sha256sum REM-android-arm64.zip
```

`android/package-rem.sh` includes only the QEMU executable, kernel, rootfs,
launcher, deployment instructions, checksums, and approved runtime libraries.
It rejects a non-empty output directory. `android/validate-package.sh` rejects
unexpected files, verifies ARM64 format, checks `/system/bin/linker64`,
checks dependencies and permissions, and verifies `SHA256SUMS`.

## Remaining integration tests

The following tests remain before a release can be called verified:

1. GCC/Linux agent confirms `/tmp/rem-verified-rootfs.ext4` is the corrected
   image and supplies its size and SHA-256.
2. Host QEMU boots that image to `/sbin/init` and an interactive BusyBox shell.
3. A shell command creates a marker, QEMU exits cleanly, and a second boot with
   the same disposable image reads the marker back.
4. The validated ZIP is transferred to the Galaxy Fold.
5. Termux launches the Android ARM64 QEMU binary and reaches the REM console.
6. A marker survives a clean Fold-side REM shutdown and restart.

Until steps 4–6 pass, do not claim that REM boots successfully on Android.

## Galaxy Fold transfer and launch

From Linux Mint, with USB debugging enabled:

```bash
cd /home/rick/Development/Active/REM-android
adb devices
adb push REM-android-arm64.zip /sdcard/Download/
```

In Termux:

```sh
termux-setup-storage
pkg update
pkg install bash coreutils unzip glib zlib
mkdir -p "$HOME/rem"
unzip -q "$HOME/storage/downloads/REM-android-arm64.zip" -d "$HOME/rem"
mv "$HOME/rem/rem-deployment"/* "$HOME/rem/"
rmdir "$HOME/rem/rem-deployment"
chmod 0755 "$HOME/rem/qemu-system-myemulator32" "$HOME/rem/launch-rem.sh"
cd "$HOME/rem"
./launch-rem.sh
```

The launcher uses:

```text
-M myemulator32
-m 16M
-kernel vmlinux
-drive file=rootfs.ext4,format=raw,if=none,id=myemulator2-disk
-nographic -monitor none -serial stdio
```

Exit using the guest's normal shutdown/exit path. Do not force-stop Termux
while the guest is writing the disk. Restart with the same persistent image:

```sh
cd "$HOME/rem"
./launch-rem.sh
```

Do not recreate or overwrite `$HOME/rem/rootfs.ext4` between launches.
