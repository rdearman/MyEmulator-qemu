#!/usr/bin/env bash
set -euo pipefail

root=$(cd "$(dirname "$0")/../.." && pwd)
"$root/toolchain/scripts/configure-linux.sh" >/dev/null
"$root/toolchain/scripts/build-linux.sh" >/dev/null
echo "Linux build completed: $root/.linux-build/build/vmlinux"
