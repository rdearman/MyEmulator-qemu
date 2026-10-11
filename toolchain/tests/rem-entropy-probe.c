#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <sys/random.h>
#include <unistd.h>

int main(void)
{
    unsigned char data[32];
    ssize_t n;
    int saved;
    int fd;

    errno = 0;
    n = getrandom(data, sizeof(data), GRND_NONBLOCK);
    saved = errno;
    printf("GETRANDOM n=%ld errno=%d\n", (long)n, saved);

    errno = 0;
    fd = open("/dev/urandom", O_RDONLY);
    saved = errno;
    printf("URANDOM_OPEN fd=%d errno=%d\n", fd, saved);
    if (fd >= 0) {
        errno = 0;
        n = read(fd, data, sizeof(data));
        saved = errno;
        printf("URANDOM_READ n=%ld errno=%d\n", (long)n, saved);
        close(fd);
    }
    return 0;
}
