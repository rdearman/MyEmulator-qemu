#define _GNU_SOURCE
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/reboot.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

int main(int argc, char **argv)
{
    const char *base = strrchr(argv[0], '/');
    base = base ? base + 1 : argv[0];
    int command = RB_POWER_OFF;
    if (!strcmp(base, "reboot") || (argc > 1 && !strcmp(argv[1], "reboot")))
        command = RB_AUTOBOOT;
    else if (!strcmp(base, "halt") || (argc > 1 && !strcmp(argv[1], "halt")))
        command = RB_HALT_SYSTEM;
    else if (strcmp(base, "poweroff") && !(argc > 1 && !strcmp(argv[1], "poweroff"))) {
        fprintf(stderr, "usage: poweroff|reboot|halt\n");
        return 2;
    }
    puts("REM_SHUTDOWN|runlevel-stop");
    fflush(stdout);
    pid_t pid = fork();
    if (pid == 0) {
        execl("/usr/bin/bash", "bash", "-p", "/etc/init.d/rc", "shutdown", (char *)NULL);
        _exit(127);
    }
    if (pid > 0) {
        int status;
        while (waitpid(pid, &status, 0) < 0 && errno == EINTR)
            ;
    }
    sync();
    sync();
    if (reboot(command)) {
        fprintf(stderr, "shutdown: reboot syscall: %s\n", strerror(errno));
        return 1;
    }
    return 0;
}
