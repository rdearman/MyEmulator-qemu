#ifndef MYEMU_UNISTD_H
#define MYEMU_UNISTD_H
#include <stddef.h>
long read(int fd, void *buf, size_t count);
long write(int fd, const void *buf, size_t count);
int close(int fd);
int open(const char *path, int flags, int mode);
int openat(int dirfd, const char *path, int flags, int mode);
long lseek(int fd, long offset, int whence);
int chdir(const char *path);
char *getcwd(char *buf, size_t size);
int unlink(const char *path);
int rmdir(const char *path);
int mkdir(const char *path, int mode);
int rename(const char *oldpath, const char *newpath);
int usleep(unsigned int usec);
int getpid(void);
int sched_yield(void);
#endif
