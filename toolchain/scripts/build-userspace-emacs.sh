#!/usr/bin/env bash
set -euo pipefail

root=$(cd "$(dirname "$0")/../.." && pwd)
. "$root/toolchain/scripts/userspace-common.sh"

emacs_version=${MYEMU_EMACS_VERSION:-30.1}
emacs_source=${MYEMU_EMACS_SOURCE:-$root/.emacs-build/emacs-$emacs_version}
emacs_build=${MYEMU_EMACS_BUILD:-$root/.emacs-build/target}
emacs_binary=$emacs_build/src/emacs

if [[ ! -x "$emacs_binary" ]]; then
	JOBS="$jobs" "$root/toolchain/scripts/build-emacs-rem.sh"
fi
[[ -x "$emacs_binary" ]] ||
	{ echo "missing REM Emacs binary: $emacs_binary" >&2; exit 1; }
[[ -d "$emacs_source/lisp" && -d "$emacs_source/etc" ]] ||
	{ echo "missing Emacs Lisp/data source: $emacs_source" >&2; exit 1; }

mkdir -p "$stage_dir/usr/bin" "$stage_dir/usr/share/emacs/$emacs_version" \
	"$stage_dir/usr/share/terminfo"
install -m 0755 "$emacs_binary" "$stage_dir/usr/bin/emacs"
cp -a "$emacs_source/lisp" "$stage_dir/usr/share/emacs/$emacs_version/"
cp -a "$emacs_source/etc" "$stage_dir/usr/share/emacs/$emacs_version/"
if [[ -d "$emacs_source/leim" ]]; then
	cp -a "$emacs_source/leim" "$stage_dir/usr/share/emacs/$emacs_version/"
fi
if [[ -d "$root/.ncurses-build/stage/usr/share/terminfo" ]]; then
	cp -a "$root/.ncurses-build/stage/usr/share/terminfo/." \
		"$stage_dir/usr/share/terminfo/"
fi
userspace_link_bins emacs
userspace_verify_bins emacs
printf '%s\n' "$stage_dir/usr/bin/emacs"
