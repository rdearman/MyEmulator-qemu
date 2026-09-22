#!/usr/bin/env bash
set -euo pipefail

root=$(cd "$(dirname "$0")/../.." && pwd)
stage=${MYEMU_USERSPACE_STAGE:-$root/.userspace-stage}
destination=

usage() {
	cat >&2 <<'EOF'
usage: integrate-userspace.sh ROOTFS [--stage STAGE]

Install the REM userspace stage without overwriting existing non-directory
paths. All conflicts are found before ROOTFS is changed.
EOF
	exit 2
}

while (($#)); do
	case "$1" in
		--stage) (($# >= 2)) || usage; stage=$2; shift 2 ;;
		--help|-h) usage ;;
		-*) echo "unknown option: $1" >&2; usage ;;
		*) [[ -z "$destination" ]] || usage; destination=$1; shift ;;
	esac
done
[[ -n "$destination" ]] || usage

stage=$(realpath -e "$stage")
destination=$(realpath -m "$destination")
[[ -d "$stage" ]] || { echo "userspace stage is not a directory: $stage" >&2; exit 1; }
[[ "$stage" != "$destination" ]] || { echo "refusing to integrate a stage into itself" >&2; exit 1; }
case "$destination/" in
	"$stage/"*) echo "refusing destination inside userspace stage: $destination" >&2; exit 1 ;;
esac

declare -a entries=()
declare -a conflicts=()
while IFS= read -r -d '' relative; do
	case "$relative" in *.la) continue ;; esac
	entries+=("$relative")
	source=$stage/$relative
	target=$destination/$relative
	if [[ -e "$target" || -L "$target" ]] &&
		[[ ! ( -d "$source" && -d "$target" && ! -L "$target" ) ]]; then
		conflicts+=("$relative")
	fi
done < <(cd "$stage" && find -P . -mindepth 1 -printf '%P\0' | sort -z)

if ((${#conflicts[@]})); then
	echo "userspace integration conflicts (nothing was changed):" >&2
	printf '  %s\n' "${conflicts[@]}" >&2
	exit 3
fi

mkdir -p "$destination"
for relative in "${entries[@]}"; do
	source=$stage/$relative
	target=$destination/$relative
	if [[ -d "$source" && ! -L "$source" ]]; then
		if [[ ! -e "$target" ]]; then
			mkdir -p "$target"
			chmod "$(stat -c '%a' "$source")" "$target"
		else
			mkdir -p "$target"
		fi
	else
		mkdir -p "$(dirname "$target")"
		cp -a --no-preserve=ownership "$source" "$target"
	fi
done

printf 'integrated userspace stage %s into %s (%d entries)\n' \
	"$stage" "$destination" "${#entries[@]}"
