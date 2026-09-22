#!/usr/bin/env bash
set -euo pipefail

# This probe deliberately stops before a build if QEMU's cross dependencies
# are unavailable. Host GLib libraries must never be passed to an ARM link.
project_root="$(cd "$(dirname "$0")/.." && pwd)"
ndk="${ANDROID_NDK:-/home/rick/Android/sdk/ndk/26.1.10909125}"
api="${ANDROID_API:-28}"
target="aarch64-linux-android${api}"
cc="$ndk/toolchains/llvm/prebuilt/linux-x86_64/bin/${target}-clang"
qemu_source="${QEMU_SOURCE:-/tmp/rem-android-qemu-v9.2.0}"
probe_build="${QEMU_BUILD:-$project_root/.android-build/ndk-probe}"

[[ -x "$cc" ]] || { echo "missing NDK compiler: $cc" >&2; exit 2; }
[[ -x "$qemu_source/configure" ]] || { echo "missing QEMU source: $qemu_source" >&2; exit 2; }
pkg_config="$(command -v pkg-config || true)"
if [[ -n "$pkg_config" ]] && pkg-config --exists glib-2.0; then
  echo "NDK probe: host GLib is present, but is intentionally not used." >&2
fi
echo "NDK compiler: $cc"
echo "QEMU source: $qemu_source"
echo "A complete NDK cross build requires ARM64 GLib (and its dependencies);"
echo "use android/build-termux-qemu.sh for the supported device-native build."
exit 0
