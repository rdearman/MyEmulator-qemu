#!/bin/sh
# Runs inside REM Linux.  Keep each application in a child shell so one
# crash does not prevent the remaining tests from producing a result marker.

root=/tmp/rem-userspace-regression
rm -rf "$root"
mkdir -p "$root/work" || exit 2

run_test() {
	name=$1
	path=$2
	shift 2
	if [ -n "$REMTEST_ONLY" ] && [ "$REMTEST_ONLY" != "$name" ]; then
		return 0
	fi
	if [ -n "$REMTEST_GROUP" ] && [ "${name%%-*}" != "$REMTEST_GROUP" ]; then
		return 0
	fi
	echo "REMTEST_BEGIN|$name|$path"
	(
		cd "$root/work" || exit 125
		"$@"
	) >"$root/$name.out" 2>&1
	rc=$?
	echo "REMTEST_END|$name|$rc"
}

run_test coreutils-true /usr/bin/true /usr/bin/true
run_test coreutils-echo /usr/bin/echo /usr/bin/echo REM_ECHO_OK
run_test coreutils-ls /usr/bin/ls /usr/bin/ls /
run_test coreutils-ls-version /usr/bin/ls /usr/bin/ls --version
run_test coreutils-cat /usr/bin/cat /usr/bin/cat /etc/passwd
run_test coreutils-mkdir /usr/bin/mkdir /usr/bin/mkdir -p "$root/work/a/b"
run_test coreutils-touch /usr/bin/touch /usr/bin/touch "$root/work/touched"
run_test coreutils-cp /usr/bin/cp /usr/bin/cp "$root/work/touched" "$root/work/copied"
run_test coreutils-mv /usr/bin/mv /usr/bin/mv "$root/work/copied" "$root/work/moved"
run_test coreutils-head /usr/bin/head /usr/bin/head -n 2 /etc/passwd
run_test coreutils-tail /usr/bin/tail /usr/bin/tail -n 2 /etc/passwd
run_test coreutils-wc /usr/bin/wc /usr/bin/wc -l /etc/passwd
run_test coreutils-stat /usr/bin/stat /usr/bin/stat "$root/work/moved"
run_test coreutils-sha256sum /usr/bin/sha256sum /usr/bin/sha256sum "$root/work/moved"
run_test findutils-find /usr/bin/find /usr/bin/find "$root/work" -type f
run_test findutils-xargs /usr/bin/xargs /usr/bin/xargs /usr/bin/echo XARGS_OK

run_test bash-script /usr/bin/bash /usr/bin/bash -c 'x=6; [ $((x*7)) -eq 42 ] && echo BASH_OK'
run_test python-startup /usr/bin/python3 /usr/bin/python3 --version
run_test python-script /usr/bin/python3 /usr/bin/python3 -c 'import json, pathlib; p=pathlib.Path("py.out"); p.write_text(json.dumps({"n": 42})); print(json.loads(p.read_text())["n"])'
run_test emacs-batch /usr/bin/emacs /usr/bin/emacs --batch -Q --eval '(princ "EMACS_OK")'
run_test make-version /usr/bin/make /usr/bin/make --version
run_test git-startup /usr/bin/git /usr/bin/git --version
run_test sqlite-query /usr/bin/sqlite3 /usr/bin/sqlite3 "$root/work/test.db" 'create table t(n); insert into t values(42); select n from t;'
run_test openssl-hash /usr/bin/openssl /usr/bin/openssl dgst -sha256 "$root/work/moved"
run_test curl-startup /usr/bin/curl /usr/bin/curl --version
run_test ssh-startup /usr/bin/ssh /usr/bin/ssh -V

echo REMTEST_COMPLETE
