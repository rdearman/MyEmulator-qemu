#!/usr/bin/env bash
set -euo pipefail

root=$(cd "$(dirname "$0")/../.." && pwd)
prefix=${MYEMU_TOOLCHAIN_PREFIX:-$root/.toolchain-install}
bin="$prefix/bin"
for tool in as ld objdump objcopy readelf nm ar ranlib; do
  test -x "$bin/myemulator2-elf-$tool" || {
    echo "missing $bin/myemulator2-elf-$tool; run build-binutils.sh first" >&2
    exit 1
  }
done
python3 "$root/toolchain/tests/test-binutils.py" "$bin" "$root"
python3 "$root/toolchain/tests/test-isa.py" "$bin"
