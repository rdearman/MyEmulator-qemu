#!/usr/bin/env bash

set -euo pipefail

userspace_init() {
	root=$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)
	download_dir=${MYEMU_USERSPACE_DOWNLOADS:-$root/.userspace-downloads}
	build_root=${MYEMU_USERSPACE_BUILD:-$root/.userspace-build}
	stage_dir=${MYEMU_USERSPACE_STAGE:-$root/.userspace-stage}
	tool_prefix=${MYEMU_TOOLCHAIN_PREFIX:-$root/.toolchain-install}
	musl_prefix=${MYEMU_MUSL_PREFIX:-$root/.musl-install}
	target_prefix=${MYEMU_TARGET_PREFIX:-$tool_prefix/bin/myemulator2-elf-}
	target_gcc=${MYEMU_TARGET_GCC:-${target_prefix}gcc}
	target_readelf=${MYEMU_TARGET_READELF:-${target_prefix}readelf}
	target_triplet=${MYEMU_USERSPACE_HOST:-myemulator2-linux-musl}
	build_triplet=${MYEMU_BUILD_TRIPLET:-$(gcc -dumpmachine 2>/dev/null || printf 'x86_64-pc-linux-gnu')}
	cc_wrapper=${MYEMU_USERSPACE_CC:-$root/toolchain/scripts/myemulator2-musl-gcc}
	linker_script=${MYEMU_USERSPACE_LDSCRIPT:-$root/toolchain/userspace/myemulator2-user.ld}
	jobs=${JOBS:-$(getconf _NPROCESSORS_ONLN 2>/dev/null || printf '1')}
	log_dir="$build_root/logs"
	mkdir -p "$download_dir" "$build_root/src" "$build_root/build" "$log_dir"
	userspace_prepare_stage
}

userspace_prepare_stage() {
	mkdir -p "$stage_dir/bin" "$stage_dir/usr/bin" "$stage_dir/usr/include" \
		"$stage_dir/usr/lib" "$stage_dir/usr/share/terminfo"
}

userspace_require_target() {
	[[ -x "$target_gcc" ]] ||
		{ echo "missing REM GCC: $target_gcc" >&2; exit 2; }
	[[ -x "$target_readelf" ]] ||
		{ echo "missing REM readelf: $target_readelf" >&2; exit 2; }
	[[ -f "$musl_prefix/lib/libc.a" ]] ||
		{ echo "missing REM musl sysroot: $musl_prefix" >&2; exit 2; }
	[[ -x "$cc_wrapper" ]] ||
		{ echo "missing musl GCC wrapper: $cc_wrapper" >&2; exit 2; }
	[[ -f "$linker_script" ]] ||
		{ echo "missing userspace linker script: $linker_script" >&2; exit 2; }
}

userspace_fetch_archive() {
	local name=$1 url=$2 sha256=$3 archive=$download_dir/${url##*/}
	if [[ ! -f "$archive" ]]; then
		curl -fL --retry 3 "$url" -o "$archive"
	fi
	printf '%s  %s\n' "$sha256" "$archive" | sha256sum -c - >&2
	printf '%s\n' "$archive"
}

userspace_extract_archive() {
	local name=$1 archive=$2 dirname=$3 source_dir=$build_root/src/$dirname
	if [[ ! -d "$source_dir" ]]; then
		mkdir -p "$build_root/src"
		tar -xf "$archive" -C "$build_root/src"
	fi
	[[ -d "$source_dir" ]] ||
		{ echo "archive for $name did not extract $dirname" >&2; exit 2; }
	printf '%s\n' "$source_dir"
}

userspace_patch_config_sub() {
	local source_dir=$1 config_sub
	while IFS= read -r -d '' config_sub; do
		if ! grep -q 'myemulator2' "$config_sub"; then
			if grep -q '| moxie \\' "$config_sub"; then
				sed -i 's/| moxie \\/| moxie | myemulator2 \\/' "$config_sub"
			else
				echo "cannot teach $config_sub about myemulator2" >&2
				exit 2
			fi
		fi
	done < <(find "$source_dir" -name config.sub -type f -print0)
}

userspace_set_cross_env() {
	export MYEMU_MUSL_PREFIX="$musl_prefix"
	export MYEMU_TARGET_GCC="$target_gcc"
	export CC="$cc_wrapper"
	export AR="${target_prefix}ar"
	export RANLIB="${target_prefix}ranlib"
	export NM="${target_prefix}nm"
	export STRIP="${target_prefix}strip"
	export PKG_CONFIG="${PKG_CONFIG:-/bin/false}"
	export CPPFLAGS="${MYEMU_USERSPACE_CPPFLAGS:--isystem$musl_prefix/include -I$stage_dir/usr/include}"
	export CFLAGS="${MYEMU_USERSPACE_CFLAGS:--O2 -static -ffreestanding -Wno-error=pointer-sign -isystem$musl_prefix/include -I$stage_dir/usr/include}"
	export LDFLAGS="${MYEMU_USERSPACE_LDFLAGS:--static -L$stage_dir/usr/lib -L$musl_prefix/lib -Wl,-T,$linker_script}"
	export LIBS="${MYEMU_USERSPACE_LIBS:--lc -lgcc}"
	export FORCE_UNSAFE_CONFIGURE=1
}

userspace_autoconf_build() {
	local name=$1 source_dir=$2 build_dir=$3
	shift 3
	userspace_require_target
	userspace_patch_config_sub "$source_dir"
	rm -rf "$build_dir"
	mkdir -p "$build_dir"
	(
		cd "$build_dir"
		userspace_set_cross_env
		"$source_dir/configure" \
			--build="$build_triplet" \
			--host="$target_triplet" \
			--prefix=/usr \
			--disable-shared \
			--enable-static \
			"$@"
		make -j"$jobs" LIBS="$LIBS"
		make DESTDIR="$stage_dir" LIBS="$LIBS" ${MYEMU_USERSPACE_INSTALL_ARGS:-} install
	)
}

userspace_fetch_extract() {
	local name=$1 version=$2 url=$3 sha256=$4 dirname
	dirname=${5:-$name-$version}
	local archive
	archive=$(userspace_fetch_archive "$name" "$url" "$sha256")
	userspace_extract_archive "$name" "$archive" "$dirname"
}

userspace_link_bins() {
	local bin
	mkdir -p "$stage_dir/bin"
	for bin in "$@"; do
		[[ -e "$stage_dir/usr/bin/$bin" ]] || continue
		ln -sfn "../usr/bin/$bin" "$stage_dir/bin/$bin"
	done
}

userspace_verify_elf() {
	local path=$1 rel log
	rel=${path#"$stage_dir"/}
	log="$log_dir/readelf-${rel//\//_}.txt"
	"$target_readelf" -h -l "$path" >"$log"
	grep -q 'Class:.*ELF32' "$log"
	grep -q 'Machine:.*MyEmulator2' "$log"
	if grep -q 'INTERP' "$log"; then
		echo "$path unexpectedly has an interpreter segment" >&2
		exit 1
	fi
	printf '%s\n' "$log"
}

userspace_verify_bins() {
	local bin path
	for bin in "$@"; do
		path="$stage_dir/usr/bin/$bin"
		[[ -x "$path" ]] ||
			{ echo "missing staged binary: $path" >&2; exit 1; }
		userspace_verify_elf "$path" >/dev/null
	done
}

userspace_verify_stage_elfs() {
	local file rel log summary=$log_dir/stage-readelf-summary.txt
	: >"$summary"
	while IFS= read -r -d '' file; do
		rel=${file#"$stage_dir"/}
		if "$target_readelf" -h -l "$file" >"$log_dir/readelf-${rel//\//_}.txt" 2>/dev/null; then
			log="$log_dir/readelf-${rel//\//_}.txt"
			grep -q 'Class:.*ELF32' "$log"
			grep -q 'Machine:.*MyEmulator2' "$log"
			if grep -q 'INTERP' "$log"; then
				echo "$file unexpectedly has an interpreter segment" >&2
				exit 1
			fi
			printf '%s\n' "$rel" >>"$summary"
		fi
	done < <(find "$stage_dir" -type f -perm /111 -print0)
}

userspace_stage_sysroot() {
	userspace_require_target
	mkdir -p "$stage_dir/usr/include" "$stage_dir/usr/lib" "$stage_dir/usr/share/terminfo"
	cp -a "$musl_prefix/include/." "$stage_dir/usr/include/"
	find "$musl_prefix/lib" -maxdepth 1 \( -name '*.a' -o -name 'crt*.o' \) \
		-exec cp -a {} "$stage_dir/usr/lib/" \;
	if [[ -d "$root/.ncurses-build/stage/usr/share/terminfo" ]]; then
		cp -a "$root/.ncurses-build/stage/usr/share/terminfo/." \
			"$stage_dir/usr/share/terminfo/"
	fi
}

userspace_init
