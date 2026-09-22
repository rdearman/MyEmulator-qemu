#!/usr/bin/env bash
set -euo pipefail

root=$(cd "$(dirname "$0")/../.." && pwd)
. "$root/toolchain/scripts/userspace-common.sh"

emacs_version=${MYEMU_EMACS_VERSION:-30.1}
emacs_source=${MYEMU_EMACS_SOURCE:-$root/.emacs-build/emacs-$emacs_version}
emacs_build=${MYEMU_EMACS_BUILD:-$root/.emacs-build/target}
emacs_binary=$emacs_build/src/emacs
emacs_share=$stage_dir/usr/share/emacs
emacs_version_dir=$emacs_share/$emacs_version
emacs_manifest=$log_dir/emacs-runtime-manifest.txt

[[ -x "$emacs_binary" ]] ||
	{ echo "missing completed REM Emacs binary: $emacs_binary" >&2; exit 1; }
[[ -d "$emacs_source/lisp" && -d "$emacs_source/etc" && -d "$emacs_source/etc/charsets" ]] ||
	{ echo "missing Emacs Lisp/data source: $emacs_source" >&2; exit 1; }
[[ -s "$emacs_build/etc/DOC" ]] ||
	{ echo "missing generated Emacs DOC runtime file: $emacs_build/etc/DOC" >&2; exit 1; }
[[ -d "$root/.ncurses-build/stage/usr/share/terminfo" ]] ||
	{ echo "missing staged ncurses terminfo: $root/.ncurses-build/stage/usr/share/terminfo" >&2; exit 1; }

mkdir -p "$stage_dir/usr/bin" "$emacs_version_dir" "$emacs_share/site-lisp" \
	"$stage_dir/usr/share/terminfo"
install -m 0755 "$emacs_binary" "$stage_dir/usr/bin/emacs"
cp -a "$emacs_source/lisp" "$emacs_version_dir/"
cp -a "$emacs_source/etc" "$emacs_version_dir/"
install -m 0644 "$emacs_build/etc/DOC" "$emacs_version_dir/etc/DOC"
if [[ -d "$emacs_source/leim" ]]; then
	cp -a "$emacs_source/leim" "$emacs_version_dir/"
fi
mkdir -p "$emacs_version_dir/site-lisp"
cp -a "$root/.ncurses-build/stage/usr/share/terminfo/." \
	"$stage_dir/usr/share/terminfo/"
userspace_link_bins emacs
userspace_verify_bins emacs

[[ -s "$stage_dir/usr/bin/emacs" ]] ||
	{ echo "missing staged Emacs executable" >&2; exit 1; }
[[ -s "$emacs_version_dir/etc/DOC" ]] ||
	{ echo "missing staged Emacs DOC file" >&2; exit 1; }
[[ -d "$emacs_version_dir/lisp" && -d "$emacs_version_dir/etc/charsets" ]] ||
	{ echo "missing staged Emacs Lisp or charset data" >&2; exit 1; }
[[ $(find "$emacs_version_dir/lisp" -type f | wc -l) -gt 0 ]] ||
	{ echo "staged Emacs lisp tree is empty" >&2; exit 1; }
[[ $(find "$emacs_version_dir/etc/charsets" -type f | wc -l) -gt 0 ]] ||
	{ echo "staged Emacs charset tree is empty" >&2; exit 1; }
[[ $(find "$stage_dir/usr/share/terminfo" -type f | wc -l) -gt 0 ]] ||
	{ echo "staged terminfo tree is empty" >&2; exit 1; }

{
	printf 'emacs_binary=%s\n' "$stage_dir/usr/bin/emacs"
	printf 'runtime_root=%s\n' "$emacs_version_dir"
	printf 'lisp_files=%s\n' "$(find "$emacs_version_dir/lisp" -type f | wc -l)"
	printf 'etc_files=%s\n' "$(find "$emacs_version_dir/etc" -type f | wc -l)"
	printf 'charset_files=%s\n' "$(find "$emacs_version_dir/etc/charsets" -type f | wc -l)"
	printf 'leim_files=%s\n' "$(find "$emacs_version_dir/leim" -type f 2>/dev/null | wc -l)"
	printf 'terminfo_files=%s\n' "$(find "$stage_dir/usr/share/terminfo" -type f | wc -l)"
	printf 'site_lisp_dirs=%s:%s\n' "$emacs_version_dir/site-lisp" "$emacs_share/site-lisp"
	find "$emacs_version_dir" -maxdepth 2 \( -type d -o -type f \) -printf '%P\n' | sort
} >"$emacs_manifest"

printf '%s\n' "$stage_dir/usr/bin/emacs"
