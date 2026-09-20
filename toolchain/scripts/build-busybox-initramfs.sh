#!/usr/bin/env bash
set -euo pipefail

root=$(cd "$(dirname "$0")/../.." && pwd)
out=${1:-${MYEMU_BUSYBOX_INITRAMFS_OUT:-/tmp/myemu-busybox-initramfs}}
busybox=${MYEMU_BUSYBOX_BINARY:-${MYEMU_BUSYBOX_BUILD:-$root/.busybox-build}/busybox}
mkdir -p "$out/rootfs" "$out/rootfs/bin" "$out/rootfs/sbin" \
	"$out/rootfs/dev" "$out/rootfs/proc" "$out/rootfs/sys" "$out/rootfs/tmp" \
	"$out/rootfs/etc" "$out/rootfs/root"
install -m 0755 "$busybox" "$out/rootfs/bin/busybox"
for applet in sh ls cat echo pwd uname mkdir touch cp mv rm head tail grep find ps mount umount dmesg vi; do
	ln -sf busybox "$out/rootfs/bin/$applet"
done
cat > "$out/init" <<'EOF'
#!/bin/sh
mount -t proc proc /proc 2>/dev/null || true
mount -t sysfs sysfs /sys 2>/dev/null || true
exec /bin/busybox sh
EOF
chmod 0755 "$out/init"
install -m 0755 "$out/init" "$out/rootfs/init"
(
	cd "$out/rootfs"
	find . -print | cpio -o -H newc --quiet > "$out/initramfs.cpio"
)
printf '%s\n' "$out/initramfs.cpio"
