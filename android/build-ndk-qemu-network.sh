#!/usr/bin/env bash
set -euo pipefail

# Separate Android networking build. The existing build-ndk-qemu.sh remains
# unchanged and continues to produce the verified non-networking artifact.
project_root="$(cd "$(dirname "$0")/.." && pwd)"
qemu_source="${QEMU_SOURCE:-/tmp/rem-android-qemu-v9.2.0}"
build="${QEMU_BUILD:-$project_root/.android-build/network-ndk-qemu}"

[[ -x "$qemu_source/configure" ]] || {
  echo "missing QEMU source: $qemu_source" >&2
  exit 2
}

ANDROID_NDK="${ANDROID_NDK:-/home/rick/Android/sdk/ndk/26.1.10909125}" \
QEMU_BUILD="$build" \
REM_ENABLE_NETWORKING=1 \
"$project_root/android/build-ndk-qemu.sh"
