#!/bin/sh
set -eu
ROOT=$(CDPATH= cd -- "$(dirname -- "$0")/../.." && pwd)
IMAGE=${1:-/tmp/rem-linux-0.2-desktop/rootfs.ext4}
[ "$#" -eq 0 ] || shift
exec python3 "$ROOT/distribution/tests/test-poweroff.py" "$IMAGE" "$@"
