#!/data/data/com.termux/files/usr/bin/bash
set -euo pipefail

project_root="$(cd "$(dirname "$0")/.." && pwd)"
qemu_repo="${QEMU_REPO:-https://gitlab.com/qemu-project/qemu.git}"
qemu_ref="${QEMU_REF:-v9.2.0}"
qemu_source="${QEMU_SOURCE:-$TMPDIR/rem-qemu-v9.2.0}"
qemu_build="${QEMU_BUILD:-$PREFIX/var/tmp/rem-qemu-build}"
jobs="${JOBS:-$(getconf _NPROCESSORS_ONLN 2>/dev/null || printf '2')}"

command -v pkg-config >/dev/null || { echo 'missing pkg-config; pkg install pkg-config' >&2; exit 2; }
pkg-config --exists glib-2.0 || { echo 'missing Termux GLib; pkg install glib' >&2; exit 2; }

if [[ ! -x "$qemu_source/configure" ]]; then
  mkdir -p "$(dirname "$qemu_source")"
  git clone --branch "$qemu_ref" --depth 1 "$qemu_repo" "$qemu_source"
fi

"$project_root/qemu/tools/apply-overlay.sh" "$qemu_source"
mkdir -p "$qemu_build"
(
  cd "$qemu_build"
  "$qemu_source/configure" \
    --target-list=myemulator32-softmmu,myemulator-softmmu \
    --without-default-features \
    --enable-system \
    --enable-tools \
    --enable-fdt=disabled \
    --disable-dbus-display \
    --disable-docs \
    --disable-gtk \
    --disable-sdl \
    --disable-opengl \
    --disable-vnc \
    --disable-slirp \
    --disable-pipewire \
    --disable-alsa \
    --disable-pa \
    --disable-plugins \
    --disable-werror \
    --enable-debug-tcg
  make -j"$jobs" qemu-system-myemulator32 qemu-system-myemulator
)

binary="$qemu_build/qemu-system-myemulator32"
file "$binary"
readelf -l "$binary" | sed -n '/INTERP/,+1p'
readelf -d "$binary" | sed -n '/NEEDED/p'
printf 'Built %s\n' "$binary"
