#!/usr/bin/env bash
set -euo pipefail

root=$(cd "$(dirname "$0")/../.." && pwd)
prefix=${MYEMU_GCC_PREFIX:-$root/.toolchain-install}
gcc_bin=${MYEMU_GCC_BIN:-$prefix/bin/myemulator2-elf-gcc}

if [[ ! -x "$gcc_bin" ]]; then
  echo "missing $gcc_bin; run toolchain/scripts/build-gcc.sh first" >&2
  exit 2
fi

exec python3 "$root/toolchain/tests/test-gcc.py" "$gcc_bin" "$root"
