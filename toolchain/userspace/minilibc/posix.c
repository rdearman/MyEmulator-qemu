#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <dirent.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <sys/utsname.h>
#include <time.h>
#include <stdio.h>
#include <myemu/syscall.h>

long read(int fd, void *buf, size_t count) { return myemu_read(fd, buf, count); }
long write(int fd, const void *buf, size_t count) { return myemu_write(fd, buf, count); }
int close(int fd) { return (int)myemu_close(fd); }
long lseek(int fd, long off, int whence) { return myemu_lseek(fd, off, whence); }
int getpid(void) { return (int)myemu_getpid(); }
int sched_yield(void) { return (int)myemu_sched_yield(); }
int chdir(const char *path) { return (int)myemu_chdir(path); }
int mkdir(const char *path, int mode) { return (int)myemu_mkdirat(AT_FDCWD, path, mode); }
int unlink(const char *path) { return (int)myemu_unlinkat(AT_FDCWD, path, 0); }
int rmdir(const char *path) { return (int)myemu_unlinkat(AT_FDCWD, path, AT_REMOVEDIR); }
int rename(const char *a, const char *b) { return (int)myemu_renameat(AT_FDCWD, a, AT_FDCWD, b); }
int fstat(int fd, struct stat *st) { return (int)myemu_fstat(fd, st); }
int stat(const char *path, struct stat *st) { return (int)myemu_fstatat(AT_FDCWD, path, st, 0); }
char *getcwd(char *buf, size_t size) { return myemu_getcwd(buf, size) < 0 ? 0 : buf; }
int uname(struct utsname *name) { return (int)myemu_uname(name); }
int clock_gettime(int id, struct timespec *ts) { return (int)myemu_clock_gettime(id, ts); }
int nanosleep(const struct timespec *req, struct timespec *rem) { return (int)myemu_nanosleep(req, rem); }
int usleep(unsigned int usec) { struct timespec t = {(long)(usec / 1000000), (long)(usec % 1000000) * 1000}; return nanosleep(&t, 0); }
int waitpid(int pid, int *status, int options) { return (int)myemu_wait4(pid, status, options, 0); }

int open(const char *path, int flags, int mode) { return (int)myemu_openat(AT_FDCWD, path, flags, mode); }
int openat(int dirfd, const char *path, int flags, int mode) { return (int)myemu_openat(dirfd, path, flags, mode); }
struct linux_dirent64 { unsigned long long ino; long long off; unsigned short reclen; unsigned char type; char name[0]; };
DIR *opendir(const char *path)
{
	DIR *d = malloc(sizeof(*d));
	if (!d) return 0;
	d->fd = open(path, O_RDONLY, 0); d->length = d->offset = 0;
	if (d->fd < 0) { free(d); return 0; }
	return d;
}
struct dirent *readdir(DIR *d)
{
	struct linux_dirent64 *in;
	if (d->offset >= d->length) {
		d->length = (int)myemu_getdents64(d->fd, d->buffer, sizeof(d->buffer)); d->offset = 0;
		if (d->length <= 0) return 0;
	}
	in = (struct linux_dirent64 *)(d->buffer + d->offset);
	d->offset += in->reclen;
	d->current.d_ino = in->ino; d->current.d_off = (long)in->off;
	d->current.d_reclen = in->reclen; d->current.d_type = in->type;
	strncpy(d->current.d_name, in->name, sizeof(d->current.d_name) - 1);
	d->current.d_name[sizeof(d->current.d_name) - 1] = 0;
	return &d->current;
}
int closedir(DIR *d) { int ret = close(d->fd); free(d); return ret; }
