#!/usr/bin/env bash
set -euo pipefail

root=$(cd "$(dirname "$0")/../.." && pwd)
. "$root/toolchain/scripts/userspace-common.sh"

version=9.9p2
url=https://cdn.openbsd.org/pub/OpenBSD/OpenSSH/portable/openssh-$version.tar.gz
sha256=91aadb603e08cc285eddf965e1199d02585fa94d994d6cae5b41e1721e215673
source_dir=$(userspace_fetch_extract openssh "$version" "$url" "$sha256")
build_dir="$build_root/build/openssh-$version"

export ac_cv_func_getaddrinfo=yes
export ac_cv_func_getnameinfo=yes
export ac_cv_func_strnvis=no
export ac_cv_func_strvis=no
MYEMU_USERSPACE_LIBS="-lcrypto -lz -lc -lgcc" \
	userspace_autoconf_build openssh "$source_dir" "$build_dir" \
	--with-ssl-dir="$stage_dir/usr" --with-zlib="$stage_dir/usr" \
	--without-pam --without-selinux --without-libedit \
	--without-kerberos5 --without-security-key-builtin \
	--disable-strip --sysconfdir=/etc/ssh
userspace_link_bins ssh scp sftp ssh-add ssh-agent ssh-keygen ssh-keyscan
userspace_verify_bins ssh scp sftp ssh-keygen
