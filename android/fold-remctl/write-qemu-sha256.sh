#!/data/data/com.termux/files/usr/bin/bash
set -euo pipefail

if [[ $# -ne 3 ]]; then
  printf 'usage: %s BASE_QEMU ACTIVE_QEMU OUTPUT\n' "$0" >&2
  exit 2
fi
base_qemu=$1
active_qemu=$2
output=$3
[[ -f "$base_qemu" && -r "$base_qemu" ]] || { printf 'unreadable base QEMU: %s\n' "$base_qemu" >&2; exit 2; }
[[ -f "$active_qemu" && -r "$active_qemu" ]] || { printf 'unreadable active QEMU: %s\n' "$active_qemu" >&2; exit 2; }
base_hash=$(sha256sum -- "$base_qemu")
base_hash=${base_hash%% *}
active_hash=$(sha256sum -- "$active_qemu")
active_hash=${active_hash%% *}
[[ "$base_hash" =~ ^[0-9a-fA-F]{64}$ && "$active_hash" =~ ^[0-9a-fA-F]{64}$ ]] || { printf 'sha256sum returned an invalid digest\n' >&2; exit 1; }
tmp="${output}.tmp.$$"
trap 'rm -f -- "$tmp"' EXIT HUP INT TERM
( umask 077; printf 'base %s\nactive %s\n' "$base_hash" "$active_hash" > "$tmp" )
mv -f -- "$tmp" "$output"
trap - EXIT HUP INT TERM
