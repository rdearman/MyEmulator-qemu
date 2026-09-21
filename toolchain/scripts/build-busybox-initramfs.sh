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
# Provide the console devices expected by the serial/TTY path and by
# ordinary shell redirection.  These are harmless when devtmpfs later
# replaces the early initramfs /dev contents.
make_nodes() {
	mknod -m 600 "$out/rootfs/dev/console" c 5 1 2>/dev/null || true
	mknod -m 666 "$out/rootfs/dev/null" c 1 3 2>/dev/null || true
	mknod -m 600 "$out/rootfs/dev/ttyMY0" c 240 0 2>/dev/null || true
}
cat > "$out/init" <<'EOF'
#!/bin/sh
mount -t proc proc /proc 2>/dev/null || true
mount -t sysfs sysfs /sys 2>/dev/null || true
# PID 1 is created without inherited file descriptors on this minimal
# architecture port.  Attach the shell explicitly to the kernel console;
# this is the stable tty endpoint even when serial-core chooses the tty line
# major/minor dynamically.
exec /bin/busybox sh -i </dev/console >/dev/console 2>&1
EOF
chmod 0755 "$out/init"
install -m 0755 "$out/init" "$out/rootfs/init"
if command -v fakeroot >/dev/null 2>&1; then
	fakeroot sh -c 'mknod -m 600 "$1/dev/console" c 5 1; mknod -m 666 "$1/dev/null" c 1 3; mknod -m 600 "$1/dev/ttyMY0" c 240 0; cd "$1"; find . -print | cpio -o -H newc --quiet > "$2"' \
		-- "$out/rootfs" "$out/initramfs.cpio"
else
	make_nodes
	(
		cd "$out/rootfs"
		find . -print | cpio -o -H newc --quiet > "$out/initramfs.cpio"
	)
fi
printf '%s\n' "$out/initramfs.cpio"
