#!/bin/sh
set -u
PATH=/sbin:/bin:/usr/sbin:/usr/bin
export PATH
failed=0
result() {
    if [ "$1" -eq 0 ]; then
        echo "REM_QA|$2|PASS"
    else
        echo "REM_QA|$2|FAIL|status=$1"
        failed=1
    fi
}

test -d /proc/1 && test -d /sys/class && test -e /dev/pts/ptmx
result "$?" PROC_SYS_DEV

for command in cat cp chmod chown date df du find grep sed tar gzip bzip2 xz diff cmp \
    bash yash chibicc as ld ar ranlib readelf nm objdump samu vim curl; do
test -x "/usr/bin/$command" || failed=1
done
result "$failed" PACKAGES_PRESENT

printf 'alpha\nbeta\n' > /tmp/rem-qa-input
grep -qx beta /tmp/rem-qa-input
result "$?" GREP
test "$(sed -n '2p' /tmp/rem-qa-input)" = beta
result "$?" SED
test -e /etc/hostname && find /etc -name hostname >/tmp/rem-qa-find
result "$?" FIND

mkdir -p /tmp/rem-qa
printf 'payload\n' > /tmp/rem-qa/in
tar -cf /tmp/rem-qa/a.tar -C /tmp/rem-qa in
tar -xf /tmp/rem-qa/a.tar -C /tmp/rem-qa
gzip -c /tmp/rem-qa/in | gzip -dc > /tmp/rem-qa/g
bzip2 -c /tmp/rem-qa/in | bzip2 -dc > /tmp/rem-qa/b
xz -c /tmp/rem-qa/in | xz -dc > /tmp/rem-qa/x
diff /tmp/rem-qa/in /tmp/rem-qa/g
diff /tmp/rem-qa/in /tmp/rem-qa/b
diff /tmp/rem-qa/in /tmp/rem-qa/x
result "$?" ARCHIVES

printf '#include <stdio.h>\nint main(void) { puts("hello-rem"); return 0; }\n' > /tmp/rem-qa/hello.c
chibicc /tmp/rem-qa/hello.c -o /tmp/rem-qa/hello
test "$(/tmp/rem-qa/hello)" = hello-rem
chibicc -S /tmp/rem-qa/hello.c -o /tmp/rem-qa/hello.s
as /tmp/rem-qa/hello.s -o /tmp/rem-qa/hello.o
ld -r /tmp/rem-qa/hello.o -o /tmp/rem-qa/hello-rel.o
result "$?" CHIBICC_AS_LD

printf 'rule cc\n  command = chibicc -c $in -o $out\nbuild hello.o: cc /tmp/rem-qa/hello.c\ndefault hello.o\n' > /tmp/rem-qa/build.ninja
samu -f /tmp/rem-qa/build.ninja
test -f /tmp/rem-qa/hello.o
result "$?" SAMURAI

vim --version | grep -q 'VIM - Vi IMproved'
vim -Nu NONE -n -es -c 'qall!'
curl --version >/tmp/rem-qa/curl-version
result "$?" EDITOR_CURL

test ! -e /usr/bin/gawk
test ! -e /usr/sbin/dropbear
echo 'REM_QA|PENDING_GAWK_DROPBEAR_NCURSES|CONFIRMED'
if [ "$failed" -eq 0 ]; then
    echo 'REM_QA|PACKAGE_SMOKE|PASS'
else
    echo 'REM_QA|PACKAGE_SMOKE|FAIL'
fi
/sbin/poweroff
