#!/bin/sh
# Current native REM as crashes on chibicc's source .file directives.
# These records add no useful information for this chibicc build (it emits no
# DWARF line rows); preserve every other generated assembly line unchanged.
set -eu
[ "$#" -eq 2 ] || { echo "usage: $0 input.s output.s" >&2; exit 2; }
input=$1
output=$2
while IFS= read -r line || [ -n "$line" ]; do
    case "$line" in
        *'.file '*) ;;
        *) printf '%s\n' "$line" ;;
    esac
done <"$input" >"$output"
