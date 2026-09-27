/*
 * Focused terminal/process handoff probe for an interactive Vim session.
 * Build statically for REM and run from an interactive Yash prompt.
 */
#define _GNU_SOURCE
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/ioctl.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <termios.h>
#include <unistd.h>

static void result(const char *name, long rc, int saved_errno)
{
	printf("%s rc=%ld errno=%d\n", name, rc, saved_errno);
	fflush(stdout);
}

static void probe_sets_id(const char *name, int do_setsid)
{
	pid_t pid = fork();
	int saved_errno;

	if (pid < 0) {
		saved_errno = errno;
		result("fork_for_session_probe", -1, saved_errno);
		return;
	}
	if (pid == 0) {
		pid_t rc;
		errno = 0;
		if (do_setsid)
			rc = setsid();
		else
			rc = setpgid(0, 0);
		saved_errno = errno;
		result(name, rc, saved_errno);
		_exit(rc < 0 ? 1 : 0);
	}
	{
		int status = 0;
		pid_t waited = waitpid(pid, &status, 0);
		printf("%s_child waited=%ld exited=%d status=%d\n", name,
		       (long)waited, WIFEXITED(status),
		       WIFEXITED(status) ? WEXITSTATUS(status) : -1);
		fflush(stdout);
	}
}

static void probe_controlling_tty(void)
{
	pid_t pid = fork();
	if (pid < 0) {
		int saved_errno = errno;
		result("fork_for_controlling_tty", -1, saved_errno);
		return;
	}
	if (pid == 0) {
		pid_t rc;
		int saved_errno;

		errno = 0;
		rc = setsid();
		saved_errno = errno;
		result("controlling_tty_setsid", rc, saved_errno);
		if (rc < 0)
			_exit(1);

		errno = 0;
		rc = ioctl(STDIN_FILENO, TIOCSCTTY, 0);
		saved_errno = errno;
		result("ioctl(TIOCSCTTY)", rc, saved_errno);
		if (rc < 0)
			_exit(2);

		errno = 0;
		rc = tcsetpgrp(STDIN_FILENO, getpgrp());
		saved_errno = errno;
		result("controlling_tty_tcsetpgrp", rc, saved_errno);

		errno = 0;
		rc = tcgetpgrp(STDIN_FILENO);
		saved_errno = errno;
		result("controlling_tty_tcgetpgrp", rc, saved_errno);
		_exit(0);
	}
	{
		int status = 0;
		pid_t waited = waitpid(pid, &status, 0);
		printf("controlling_tty_child waited=%ld exited=%d status=%d\n",
		       (long)waited, WIFEXITED(status),
		       WIFEXITED(status) ? WEXITSTATUS(status) : -1);
		fflush(stdout);
	}
}

int main(void)
{
	struct termios term;
	int saved_errno;
	int rc;

	errno = 0;
	rc = isatty(STDIN_FILENO);
	saved_errno = errno;
	result("isatty(stdin)", rc, saved_errno);

	errno = 0;
	rc = tcgetattr(STDIN_FILENO, &term);
	saved_errno = errno;
	result("tcgetattr(stdin)", rc, saved_errno);
	if (rc == 0) {
		errno = 0;
		rc = ioctl(STDIN_FILENO, TCGETS, &term);
		saved_errno = errno;
		result("ioctl(TCGETS)", rc, saved_errno);

		errno = 0;
		rc = ioctl(STDIN_FILENO, TCSETS, &term);
		saved_errno = errno;
		result("ioctl(TCSETS)", rc, saved_errno);

		errno = 0;
		rc = tcsetattr(STDIN_FILENO, TCSANOW, &term);
		saved_errno = errno;
		result("tcsetattr(TCSANOW)", rc, saved_errno);
	}

	errno = 0;
	rc = tcsetpgrp(STDIN_FILENO, getpgrp());
	saved_errno = errno;
	result("tcsetpgrp(stdin,getpgrp)", rc, saved_errno);

	{
		pid_t pgrp = -1;
		errno = 0;
		pgrp = tcgetpgrp(STDIN_FILENO);
		saved_errno = errno;
		result("tcgetpgrp(stdin)", pgrp, saved_errno);
	}

	{
		pid_t pgrp = -1;
		errno = 0;
		rc = ioctl(STDIN_FILENO, TIOCGPGRP, &pgrp);
		saved_errno = errno;
		result("ioctl(TIOCGPGRP)", rc, saved_errno);
		if (rc == 0)
			printf("tty_foreground_pgrp=%ld\n", (long)pgrp);
	}

	printf("process_pgrp=%ld process_sid=%ld\n", (long)getpgrp(),
	       (long)getsid(0));
	fflush(stdout);

	probe_sets_id("child_setpgid(0,0)", 0);
	probe_sets_id("child_setsid()", 1);
	probe_controlling_tty();
	return 0;
}
