#!/usr/bin/env bash
set -euo pipefail

root=$(cd "$(dirname "$0")/../.." && pwd)
. "$root/toolchain/scripts/userspace-common.sh"

version=5.2.37
url=https://ftp.gnu.org/gnu/bash/bash-$version.tar.gz
sha256=9599b22ecd1d5787ad7d3b7bf0c59f312b3396d1e281175dd1f8a4014da621ff
source_dir=$(userspace_fetch_extract bash "$version" "$url" "$sha256")
build_dir="$build_root/build/bash-$version"

export bash_cv_func_sigsetjmp=missing
export bash_cv_must_reinstall_sighandlers=no
export bash_cv_job_control_missing=present
export ac_cv_func_setvbuf_reversed=no
export MYEMU_USERSPACE_CFLAGS="${MYEMU_USERSPACE_CFLAGS:--O2 -static -ffreestanding -std=gnu17 -Wno-error=pointer-sign -isystem$musl_prefix/include -I$stage_dir/usr/include}"
export MYEMU_USERSPACE_MAKE_ARGS="LOCAL_LDFLAGS="
export MYEMU_USERSPACE_PRESERVE_MAKE_LIBS=1
userspace_configure_fixup() {
	sed -i 's/^LOCAL_LDFLAGS = -rdynamic$/LOCAL_LDFLAGS =/' Makefile
	sed -i '/$(PURIFY) $(CC).*$(OBJECTS) $(LIBS)$/s|$(LIBS)$|$(BUILTINS_LIB) $(LIBRARIES) -ldl -lreadline -lncursesw -ltinfo -lc -lgcc|' Makefile
}
MYEMU_USERSPACE_LIBS="-lreadline -lncursesw -ltinfo -lc -lgcc" \
	userspace_autoconf_build bash "$source_dir" "$build_dir" \
	--with-installed-readline --without-bash-malloc --disable-nls
ln -sfn bash "$stage_dir/usr/bin/sh"
userspace_link_bins bash sh
userspace_verify_bins bash
