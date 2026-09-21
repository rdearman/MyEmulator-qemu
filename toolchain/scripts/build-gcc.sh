#!/usr/bin/env bash
set -euo pipefail

root=$(cd "$(dirname "$0")/../.." && pwd)
source "$root/toolchain/scripts/gcc-versions.env"

downloads=${MYEMU_GCC_DOWNLOADS:-$root/.toolchain-downloads}
build_root=${MYEMU_GCC_BUILD:-$root/.toolchain-build/gcc}
prefix=${MYEMU_GCC_PREFIX:-$root/.toolchain-install}
binutils_prefix=${MYEMU_TOOLCHAIN_PREFIX:-$root/.toolchain-install}
target=${MYEMU_TARGET_TRIPLET:-myemulator2-elf}
hosted_linux=${MYEMU_HOSTED_LINUX:-0}
jobs=${JOBS:-$(getconf _NPROCESSORS_ONLN 2>/dev/null || echo 2)}
archive="$downloads/gcc-$GCC_VERSION.tar.xz"
source_dir="$build_root/gcc-$GCC_VERSION"
build_dir="$build_root/build"

if [[ ! -f "$archive" ]]; then
  "$root/toolchain/scripts/fetch-gcc.sh"
fi

mkdir -p "$build_root"
if [[ "$hosted_linux" == 1 ]]; then
  mkdir -p "$prefix/$target/lib"
  install -m 0644 "$root/toolchain/userspace/myemulator2-user.ld" \
    "$prefix/$target/lib/myemulator2-user.ld"
fi
if [[ ! -d "$source_dir/gcc" ]]; then
  tar -xf "$archive" -C "$build_root"
fi
python3 "$root/toolchain/scripts/prepare-gcc-source.py" "$source_dir"

# GCC's own prerequisite helper is used in the unpacked, ignored source tree.
# This keeps a clean checkout independent of host distro development-package
# names while keeping all downloaded/generated material outside Git.
if [[ ! -d "$source_dir/gmp" || ! -f "$source_dir/gmp/gmp.h" ]]; then
  (cd "$source_dir" && ./contrib/download_prerequisites)
fi

rm -rf "$build_dir"
mkdir -p "$build_dir"
site_file="$build_root/config.site"
cat >"$site_file" <<'EOF'
# The GCC release's nested libcpp configure probes a dependency helper that
# is not needed for this reproducible cross build.
ac_cv_prog_CXX_dependencies_compiler_type=none
ac_cv_prog_CC_dependencies_compiler_type=none
am_cv_CXX_dependencies_compiler_type=none
am_cv_CC_dependencies_compiler_type=none
EOF
cd "$build_dir"

configure_args=(
  --target="$target"
  --prefix="$prefix" \
  --with-as="$binutils_prefix/bin/myemulator2-elf-as" \
  --with-ld="$binutils_prefix/bin/myemulator2-elf-ld" \
  --disable-nls \
  --disable-libssp \
  --disable-libquadmath \
  --disable-libgomp \
  --disable-libatomic \
  --disable-libstdcxx \
  --disable-threads \
  --disable-dependency-tracking \
  --disable-lto \
  --disable-gcov \
  --disable-shared \
  --disable-multilib \
  --enable-languages=c
)
if [[ "$hosted_linux" == 1 ]]; then
  configure_args+=(--with-sysroot="$prefix/$target" --disable-newlib)
else
  configure_args+=(--with-sysroot="$prefix/myemulator2-elf" --without-headers --with-newlib)
fi
CONFIG_SITE="$site_file" "$source_dir/configure" "${configure_args[@]}"

make -j"$jobs" all-gcc all-target-libgcc
make install-gcc install-target-libgcc

echo "installed MyEmulator2 GCC $GCC_VERSION under $prefix"
