#!/usr/bin/env bash
set -euo pipefail

root=$(cd "$(dirname "$0")/../.." && pwd)
tree=${MYEMU_USERSPACE_TREE:-$root/.userspace-stage}
readelf=${MYEMU_TARGET_READELF:-$root/.toolchain-install/bin/myemulator2-elf-readelf}
errors=0
elf_count=0
archive_count=0

usage() {
	cat >&2 <<'EOF'
usage: validate-userspace.sh [ROOTFS] [--tree TREE]

Validate REM ELF32 objects, static archive members, dependencies, symlinks,
script interpreters, and the Emacs 30.1 runtime tree.
EOF
	exit 2
}

while (($#)); do
	case "$1" in
		--tree) (($# >= 2)) || usage; tree=$2; shift 2 ;;
		--help|-h) usage ;;
		-*) echo "unknown option: $1" >&2; usage ;;
		*) [[ "$tree" == "$root/.userspace-stage" ]] || usage; tree=$1; shift ;;
	esac
done

tree=$(realpath -e "$tree")
[[ -d "$tree" ]] || { echo "validation tree is not a directory: $tree" >&2; exit 1; }
[[ -x "$readelf" ]] || { echo "REM readelf is missing: $readelf" >&2; exit 1; }
fail() { echo "ERROR: $*" >&2; errors=$((errors + 1)); }

check_elf() {
	local file=$1 label=${2:-$1} header dynamic
	header=$(mktemp)
	if ! "$readelf" -h -l "$file" >"$header" 2>/dev/null; then
		rm -f "$header"; fail "$label is not readable as ELF"; return
	fi
	elf_count=$((elf_count + 1))
	grep -q 'Class:.*ELF32' "$header" || fail "$label is not ELF32"
	grep -q 'Machine:.*MyEmulator2' "$header" || fail "$label is not a REM MyEmulator2 ELF"
	grep -q 'INTERP' "$header" && fail "$label has a dynamic interpreter"
	dynamic=$("$readelf" -d "$file" 2>/dev/null || true)
	grep -q 'NEEDED' <<<"$dynamic" && fail "$label has dynamic library dependencies"
	rm -f "$header"
}

check_elf_file() {
	local file=$1
	if [[ "$(od -An -tx1 -N4 "$file" 2>/dev/null | tr -d ' \n')" == 7f454c46 ]]; then
		check_elf "$file"
	fi
}

check_archive() {
	local archive=$1 member temp
	archive_count=$((archive_count + 1))
	temp=$(mktemp -d)
	if ! (cd "$temp" && ar x "$archive") 2>/dev/null; then
		fail "cannot unpack archive $archive"; rmdir "$temp" 2>/dev/null || true; return
	fi
	while IFS= read -r -d '' member; do
		check_elf "$member" "$archive member ${member#$temp/}"
	done < <(find "$temp" -type f -print0)
	rm -rf "$temp"
}

while IFS= read -r -d '' file; do
	case "$file" in *.la) continue ;; esac
	check_elf_file "$file"
	case "$file" in *.a) check_archive "$file" ;; esac
done < <(find -P "$tree" -type f -print0)

while IFS= read -r -d '' file; do
	[[ -e "$file" ]] || fail "dangling symlink: ${file#$tree/} -> $(readlink "$file")"
done < <(find -P "$tree" -type l -print0)

check_script() {
	local file=$1 line interpreter
	line=$(head -n 1 "$file" 2>/dev/null || true)
	[[ "$line" == '#!'* ]] || { fail "executable is neither ELF nor shebang: ${file#$tree/}"; return; }
	interpreter=${line#\#!}; interpreter=${interpreter%%[[:space:]]*}
	[[ "$interpreter" == /* ]] || { fail "relative script interpreter in ${file#$tree/}"; return; }
	[[ -x "$tree$interpreter" ]] || fail "missing script interpreter $interpreter for ${file#$tree/}"
}

while IFS= read -r -d '' file; do
	case "$file" in *.la) continue ;; esac
	if [[ "$(od -An -tx1 -N4 "$file" 2>/dev/null | tr -d ' \n')" != 7f454c46 ]]; then
		check_script "$file"
	fi
done < <(find -P "$tree" -type f -perm /111 -print0)

required=(usr/bin/emacs usr/share/emacs/30.1/lisp usr/share/emacs/30.1/etc
	usr/share/emacs/30.1/etc/DOC usr/share/emacs/30.1/etc/charsets
	usr/share/emacs/30.1/leim usr/share/emacs/30.1/site-lisp
	usr/share/emacs/site-lisp usr/share/terminfo)
for relative in "${required[@]}"; do
	[[ -e "$tree/$relative" ]] || fail "missing Emacs/runtime path: /$relative"
done
for relative in usr/share/emacs/30.1/lisp usr/share/emacs/30.1/etc/charsets usr/share/terminfo; do
	if [[ -d "$tree/$relative" ]] && ! find "$tree/$relative" -type f -print -quit | grep -q .; then
		fail "runtime directory is empty: /$relative"
	fi
done
if [[ -L "$tree/bin/emacs" ]]; then
	[[ "$(readlink "$tree/bin/emacs")" == '../usr/bin/emacs' ]] || fail "/bin/emacs has the wrong target"
elif [[ -e "$tree/bin/emacs" ]]; then
	fail "/bin/emacs is not the expected symlink"
fi

printf 'validated %d REM ELF objects and %d static archives in %s\n' "$elf_count" "$archive_count" "$tree"
if ((errors)); then
	echo "validation failed with $errors error(s)" >&2
	exit 1
fi
