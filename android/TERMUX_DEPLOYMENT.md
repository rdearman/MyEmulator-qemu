# REM Linux deployment to a Galaxy Fold

This procedure is intentionally split into desktop and phone steps. The
desktop cannot execute the REM CPU, and Android execution has not been
verified until the phone test below is completed.

## Desktop: obtain the guest artifacts

The Android clone currently contains the QEMU executable, but not the guest
kernel or ext4 image. The original checkout has the current kernel at:

```text
/home/rick/Development/Active/MyEmulator-qemu/.linux-build/build/vmlinux
```

The original checkout's `myemulator.img` files are legacy MyFS images and are
not suitable as the Linux root filesystem. Do not use them with this launcher.

After obtaining a tested ext4 image from the Linux build work, copy only the
artifacts into this clone's disposable staging directory. These commands are
to be run by the user; they do not modify the original checkout:

```sh
cd /home/rick/Development/Active/REM-android
mkdir -p .android-build/deployment-input
install -m 0644 \
  /home/rick/Development/Active/MyEmulator-qemu/.linux-build/build/vmlinux \
  .android-build/deployment-input/vmlinux
install -m 0644 /path/to/verified/rem-rootfs.ext4 \
  .android-build/deployment-input/rootfs.ext4
```

Confirm the image is ext4 before packaging:

```sh
file .android-build/deployment-input/rootfs.ext4
dumpe2fs -h .android-build/deployment-input/rootfs.ext4 | sed -n '1,12p'
```

## Desktop: build the deployment archive

Use the ARM64 binary produced by the committed NDK build. The package script
also bundles GLib, zlib, PCRE2, libffi-related Termux support, and iconv when
the cross-build sysroot is available:

```sh
cd /home/rick/Development/Active/REM-android
rm -rf .android-build/rem-deployment
./android/package-rem.sh \
  .android-build/ndk-test/qemu-system-myemulator32 \
  .android-build/deployment-input/vmlinux \
  .android-build/deployment-input/rootfs.ext4 \
  .android-build/rem-deployment
tar -C .android-build -czf REM-android-arm64.tar.gz rem-deployment
sha256sum REM-android-arm64.tar.gz
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
adb push REM-android-arm64.tar.gz /sdcard/Download/
```

In Termux, grant storage access once and extract the archive into the Termux
home directory:

```sh
termux-setup-storage
pkg update
pkg install bash coreutils tar
mkdir -p "$HOME/rem"
tar -xzf "$HOME/storage/downloads/REM-android-arm64.tar.gz" \
  -C "$HOME/rem" --strip-components=1
cd "$HOME/rem"
chmod 700 launch-rem.sh qemu-system-myemulator32
```

If the package was built without bundled libraries, install the required
runtime packages:

```sh
pkg install glib zlib
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
