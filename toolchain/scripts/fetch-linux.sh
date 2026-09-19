#!/usr/bin/env bash
set -euo pipefail

root=$(cd "$(dirname "$0")/../.." && pwd)
source="$root/.linux-downloads"
version=${MYEMU_LINUX_VERSION:-6.12.1}
url="https://cdn.kernel.org/pub/linux/kernel/v6.x/linux-${version}.tar.xz"
archive="$source/linux-${version}.tar.xz"
mkdir -p "$source"
if [[ ! -f "$archive" ]]; then
  curl --fail --location --retry 2 --output "$archive" "$url"
fi
case "$version" in
  6.12.1)
    expected=0193b1d86dd372ec891bae799f6da20deef16fc199f30080a4ea9de8cef0c619
    ;;
  *)
    echo "No pinned checksum for Linux $version" >&2
    exit 2
    ;;
esac
actual=$(sha256sum "$archive" | awk '{print $1}')
[[ "$actual" == "$expected" ]] || { echo "Linux checksum mismatch" >&2; exit 1; }
echo "$archive"
