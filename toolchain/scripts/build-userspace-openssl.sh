#!/usr/bin/env bash
set -euo pipefail

root=$(cd "$(dirname "$0")/../.." && pwd)
. "$root/toolchain/scripts/userspace-common.sh"

version=3.4.1
url=https://www.openssl.org/source/openssl-$version.tar.gz
sha256=002a2d6b30b58bf4bea46c43bdd96365aaf8daa6c428782aa4feee06da197df3
source_dir=$(userspace_fetch_extract openssl "$version" "$url" "$sha256")
build_dir="$build_root/build/openssl-$version"

userspace_require_target
rm -rf "$build_dir"
cp -a "$source_dir" "$build_dir"
(
	cd "$build_dir"
	userspace_set_cross_env
	configure_cflags=${CFLAGS//-static/}
	configure_ldflags=${LDFLAGS//-static/}
	CC="$CC" AR="$AR" RANLIB="$RANLIB" \
		CFLAGS="$configure_cflags" LDFLAGS="$configure_ldflags" \
		perl ./Configure linux-generic32 threads no-asm no-shared no-module no-tests \
		no-dso no-ui-console no-afalgeng no-quic \
		--prefix=/usr --openssldir=/etc/ssl \
		"--with-rand-seed=getrandom" \
		"-DOPENSSL_USE_NODELETE"
	sed -i 's/-pthread//g' Makefile
	sed -i 's|^LDFLAGS=|LDFLAGS=-static |' Makefile
	sed -i 's|^EX_LIBS=|EX_LIBS=-latomic_compat |' Makefile
	make -j"$jobs"
	make DESTDIR="$stage_dir" install_sw install_ssldirs
)
userspace_stage_ca_bundle
userspace_link_bins openssl
userspace_verify_bins openssl
