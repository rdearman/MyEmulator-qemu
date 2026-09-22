#!/usr/bin/env bash
set -euo pipefail

root=$(cd "$(dirname "$0")/../.." && pwd)
. "$root/toolchain/scripts/userspace-common.sh"

userspace_stage_sysroot
printf '%s\n' "$stage_dir"
