#!/usr/bin/env bash
set -euo pipefail

root=$(cd "$(dirname "$0")/../.." && pwd)
image=${1:-${MYEMU_EXT4_IMAGE:-/tmp/myemu-ext4-root.img}}
busybox=${MYEMU_BUSYBOX_BINARY:-${MYEMU_BUSYBOX_BUILD:-$root/.busybox-build}/busybox}
work=${MYEMU_EXT4_WORK:-${image}.root}

[[ -x "$busybox" ]] || { echo "missing executable BusyBox: $busybox" >&2; exit 2; }
rm -rf "$work"
mkdir -p "$work"/{bin,sbin,dev,proc,sys,tmp,etc,root,var,home}
install -m 0755 "$busybox" "$work/bin/busybox"
for applet in sh ls cat echo pwd uname mkdir touch cp mv rm head tail grep find ps mount umount dmesg vi sync; do
	ln -sf busybox "$work/bin/$applet"
done
cat > "$work/sbin/init" <<'EOF'
#!/bin/sh
exec /bin/busybox sh -i </dev/console >/dev/console 2>&1
EOF
chmod 0755 "$work/sbin/init"
if command -v fakeroot >/dev/null 2>&1; then
	fakeroot sh -c 'mknod -m 600 "$1/dev/console" c 5 1; mknod -m 666 "$1/dev/null" c 1 3; truncate -s "$3" "$2"; mkfs.ext4 -q -F -d "$1" "$2"' \
		-- "$work" "$image" "${MYEMU_EXT4_SIZE:-16M}"
else
	mknod -m 600 "$work/dev/console" c 5 1 2>/dev/null || true
	mknod -m 666 "$work/dev/null" c 1 3 2>/dev/null || true
	truncate -s "${MYEMU_EXT4_SIZE:-16M}" "$image"
	mkfs.ext4 -q -F -d "$work" "$image"
fi
printf '%s\n' "$image"
