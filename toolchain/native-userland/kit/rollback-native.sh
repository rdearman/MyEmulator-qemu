#!/bin/bash
set -eu
case $0 in */*) cd "${0%/*}" ;; esac
if [ "$#" = 1 ]; then backup=$1; else backup=$(cat LAST-INSTALL); fi
test -f "$backup/manifest" && test -f "$backup/prefix" || {
    echo "Invalid rollback backup: $backup" >&2; exit 1
}
prefix=$(cat "$backup/prefix")
lock="$prefix/.rem-native-install-lock"
RM="$backup/bin/rm"
if [ "${REM_INSTALL_LOCK_HELD:-0}" != 1 ]; then
    mkdir "$lock" || { echo "Another installation/rollback holds $lock" >&2; exit 1; }
    trap '"$RM" -d "$lock"' EXIT
fi
while IFS=$'\t' read -r index existed relative; do
    case $relative in bin/*|lib/rem-native-userland/*|libexec/rem-chibicc/*|share/vim/vim91/*|share/terminfo/*) ;;
    *) echo "Invalid rollback path: $relative" >&2; exit 1 ;; esac
    case $relative in *../*) echo "Invalid rollback path: $relative" >&2; exit 1 ;; esac
    target="$prefix/$relative"
    if [ "$existed" = 1 ]; then
        temporary="$target.rem-rollback-$$"
        test ! -e "$temporary" && test ! -L "$temporary"
        "$backup/bin/cp" -a "$backup/files/$index" "$temporary"
        "$backup/bin/mv" -Tf "$temporary" "$target"
    else
        "$RM" -f "$target"
    fi
done < "$backup/manifest"
echo "Rollback restored files from $backup"
