#define _GNU_SOURCE
#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mount.h>
#include <sys/reboot.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

static volatile sig_atomic_t stopping;

static void request_stop(int sig)
{
    (void)sig;
    stopping = 1;
}

static void make_dir(const char *path, mode_t mode)
{
    if (mkdir(path, mode) && errno != EEXIST)
        fprintf(stderr, "REM_INIT|mkdir-failed|%s|%s\n", path, strerror(errno));
}

static void mount_fs(const char *source, const char *target, const char *type,
                     unsigned long flags)
{
    if (mount(source, target, type, flags, NULL) && errno != EBUSY)
        fprintf(stderr, "REM_INIT|mount-warning|%s|%s|%s\n",
                target, type, strerror(errno));
}

static void set_host(void)
{
    char name[128];
    FILE *f = fopen("/etc/hostname", "r");
    if (!f) {
        puts("REM_INIT|hostname|FAIL|/etc/hostname unavailable");
        return;
    }
    if (fgets(name, sizeof(name), f)) {
        name[strcspn(name, "\r\n")] = '\0';
        if (*name && sethostname(name, strlen(name))) {
            fprintf(stderr, "REM_INIT|hostname-warning|%s\n", strerror(errno));
            puts("REM_INIT|hostname|FAIL|sethostname unavailable");
        } else {
            puts("REM_INIT|hostname|PASS");
        }
    }
    fclose(f);
}

static int verify_mounts(void)
{
    if (access("/proc/1", F_OK)) {
        fprintf(stderr, "REM_INIT|mount-fail|proc|errno=%d\n", errno);
        return -1;
    }
    if (access("/sys/class", F_OK)) {
        fprintf(stderr, "REM_INIT|mount-fail|sys|errno=%d\n", errno);
        return -1;
    }
    if (access("/dev/pts/ptmx", F_OK)) {
        fprintf(stderr, "REM_INIT|mount-fail|devpts|errno=%d\n", errno);
        return -1;
    }
    if (access("/dev/ttyMY0", F_OK)) {
        fprintf(stderr, "REM_INIT|mount-fail|ttyMY0|errno=%d\n", errno);
        return -1;
    }
    return 0;
}

static int run_boot_scripts(void)
{
    pid_t pid = fork();
    if (pid < 0) {
        fprintf(stderr, "REM_INIT|rcS-fork-failed|%s\n", strerror(errno));
        return -1;
    }
    if (!pid) {
        execl("/etc/init.d/rcS", "rcS", (char *)NULL);
        dprintf(STDERR_FILENO, "REM_INIT|rcS-exec-failed|%s\n", strerror(errno));
        _exit(127);
    }
    int status = 0;
    pid_t waited;
    do {
        waited = waitpid(pid, &status, 0);
    } while (waited < 0 && errno == EINTR);
    if (waited != pid)
        return -1;
    if (!WIFEXITED(status) || WEXITSTATUS(status)) {
        fprintf(stderr, "REM_INIT|rcS-failed|status=%d\n", status);
        return -1;
    }
    puts("REM_INIT|rcS|PASS");
    return 0;
}

static pid_t start_getty(void)
{
    pid_t pid = fork();
    if (pid < 0) {
        fprintf(stderr, "REM_INIT|getty-fork-failed|%s\n", strerror(errno));
        return -1;
    }
    if (!pid) {
        execl("/sbin/getty", "getty", "/dev/ttyMY0", (char *)NULL);
        dprintf(STDERR_FILENO, "REM_INIT|getty-exec-failed|%s\n", strerror(errno));
        _exit(127);
    }
    printf("REM_INIT|getty|pid=%ld\n", (long)pid);
    return pid;
}

int main(void)
{
    struct sigaction sa;
    memset(&sa, 0, sizeof(sa));
    sa.sa_handler = request_stop;
    sigemptyset(&sa.sa_mask);
    sigaction(SIGTERM, &sa, NULL);
    sigaction(SIGINT, &sa, NULL);
    sigaction(SIGPWR, &sa, NULL);

    setenv("PATH", "/usr/local/bin:/usr/bin:/bin:/usr/local/sbin:/usr/sbin:/sbin", 1);
    make_dir("/proc", 0555);
    make_dir("/sys", 0555);
    make_dir("/run", 0755);
    make_dir("/tmp", 01777);
    make_dir("/dev", 0755);
    make_dir("/dev/pts", 0755);
    mount_fs("devtmpfs", "/dev", "devtmpfs", MS_NOSUID);
    mount_fs("proc", "/proc", "proc", MS_NOSUID | MS_NOEXEC | MS_NODEV);
    mount_fs("sysfs", "/sys", "sysfs", MS_NOSUID | MS_NOEXEC | MS_NODEV);
    mount_fs("tmpfs", "/run", "tmpfs", MS_NOSUID | MS_NODEV);
    mount_fs("tmpfs", "/tmp", "tmpfs", MS_NOSUID | MS_NODEV);
    (void)chmod("/tmp", 01777);
    mount_fs("devpts", "/dev/pts", "devpts", MS_NOSUID | MS_NOEXEC);

    int console = open("/dev/console", O_RDWR);
    if (console >= 0) {
        for (int fd = 0; fd <= 2; fd++)
            if (fd != console)
                dup2(console, fd);
        if (console > 2)
            close(console);
    }
    set_host();
    printf("REM_INIT|pid=%ld|REM Linux 0.2\n", (long)getpid());
    if (getpid() != 1)
        puts("REM_INIT|warning|not-pid1");
    if (verify_mounts())
        puts("REM_INIT|proc-sys-dev|FAIL");
    else
        puts("REM_INIT|proc-sys-dev|PASS");
    fflush(NULL);

    run_boot_scripts();
    pid_t getty = start_getty();
    while (!stopping) {
        int status;
        pid_t dead = waitpid(getty, &status, 0);
        if (dead < 0 && errno == EINTR)
            continue;
        if (stopping)
            break;
        if (dead == getty)
            fprintf(stderr, "REM_INIT|getty-exit|status=%d|respawn=1\n", status);
        while (waitpid(-1, &status, WNOHANG) > 0)
            ;
        sleep(1);
        getty = start_getty();
        if (getty < 0)
            sleep(2);
    }

    puts("REM_INIT|shutdown|starting");
    fflush(NULL);
    sync();
    reboot(RB_POWER_OFF);
    for (;;)
        pause();
}
