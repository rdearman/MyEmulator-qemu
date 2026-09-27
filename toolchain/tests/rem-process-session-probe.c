/* Standalone process/session compatibility probe for a REM Linux guest. */
#define _GNU_SOURCE
#include <errno.h>
#include <sched.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

static void report_pid(const char *name, pid_t value, int error)
{
	printf("%s=%ld errno=%d\n", name, (long)value, error);
	fflush(stdout);
}

static int clone_child(void *arg)
{
	int status = *(int *)arg;
	_exit(status);
}

static void report_wait(const char *name, pid_t pid)
{
	int status = 0;
	pid_t rc;
	errno = 0;
	rc = waitpid(pid, &status, 0);
	printf("%s=%ld errno=%d exited=%d exit_status=%d signaled=%d signal=%d\n",
	       name, (long)rc, errno, WIFEXITED(status),
	       WIFEXITED(status) ? WEXITSTATUS(status) : -1,
	       WIFSIGNALED(status), WIFSIGNALED(status) ? WTERMSIG(status) : 0);
	fflush(stdout);
}

int main(void)
{
	pid_t pid;
	static unsigned char clone_stack[65536] __attribute__((aligned(16)));
	static int clone_exit = 23;
	char *const child_argv[] = { "busybox", "true", NULL };
	char *const child_envp[] = { "PATH=/bin:/usr/bin", NULL };

	errno = 0;
	report_pid("getpid", getpid(), errno);
	errno = 0;
	report_pid("getpgrp", getpgrp(), errno);
	errno = 0;
	report_pid("getsid_self", getsid(0), errno);

	errno = 0;
	pid = fork();
	report_pid("fork", pid, errno);
	if (pid == 0) {
		errno = 0;
		pid_t rc = setpgid(0, 0);
		printf("child_setpgid=%ld errno=%d child_pgid=%ld\n",
		       (long)rc, errno, (long)getpgrp());
		fflush(stdout);
		_exit(rc < 0 ? 31 : 0);
	}
	if (pid > 0)
		report_wait("wait_after_setpgid", pid);

	errno = 0;
	pid = fork();
	report_pid("fork_for_exec", pid, errno);
	if (pid == 0) {
		execve("/bin/busybox", child_argv, child_envp);
		printf("execve_failed errno=%d\n", errno);
		fflush(stdout);
		_exit(32);
	}
	if (pid > 0)
		report_wait("wait_after_exec", pid);

	errno = 0;
	pid = fork();
	report_pid("fork_for_setsid", pid, errno);
	if (pid == 0) {
		errno = 0;
		pid_t sid = setsid();
		printf("child_setsid=%ld errno=%d child_pgid=%ld child_sid=%ld\n",
		       (long)sid, errno, (long)getpgrp(), (long)getsid(0));
		fflush(stdout);
		_exit(sid < 0 ? 33 : 0);
	}
	if (pid > 0)
		report_wait("wait_after_setsid", pid);

	errno = 0;
	pid = fork();
	report_pid("fork_for_exit24", pid, errno);
	if (pid == 0)
		_exit(24);
	if (pid > 0)
		report_wait("wait_after_exit24", pid);

	errno = 0;
	pid = clone(clone_child, clone_stack + sizeof(clone_stack), SIGCHLD,
		    &clone_exit);
	report_pid("clone(SIGCHLD)", pid, errno);
	if (pid > 0)
		report_wait("wait_after_clone", pid);

	errno = 0;
	pid = (pid_t)syscall(220, SIGCHLD, 0, 0, 0, 0);
	report_pid("raw_clone_fork_style", pid, errno);
	if (pid == 0)
		_exit(24);
	if (pid > 0)
		report_wait("wait_after_raw_clone", pid);

	errno = 0;
	report_pid("getpgid_self", getpgid(0), errno);
	errno = 0;
	report_pid("getsid_self_final", getsid(0), errno);
	return 0;
}
