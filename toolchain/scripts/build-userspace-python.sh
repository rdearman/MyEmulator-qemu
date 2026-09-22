#!/usr/bin/env bash
set -euo pipefail

root=$(cd "$(dirname "$0")/../.." && pwd)
. "$root/toolchain/scripts/userspace-common.sh"

version=3.12.9
url=https://www.python.org/ftp/python/$version/Python-$version.tar.xz
sha256=7220835d9f90b37c006e9842a8dff4580aaca4318674f947302b8d28f3f81112
source_dir=$(userspace_fetch_extract Python "$version" "$url" "$sha256" "Python-$version")
host_build="$build_root/build/python-$version-host"
build_dir="$build_root/build/python-$version-target"

userspace_require_target
userspace_patch_config_sub "$source_dir"
# The current GCC target describes wchar_t as signed while musl exposes it as
# unsigned. Their 32-bit representation is identical, but GCC diagnoses this
# conditional expression before the existing pointer-sign compatibility flag
# can apply.
sed -i 's/return sep ? sep + 1 : L"";/return sep ? sep + 1 : (const wchar_t *)L"";/' \
	"$source_dir/Python/initconfig.c"
rm -rf "$host_build" "$build_dir"
mkdir -p "$host_build" "$build_dir/Modules"

(
	cd "$host_build"
	"$source_dir/configure" --prefix="$host_build/install" \
		--without-ensurepip --disable-test-modules
	make -j"$jobs" python
)

cat >"$build_dir/Modules/Setup.local" <<EOF
*static*
_ssl _ssl.c -I$stage_dir/usr/include -L$stage_dir/usr/lib -lssl -lcrypto
_hashlib _hashopenssl.c -I$stage_dir/usr/include -L$stage_dir/usr/lib -lcrypto
zlib zlibmodule.c -I$stage_dir/usr/include -L$stage_dir/usr/lib -lz
_bz2 _bz2module.c -I$stage_dir/usr/include -L$stage_dir/usr/lib -lbz2
_lzma _lzmamodule.c -I$stage_dir/usr/include -L$stage_dir/usr/lib -llzma
_sqlite3 _sqlite/blob.c _sqlite/connection.c _sqlite/cursor.c _sqlite/microprotocols.c _sqlite/module.c _sqlite/prepare_protocol.c _sqlite/row.c _sqlite/statement.c _sqlite/util.c -I$stage_dir/usr/include -L$stage_dir/usr/lib -lsqlite3
readline readline.c -I$stage_dir/usr/include -L$stage_dir/usr/lib -lreadline -lncursesw -ltinfo
_curses _cursesmodule.c -I$stage_dir/usr/include -L$stage_dir/usr/lib -lncursesw -ltinfo
_curses_panel _curses_panel.c -I$stage_dir/usr/include -L$stage_dir/usr/lib -lpanelw -lncursesw -ltinfo
EOF

(
	cd "$build_dir"
	userspace_set_cross_env
	export ac_cv_file__dev_ptmx=yes
	export ac_cv_file__dev_ptc=no
	export ac_cv_buggy_getaddrinfo=no
	export ac_cv_func_getentropy=no
	export ac_cv_func_setpgrp_void=yes
	export ac_cv_working_tzset=yes
	"$source_dir/configure" \
		--build="$build_triplet" \
		--host="$target_triplet" \
		--prefix=/usr \
		--with-build-python="$host_build/python" \
		--disable-shared \
		--without-ensurepip \
		--disable-test-modules \
		--without-static-libpython
	make -j"$jobs" \
		LIBS="-lssl -lcrypto -latomic_compat -lsqlite3 -llzma -lbz2 -lz -lreadline -lncursesw -ltinfo -lm -lc -lgcc"
	make DESTDIR="$stage_dir" install \
		LIBS="-lssl -lcrypto -latomic_compat -lsqlite3 -llzma -lbz2 -lz -lreadline -lncursesw -ltinfo -lm -lc -lgcc"
)

ln -sfn python3 "$stage_dir/usr/bin/python"
userspace_link_bins python python3
userspace_verify_bins python3

stdlib="$stage_dir/usr/lib/python3.12"
[[ -f "$stdlib/os.py" && -f "$stdlib/ssl.py" && -f "$stdlib/sqlite3/__init__.py" ]] ||
	{ echo "Python standard library is incomplete under $stdlib" >&2; exit 1; }

manifest="$log_dir/python-runtime-manifest.txt"
{
	printf 'Python %s\n' "$version"
	printf 'stdlib files: %s\n' "$(find "$stdlib" -type f | wc -l)"
	printf 'static modules:\n'
	sed -n '/^# Built-in modules/,/^# Dynamic modules/p' "$build_dir/pyconfig.h" 2>/dev/null || true
	printf 'required library modules:\n'
	for module in os.py ssl.py hashlib.py bz2.py lzma.py sqlite3/__init__.py \
		readline.py curses/__init__.py multiprocessing/__init__.py \
		asyncio/__init__.py venv/__init__.py; do
		[[ -e "$stdlib/$module" ]] || {
			echo "missing Python library module: $module" >&2
			exit 1
		}
		printf '  %s\n' "$module"
	done
} >"$manifest"
printf '%s\n' "$stage_dir/usr/bin/python3"
