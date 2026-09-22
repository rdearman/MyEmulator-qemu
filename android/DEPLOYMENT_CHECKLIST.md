# REM Android deployment checklist

This checklist deliberately separates build artifacts from verified release
artifacts. The deployment package contains QEMU and guest-image placeholders;
the current staged root filesystem is never copied into it.

## Available in this clone

- [x] Android ARM64 `qemu-system-myemulator32`
- [x] Android interpreter identified as `/system/bin/linker64`
- [x] QEMU dependencies identified: `libz.so.1`, `libm.so`, `libglib-2.0.so.0`, `libc.so`
- [x] Kernel and rootfs placeholders with installation instructions
- [x] Termux launcher with serial stdio and persistent guest image
- [x] Packaging script and package validator
- [x] Termux installation and restart instructions
- [ ] Galaxy Fold execution and persistence test

## Required from the GCC/Linux agent

- [ ] Corrected REM Linux ext4 image containing `/bin/busybox`, `/sbin/init`,
      and `/bin/sh`
- [ ] Host QEMU boot reaching the interactive `#` shell
- [ ] Host shell command execution test
- [ ] Host persistence test: create a file, restart QEMU with the same image,
      and read it back
- [ ] Image path, byte size, and SHA-256 checksum

## Package steps

```sh
cd /home/rick/Development/Active/REM-android
rm -rf .android-build/rem-deployment
ANDROID_TERMUX_SYSROOT=/tmp/rem-termux-sysroot \
  ./android/package-rem.sh \
    .android-build/ndk-test/qemu-system-myemulator32 \
    .android-build/rem-deployment
./android/validate-package.sh .android-build/rem-deployment
(
  cd .android-build
  zip -qr ../REM-android-arm64.zip rem-deployment
)
sha256sum REM-android-arm64.zip
unzip -l REM-android-arm64.zip
```

Install the verified kernel and root filesystem on the phone only after
extracting the package, as described in `TERMUX_DEPLOYMENT.md`.

Do not mark the release complete until the host tests and the physical Fold
test both pass. Android boot remains unverified until the user runs it.
