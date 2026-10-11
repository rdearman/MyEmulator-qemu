#!/bin/sh
# Apply canonical REM administration files to a fresh rootfs staging tree.
set -eu
[ "$(id -u)" = 0 ] || { echo "run as root (needed for root ownership and setuid mode)" >&2; exit 1; }
[ "$#" = 2 ] || { echo "usage: $0 ROOTFS-TREE BUILT-SUDO" >&2; exit 2; }
tree=$(CDPATH= cd -- "$1" && pwd)
bin=$2
accounts=$(CDPATH= cd -- "$(dirname -- "$0")/../rem-account-base" && pwd)
[ -x "$bin" ] || { echo "missing built sudo: $bin" >&2; exit 2; }
install -D -o 0 -g 0 -m 0644 "$accounts/passwd" "$tree/etc/passwd"
install -D -o 0 -g 0 -m 0600 "$accounts/shadow" "$tree/etc/shadow"
install -D -o 0 -g 0 -m 0644 "$accounts/group" "$tree/etc/group"
install -D -o 0 -g 0 -m 0644 "$accounts/securetty" "$tree/etc/securetty"
install -D -o 0 -g 0 -m 0440 "$accounts/sudoers" "$tree/etc/sudoers"
install -D -o 0 -g 0 -m 4755 "$bin" "$tree/usr/bin/sudo"
echo "REM accounts and dedicated sudo staged in $tree"
