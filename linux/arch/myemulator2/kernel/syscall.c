// SPDX-License-Identifier: GPL-2.0
#include <linux/kernel.h>
#include <linux/fs.h>
#include <linux/fcntl.h>
#include <linux/sched/signal.h>
#include <linux/syscalls.h>
#include <linux/uaccess.h>
#include <linux/file.h>

/* Linux's generic 32-bit syscall numbering is used by this initial port.
 * Keep the small bootstrap dispatcher independent of generated syscall-table
 * headers until the full architecture syscall table is wired in. */
#define MYEMU2_NR_READ       63
#define MYEMU2_NR_WRITE      64
#define MYEMU2_NR_IOCTL      29
#define MYEMU2_NR_OPENAT     56
#define MYEMU2_NR_CLOSE      57
#define MYEMU2_NR_FSYNC      74
#define MYEMU2_NR_PIPE2      59
#define MYEMU2_NR_GETDENTS64 61
#define MYEMU2_NR_DUP        23
#define MYEMU2_NR_DUP3       24
#define MYEMU2_NR_MUNMAP     215
#define MYEMU2_NR_BRK        214
#define MYEMU2_NR_MMAP       222
#define MYEMU2_NR_EXIT       93
#define MYEMU2_NR_EXIT_GROUP 94
#define MYEMU2_NR_NANOSLEEP  101
#define MYEMU2_NR_GETPID     172
#define MYEMU2_NR_EXECVE     221
#define MYEMU2_NR_CLOCK_GETTIME64 403
#define MYEMU2_NR_CLONE      220
#define MYEMU2_NR_WAIT4      260
#define MYEMU2_NR_GETCWD     17
#define MYEMU2_NR_MKDIRAT    34
#define MYEMU2_NR_UNLINKAT   35
#define MYEMU2_NR_RENAMEAT   38
#define MYEMU2_NR_CHDIR      49
#define MYEMU2_NR_LSEEK      62
#define MYEMU2_NR_FSTATAT   79
#define MYEMU2_NR_FSTAT      80
#define MYEMU2_NR_UNAME      160
#define MYEMU2_NR_SCHED_YIELD 124

extern ssize_t ksys_write(unsigned int fd, const char __user *buf,
			  size_t count);
extern ssize_t myemulator2_console_write_user(const char __user *buf,
						      size_t count);
extern ssize_t myemulator2_console_read_user(char __user *buf,
						     size_t count);
extern long sys_ioctl(unsigned int fd, unsigned int cmd,
				      unsigned long arg);
extern long sys_close(unsigned int fd);
extern long sys_fsync(unsigned int fd);
extern long sys_dup(unsigned int fd);
extern long sys_dup3(unsigned int oldfd, unsigned int newfd, int flags);
extern long sys_pipe2(int __user *fildes, int flags);
extern long sys_getdents64(unsigned int fd, struct linux_dirent64 __user *dirent,
			   unsigned int count);
extern long sys_brk(unsigned long brk);
extern long sys_munmap(unsigned long addr, size_t len);
extern long sys_mmap_pgoff(unsigned long addr, unsigned long len,
			   unsigned long prot, unsigned long flags,
			   unsigned long fd, unsigned long pgoff);
extern long sys_execve(const char __user *filename,
			       const char __user *const __user *argv,
			       const char __user *const __user *envp);
extern long sys_nanosleep_time32(struct old_timespec32 __user *rqtp,
					 struct old_timespec32 __user *rmtp);
extern long sys_clock_gettime32(clockid_t which_clock,
					 struct old_timespec32 __user *tp);
extern long sys_getpid(void);
extern long sys_clone(unsigned long flags, unsigned long newsp,
			      int __user *parent_tid, int __user *child_tid,
			      unsigned long tls);
extern long sys_wait4(pid_t pid, int __user *stat_addr, int options,
			      struct rusage __user *ru);
extern long sys_getcwd(char __user *buf, unsigned long size);
extern long sys_mkdirat(int dfd, const char __user *pathname, umode_t mode);
extern long sys_unlinkat(int dfd, const char __user *pathname, int flag);
extern long sys_renameat(int olddfd, const char __user *oldname,
                         int newdfd, const char __user *newname);
extern long sys_chdir(const char __user *filename);
extern long sys_lseek(unsigned int fd, off_t offset, unsigned int whence);
extern long sys_newfstatat(int dfd, const char __user *filename,
                           struct stat __user *statbuf, int flag);
extern long sys_newfstat(unsigned int fd, struct stat __user *statbuf);
extern long sys_newuname(struct new_utsname __user *name);
extern long sys_sched_yield(void);

asmlinkage long myemulator2_syscall(unsigned long nr,
		unsigned long a0, unsigned long a1, unsigned long a2,
		unsigned long a3, unsigned long a4, unsigned long a5)
{
	switch (nr) {
	case MYEMU2_NR_READ:
		if (a0 == 0) {
			struct file *file = fget((unsigned int)a0);
			ssize_t ret;
			if (file) {
				fput(file);
				ret = ksys_read((unsigned int)a0, (char __user *)a1,
						(size_t)a2);
				return ret;
			}
			return myemulator2_console_read_user((char __user *)a1,
							     (size_t)a2);
		}
		return ksys_read((unsigned int)a0, (char __user *)a1,
				 (size_t)a2);
	case MYEMU2_NR_WRITE:
		if (a0 == 1 || a0 == 2) {
			struct file *file = fget((unsigned int)a0);
			if (file) {
				fput(file);
				return ksys_write((unsigned int)a0,
						  (const char __user *)a1, (size_t)a2);
			}
			return myemulator2_console_write_user(
				(const char __user *)a1, (size_t)a2);
		}
		return ksys_write((unsigned int)a0,
			(const char __user *)a1, (size_t)a2);
	case MYEMU2_NR_OPENAT:
		return do_sys_open((int)a0, (const char __user *)a1,
				   (int)a2, (umode_t)a3);
	case MYEMU2_NR_CLOSE:
		return sys_close((unsigned int)a0);
	case MYEMU2_NR_FSYNC:
		return sys_fsync((unsigned int)a0);
	case MYEMU2_NR_DUP:
		return sys_dup((unsigned int)a0);
	case MYEMU2_NR_DUP3:
		return sys_dup3((unsigned int)a0, (unsigned int)a1, (int)a2);
	case MYEMU2_NR_PIPE2:
		return sys_pipe2((int __user *)a0, (int)a1);
	case MYEMU2_NR_GETDENTS64:
		return sys_getdents64((unsigned int)a0,
			(struct linux_dirent64 __user *)a1, (unsigned int)a2);
	case MYEMU2_NR_BRK:
		return sys_brk(a0);
	case MYEMU2_NR_MUNMAP:
		return sys_munmap(a0, a1);
	case MYEMU2_NR_MMAP:
		return sys_mmap_pgoff(a0, a1, a2, a3, a4, a5);
	case MYEMU2_NR_IOCTL:
		return sys_ioctl((unsigned int)a0, (unsigned int)a1, a2);
	case MYEMU2_NR_EXIT:
	case MYEMU2_NR_EXIT_GROUP:
		do_group_exit((int)a0);
		return 0;
	case MYEMU2_NR_GETPID:
		return sys_getpid();
	case MYEMU2_NR_EXECVE:
		return sys_execve((const char __user *)a0,
			(const char __user *const __user *)a1,
			(const char __user *const __user *)a2);
	case MYEMU2_NR_NANOSLEEP:
		return sys_nanosleep_time32((struct old_timespec32 __user *)a0,
			(struct old_timespec32 __user *)a1);
	case MYEMU2_NR_CLOCK_GETTIME64:
		return sys_clock_gettime32((clockid_t)a0,
			(struct old_timespec32 __user *)a1);
	case MYEMU2_NR_CLONE:
		return sys_clone(a0, a1, (int __user *)a2,
				 (int __user *)a3, 0);
	case MYEMU2_NR_WAIT4:
		return sys_wait4((pid_t)a0, (int __user *)a1, (int)a2,
				 (struct rusage __user *)a3);
	case MYEMU2_NR_GETCWD:
		return sys_getcwd((char __user *)a0, a1);
	case MYEMU2_NR_MKDIRAT:
		return sys_mkdirat((int)a0, (const char __user *)a1,
				   (umode_t)a2);
	case MYEMU2_NR_UNLINKAT:
		return sys_unlinkat((int)a0, (const char __user *)a1, (int)a2);
	case MYEMU2_NR_RENAMEAT:
		return sys_renameat((int)a0, (const char __user *)a1,
				   (int)a2, (const char __user *)a3);
	case MYEMU2_NR_CHDIR:
		return sys_chdir((const char __user *)a0);
	case MYEMU2_NR_LSEEK:
		return sys_lseek((unsigned int)a0, (off_t)a1, (unsigned int)a2);
	case MYEMU2_NR_FSTATAT:
		return sys_newfstatat((int)a0, (const char __user *)a1,
				      (struct stat __user *)a2, (int)a3);
	case MYEMU2_NR_FSTAT:
		return sys_newfstat((unsigned int)a0, (struct stat __user *)a1);
	case MYEMU2_NR_UNAME:
		return sys_newuname((struct new_utsname __user *)a0);
	case MYEMU2_NR_SCHED_YIELD:
		return sys_sched_yield();
	default:
		return -ENOSYS;
	}
}
