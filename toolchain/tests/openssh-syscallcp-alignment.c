/* Minimal probe for the alignment exception seen after OpenSSH's connect path.
 * Build statically with the REM target libc and run on the same disposable
 * QEMU tuple. A missing route is an expected connect error; SIGBUS is not. */
#include <arpa/inet.h>
#include <errno.h>
#include <netinet/in.h>
#include <stdio.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>

int
main(void)
{
	struct sockaddr_in addr;
	int fd, rc, saved_errno;

	puts("BEFORE_SOCKET");
	fflush(stdout);
	errno = 0;
	fd = socket(AF_INET, SOCK_STREAM, 0);
	saved_errno = errno;
	printf("SOCKET rc=%d errno=%d (%s)\n", fd, saved_errno,
	    strerror(saved_errno));
	fflush(stdout);
	if (fd < 0)
		return 1;

	memset(&addr, 0, sizeof(addr));
	addr.sin_family = AF_INET;
	addr.sin_port = htons(22222);
	addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
	puts("BEFORE_CONNECT");
	fflush(stdout);
	errno = 0;
	rc = connect(fd, (struct sockaddr *)&addr, sizeof(addr));
	saved_errno = errno;
	printf("CONNECT rc=%d errno=%d (%s)\n", rc, saved_errno,
	    strerror(saved_errno));
	fflush(stdout);
	close(fd);
	return rc == 0 ? 0 : 2;
}
