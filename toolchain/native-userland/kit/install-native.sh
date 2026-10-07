#!/bin/bash
set -eu
case $0 in */*) cd "${0%/*}" ;; esac
ROOT=$PWD
prefix=${REM_PREFIX:-"$HOME/.local"}
test -f BUILD-ACCEPTED && test "$(cat BUILD-ACCEPTED)" = "$prefix" || {
    echo "Build acceptance is missing or prefix differs; installation refused." >&2
    exit 1
}
core="$ROOT/packages/coreutils-9.7/native/bin"
CP="$core/cp"
MV="$core/mv"
RM="$core/rm"
lock="$prefix/.rem-native-install-lock"
mkdir -p "$prefix"
mkdir "$lock" || { echo "Another installation/rollback holds $lock" >&2; exit 1; }
backup="$prefix/lib/rem-native-userland/backups/install-$(date +%s)-$$"
temporary=
finish() {
    result=$?
    trap - EXIT
    if [ "$result" != 0 ] && [ -f "$backup/manifest" ] &&
            [ -x "$backup/bin/cp" ] && [ -x "$backup/bin/mv" ] &&
            [ -x "$backup/bin/rm" ]; then
        echo "Installation failed; restoring preserved files." >&2
        REM_INSTALL_LOCK_HELD=1 bash "$ROOT/rollback-native.sh" "$backup" ||
            echo "Rollback failed; preserved files remain at $backup" >&2
    fi
    if [ -n "$temporary" ]; then "$RM" -f "$temporary"; fi
    "$RM" -d "$lock"
    exit "$result"
}
trap finish EXIT
mkdir -p "$backup/files" "$backup/bin"
"$CP" "$CP" "$MV" "$RM" "$backup/bin/"
: > "$backup/manifest"
printf '%s\n' "$prefix" > "$backup/prefix"
index=0
put() {
    source=$1
    relative=$2
    target="$prefix/$relative"
    mkdir -p "${target%/*}"
    if [ -d "$target" ] && [ ! -L "$target" ]; then
        echo "Refusing to replace directory $target" >&2
        return 1
    fi
    index=$((index + 1))
    existed=0
    if [ -e "$target" ] || [ -L "$target" ]; then
        "$CP" -a "$target" "$backup/files/$index"
        existed=1
    fi
    printf '%s\t%s\t%s\n' "$index" "$existed" "$relative" >> "$backup/manifest"
    temporary="$target.rem-new-$$"
    test ! -e "$temporary" && test ! -L "$temporary"
    "$CP" -a "$source" "$temporary"
    "$MV" -Tf "$temporary" "$target"
    temporary=
}
for source in packages/*/native/bin/* bootstrap/tools/native/nc; do
    name=${source##*/}
    case $name in vi|busybox|init|getty|login|mount|umount|shutdown|reboot|poweroff)
        echo "Protected utility $name; installation refused." >&2; exit 1 ;;
    esac
    put "$source" "bin/$name"
done
if [ -x bootstrap/make-4.4.1/native/bin/make ]; then
    put bootstrap/make-4.4.1/native/bin/make bin/make
fi
put bootstrap/musl/native/libc.a lib/rem-native-userland/libc.a
put bootstrap/compiler-rt-15.0.7.src/native/ieee-runtime.o lib/rem-native-userland/ieee-runtime.o
put certificates/ca-bundle.crt lib/rem-native-userland/ca-bundle.crt
put bootstrap/native/chibicc-selfbuilt libexec/rem-chibicc/chibicc
for source in bootstrap/native/include/*; do
    put "$source" "libexec/rem-chibicc/include/${source##*/}"
done
find_program="$ROOT/packages/findutils-4.10.0/native/bin/find"
"$find_program" packages/vim-9.1.0000/runtime \( -type f -o -type l \) \
    > "$backup/vim-runtime-files"
while IFS= read -r source; do
    put "$source" "share/vim/vim91/${source#packages/vim-9.1.0000/runtime/}"
done < "$backup/vim-runtime-files"
"$find_program" native-deps/share/terminfo \( -type f -o -type l \) \
    > "$backup/terminfo-files"
while IFS= read -r source; do
    put "$source" "share/terminfo/${source#native-deps/share/terminfo/}"
done < "$backup/terminfo-files"
put bootstrap/chibicc-installed bin/chibicc
put bootstrap/chibicc-installed bin/cc
printf '%s\n' "$backup" > LAST-INSTALL
echo "Installed in $prefix; rollback: bash $ROOT/rollback-native.sh"
echo "Use PATH=$prefix/bin:\$PATH. BusyBox vi and boot infrastructure were not changed."
