#ifndef MYEMU_SYSCALL_H
#define MYEMU_SYSCALL_H

#include <stddef.h>
typedef long ssize_t;

long myemu_read(int fd, void *buf, size_t count);
long myemu_write(int fd, const void *buf, size_t count);
long myemu_getpid(void);
void myemu_exit(int status);
long myemu_openat(int dirfd, const char *path, int flags, int mode);
long myemu_close(int fd);
long myemu_lseek(int fd, long offset, int whence);
long myemu_fstat(int fd, void *statbuf);
long myemu_fstatat(int dirfd, const char *path, void *statbuf, int flags);
long myemu_getdents64(int fd, void *buffer, size_t count);
long myemu_getcwd(char *buffer, size_t size);
long myemu_mkdirat(int dirfd, const char *path, int mode);
long myemu_unlinkat(int dirfd, const char *path, int flags);
long myemu_renameat(int olddirfd, const char *oldpath,
                    int newdirfd, const char *newpath);
long myemu_chdir(const char *path);
long myemu_uname(void *name);
long myemu_sched_yield(void);
long myemu_nanosleep(const void *request, void *remaining);
long myemu_clock_gettime(int clockid, void *timespec);
long myemu_brk(void *address);
long myemu_mmap(void *address, size_t length, int prot, int flags,
                int fd, long offset);
long myemu_munmap(void *address, size_t length);
long myemu_clone(unsigned long flags, void *stack, int *parent_tid,
                 int *child_tid, void *tls);
long myemu_wait4(int pid, int *status, int options, void *rusage);

#endif
