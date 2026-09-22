#!/usr/bin/env bash
set -euo pipefail

root=$(cd "$(dirname "$0")/../.." && pwd)
. "$root/toolchain/scripts/userspace-common.sh"

version=8.12.1
url=https://curl.se/download/curl-$version.tar.xz
sha256=0341f1ed97a26c811abaebd37d62b833956792b7607ea3f15d001613c76de202
source_dir=$(userspace_fetch_extract curl "$version" "$url" "$sha256")
build_dir="$build_root/build/curl-$version"

export curl_cv_recv=yes
export curl_cv_send=yes
export ac_cv_func_gethostbyname=yes
export ac_cv_header_stdatomic_h=no
MYEMU_USERSPACE_LIBS="-lssl -lcrypto -lz -lc -lgcc" \
	userspace_autoconf_build curl "$source_dir" "$build_dir" \
	--with-openssl="$stage_dir/usr" \
	--with-ca-bundle=/etc/ssl/certs/ca-certificates.crt \
	--without-libpsl --without-libidn2 --without-zstd --without-brotli \
	--disable-threaded-resolver \
	--disable-ldap --disable-ldaps --disable-rtsp --disable-dict \
	--disable-telnet --disable-tftp --disable-pop3 --disable-imap \
	--disable-smb --disable-smtp --disable-gopher --disable-mqtt \
	--disable-manual --disable-docs
userspace_stage_ca_bundle
userspace_link_bins curl
userspace_verify_bins curl
