#ifndef MYEMU_SYS_STAT_H
#define MYEMU_SYS_STAT_H
#include <stddef.h>
typedef unsigned int mode_t;
typedef unsigned int nlink_t;
typedef unsigned int uid_t;
typedef unsigned int gid_t;
typedef unsigned int dev_t;
typedef long off_t;
typedef long time_t;
struct timespec { long tv_sec; long tv_nsec; };
struct stat {
	dev_t st_dev; unsigned long long st_ino; mode_t st_mode; nlink_t st_nlink;
	uid_t st_uid; gid_t st_gid; dev_t st_rdev; unsigned int __pad0;
	off_t st_size; int st_blksize; int __pad1; long st_blocks;
	struct timespec st_atim, st_mtim, st_ctim; unsigned int __unused[2];
};
int fstat(int fd, struct stat *st);
int stat(const char *path, struct stat *st);
#define S_IFMT 0170000
#define S_IFREG 0100000
#define S_IFDIR 0040000
#define S_ISREG(m) (((m) & S_IFMT) == S_IFREG)
#define S_ISDIR(m) (((m) & S_IFMT) == S_IFDIR)
#endif
