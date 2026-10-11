#!/bin/bash
# PID remains stable across exec, so this record names the QEMU process itself.
set -euo pipefail
record=$1
purpose=$2
shift 2
tmp="${record}.tmp.$$"
{
    printf 'PID=%s\n' "$$"
    printf 'PURPOSE=%s\n' "$purpose"
    printf 'COMMAND='
    printf '%q ' "$@"
    printf '\n'
} >"$tmp"
mv "$tmp" "$record"
exec "$@"
