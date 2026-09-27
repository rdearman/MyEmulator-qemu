#!/usr/bin/env bash
set -euo pipefail

prefix=${MYEMU_MUSL_PREFIX:?set MYEMU_MUSL_PREFIX}
compiler=${MYEMU_TARGET_GCC:?set MYEMU_TARGET_GCC}
root=$(cd "$(dirname "$0")/../.." && pwd)
sysroot=$(dirname "$prefix")
linker_script=${MYEMU_USER_LINKER_SCRIPT:-$root/toolchain/userspace/myemulator2-user.ld}

for arg in "$@"; do
    case "$arg" in
        -c|-r|-E|-M|-MM|-fsyntax-only)
            exec "$compiler" --sysroot="$sysroot" "$@" ;;
    esac
done

exec "$compiler" --sysroot="$sysroot" \
    "$prefix/lib/crt1.o" "$prefix/lib/crti.o" "$@" \
    "$prefix/lib/crtn.o" -L"$prefix/lib" -lc -Wl,-T,"$linker_script"
