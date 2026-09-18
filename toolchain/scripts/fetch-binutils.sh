#!/usr/bin/env bash
set -euo pipefail

root=$(cd "$(dirname "$0")/../.." && pwd)
source "$root/toolchain/scripts/versions.env"
downloads=${MYEMU_TOOLCHAIN_DOWNLOADS:-$root/.toolchain-downloads}
mkdir -p "$downloads"
archive="$downloads/binutils-$BINUTILS_VERSION.tar.xz"

if [[ ! -f "$archive" ]]; then
  curl --fail --location --output "$archive" "$BINUTILS_URL"
fi

if [[ "$BINUTILS_SHA256" != SKIP_* ]]; then
  echo "$BINUTILS_SHA256  $archive" | sha256sum --check --status -
fi

echo "$archive"
