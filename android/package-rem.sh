#!/usr/bin/env bash
set -euo pipefail

if [[ $# -ne 4 ]]; then
  echo "usage: $0 QEMU KERNEL ROOTFS OUTPUT_DIR" >&2
  exit 2
fi

qemu=$1
kernel=$2
rootfs=$3
output=$4
for path in "$qemu" "$kernel" "$rootfs"; do
  [[ -r "$path" ]] || { echo "missing input: $path" >&2; exit 2; }
done
[[ -x "$qemu" ]] || { echo "QEMU is not executable: $qemu" >&2; exit 2; }
[[ -w "$(dirname "$output")" ]] || { echo "output parent is not writable: $(dirname "$output")" >&2; exit 2; }

mkdir -p "$output"
if [[ -n "$(find "$output" -mindepth 1 -maxdepth 1 -print -quit)" ]]; then
	echo "output directory is not empty: $output (remove it before packaging)" >&2
	exit 2
fi
install -m 0755 "$qemu" "$output/qemu-system-myemulator32"
install -m 0644 "$kernel" "$output/vmlinux"
install -m 0644 "$rootfs" "$output/rootfs.ext4"
install -m 0755 "$(dirname "$0")/launch-rem.sh" "$output/launch-rem.sh"
default_sysroot="$(dirname "$0")/../.android-build/termux-sysroot"
if [[ -n "${ANDROID_TERMUX_SYSROOT:-}" ]]; then
	sysroot="$ANDROID_TERMUX_SYSROOT"
else
	sysroot="$default_sysroot"
fi
runtime_libs="$sysroot/data/data/com.termux/files/usr/lib"
if [[ -d "$runtime_libs" && -e "$runtime_libs/libglib-2.0.so.0" ]]; then
	mkdir -p "$output/lib"
	for pattern in libglib-2.0.so* libz.so* libandroid-support.so* libiconv.so* libpcre2-8.so*; do
		for library in "$runtime_libs"/$pattern; do
			[[ -e "$library" ]] && cp -a "$library" "$output/lib/"
		done
	done
else
	echo "warning: Android runtime libraries not bundled; install Termux glib and zlib" >&2
fi
cat > "$output/README" <<'EOF'
Run in Termux with: ./launch-rem.sh
The rootfs.ext4 file is persistent and must remain writable.
Bundled libraries, when present, are under ./lib.
Install the Termux runtime dependencies with: pkg install bash coreutils tar glib zlib
EOF
install -m 0644 "$(dirname "$0")/TERMUX_DEPLOYMENT.md" "$output/TERMUX_DEPLOYMENT.md"
(
	cd "$output"
	sha256sum qemu-system-myemulator32 vmlinux rootfs.ext4 launch-rem.sh \
		TERMUX_DEPLOYMENT.md README > SHA256SUMS
)
printf 'Deployment package: %s\n' "$output"
