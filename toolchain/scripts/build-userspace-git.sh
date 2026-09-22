#!/usr/bin/env bash
set -euo pipefail

root=$(cd "$(dirname "$0")/../.." && pwd)
. "$root/toolchain/scripts/userspace-common.sh"

version=2.48.1
url=https://www.kernel.org/pub/software/scm/git/git-$version.tar.xz
sha256=1c5d545f5dc1eb51e95d2c50d98fdf88b1a36ba1fa30e9ae5d5385c6024f82ad
source_dir=$(userspace_fetch_extract git "$version" "$url" "$sha256")
build_dir="$build_root/build/git-$version"

userspace_require_target
rm -rf "$build_dir"
cp -a "$source_dir" "$build_dir"
(
	cd "$build_dir"
	userspace_set_cross_env
	make -j"$jobs" \
		CC="$CC" AR="$AR" CPPFLAGS="-I. -Icompat/regex $CPPFLAGS" \
		CFLAGS="$CFLAGS" LDFLAGS="$LDFLAGS" \
		prefix=/usr V=1 NO_GETTEXT=YesPlease NO_TCLTK=YesPlease \
		NO_PERL=YesPlease NO_PYTHON=YesPlease NO_ICONV=YesPlease \
		NO_EXPAT=YesPlease NO_REGEX=NeedsStartEnd NO_INSTALL_HARDLINKS=YesPlease \
		CURLDIR="$stage_dir/usr" OPENSSLDIR="$stage_dir/usr" \
		EXTLIBS="-lcurl -lssl -lcrypto -lz -lc -lgcc" \
		all
	make DESTDIR="$stage_dir" prefix=/usr \
		CC="$CC" AR="$AR" CPPFLAGS="-I. -Icompat/regex $CPPFLAGS" \
		CFLAGS="$CFLAGS" LDFLAGS="$LDFLAGS" \
		NO_INSTALL_HARDLINKS=YesPlease NO_REGEX=NeedsStartEnd \
		NO_EXPAT=YesPlease \
		CURLDIR="$stage_dir/usr" OPENSSLDIR="$stage_dir/usr" \
		EXTLIBS="-lcurl -lssl -lcrypto -lz -lc -lgcc" \
		NO_GETTEXT=YesPlease NO_TCLTK=YesPlease NO_PERL=YesPlease \
		NO_PYTHON=YesPlease install
)
userspace_link_bins git git-receive-pack git-shell git-upload-archive git-upload-pack
userspace_verify_bins git
