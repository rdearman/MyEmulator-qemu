#!/bin/sh
# Install the initial locked-root account files in a stopped disposable rootfs.
set -eu

if [ "$#" -ne 2 ]; then
    echo "usage: install-rem-account-base.sh SOURCE_ROOT ROOTFS_MOUNTPOINT" >&2
    exit 2
fi

source_root=$1
dest=$2
if [ ! -d "$dest/etc" ] || [ ! -d "$dest/bin" ] || [ ! -d "$dest/usr" ]; then
    echo "refusing: expected a rootfs containing bin/, etc/, and usr/" >&2
    exit 2
fi
dest=$(cd "$dest" && pwd -P)
if [ "$dest" = / ]; then
    echo "refusing to operate on the host root filesystem" >&2
    exit 2
fi
if [ "$(id -u)" -ne 0 ]; then
    echo "must run as root to install account files with root ownership" >&2
    exit 1
fi

for name in passwd group shadow shells securetty busybox.conf sudoers; do
    if [ -f "$dest/etc/$name" ] || [ -d "$dest/etc/$name" ] || [ -L "$dest/etc/$name" ]; then
        echo "refusing to overwrite existing /etc/$name" >&2
        exit 1
    fi
done

base=$source_root/toolchain/userspace/rem-account-base
for name in passwd group shadow shells securetty; do
    if [ ! -f "$base/$name" ]; then
        echo "missing account template: $base/$name" >&2
        exit 1
    fi
done
if [ ! -f "$source_root/toolchain/userspace/busybox-rem-user.conf" ]; then
    echo "missing BusyBox SUID policy: $source_root/toolchain/userspace/busybox-rem-user.conf" >&2
    exit 1
fi
cp "$base/passwd" "$dest/etc/passwd"
cp "$base/group" "$dest/etc/group"
cp "$base/shadow" "$dest/etc/shadow"
cp "$base/shells" "$dest/etc/shells"
cp "$base/securetty" "$dest/etc/securetty"
cp "$source_root/toolchain/userspace/busybox-rem-user.conf" "$dest/etc/busybox.conf"
if [ -f "$source_root/toolchain/userspace/rem-account-base/sudoers" ]; then
    install -m 0440 -o 0 -g 0 "$source_root/toolchain/userspace/rem-account-base/sudoers" "$dest/etc/sudoers"
fi
chown 0:0 "$dest/etc/passwd" "$dest/etc/group" "$dest/etc/shadow" \
    "$dest/etc/shells" "$dest/etc/securetty" "$dest/etc/busybox.conf"
chmod 0644 "$dest/etc/passwd" "$dest/etc/group" "$dest/etc/shells" \
    "$dest/etc/securetty"
chmod 0600 "$dest/etc/shadow" "$dest/etc/busybox.conf"

# BusyBox's `su` applet is intentionally not made setuid. Install the dedicated
# helper instead, so only the configured dev account receives sudo elevation.
sudo_bin=${REM_ADMIN_SUDO:-$source_root/toolchain/userspace/rem-admin/sudo}
if [ ! -x "$sudo_bin" ]; then
    "$source_root/toolchain/userspace/rem-admin/build.sh" "$sudo_bin"
fi
install -D -o 0 -g 0 -m 4755 "$sudo_bin" "$dest/usr/bin/sudo"
echo "installed root/dev account database under $dest/etc"
echo "installed dedicated root-owned /usr/bin/sudo (mode 4755); BusyBox remains non-setuid"
