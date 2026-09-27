# REM on Android ARM64 / Termux

The ready-to-transfer Fold package is [`REM-FLIGHT-ANDROID-MVP.zip`](../REM-FLIGHT-ANDROID-MVP.zip).
Follow [`TERMUX_DEPLOYMENT.md`](TERMUX_DEPLOYMENT.md). After extraction,
launch from Termux with one command:

```sh
~/rem/rem
```

The package carries QEMU, its required Termux libraries, the matching REM32
Linux kernel, and the persistent ext4 guest disk. It runs machine
`myemulator32` with 128 MiB RAM and an interactive serial console. Networking
is optional and is not required to boot. The package and disk are relocatable
within Termux private storage; shared Android storage may prevent executable
files from running.

## Build QEMU from Linux

The Android ARM64 QEMU binary can be cross-built with the Android NDK and
Termux ARM64 runtime packages:

```sh
ANDROID_NDK=/path/to/android-ndk \
  QEMU_SOURCE=/path/to/qemu-v9.2.0 \
  ./android/build-ndk-qemu.sh
```

The script defaults to Android API 28 and builds a PIE executable using
`/system/bin/linker64`. Its runtime needs Termux GLib and zlib; the flight
package bundles QEMU's transitive Termux library dependencies. The binary
cannot execute on this x86-64 host.

The alternative `build-termux-qemu.sh` builds QEMU inside an ARM64 Termux
environment and requires the build dependencies listed in that script's
output. The Fold launch package does not require building QEMU on the phone.

## Verification

Host checks verify the package contents, checksums, ELF architecture and
interpreter, dynamic dependencies, launcher syntax, and QEMU arguments.
Physical Galaxy Fold execution, guest boot, and a write/reboot/read
persistence check remain pending.
