#define _GNU_SOURCE
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#include <termios.h>

static void report(const char *name, long result, int error)
{
	printf("%s: return=%ld errno=%d\n", name, result, error);
}

int main(void)
{
	pid_t pid;
	int status;
	int saved_errno;
	char *const child_argv[] = { "busybox", "true", NULL };

	errno = 0;
	pid = setpgid(0, 0);
	saved_errno = errno;
	report("setpgid(0,0)", pid, saved_errno);

	errno = 0;
	pid = fork();
	saved_errno = errno;
	report("fork()", pid, saved_errno);
	if (pid == 0) {
		execv("/bin/busybox", child_argv);
		saved_errno = errno;
		report("execv(/bin/busybox,true)", -1, saved_errno);
		_exit(120);
	}
	if (pid < 0)
		return 1;

	errno = 0;
	status = 0;
	{
		pid_t waited = waitpid(pid, &status, 0);
		saved_errno = errno;
		report("waitpid(child)", waited, saved_errno);
		if (waited == pid)
			printf("child_status: exited=%d status=%d signaled=%d signal=%d\n",
			       WIFEXITED(status), WIFEXITED(status) ? WEXITSTATUS(status) : -1,
			       WIFSIGNALED(status), WIFSIGNALED(status) ? WTERMSIG(status) : 0);
	}

	errno = 0;
	status = tcsetpgrp(STDIN_FILENO, getpgrp());
	saved_errno = errno;
	report("tcsetpgrp(stdin,getpgrp())", status, saved_errno);
	return 0;
}
