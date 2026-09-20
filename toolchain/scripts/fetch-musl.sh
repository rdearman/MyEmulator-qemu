#!/usr/bin/env bash
set -euo pipefail

version=1.2.5
sha256=a9a118bbe84d8764da0ea0d28b3ab3fae8477fc7e4085d90102b8596fc7c75e4
cache=${MYEMU_SOURCE_CACHE:-${TMPDIR:-/tmp}/myemulator2-sources}
archive="$cache/musl-$version.tar.gz"
mkdir -p "$cache"
if [[ ! -f "$archive" ]]; then
	url="https://musl.libc.org/releases/musl-$version.tar.gz"
	curl -fL "$url" -o "$archive"
fi
printf '%s  %s\n' "$sha256" "$archive" | sha256sum -c -
printf '%s\n' "$archive"
