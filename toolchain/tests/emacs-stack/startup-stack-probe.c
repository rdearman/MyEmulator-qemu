#define _GNU_SOURCE
#include <alloca.h>
#include <errno.h>
#include <fcntl.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/resource.h>
#include <unistd.h>

static volatile unsigned long touched;

static void show_maps(void)
{
    char buf[512];
    int fd = open("/proc/self/maps", O_RDONLY);
    if (fd < 0) {
        printf("maps unavailable errno=%d\n", errno);
        return;
    }
    puts("MAPS_BEGIN");
    ssize_t n;
    while ((n = read(fd, buf, sizeof(buf))) > 0)
        (void)write(STDOUT_FILENO, buf, (size_t)n);
    close(fd);
    puts("MAPS_END");
}

__attribute__((noinline)) static void recurse(unsigned long depth)
{
    volatile unsigned char frame[2048];
    frame[0] = (unsigned char)depth;
    frame[sizeof(frame) - 1] = (unsigned char)(depth >> 8);
    touched += frame[0] + frame[sizeof(frame) - 1];
    if (depth == 1) {
        printf("depth=%lu sp=%p\n", depth, (void *)&frame[0]);
        fflush(stdout);
    }
    recurse(depth + 1);
    touched += frame[0];
}

static void show_limits(void)
{
    struct rlimit stack;
    if (getrlimit(RLIMIT_STACK, &stack) == 0)
        printf("RLIMIT_STACK soft=%lu hard=%lu\n",
               (unsigned long)stack.rlim_cur, (unsigned long)stack.rlim_max);
    else
        printf("getrlimit failed errno=%d\n", errno);
    printf("brk=%p\n", sbrk(0));
    show_maps();
    fflush(stdout);
}

int main(int argc, char **argv)
{
    show_limits();
    if (argc < 2) {
        puts("usage: startup-stack-probe malloc|malloc-nofree|alloca|alloca-malloc|recurse [bytes]");
        return 2;
    }
    if (strcmp(argv[1], "malloc") == 0 || strcmp(argv[1], "malloc-nofree") == 0) {
        size_t n = argc > 2 ? strtoul(argv[2], NULL, 0) : 4096;
        volatile unsigned char *p = malloc(n);
        if (p)
            for (size_t i = 0; i < n; i += 4096)
                p[i] = (unsigned char)i;
        printf("malloc=%p size=%lu errno=%d brk=%p\n", (void *)p,
               (unsigned long)n, errno, sbrk(0));
        show_maps();
        if (strcmp(argv[1], "malloc-nofree") != 0) {
            puts("before-free");
            fflush(stdout);
            free((void *)p);
            puts("after-free");
            fflush(stdout);
        }
        return p ? 0 : 1;
    }
    if (strcmp(argv[1], "recurse") == 0) {
        recurse(1);
        return 0;
    }
    if (strcmp(argv[1], "alloca") == 0 || strcmp(argv[1], "alloca-malloc") == 0) {
        size_t n = argc > 2 ? strtoul(argv[2], NULL, 0) : 1024 * 1024;
        volatile unsigned char *p = alloca(n);
        for (size_t i = 0; i < n; i += 4096)
            p[i] = (unsigned char)i;
        if (n)
            p[n - 1] = 0x5a;
        printf("alloca=%lu touched=%u brk=%p\n", (unsigned long)n,
               n ? p[n - 1] : 0, sbrk(0));
        if (strcmp(argv[1], "alloca-malloc") == 0) {
            void *m = malloc(4096);
            printf("malloc-near-stack=%p errno=%d brk=%p\n", m, errno, sbrk(0));
            free(m);
            return m ? 0 : 1;
        }
        return 0;
    }
    return 2;
}
