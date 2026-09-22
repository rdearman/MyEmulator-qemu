# REM Android Deployment Handoff

## Workspace

- Workspace: `/home/rick/Development/Active/REM-android`
- Branch: `android-arm64`
- Known commits: `8d49fe9` (Android ARM64 QEMU build), `a628bb2` (deployment preparation).
- Do not modify, build, clean, reset, inspect with Git, or otherwise interfere with `/home/rick/Development/Active/MyEmulator-qemu`.
- Do not push unless explicitly requested. Before Git add/commit, verify:

```bash
git rev-parse --show-toplevel
```

It must print `/home/rick/Development/Active/REM-android`.

## Objective

Deploy the custom REM QEMU system emulator to a Samsung Galaxy Fold through Termux. The target is `myemulator32-softmmu`, machine `myemulator32`. It must boot REM Linux into an interactive serial shell with a persistent writable ext4 root filesystem.

Android execution has not been tested and must not be claimed as working until the user tests the phone.

## Completed build work

The Android ARM64 QEMU executable is:

```text
.android-build/ndk-test/qemu-system-myemulator32
```

Host QEMU executables are:

```text
.android-build/minimal-host/qemu-system-myemulator32
.android-build/host-qemu/qemu-system-myemulator32
```

Deployment scripts/docs already committed under `android/` include:

```text
android/package-rem.sh
android/launch-rem.sh
android/TERMUX_DEPLOYMENT.md
```

The launcher uses:

```text
-M myemulator32
-kernel vmlinux
-drive file=rootfs.ext4,format=raw,if=none,id=myemulator2-disk
-nographic -monitor none -serial stdio
```

## Current inputs

The user copied the kernel to:

```text
.android-build/deployment-input/vmlinux
```

Kernel SHA256:

```text
244fcd20616b52a4c8f9acb560c85d1192708bd2e16182ae2824972a74800798
```

The current rootfs is only a disposable test image:

```text
.android-build/deployment-input/rootfs.ext4
```

Rootfs SHA256:

```text
242b88e142b369a4a6222f092c1fea311e5a5cf0b6820ab9b7a17574f01d6ef6
```

It is a 16 MiB ext4 image created with the repository script and the existing disposable BusyBox candidate:

```bash
MYEMU_BUSYBOX_BINARY=/tmp/myemu-busybox-build23/busybox \
MYEMU_EXT4_WORK=/tmp/rem-android-rootfs-build \
MYEMU_EXT4_SIZE=16M \
  ./toolchain/scripts/build-ext4-rootfs.sh /tmp/rem-rootfs.ext4
```

The clone does not contain the REM cross-toolchain/musl installation needed to rebuild BusyBox independently:

```text
.toolchain-install/bin/myemulator2-elf-gcc   missing
.musl-install                            missing
```

Do not use legacy `myemulator.img` files as Linux root filesystems; they were not valid ext4 deployment images.

## Host test result and blocker

Host QEMU reaches the kernel, detects the disk, mounts ext4 read/write, and starts `/sbin/init`. It then executes BusyBox and fails immediately:

```text
EXT4-fs (myemu0): mounted filesystem ... r/w
VFS: Mounted root (ext4 filesystem) on device 259:0
Run /sbin/init as init process
process '/bin/busybox' started with executable stack
MYEMU_USER_FAULT ... pc=00700074
Kernel panic - not syncing: unhandled MyEmulator2 exception
```

No interactive shell appeared. The current rootfs is therefore not verified, persistence has not been tested, and no final archive has been produced. Do not deploy or claim this image as working.

This is a BusyBox/userspace compatibility failure at the first instruction, not evidence that ext4 mounting or the Android QEMU binary is broken. Do not change the REM ISA, MMU, Linux kernel, or native GCC to work around it.

## Required next action

Ask the GCC agent for the exact path of the known-good ext4 image used by the existing Linux/ext4 persistence regressions. Have the user copy it into this clone:

```bash
install -m 0644 \
  /path/to/verified/rem-rootfs.ext4 \
  ~/Development/Active/REM-android/.android-build/deployment-input/rootfs.ext4
```

Then:

1. Verify it with `file`, `dumpe2fs`, and `debugfs`.
2. Copy it to a disposable `/tmp/rem-host-rootfs-test.ext4`.
3. Boot with:

```bash
timeout --signal=TERM 60s \
  .android-build/minimal-host/qemu-system-myemulator32 \
    -M myemulator32 -m 16M \
    -kernel .android-build/deployment-input/vmlinux \
    -drive file=/tmp/rem-host-rootfs-test.ext4,format=raw,if=none,id=myemulator2-disk \
    -nographic -monitor none -serial stdio
```

4. Run the repository shell regression with `QEMU_MYEMULATOR32` and `MYEMU_EXT4_IMAGE` overridden to these paths:

```bash
QEMU_MYEMULATOR32="$PWD/.android-build/minimal-host/qemu-system-myemulator32" \
MYEMU_EXT4_IMAGE=/tmp/rem-host-rootfs-test.ext4 \
  python3 toolchain/scripts/test-linux-ext4-busybox-shell.py
```

5. Write a marker under `/root` or `/tmp`, stop QEMU, restart with the same image, and verify the marker remains.
6. Only after shell and persistence pass, package:

```bash
rm -rf .android-build/rem-deployment .android-build/REM-android-arm64.tar.gz
ANDROID_TERMUX_SYSROOT=/tmp/rem-termux-sysroot \
  ./android/package-rem.sh \
    .android-build/ndk-test/qemu-system-myemulator32 \
    .android-build/deployment-input/vmlinux \
    .android-build/deployment-input/rootfs.ext4 \
    .android-build/rem-deployment
tar -C .android-build -czf .android-build/REM-android-arm64.tar.gz rem-deployment
tar -tzf .android-build/REM-android-arm64.tar.gz
sha256sum .android-build/REM-android-arm64.tar.gz
```

The archive must contain:

```text
rem-deployment/qemu-system-myemulator32
rem-deployment/vmlinux
rem-deployment/rootfs.ext4
rem-deployment/launch-rem.sh
rem-deployment/lib/...
```

Update `android/TERMUX_DEPLOYMENT.md` with final checksums and test results, then commit Android-only documentation/script changes. Do not push.

## Transfer and Termux commands

From Linux Mint:

```bash
adb push .android-build/REM-android-arm64.tar.gz /sdcard/Download/
```

In Termux:

```bash
termux-setup-storage
pkg update
pkg install bash coreutils tar
mkdir -p "$HOME/rem"
tar -xzf "$HOME/storage/downloads/REM-android-arm64.tar.gz" \
  -C "$HOME/rem" --strip-components=1
chmod 0755 "$HOME/rem/qemu-system-myemulator32" "$HOME/rem/launch-rem.sh"
cd "$HOME/rem"
./launch-rem.sh
```

Persistence comes from reusing the same `$HOME/rem/rootfs.ext4`; do not recreate or overwrite it between restarts.

Actual Galaxy Fold testing remains outstanding.
