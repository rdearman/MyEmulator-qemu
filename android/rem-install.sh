#!/usr/bin/env bash
set -euo pipefail

home="${REM_HOME:-$(cd "$(dirname "$0")" && pwd)}"
qemu="${REM_QEMU:-$home/qemu-system-myemulator32}"
kernel="${REM_KERNEL:-$home/vmlinux}"
rootfs="${REM_ROOTFS:-$home/rootfs.ext4}"
usage() {
	echo "usage: $0 check|launch|backup BACKUP_DIR|restore BACKUP_DIR" >&2
	exit 2
}
[[ $# -ge 1 && $# -le 2 ]] || usage
command="$1"
[[ "$command" == check || "$command" == launch || "$command" == backup || "$command" == restore ]] || usage

check_environment() {
	command -v sha256sum >/dev/null || { echo "missing Termux coreutils (sha256sum)" >&2; return 1; }
	command -v readelf >/dev/null || { echo "missing Termux binutils (readelf)" >&2; return 1; }
	[[ -x "$qemu" ]] || { echo "missing executable: $qemu" >&2; return 1; }
	[[ "$(readelf -l "$qemu" 2>/dev/null | sed -n 's/.*Requesting program interpreter: \(.*\)]/\1/p')" == "/system/bin/linker64" ]] ||
		{ echo "QEMU is not an Android ARM64 executable" >&2; return 1; }
	for library in libz.so.1 libglib-2.0.so.0 libm.so libc.so; do
		readelf -d "$qemu" | grep -q "Shared library: \[$library\]" ||
			{ echo "QEMU dependency missing from binary: $library" >&2; return 1; }
	done
	[[ -r "$kernel" && -w "$rootfs" ]] ||
		{ echo "install readable vmlinux and writable rootfs.ext4 before launch" >&2; return 1; }
	for image in "$kernel" "$rootfs"; do
		sidecar="$image.sha256"
		[[ -r "$sidecar" ]] || { echo "missing checksum sidecar: $sidecar" >&2; return 1; }
		(cd "$(dirname "$image")" && sha256sum --check "$(basename "$sidecar")") ||
			{ echo "checksum failed: $image" >&2; return 1; }
	done
	echo "REM Termux environment, QEMU dependencies, guest files, and checksums: PASS"
}

case "$command" in
	check) check_environment ;;
	launch) check_environment; exec "$home/launch-rem.sh" ;;
	backup)
		[[ $# -eq 2 ]] || usage
		dest="$2"; mkdir -p "$dest"
		for path in qemu-system-myemulator32 launch-rem.sh vmlinux vmlinux.sha256 rootfs.ext4.sha256; do
			[[ -e "$home/$path" ]] && cp -a "$home/$path" "$dest/"
		done
		echo "Recovery backup created without copying or deleting rootfs.ext4: $dest"
		;;
	restore)
		[[ $# -eq 2 ]] || usage
		source="$2"
		for path in qemu-system-myemulator32 launch-rem.sh vmlinux vmlinux.sha256 rootfs.ext4.sha256; do
			[[ -e "$source/$path" ]] && cp -a "$source/$path" "$home/"
		done
		echo "Known-working REM files restored; persistent rootfs.ext4 was preserved."
		;;
esac
