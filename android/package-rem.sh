#!/usr/bin/env bash
set -euo pipefail

if [[ $# -ne 2 ]]; then
  echo "usage: $0 QEMU OUTPUT_DIR" >&2
  exit 2
fi

qemu=$1
output=$2
script_dir="$(cd "$(dirname "$0")" && pwd)"
[[ -r "$qemu" ]] || { echo "missing QEMU executable: $qemu" >&2; exit 2; }
[[ -x "$qemu" ]] || { echo "QEMU is not executable: $qemu" >&2; exit 2; }
[[ -w "$(dirname "$output")" ]] || { echo "output parent is not writable: $(dirname "$output")" >&2; exit 2; }

mkdir -p "$output"
if [[ -n "$(find "$output" -mindepth 1 -maxdepth 1 -print -quit)" ]]; then
	echo "output directory is not empty: $output (remove it before packaging)" >&2
	exit 2
fi
install -m 0755 "$qemu" "$output/qemu-system-myemulator32"
install -m 0755 "$script_dir/launch-rem.sh" "$output/launch-rem.sh"
mkdir -p "$output/placeholders"
printf '%s\n' 'PLACEHOLDER: install the verified REM Linux kernel here as ../vmlinux.' \
	> "$output/placeholders/vmlinux"
printf '%s\n' 'PLACEHOLDER: install the verified writable ext4 image here as ../rootfs.ext4.' \
	> "$output/placeholders/rootfs.ext4"
default_sysroot="$(dirname "$0")/../.android-build/termux-sysroot"
if [[ -n "${ANDROID_TERMUX_SYSROOT:-}" ]]; then
	sysroot="$ANDROID_TERMUX_SYSROOT"
else
	sysroot="$default_sysroot"
fi
runtime_libs="$sysroot/data/data/com.termux/files/usr/lib"
if [[ -d "$runtime_libs" && -e "$runtime_libs/libglib-2.0.so.0" ]]; then
	mkdir -p "$output/lib"
	for pattern in libglib-2.0.so* libz.so* libandroid-support.so* libiconv.so* libpcre2-8.so* libffi.so* libslirp.so*; do
		for library in "$runtime_libs"/$pattern; do
			[[ -e "$library" ]] && cp -a "$library" "$output/lib/"
		done
	done
else
	echo "warning: Android runtime libraries not bundled; install Termux glib and zlib" >&2
fi
cat > "$output/README" <<'EOF'
REM Android ARM64 deployment package

This package contains Android QEMU, its optional bundled Termux libraries,
the launcher, and guest-image installation instructions. The kernel and
root filesystem are intentionally placeholders; no experimental GCC
integration artifact is included.

Read TERMUX_DEPLOYMENT.md, then install the verified guest files as ./vmlinux
and ./rootfs.ext4 before running ./launch-rem.sh.
EOF
install -m 0644 "$script_dir/TERMUX_DEPLOYMENT.md" "$output/TERMUX_DEPLOYMENT.md"
(
	cd "$output"
	find . -type f ! -path './SHA256SUMS' -print0 | sort -z | xargs -0 sha256sum > SHA256SUMS
)
printf 'Deployment package: %s\n' "$output"
