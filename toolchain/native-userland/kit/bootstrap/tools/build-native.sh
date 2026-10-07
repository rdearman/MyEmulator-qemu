#!/bin/bash
set -eu
case $0 in */*) cd "${0%/*}" ;; esac
mkdir -p native
"${CC:-chibicc}" expr.c -o native/expr
"${CC:-chibicc}" netcat.c -o native/nc
for program in rm cp wc; do
    "${CC:-chibicc}" "$program.c" -o "native/$program"
done
cp dirname basename native/
chmod +x native/dirname native/basename
test "$(native/expr 2 + 3 '*' 4)" = 14
test "$(native/expr Xabc : 'X\(.*\)')" = abc
test "$(native/expr 00001 : '.*\(...\)')" = 001
test "$(native/dirname /a/b/)" = /a
test "$(native/basename /a/b/)" = b
work="native/fileops-test-$$"
mkdir "$work"
printf 'one two\n' > "$work/input"
native/cp -p "$work/input" "$work/copy"
test "$(cat "$work/copy")" = "one two"
test "$(native/wc -lc < "$work/copy")" = " 1 8"
native/rm -f "$work/no-such-file"
native/rm -rf "$work"
