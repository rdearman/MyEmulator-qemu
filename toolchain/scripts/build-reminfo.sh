#!/usr/bin/env bash
set -euo pipefail

root="$(cd "$(dirname "$0")/../.." && pwd)"
prefix="${MYEMU_TOOLCHAIN_PREFIX:-$root/.toolchain-install}"
out="${1:-$root/.toolchain-build/reminfo}"
gcc="$prefix/bin/myemulator2-elf-gcc"
as="$prefix/bin/myemulator2-elf-as"
inc="$root/toolchain/userspace/minilibc/include"

[[ -x "$gcc" ]] || { echo "missing REM compiler: $gcc" >&2; exit 2; }
[[ -x "$as" ]] || { echo "missing REM assembler: $as" >&2; exit 2; }
mkdir -p "$(dirname "$out")"
tmp="$(mktemp -d "${TMPDIR:-/tmp}/reminfo-build.XXXXXX")"
trap 'rm -rf "$tmp"' EXIT
cflags=(-ffreestanding -fno-builtin -nostdinc -O2 -I "$inc")

"$as" -o "$tmp/crt0.o" "$root/toolchain/userspace/minilibc/crt0-linux.S"
"$as" -o "$tmp/syscall.o" "$root/toolchain/userspace/minilibc/syscall.S"
for source in string.c stdlib.c posix.c stdio.c; do
	"$gcc" "${cflags[@]}" -c "$root/toolchain/userspace/minilibc/$source" \
		-o "$tmp/${source%.c}.o"
done
"$gcc" "${cflags[@]}" -c "$root/toolchain/examples/reminfo.c" -o "$tmp/reminfo.o"
"$gcc" -nostdlib -Ttext=0x02000000 -o "$out" \
	"$tmp/crt0.o" "$tmp/syscall.o" "$tmp/string.o" "$tmp/stdlib.o" \
	"$tmp/posix.o" "$tmp/stdio.o" "$tmp/reminfo.o" -lgcc
chmod 0755 "$out"
file "$out"
printf 'Built REM diagnostic: %s\n' "$out"
