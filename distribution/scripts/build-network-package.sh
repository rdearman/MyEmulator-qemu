#!/bin/sh
set -eu
ROOT=$(CDPATH= cd -- "$(dirname -- "$0")/../.." && pwd)
OUT=${1:?usage: build-network-package.sh OUTPUT_DIR ROOTFS_USR_PREFIX}
SYSROOT=${2:?usage: build-network-package.sh OUTPUT_DIR ROOTFS_USR_PREFIX}
GCC=${MYEMU_TARGET_GCC:-$ROOT/.toolchain-install/bin/myemulator2-elf-gcc}
WRAPPER=$ROOT/toolchain/scripts/myemulator2-musl-gcc
[ -x "$GCC" ] || { echo "missing MyEmulator target GCC: $GCC" >&2; exit 2; }
[ -f "$SYSROOT/lib/libc.a" ] || { echo "missing pinned REM libc: $SYSROOT/lib/libc.a" >&2; exit 2; }
mkdir -p "$OUT/usr/sbin"
MYEMU_TARGET_GCC=$GCC MYEMU_MUSL_PREFIX=$SYSROOT \
    "$WRAPPER" -I"$SYSROOT/include" -L"$SYSROOT/lib" -static \
    "$ROOT/distribution/init/rem-net.c" -o "$OUT/usr/sbin/rem-net" -lc -lgcc
chmod 0755 "$OUT/usr/sbin/rem-net"
