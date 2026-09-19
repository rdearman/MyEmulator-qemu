#!/usr/bin/env bash
set -euo pipefail
root=$(cd "$(dirname "$0")/../.." && pwd)
source "$root/toolchain/scripts/gcc-versions.env"
downloads=${MYEMU_GCC_DOWNLOADS:-$root/.toolchain-downloads}
mkdir -p "$downloads"
archive="$downloads/gcc-$GCC_VERSION.tar.xz"
if [[ ! -f "$archive" ]]; then curl --fail --location --output "$archive" "$GCC_URL"; fi
echo "$GCC_SHA256  $archive" | sha256sum --check --status -
echo "$archive"
