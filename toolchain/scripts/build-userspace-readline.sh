#!/usr/bin/env bash
set -euo pipefail

root=$(cd "$(dirname "$0")/../.." && pwd)
. "$root/toolchain/scripts/userspace-common.sh"

version=8.2
url=https://ftp.gnu.org/gnu/readline/readline-$version.tar.gz
sha256=3feb7171f16a84ee82ca18a36d7b9be109a52c04f492a053331d7d1095007c35
source_dir=$(userspace_fetch_extract readline "$version" "$url" "$sha256")
build_dir="$build_root/build/readline-$version"

# The current REM musl port has setjmp/longjmp but no linkable sigsetjmp.
export bash_cv_func_sigsetjmp=missing
export MYEMU_USERSPACE_INSTALL_ARGS="install_examples="
MYEMU_USERSPACE_LIBS="-lncursesw -ltinfo -lc -lgcc" \
	userspace_autoconf_build readline "$source_dir" "$build_dir" \
	--with-curses
cat >"$build_dir/readline-smoke.c" <<'EOF'
#include <stdio.h>
#include <readline/readline.h>
int main(void) { return readline(0) != 0; }
EOF
(
	cd "$build_dir"
	userspace_set_cross_env
	"$CC" $CPPFLAGS $CFLAGS $LDFLAGS readline-smoke.c \
		-lreadline -lncursesw -ltinfo -o readline-smoke
	"$target_readelf" -h -l readline-smoke >"$log_dir/readelf-readline-smoke.txt"
	grep -q 'Class:.*ELF32' "$log_dir/readelf-readline-smoke.txt"
	grep -q 'Machine:.*MyEmulator2' "$log_dir/readelf-readline-smoke.txt"
)
printf '%s\n' "$stage_dir/usr/lib/libreadline.a"
