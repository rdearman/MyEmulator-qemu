/*
 * Small guest-side probe for the REM Vim E667/E1286 runtime boundaries.
 * Build statically for myemulator2-linux-musl and run from a REM shell.
 */
#define _GNU_SOURCE
#include <errno.h>
#include <fcntl.h>
#include <pthread.h>
#include <signal.h>
#include <stdio.h>
#include <sys/syscall.h>
#include <time.h>
#include <unistd.h>

static void *noop(void *arg)
{
    return arg;
}

static void timer_callback(union sigval value)
{
    (void)value;
}

int main(void)
{
    int fd = open("/tmp/rem-vim-syscall-probe", O_CREAT | O_TRUNC | O_WRONLY,
                  0600);
    if (fd < 0) {
        perror("open");
        return 2;
    }
    (void)write(fd, "probe", 5);

    errno = 0;
    int rc = fsync(fd);
    int err = errno;
    printf("libc_fsync rc=%d errno=%d\n", rc, err);

    errno = 0;
    long raw74 = syscall(74, fd);
    err = errno;
    printf("raw_fsync_74 rc=%ld errno=%d\n", raw74, err);

    errno = 0;
    long raw82 = syscall(82, fd);
    err = errno;
    printf("raw_fsync_82 rc=%ld errno=%d\n", raw82, err);

    pthread_t thread;
    errno = 0;
    int thread_rc = pthread_create(&thread, NULL, noop, NULL);
    err = errno;
    printf("pthread_create rc=%d errno=%d\n", thread_rc, err);
    if (thread_rc == 0)
        (void)pthread_join(thread, NULL);

    struct sigevent event = {0};
    event.sigev_notify = SIGEV_THREAD;
    event.sigev_notify_function = timer_callback;
    timer_t timer;
    errno = 0;
    int timer_rc = timer_create(CLOCK_MONOTONIC, &event, &timer);
    err = errno;
    printf("libc_timer_create_sigev_thread rc=%d errno=%d\n", timer_rc, err);

    errno = 0;
    long raw_timer_create = syscall(107, CLOCK_MONOTONIC, NULL, &timer);
    err = errno;
    printf("raw_timer_create_107 rc=%ld errno=%d\n", raw_timer_create, err);

    errno = 0;
    long raw_timer_settime = syscall(409, (long)timer, 0, NULL, NULL);
    err = errno;
    printf("raw_timer_settime_409 rc=%ld errno=%d\n", raw_timer_settime, err);

    close(fd);
    return 0;
}
