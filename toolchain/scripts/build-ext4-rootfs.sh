#!/usr/bin/env bash
set -euo pipefail

root=$(cd "$(dirname "$0")/../.." && pwd)
image=${1:-${MYEMU_EXT4_IMAGE:-/tmp/myemu-ext4-root.img}}
busybox=${MYEMU_BUSYBOX_BINARY:-${MYEMU_BUSYBOX_BUILD:-$root/.busybox-build}/busybox}
work=${MYEMU_EXT4_WORK:-${image}.root}

[[ -x "$busybox" ]] || { echo "missing executable BusyBox: $busybox" >&2; exit 2; }
rm -rf "$work"
mkdir -p "$work"/{bin,sbin,dev,proc,sys,tmp,etc,root,var}
install -m 0755 "$busybox" "$work/bin/busybox"
for applet in sh ls cat echo pwd uname mkdir touch cp mv rm head tail grep find ps mount umount dmesg vi; do
	ln -sf busybox "$work/bin/$applet"
done
cat > "$work/sbin/init" <<'EOF'
#!/bin/sh
mount -t proc proc /proc 2>/dev/null || true
mount -t sysfs sysfs /sys 2>/dev/null || true
exec /bin/busybox sh
EOF
chmod 0755 "$work/sbin/init"
truncate -s "${MYEMU_EXT4_SIZE:-16M}" "$image"
mkfs.ext4 -q -F -d "$work" "$image"
printf '%s\n' "$image"
