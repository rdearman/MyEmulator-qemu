#define _GNU_SOURCE
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <termios.h>
#include <unistd.h>

static int read_line(char *line, size_t size)
{
    size_t used = 0;
    while (used + 1 < size) {
        char c;
        ssize_t n = read(STDIN_FILENO, &c, 1);
        if (n <= 0)
            return -1;
        if (c == '\r' || c == '\n') {
            line[used] = '\0';
            write(STDOUT_FILENO, "\r\n", 2);
            return (int)used;
        }
        if ((c == 0x7f || c == '\b') && used) {
            used--;
            write(STDOUT_FILENO, "\b \b", 3);
        } else if (c >= 0x20 && c < 0x7f) {
            line[used++] = c;
            write(STDOUT_FILENO, &c, 1);
        }
    }
    line[used] = '\0';
    return (int)used;
}

int main(int argc, char **argv)
{
    const char *device = argc > 1 ? argv[1] : "/dev/ttyMY0";
    if (setsid() < 0 && errno != EPERM) {
        perror("rem-getty: setsid");
        return 1;
    }
    int fd = open(device, O_RDWR);
    if (fd < 0) {
        perror("rem-getty: open console");
        return 1;
    }
    if (ioctl(fd, TIOCSCTTY, 0) < 0 && errno != EINVAL)
        perror("rem-getty: TIOCSCTTY");
    for (int n = 0; n <= 2; n++)
        if (dup2(fd, n) < 0)
            return 1;
    if (fd > 2)
        close(fd);

    struct termios t;
    if (!tcgetattr(STDIN_FILENO, &t)) {
        t.c_iflag = ICRNL | IXON;
        t.c_oflag = OPOST | ONLCR;
        t.c_cflag = CS8 | CREAD | CLOCAL;
        t.c_lflag = ISIG | ICANON | ECHO | ECHOE | ECHOK;
        t.c_cc[VINTR] = 3;
        t.c_cc[VEOF] = 4;
        t.c_cc[VERASE] = 127;
        t.c_cc[VKILL] = 21;
        cfsetispeed(&t, B115200);
        cfsetospeed(&t, B115200);
        tcsetattr(STDIN_FILENO, TCSANOW, &t);
    }

    for (;;) {
        char user[64];
        dprintf(STDOUT_FILENO, "REM Linux 0.2 login: ");
        if (read_line(user, sizeof(user)) < 0)
            return 0;
        if (!*user)
            continue;
        execl("/sbin/login", "login", user, (char *)NULL);
        dprintf(STDERR_FILENO, "rem-getty: login: %s\r\n", strerror(errno));
        return 1;
    }
}
