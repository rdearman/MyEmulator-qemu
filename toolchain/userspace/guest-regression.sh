#!/bin/bash
# Bash is the regression driver; BusyBox remains init/recovery.  Tests are
# deliberately expanded inline because REM Bash currently faults in helper
# function argument handling after child-process returns.
printf '%s\n%s\n' REM_TEST_LINE_1 REM_TEST_LINE_2 >/tmp/rem-userspace-input.txt

echo 'REMTEST_BEGIN|coreutils-true|/usr/bin/true'
/usr/bin/true > /tmp/rem-userspace-coreutils-true.out 2>&1; echo "REMTEST_END|coreutils-true|$?"
echo 'REMTEST_BEGIN|coreutils-echo|/usr/bin/echo'
/usr/bin/echo REM_ECHO_OK > /tmp/rem-userspace-coreutils-echo.out 2>&1; echo "REMTEST_END|coreutils-echo|$?"
echo 'REMTEST_BEGIN|coreutils-ls|/usr/bin/ls'
/usr/bin/ls / > /tmp/rem-userspace-coreutils-ls.out 2>&1; echo "REMTEST_END|coreutils-ls|$?"
echo 'REMTEST_BEGIN|coreutils-ls-version|/usr/bin/ls'
/usr/bin/ls --version > /tmp/rem-userspace-coreutils-ls-version.out 2>&1; echo "REMTEST_END|coreutils-ls-version|$?"
echo 'REMTEST_BEGIN|coreutils-cat|/usr/bin/cat'
/usr/bin/cat /tmp/rem-userspace-input.txt > /tmp/rem-userspace-coreutils-cat.out 2>&1; echo "REMTEST_END|coreutils-cat|$?"
echo 'REMTEST_BEGIN|coreutils-mkdir|/usr/bin/mkdir'
/usr/bin/mkdir -p /tmp/rem-userspace-a/b > /tmp/rem-userspace-coreutils-mkdir.out 2>&1; echo "REMTEST_END|coreutils-mkdir|$?"
echo 'REMTEST_BEGIN|coreutils-touch|/usr/bin/touch'
/usr/bin/touch /tmp/rem-userspace-touched > /tmp/rem-userspace-coreutils-touch.out 2>&1; echo "REMTEST_END|coreutils-touch|$?"
echo 'REMTEST_BEGIN|coreutils-cp|/usr/bin/cp'
/usr/bin/cp /tmp/rem-userspace-touched /tmp/rem-userspace-copied > /tmp/rem-userspace-coreutils-cp.out 2>&1; echo "REMTEST_END|coreutils-cp|$?"
echo 'REMTEST_BEGIN|coreutils-mv|/usr/bin/mv'
/usr/bin/mv /tmp/rem-userspace-copied /tmp/rem-userspace-moved > /tmp/rem-userspace-coreutils-mv.out 2>&1; echo "REMTEST_END|coreutils-mv|$?"
echo 'REMTEST_BEGIN|coreutils-head|/usr/bin/head'
/usr/bin/head -n 2 /tmp/rem-userspace-input.txt > /tmp/rem-userspace-coreutils-head.out 2>&1; echo "REMTEST_END|coreutils-head|$?"
echo 'REMTEST_BEGIN|coreutils-tail|/usr/bin/tail'
/usr/bin/tail -n 2 /tmp/rem-userspace-input.txt > /tmp/rem-userspace-coreutils-tail.out 2>&1; echo "REMTEST_END|coreutils-tail|$?"
echo 'REMTEST_BEGIN|coreutils-wc|/usr/bin/wc'
/usr/bin/wc -l /tmp/rem-userspace-input.txt > /tmp/rem-userspace-coreutils-wc.out 2>&1; echo "REMTEST_END|coreutils-wc|$?"
echo 'REMTEST_BEGIN|coreutils-stat|/usr/bin/stat'
/usr/bin/stat /tmp/rem-userspace-moved > /tmp/rem-userspace-coreutils-stat.out 2>&1; echo "REMTEST_END|coreutils-stat|$?"
echo 'REMTEST_BEGIN|coreutils-sha256sum|/usr/bin/sha256sum'
/usr/bin/sha256sum /tmp/rem-userspace-moved > /tmp/rem-userspace-coreutils-sha256sum.out 2>&1; echo "REMTEST_END|coreutils-sha256sum|$?"
echo 'REMTEST_BEGIN|findutils-find|/usr/bin/find'
/usr/bin/find /tmp -type f > /tmp/rem-userspace-findutils-find.out 2>&1; echo "REMTEST_END|findutils-find|$?"
echo 'REMTEST_BEGIN|findutils-xargs|/usr/bin/xargs'
/usr/bin/xargs /usr/bin/echo XARGS_OK < /tmp/rem-userspace-input.txt > /tmp/rem-userspace-findutils-xargs.out 2>&1; echo "REMTEST_END|findutils-xargs|$?"
echo 'REMTEST_BEGIN|bash-old-simple-script|/usr/bin/bash'
/usr/bin/bash -c 'x=6; [ $((x*7)) -eq 42 ] && echo BASH_OK' > /tmp/rem-userspace-bash-script.out 2>&1; echo "REMTEST_END|bash-old-simple-script|$?"
echo 'REMTEST_BEGIN|python-startup|/usr/bin/python3'
/usr/bin/python3 --version > /tmp/rem-userspace-python-startup.out 2>&1; echo "REMTEST_END|python-startup|$?"
echo 'REMTEST_BEGIN|python-script|/usr/bin/python3'
/usr/bin/python3 -c 'import json, pathlib; p=pathlib.Path("py.out"); p.write_text(json.dumps({"n": 42})); print(json.loads(p.read_text())["n"])' > /tmp/rem-userspace-python-script.out 2>&1; echo "REMTEST_END|python-script|$?"
echo 'REMTEST_BEGIN|emacs-batch|/usr/bin/emacs'
/usr/bin/emacs --batch -Q --eval '(princ "EMACS_OK")' > /tmp/rem-userspace-emacs-batch.out 2>&1; echo "REMTEST_END|emacs-batch|$?"
echo 'REMTEST_BEGIN|make-version|/usr/bin/make'
/usr/bin/make --version > /tmp/rem-userspace-make-version.out 2>&1; echo "REMTEST_END|make-version|$?"
echo 'REMTEST_BEGIN|git-startup|/usr/bin/git'
/usr/bin/git --version > /tmp/rem-userspace-git-startup.out 2>&1; echo "REMTEST_END|git-startup|$?"
echo 'REMTEST_BEGIN|sqlite-query|/usr/bin/sqlite3'
/usr/bin/sqlite3 /tmp/rem-userspace-test.db 'create table t(n); insert into t values(42); select n from t;' > /tmp/rem-userspace-sqlite-query.out 2>&1; echo "REMTEST_END|sqlite-query|$?"
echo 'REMTEST_BEGIN|openssl-hash|/usr/bin/openssl'
/usr/bin/openssl dgst -sha256 /tmp/rem-userspace-moved > /tmp/rem-userspace-openssl-hash.out 2>&1; echo "REMTEST_END|openssl-hash|$?"
echo 'REMTEST_BEGIN|curl-startup|/usr/bin/curl'
/usr/bin/curl --version > /tmp/rem-userspace-curl-startup.out 2>&1; echo "REMTEST_END|curl-startup|$?"
echo 'REMTEST_BEGIN|ssh-startup|/usr/bin/ssh'
/usr/bin/ssh -V > /tmp/rem-userspace-ssh-startup.out 2>&1; echo "REMTEST_END|ssh-startup|$?"
echo 'REMTEST_BEGIN|bash-version|/usr/bin/bash'
/usr/bin/bash --version; echo "REMTEST_END|bash-version|$?"
echo 'REMTEST_BEGIN|bash-c-echo|/usr/bin/bash'
/usr/bin/bash -c 'echo hello'; echo "REMTEST_END|bash-c-echo|$?"
echo 'REMTEST_BEGIN|bash-simple-command|/usr/bin/bash'
/usr/bin/bash -c 'test "$(echo hello)" = hello'; echo "REMTEST_END|bash-simple-command|$?"
echo 'REMTEST_BEGIN|bash-variables|/usr/bin/bash'
/usr/bin/bash -c 'value=REM_BASH_VALUE; test "$value" = REM_BASH_VALUE'; echo "REMTEST_END|bash-variables|$?"
echo 'REMTEST_BEGIN|bash-functions|/usr/bin/bash'
/usr/bin/bash -c 'greet() { local value=REM_BASH_FUNCTION; test "$value" = REM_BASH_FUNCTION; }; greet'; echo "REMTEST_END|bash-functions|$?"
echo 'REMTEST_BEGIN|bash-command-substitution|/usr/bin/bash'
/usr/bin/bash -c 'value=$(printf REM_BASH_SUBST); test "$value" = REM_BASH_SUBST'; echo "REMTEST_END|bash-command-substitution|$?"
echo 'REMTEST_BEGIN|bash-pipeline|/usr/bin/bash'
/usr/bin/bash -c 'test "$(printf REM_BASH_PIPE | /bin/busybox tr A-Z a-z)" = rem_bash_pipe'; echo "REMTEST_END|bash-pipeline|$?"
echo 'REMTEST_BEGIN|bash-redirection|/usr/bin/bash'
/usr/bin/bash -c 'printf REM_BASH_REDIRECT >/tmp/rem-bash-redirect; IFS= read -r value </tmp/rem-bash-redirect; test "$value" = REM_BASH_REDIRECT'; echo "REMTEST_END|bash-redirection|$?"
echo 'REMTEST_BEGIN|bash-child-script|/usr/bin/bash'
/usr/bin/bash -c 'printf "%s\\n" "test \"$BASH_VERSION\" != \"\"" > /tmp/rem-bash-child.sh; /usr/bin/bash /tmp/rem-bash-child.sh'; echo "REMTEST_END|bash-child-script|$?"
echo 'REMTEST_BEGIN|bash-environment|/usr/bin/bash'
/usr/bin/bash -c 'export REM_BASH_ENV=REM_BASH_ENV_OK; /usr/bin/bash -c '\''test "$REM_BASH_ENV" = REM_BASH_ENV_OK'\''' ; echo "REMTEST_END|bash-environment|$?"
echo REMTEST_COMPLETE
