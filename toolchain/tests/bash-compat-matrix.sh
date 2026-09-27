#!/bin/busybox sh
# Run against a disposable rootfs containing the controlled REM Bash build.
# Each marker records the exit status of one independent Bash invocation.

run_case() {
	name=$1
	shift
	"$@"
	rc=$?
	echo "REM_BASH_TEST_${name}=${rc}"
}

run_case VERSION /usr/bin/bash --version
/usr/bin/bash --version >/root/rem-bash-version.out 2>&1
version_status=$?
/bin/busybox grep -q 'GNU bash, version 5.2.37' /root/rem-bash-version.out
version_text_status=$?
if [ "$version_status" -eq 0 ] && [ "$version_text_status" -eq 0 ]; then
	echo REM_BASH_TEST_VERSION_REDIRECT=0
else
	echo "REM_BASH_TEST_VERSION_REDIRECT=$version_status"
fi
run_case COMMAND /usr/bin/bash -c 'echo hello'
run_case VARIABLES /usr/bin/bash -c 'value=REM_BASH_VALUE; test "$value" = REM_BASH_VALUE && echo BASH_VARIABLE_OK'
run_case FUNCTIONS /usr/bin/bash -c 'greet() { local value=REM_BASH_FUNCTION; test "$value" = REM_BASH_FUNCTION; }; greet && echo BASH_FUNCTION_OK'
run_case COMMAND_SUBSTITUTION /usr/bin/bash -c 'value=$(printf REM_BASH_SUBST); test "$value" = REM_BASH_SUBST && echo BASH_SUBST_OK'
run_case PIPELINE /usr/bin/bash -c 'test "$(printf REM_BASH_PIPE | /bin/busybox cat)" = REM_BASH_PIPE && echo BASH_PIPE_OK'
run_case REDIRECTION /usr/bin/bash -c 'printf REM_BASH_REDIRECT >/root/rem-bash-redirect; IFS= read -r value </root/rem-bash-redirect; test "$value" = REM_BASH_REDIRECT && echo BASH_REDIRECT_OK'
echo 'test "$BASH_VERSION" != "" && echo BASH_SCRIPT_OK' >/root/rem-bash-child.sh
run_case SCRIPT /usr/bin/bash /root/rem-bash-child.sh
export REM_BASH_ENV=REM_BASH_ENV_OK
run_case ENVIRONMENT /usr/bin/bash -c 'test "$REM_BASH_ENV" = REM_BASH_ENV_OK && echo BASH_ENV_OK'
echo REM_BASH_MATRIX_DONE
