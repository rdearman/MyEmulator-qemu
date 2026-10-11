#!/bin/sh
set -eu
repo=$(CDPATH= cd -- "$(dirname -- "$0")/../../.." && pwd)
sysroot=${REM_MUSL_SYSROOT:-$repo/.rem-flight-build-20260924/musl-prefix}
cross=${REM_CROSS_CC:-$repo/.toolchain-install/bin/myemulator2-elf-gcc}
out=${1:-$repo/toolchain/userspace/rem-admin/sudo}
tmp=$(mktemp -d)
trap 'rm -rf "$tmp"' EXIT HUP INT TERM
[ -x "$cross" ] || { echo "missing REM cross compiler: $cross" >&2; exit 2; }
[ -f "$sysroot/lib/libcrypt.a" ] || { echo "missing REM libcrypt: $sysroot/lib/libcrypt.a" >&2; exit 2; }
cat > "$tmp/cc" <<'WRAPPER'
#!/bin/sh
set -eu
repo=$1; sysroot=$2; cross=$3; shift 3
compile_only=no
for arg do case "$arg" in -c|-E|-S) compile_only=yes ;; esac; done
if [ "$compile_only" = yes ]; then
  exec "$cross" --sysroot="$sysroot" -isystem "$sysroot/include" "$@"
fi
exec "$cross" --sysroot="$sysroot" -isystem "$sysroot/include" \
  "$sysroot/lib/crt1.o" "$sysroot/lib/crti.o" -nostdlib "$@" \
  "$sysroot/lib/crtn.o" -L"$sysroot/lib" -lcrypt -lc -lgcc \
  -Wl,-T,"$repo/toolchain/userspace/myemulator2-user.ld"
WRAPPER
chmod +x "$tmp/cc"
"$tmp/cc" "$repo" "$sysroot" "$cross" -O2 -static \
  -fno-unwind-tables -fno-asynchronous-unwind-tables -o "$out" \
  "$repo/toolchain/userspace/rem-admin/sudo.c"
chmod 755 "$out"
echo "Built $out"
