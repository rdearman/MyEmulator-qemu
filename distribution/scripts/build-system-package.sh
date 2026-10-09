#!/bin/sh
set -eu
ROOT=$(CDPATH= cd -- "$(dirname -- "$0")/../.." && pwd)
OUT=${1:?usage: build-system-package.sh OUTPUT_DIR ROOTFS_USR_PREFIX}
SYSROOT=${2:?usage: build-system-package.sh OUTPUT_DIR ROOTFS_USR_PREFIX}
GCC=${MYEMU_TARGET_GCC:-$ROOT/.toolchain-install/bin/myemulator2-elf-gcc}
WRAPPER=$ROOT/toolchain/scripts/myemulator2-musl-gcc
mkdir -p "$OUT/usr/bin" "$OUT/usr/sbin" "$OUT/sbin"
[ -x "$GCC" ] || { echo "missing MyEmulator target GCC: $GCC" >&2; exit 2; }
[ -f "$SYSROOT/lib/libc.a" ] || { echo "missing pinned REM libc: $SYSROOT/lib/libc.a" >&2; exit 2; }
build() {
    name=$1 source=$2 destination=$3 libs=${4:-}
    MYEMU_TARGET_GCC=$GCC MYEMU_MUSL_PREFIX=$SYSROOT \
        "$WRAPPER" -I"$SYSROOT/include" -L"$SYSROOT/lib" -static \
        "$ROOT/distribution/init/$source.c" -o "$OUT/$destination" \
        -lc $libs -lgcc
    chmod 0755 "$OUT/$destination"
}
build rem-init rem-init sbin/init
build rem-getty rem-getty sbin/getty
build rem-login rem-login sbin/login
build rem-mount rem-mount usr/bin/rem-mount
build rem-hostname rem-hostname usr/bin/hostname
build rem-shutdown rem-shutdown sbin/poweroff
cp "$OUT/sbin/poweroff" "$OUT/sbin/reboot"
cp "$OUT/sbin/poweroff" "$OUT/sbin/halt"
chmod 4755 "$OUT/sbin/poweroff" "$OUT/sbin/reboot" "$OUT/sbin/halt"
build rem-clear rem-clear usr/bin/clear
ln -sf rem-mount "$OUT/usr/bin/umount"
ln -sf rem-mount "$OUT/usr/bin/mount"
