#!/usr/bin/env bash
set -euo pipefail

project_root="$(cd "$(dirname "$0")/.." && pwd)"
qemu_source="${QEMU_SOURCE:-/tmp/rem-android-qemu-v9.2.0}"
ndk="${ANDROID_NDK:-/home/rick/Android/sdk/ndk/26.1.10909125}"
api="${ANDROID_API:-28}"
sysroot="${ANDROID_TERMUX_SYSROOT:-$project_root/.android-build/termux-sysroot}"
build="${QEMU_BUILD:-$project_root/.android-build/ndk-qemu}"
jobs="${JOBS:-$(getconf _NPROCESSORS_ONLN 2>/dev/null || printf '2')}"
base_url="https://packages.termux.dev/apt/termux-main/"

clang="$ndk/toolchains/llvm/prebuilt/linux-x86_64/bin/aarch64-linux-android${api}-clang"
llvm="$ndk/toolchains/llvm/prebuilt/linux-x86_64/bin"
tools="$project_root/.android-build/ndk-tools"
target_prefix="$tools/aarch64-linux-android-"
networking="${REM_ENABLE_NETWORKING:-0}"

[[ -x "$clang" ]] || { echo "missing NDK compiler: $clang" >&2; exit 2; }
[[ -x "$qemu_source/configure" ]] || { echo "missing QEMU source: $qemu_source" >&2; exit 2; }
command -v curl >/dev/null || { echo 'curl is required' >&2; exit 2; }
command -v dpkg-deb >/dev/null || { echo 'dpkg-deb is required on the Linux build host' >&2; exit 2; }

if [[ ! -f "$sysroot/data/data/com.termux/files/usr/lib/pkgconfig/glib-2.0.pc" ||
      ! -f "$sysroot/data/data/com.termux/files/usr/lib/pkgconfig/slirp.pc" ]]; then
  mkdir -p "$sysroot"
  packages=(
    pool/main/g/glib/glib_2.90.0_aarch64.deb
    pool/main/libf/libffi/libffi_3.8.0_aarch64.deb
    pool/main/liba/libandroid-support/libandroid-support_29-1_aarch64.deb
    pool/main/libi/libiconv/libiconv_1.19_aarch64.deb
    pool/main/libs/libslirp/libslirp_4.8.0-2_aarch64.deb
    pool/main/p/pcre2/pcre2_10.47_aarch64.deb
    pool/main/r/resolv-conf/resolv-conf_1.3_aarch64.deb
    pool/main/z/zlib/zlib_1.3.2_aarch64.deb
  )
  for package in "${packages[@]}"; do
    archive="$sysroot/$(basename "$package")"
    curl --fail --location --retry 2 --output "$archive" "$base_url$package"
    dpkg-deb -x "$archive" "$sysroot"
  done
fi

mkdir -p "$tools"
ln -sf /usr/bin/pkg-config "$target_prefix"pkg-config
for tool in ar nm objcopy ranlib readelf strip; do
  ln -sf "$llvm/llvm-$tool" "$target_prefix$tool"
done

"$project_root/qemu/tools/apply-overlay.sh" "$qemu_source"
if patch -d "$qemu_source" -p1 --dry-run -N < "$project_root/android/qemu-android.patch" >/dev/null 2>&1; then
  patch -d "$qemu_source" -p1 -N < "$project_root/android/qemu-android.patch"
fi
mkdir -p "$build"
(
  cd "$build"
  configure_args=(
    --cross-prefix="$target_prefix"
    --host-cc=cc
    --target-list=myemulator32-softmmu,myemulator-softmmu
    --without-default-features
    --enable-system
    --enable-tools
    --disable-fdt
    --disable-docs
    --disable-plugins
    --disable-werror
  )
  if [[ "$networking" == 1 ]]; then
    configure_args+=(--enable-slirp)
  else
    configure_args+=(--disable-slirp)
  fi
  PKG_CONFIG_SYSROOT_DIR="$sysroot" \
  PKG_CONFIG_LIBDIR="$sysroot/data/data/com.termux/files/usr/lib/pkgconfig:$sysroot/data/data/com.termux/files/usr/share/pkgconfig" \
  PKG_CONFIG_PATH='' \
  CC="$clang" \
    "$qemu_source/configure" "${configure_args[@]}"
  PKG_CONFIG_SYSROOT_DIR="$sysroot" \
  PKG_CONFIG_LIBDIR="$sysroot/data/data/com.termux/files/usr/lib/pkgconfig:$sysroot/data/data/com.termux/files/usr/share/pkgconfig" \
  PKG_CONFIG_PATH='' ninja -j"$jobs" qemu-system-myemulator32 qemu-system-myemulator
)

binary="$build/qemu-system-myemulator32"
file "$binary"
readelf -l "$binary" | sed -n '/INTERP/,+1p'
readelf -d "$binary" | sed -n '/NEEDED/p'
printf 'Built %s\n' "$binary"
