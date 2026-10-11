#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <unistd.h>

int main(void)
{
    char c = 0;
    int fd;
    ssize_t n;

    puts("READ_PROBE_BEFORE_OPEN");
    fd = open("/dev/zero", O_RDONLY);
    if (fd < 0) {
        printf("READ_PROBE_OPEN_FAIL errno=%d\n", errno);
        return 1;
    }
    puts("READ_PROBE_BEFORE_READ");
    n = read(fd, &c, 1);
    printf("READ_PROBE_RESULT n=%ld value=%d errno=%d\n", (long)n,
           (unsigned char)c, errno);
    close(fd);
    puts("READ_PROBE_PASS");
    return n == 1 && c == 0 ? 0 : 2;
}
