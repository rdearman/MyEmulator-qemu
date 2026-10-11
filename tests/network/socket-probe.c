#include <errno.h>
#include <stdio.h>
#include <sys/socket.h>
#include <unistd.h>

int main(void)
{
	int fd = socket(AF_INET, SOCK_DGRAM, 0);

	if (fd < 0) {
		printf("SOCKET_AF_INET_DGRAM=FAIL errno=%d\n", errno);
		return 1;
	}

	printf("SOCKET_AF_INET_DGRAM=PASS fd=%d\n", fd);
	close(fd);
	return 0;
}
