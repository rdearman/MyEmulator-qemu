#ifndef MYEMU_SYSCALL_H
#define MYEMU_SYSCALL_H

typedef unsigned long size_t;
typedef long ssize_t;

long myemu_read(int fd, void *buf, size_t count);
long myemu_write(int fd, const void *buf, size_t count);
long myemu_getpid(void);
void myemu_exit(int status);

#endif
