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
#define MYEMU2_NR_EXIT       93
#define MYEMU2_NR_EXIT_GROUP 94
#define MYEMU2_NR_GETPID     172
#define MYEMU2_NR_CLONE      220
#define MYEMU2_NR_WAIT4      260

extern ssize_t ksys_write(unsigned int fd, const char __user *buf,
			  size_t count);
extern ssize_t myemulator2_console_write_user(const char __user *buf,
						      size_t count);
extern ssize_t myemulator2_console_read_user(char __user *buf,
						     size_t count);
extern long sys_ioctl(unsigned int fd, unsigned int cmd,
				      unsigned long arg);
extern long sys_close(unsigned int fd);
extern long sys_getpid(void);
extern long sys_clone(unsigned long flags, unsigned long newsp,
			      int __user *parent_tid, int __user *child_tid,
			      unsigned long tls);
extern long sys_wait4(pid_t pid, int __user *stat_addr, int options,
			      struct rusage __user *ru);

asmlinkage long myemulator2_syscall(unsigned long nr,
		unsigned long a0, unsigned long a1, unsigned long a2,
		unsigned long a3)
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
	case MYEMU2_NR_IOCTL:
		return sys_ioctl((unsigned int)a0, (unsigned int)a1, a2);
	case MYEMU2_NR_EXIT:
	case MYEMU2_NR_EXIT_GROUP:
		do_group_exit((int)a0);
		return 0;
	case MYEMU2_NR_GETPID:
		return sys_getpid();
	case MYEMU2_NR_CLONE:
		return sys_clone(a0, a1, (int __user *)a2,
				 (int __user *)a3, 0);
	case MYEMU2_NR_WAIT4:
		return sys_wait4((pid_t)a0, (int __user *)a1, (int)a2,
				 (struct rusage __user *)a3);
	default:
		return -ENOSYS;
	}
}
