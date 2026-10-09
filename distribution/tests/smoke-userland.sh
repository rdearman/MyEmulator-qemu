#!/bin/sh
set -eu
ROOT=$(CDPATH= cd -- "$(dirname -- "$0")/../.." && pwd)
IMAGE=${1:-/tmp/rem-linux-0.2-desktop/rootfs.ext4}
exec python3 "$ROOT/distribution/tests/smoke-userland.py" "$IMAGE"
