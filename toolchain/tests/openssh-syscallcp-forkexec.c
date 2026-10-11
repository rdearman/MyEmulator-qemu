/* Minimal fork/connect/exec reducer for the OpenSSH child SIGBUS. */
#include <arpa/inet.h>
#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/wait.h>
#include <unistd.h>

int
main(void)
{
	struct sockaddr_in addr;
	pid_t pid;
	int status;

	pid = fork();
	if (pid < 0) {
		printf("FORK_FAIL errno=%d\n", errno);
		return 1;
	}
	if (pid == 0) {
		int fd, rc, err;

		puts("CHILD_BEFORE_SOCKET");
		fflush(stdout);
		fd = socket(AF_INET, SOCK_STREAM, 0);
		if (fd < 0) {
			printf("CHILD_SOCKET_FAIL errno=%d\n", errno);
			_exit(2);
		}
		memset(&addr, 0, sizeof(addr));
		addr.sin_family = AF_INET;
		addr.sin_port = htons(22222);
		addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
		puts("CHILD_BEFORE_CONNECT");
		fflush(stdout);
		rc = connect(fd, (struct sockaddr *)&addr, sizeof(addr));
		err = errno;
		printf("CHILD_CONNECT rc=%d errno=%d (%s)\n", rc, err,
		    strerror(err));
		fflush(stdout);
		close(fd);
		puts("CHILD_AFTER_CLOSE");
		fflush(stdout);
		execl("/bin/true", "true", (char *)0);
		printf("CHILD_EXEC_FAIL errno=%d\n", errno);
		_exit(3);
	}
	if (waitpid(pid, &status, 0) != pid) {
		printf("WAITPID_FAIL errno=%d\n", errno);
		return 4;
	}
	if (WIFEXITED(status)) {
		printf("CHILD_EXIT=%d\n", WEXITSTATUS(status));
		return WEXITSTATUS(status);
	}
	if (WIFSIGNALED(status)) {
		printf("CHILD_SIGNAL=%d\n", WTERMSIG(status));
		return 128 + WTERMSIG(status);
	}
	printf("CHILD_UNEXPECTED_STATUS=%d\n", status);
	return 5;
}
