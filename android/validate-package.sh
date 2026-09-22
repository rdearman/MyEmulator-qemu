#!/usr/bin/env bash
set -euo pipefail

if [[ $# -ne 1 ]]; then
	echo "usage: $0 DEPLOYMENT_DIR" >&2
	exit 2
fi

package=$1
[[ -d "$package" ]] || { echo "missing deployment directory: $package" >&2; exit 2; }

required=(qemu-system-myemulator32 vmlinux rootfs.ext4 launch-rem.sh README TERMUX_DEPLOYMENT.md SHA256SUMS)
for name in "${required[@]}"; do
	[[ -e "$package/$name" ]] || { echo "missing package file: $name" >&2; exit 1; }
done

[[ -x "$package/qemu-system-myemulator32" ]] || { echo "QEMU is not executable" >&2; exit 1; }
[[ -x "$package/launch-rem.sh" ]] || { echo "launcher is not executable" >&2; exit 1; }
[[ -r "$package/vmlinux" && -r "$package/rootfs.ext4" ]] || {
	echo "kernel and rootfs must be readable" >&2
	exit 1
}

file_output=$(file -b "$package/qemu-system-myemulator32")
grep -q 'ELF 64-bit.*ARM aarch64' <<<"$file_output" || {
	echo "QEMU is not an ARM64 ELF: $file_output" >&2
	exit 1
}
interp=$(readelf -l "$package/qemu-system-myemulator32" 2>/dev/null |
	sed -n 's/.*Requesting program interpreter: \(.*\)]/\1/p')
[[ "$interp" == "/system/bin/linker64" ]] || {
	echo "unexpected Android interpreter: ${interp:-<none>}" >&2
	exit 1
}
for library in libz.so.1 libglib-2.0.so.0 libm.so libc.so; do
	readelf -d "$package/qemu-system-myemulator32" 2>/dev/null |
		grep -q "Shared library: \[$library\]" || {
			echo "QEMU dependency not identified: $library" >&2
			exit 1
		}
done

file -b "$package/vmlinux" | grep -q 'ELF 32-bit' || {
	echo "kernel is not an ELF32 image" >&2
	exit 1
}
file -b "$package/rootfs.ext4" | grep -q 'ext4 filesystem' || {
	echo "rootfs is not an ext4 image" >&2
	exit 1
}

while IFS= read -r path; do
	relative=${path#"$package/"}
	case "$relative" in
		qemu-system-myemulator32|vmlinux|rootfs.ext4|launch-rem.sh|README|TERMUX_DEPLOYMENT.md|SHA256SUMS|lib/*) ;;
		*) echo "unexpected package path: $relative" >&2; exit 1 ;;
	esac
done < <(find "$package" -type f -print)

(cd "$package" && sha256sum --check SHA256SUMS)
printf 'Package structure, formats, dependencies, permissions, and checksums: PASS\n'
