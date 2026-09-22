# REM Linux deployment to a Galaxy Fold

This procedure is intentionally split into desktop and phone steps. The
desktop cannot execute the REM CPU, and Android execution has not been
verified until the phone test below is completed.

## Desktop: obtain the guest artifacts

The Android clone currently contains the QEMU executable, but not the final
guest kernel or ext4 image. The original checkout has a development kernel at:

```text
/home/rick/Development/Active/MyEmulator-qemu/.linux-build/build/vmlinux
```

The original checkout's `myemulator.img` files are legacy MyFS images and are
not suitable as the Linux root filesystem. Do not use them with this launcher.

After obtaining tested guest artifacts from the Linux build work, keep them
outside this package until the phone installation step. The package builder
intentionally does not consume `.android-build/deployment-input/*`.

```sh
mkdir -p /tmp/rem-verified-guest
install -m 0644 /path/to/verified/vmlinux /tmp/rem-verified-guest/vmlinux
install -m 0644 /path/to/verified/rem-rootfs.ext4 /tmp/rem-verified-guest/rootfs.ext4
```

Confirm the image is ext4 before packaging:

```sh
file /tmp/rem-verified-guest/rootfs.ext4
dumpe2fs -h /tmp/rem-verified-guest/rootfs.ext4 | sed -n '1,12p'
```

## Desktop: build and validate the deployment archive

Use the ARM64 binary produced by the committed NDK build. The package script
also bundles GLib, zlib, PCRE2, libffi-related Termux support, and iconv when
the cross-build sysroot is available:

```sh
cd /home/rick/Development/Active/REM-android
rm -rf .android-build/rem-deployment REM-android-arm64.zip
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

The package uses the `myemulator32` machine, 16 MiB guest RAM, the REM block
backend ID `myemulator2-disk`, an interactive serial console, and the same
rootfs file on every launch. The rootfs is not copied or recreated by the
launcher, so guest-created files persist across restarts.

## Desktop to phone transfer

Enable USB debugging on the Fold, connect it, and verify the device from the
Linux desktop:

```sh
adb devices
adb push REM-android-arm64.zip /sdcard/Download/
```

In Termux, grant storage access once and extract the archive into the Termux
home directory:

```sh
termux-setup-storage
pkg update
pkg install bash coreutils tar unzip glib zlib
mkdir -p "$HOME/rem"
unzip -q "$HOME/storage/downloads/REM-android-arm64.zip" -d "$HOME/rem"
mv "$HOME/rem/rem-deployment"/* "$HOME/rem/"
rmdir "$HOME/rem/rem-deployment"
cd "$HOME/rem"
chmod 700 launch-rem.sh qemu-system-myemulator32
```

Install the verified guest artifacts, then create the checksum sidecars used
by the preflight script:

```sh
install -m 0644 /tmp/rem-verified-guest/vmlinux "$HOME/rem/vmlinux"
install -m 0644 /tmp/rem-verified-guest/rootfs.ext4 "$HOME/rem/rootfs.ext4"
cd "$HOME/rem"
sha256sum vmlinux > vmlinux.sha256
sha256sum rootfs.ext4 > rootfs.ext4.sha256
./rem-install.sh check
```

Create a recovery copy without copying or deleting the persistent filesystem:

```sh
./rem-install.sh backup "$HOME/rem-recovery"
```

After an Android update or accidental replacement, restore the known-working
QEMU, launcher, kernel, and checksum metadata while preserving `rootfs.ext4`:

```sh
./rem-install.sh restore "$HOME/rem-recovery"
./rem-install.sh check
```

The launcher prepends its bundled `lib/` directory and Termux's `$PREFIX/lib`
to `LD_LIBRARY_PATH`, then runs the ARM64 QEMU binary directly. It does not
require root, proot, a graphical display, or an Android app wrapper.

## Launch and persistence check

Start REM Linux directly on the Termux terminal:

```sh
cd "$HOME/rem"
./launch-rem.sh
```

At the guest shell prompt, verify persistence:

```sh
echo PHONE_PERSIST_TEST > /root/phone-persist.txt
cat /root/phone-persist.txt
```

Exit the guest normally, run `./launch-rem.sh` again, and confirm:

```sh
cat /root/phone-persist.txt
```

Report the complete console output from both boots, including any dynamic
linker, kernel, filesystem, or serial-console errors. Until this physical
test passes, Android execution and REM Linux boot remain unverified.
