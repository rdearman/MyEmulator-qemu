// SPDX-License-Identifier: GPL-2.0
#include <linux/kernel.h>
#include <linux/sched/signal.h>
#include <linux/syscalls.h>
#include <linux/uaccess.h>

/* Linux's generic 32-bit syscall numbering is used by this initial port.
 * Keep the small bootstrap dispatcher independent of generated syscall-table
 * headers until the full architecture syscall table is wired in. */
#define MYEMU2_NR_READ       63
#define MYEMU2_NR_WRITE      64
#define MYEMU2_NR_EXIT       93
#define MYEMU2_NR_EXIT_GROUP 94

extern ssize_t ksys_write(unsigned int fd, const char __user *buf,
			  size_t count);
extern ssize_t myemulator2_console_write_user(const char __user *buf,
						      size_t count);
extern ssize_t myemulator2_console_read_user(char __user *buf,
						     size_t count);

asmlinkage long myemulator2_syscall(unsigned long nr,
		unsigned long a0, unsigned long a1, unsigned long a2,
		unsigned long a3)
{
	switch (nr) {
	case MYEMU2_NR_READ:
		if (a0 == 0)
			return myemulator2_console_read_user((char __user *)a1,
							     (size_t)a2);
		return -EBADF;
	case MYEMU2_NR_WRITE:
		if (a0 == 1 || a0 == 2)
			return myemulator2_console_write_user(
				(const char __user *)a1, (size_t)a2);
		return ksys_write((unsigned int)a0,
			(const char __user *)a1, (size_t)a2);
	case MYEMU2_NR_EXIT:
	case MYEMU2_NR_EXIT_GROUP:
		do_group_exit((int)a0);
		return 0;
	default:
		return -ENOSYS;
	}
}
