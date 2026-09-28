// SPDX-License-Identifier: GPL-2.0
#include <linux/kernel.h>
#include <linux/fs.h>
#include <linux/fcntl.h>
#include <linux/sched/signal.h>
#include <linux/syscalls.h>
#include <linux/uaccess.h>
#include <linux/file.h>
#include <linux/uio.h>
#include <linux/resource.h>
#include <linux/tty.h>
#include <uapi/asm-generic/ioctls.h>

/* Linux's generic 32-bit syscall numbering is used by this initial port.
 * Keep the small bootstrap dispatcher independent of generated syscall-table
 * headers until the full architecture syscall table is wired in. */
#define MYEMU2_NR_READ       63
#define MYEMU2_NR_WRITE      64
#define MYEMU2_NR_WRITEV     66
#define MYEMU2_NR_FCHMOD     52
#define MYEMU2_NR_FCHMODAT   53
#define MYEMU2_NR_FACCESSAT  48
#define MYEMU2_NR_FACCESSAT2 439
#define MYEMU2_NR_FCNTL      25
#define MYEMU2_NR_IOCTL      29
#define MYEMU2_NR_OPENAT     56
#define MYEMU2_NR_CLOSE      57
#define MYEMU2_NR_FSYNC      74
#define MYEMU2_NR_SYNC       81
#define MYEMU2_NR_PIPE2      59
#define MYEMU2_NR_GETDENTS64 61
#define MYEMU2_NR_DUP        23
#define MYEMU2_NR_DUP3       24
#define MYEMU2_NR_PPOLL      73
#define MYEMU2_NR_PSELECT6   72
#define MYEMU2_NR_MUNMAP     215
#define MYEMU2_NR_BRK        214
#define MYEMU2_NR_MMAP       222
#define MYEMU2_NR_MPROTECT   226
#define MYEMU2_NR_EXIT       93
#define MYEMU2_NR_EXIT_GROUP 94
#define MYEMU2_NR_NANOSLEEP  101
#define MYEMU2_NR_GETPID     172
#define MYEMU2_NR_TIMES      153
#define MYEMU2_NR_GETRUSAGE  165
#define MYEMU2_NR_GETPPID    173
#define MYEMU2_NR_GETUID     174
#define MYEMU2_NR_GETEUID    175
#define MYEMU2_NR_GETGID     176
#define MYEMU2_NR_GETEGID    177
#define MYEMU2_NR_SETPGID    154
#define MYEMU2_NR_GETPGID    155
#define MYEMU2_NR_GETSID     156
#define MYEMU2_NR_SETSID     157
#define MYEMU2_NR_SETGID     144
#define MYEMU2_NR_SETUID     146
#define MYEMU2_NR_EXECVE     221
#define MYEMU2_NR_CLOCK_GETTIME64 403
#define MYEMU2_NR_CLONE      220
#define MYEMU2_NR_WAIT4      260
#define MYEMU2_NR_GETCWD     17
#define MYEMU2_NR_MKDIRAT    34
#define MYEMU2_NR_UNLINKAT   35
#define MYEMU2_NR_RENAMEAT   38
#define MYEMU2_NR_CHDIR      49
#define MYEMU2_NR_UMASK      166
#define MYEMU2_NR_LSEEK      62
#define MYEMU2_NR_FSTATAT   79
#define MYEMU2_NR_FSTAT      80
#define MYEMU2_NR_UNAME      160
#define MYEMU2_NR_SCHED_YIELD 124
#define MYEMU2_NR_FUTEX       422
#define MYEMU2_NR_STATX      291
#define MYEMU2_NR_SET_TID_ADDRESS 96
#define MYEMU2_NR_GETTID      178
#define MYEMU2_NR_SET_ROBUST_LIST 99
#define MYEMU2_NR_RT_SIGACTION 134
#define MYEMU2_NR_RT_SIGPROCMASK 135
#define MYEMU2_NR_KILL        129
#define MYEMU2_NR_TKILL       130
#define MYEMU2_NR_TGKILL      131
#define MYEMU2_NR_SIGALTSTACK 132
#define MYEMU2_NR_RT_SIGSUSPEND 133
#define MYEMU2_NR_UMOUNT2    39
#define MYEMU2_NR_MOUNT      40
#define MYEMU2_NR_SOCKET    198
#define MYEMU2_NR_BIND      200
#define MYEMU2_NR_LISTEN    201
#define MYEMU2_NR_ACCEPT    202
#define MYEMU2_NR_CONNECT   203
#define MYEMU2_NR_GETSOCKNAME 204
#define MYEMU2_NR_GETPEERNAME 205
#define MYEMU2_NR_SENDTO    206
#define MYEMU2_NR_RECVFROM  207
#define MYEMU2_NR_SETSOCKOPT 208
#define MYEMU2_NR_GETSOCKOPT 209
#define MYEMU2_NR_SHUTDOWN  210
#define MYEMU2_NR_SENDMSG   211
#define MYEMU2_NR_RECVMSG   212
#define MYEMU2_NR_PTRACE    117

extern ssize_t ksys_write(unsigned int fd, const char __user *buf,
			  size_t count);
extern ssize_t myemulator2_console_write_user(const char __user *buf,
						      size_t count);
extern ssize_t myemulator2_console_read_user(char __user *buf,
						     size_t count);
extern long sys_ioctl(unsigned int fd, unsigned int cmd,
				      unsigned long arg);
extern long sys_close(unsigned int fd);
extern long sys_ptrace(long request, long pid, unsigned long addr,
		       unsigned long data);
extern long sys_fsync(unsigned int fd);
extern long sys_fdatasync(unsigned int fd);
extern long sys_fchmod(unsigned int fd, umode_t mode);
extern long sys_fchmodat(int dfd, const char __user *filename, umode_t mode);
extern long sys_faccessat(int dfd, const char __user *filename, int mode);
extern long sys_faccessat2(int dfd, const char __user *filename, int mode,
			   int flags);
extern long sys_ftruncate64(unsigned int fd, loff_t length);
extern void ksys_sync(void);
extern long sys_dup(unsigned int fd);
extern long sys_dup3(unsigned int oldfd, unsigned int newfd, int flags);
extern long sys_pipe2(int __user *fildes, int flags);
extern long sys_getdents64(unsigned int fd, struct linux_dirent64 __user *dirent,
			   unsigned int count);
extern long sys_brk(unsigned long brk);
extern long sys_munmap(unsigned long addr, size_t len);
extern long sys_mprotect(unsigned long start, size_t len, unsigned long prot);
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
extern long sys_times(struct tms __user *tbuf);
extern long sys_getrusage(int who, struct rusage __user *ru);
extern long sys_getppid(void);
extern long sys_getuid(void);
extern long sys_geteuid(void);
extern long sys_getgid(void);
extern long sys_getegid(void);
extern long sys_setpgid(pid_t pid, pid_t pgid);
extern long sys_getpgid(pid_t pid);
extern long sys_getsid(pid_t pid);
extern long sys_setsid(void);
extern long sys_setgid(gid_t gid);
extern long sys_setuid(uid_t uid);
extern long sys_fcntl(unsigned int fd, unsigned int cmd, unsigned long arg);
extern long sys_fcntl64(unsigned int fd, unsigned int cmd, unsigned long arg);
extern long sys_writev(unsigned long fd, const struct iovec __user *vec,
			unsigned long vlen);
extern long sys_readv(unsigned long fd, const struct iovec __user *vec,
			 unsigned long vlen);
extern long sys_fchdir(unsigned int fd);
extern long sys_access(const char __user *filename, int mode);
extern long sys_readlinkat(int dfd, const char __user *pathname,
			      char __user *buf, int bufsiz);
extern long sys_getrandom(char __user *buf, size_t count, unsigned int flags);
extern long sys_prlimit64(pid_t pid, unsigned int resource,
			  const struct rlimit64 __user *new_rlim,
			  struct rlimit64 __user *old_rlim);
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
extern long sys_llseek(unsigned int fd, unsigned long offset_high,
				      unsigned long offset_low,
				      loff_t __user *result, unsigned int whence);
extern long sys_newfstatat(int dfd, const char __user *filename,
                           struct stat __user *statbuf, int flag);
extern long sys_newfstat(unsigned int fd, struct stat __user *statbuf);
extern long sys_newuname(struct new_utsname __user *name);
extern long sys_sched_yield(void);
extern long sys_statx(int dfd, const char __user *path, unsigned flags,
			 unsigned mask, struct statx __user *buffer);
extern long sys_utimensat_time32(unsigned int dfd, const char __user *filename,
					 struct old_timespec32 __user *utimes,
					 int flags);
extern long sys_kill(pid_t pid, int sig);
extern long sys_tkill(pid_t pid, int sig);
extern long sys_tgkill(pid_t tgid, pid_t pid, int sig);
extern long sys_sigaltstack(const struct sigaltstack __user *uss,
				struct sigaltstack __user *uoss);
extern long sys_socket(int, int, int);
extern long sys_bind(int, struct sockaddr __user *, int);
extern long sys_listen(int, int);
extern long sys_accept(int, struct sockaddr __user *, int __user *);
extern long sys_connect(int, struct sockaddr __user *, int);
extern long sys_getsockname(int, struct sockaddr __user *, int __user *);
extern long sys_getpeername(int, struct sockaddr __user *, int __user *);
extern long sys_sendto(int, void __user *, size_t, unsigned,
			struct sockaddr __user *, int);
extern long sys_recvfrom(int, void __user *, size_t, unsigned,
			struct sockaddr __user *, int __user *);
extern long sys_setsockopt(int, int, int, char __user *, int);
extern long sys_getsockopt(int, int, int, char __user *, int __user *);
extern long sys_shutdown(int, int);
extern long sys_sendmsg(int, struct user_msghdr __user *, unsigned);
extern long sys_recvmsg(int, struct user_msghdr __user *, unsigned);
extern long sys_pselect6_time32(int n, fd_set __user *inp,
				fd_set __user *outp, fd_set __user *exp,
				struct old_timespec32 __user * tsp,
				void __user *sig);

asmlinkage long myemulator2_syscall(unsigned long nr,
		unsigned long a0, unsigned long a1, unsigned long a2,
		unsigned long a3, unsigned long a4, unsigned long a5)
{
	switch (nr) {
	case MYEMU2_NR_PTRACE:
		return sys_ptrace((long)a0, (long)a1, a2, a3);
	case MYEMU2_NR_MOUNT:
		return sys_mount((char __user *)a0, (char __user *)a1,
				 (char __user *)a2, a3, (void __user *)a4);
	case MYEMU2_NR_UMOUNT2:
		return sys_umount((char __user *)a0, (int)a1);
	case MYEMU2_NR_SOCKET:
		return sys_socket((int)a0, (int)a1, (int)a2);
	case MYEMU2_NR_BIND:
		return sys_bind((int)a0, (struct sockaddr __user *)a1, (int)a2);
	case MYEMU2_NR_LISTEN:
		return sys_listen((int)a0, (int)a1);
	case MYEMU2_NR_ACCEPT:
		return sys_accept((int)a0, (struct sockaddr __user *)a1,
				  (int __user *)a2);
	case MYEMU2_NR_CONNECT:
		return sys_connect((int)a0, (struct sockaddr __user *)a1, (int)a2);
	case MYEMU2_NR_GETSOCKNAME:
		return sys_getsockname((int)a0, (struct sockaddr __user *)a1,
					       (int __user *)a2);
	case MYEMU2_NR_GETPEERNAME:
		return sys_getpeername((int)a0, (struct sockaddr __user *)a1,
					      (int __user *)a2);
	case MYEMU2_NR_SENDTO:
		return sys_sendto((int)a0, (void __user *)a1, a2, a3,
				  (struct sockaddr __user *)a4, (int)a5);
	case MYEMU2_NR_RECVFROM:
		return sys_recvfrom((int)a0, (void __user *)a1, a2, a3,
				    (struct sockaddr __user *)a4, (int __user *)a5);
	case MYEMU2_NR_SETSOCKOPT:
		return sys_setsockopt((int)a0, (int)a1, (int)a2,
				      (char __user *)a3, (int)a4);
	case MYEMU2_NR_GETSOCKOPT:
		return sys_getsockopt((int)a0, (int)a1, (int)a2,
				      (char __user *)a3, (int __user *)a4);
	case MYEMU2_NR_SHUTDOWN:
		return sys_shutdown((int)a0, (int)a1);
	case MYEMU2_NR_SENDMSG:
		return sys_sendmsg((int)a0, (struct user_msghdr __user *)a1,
				   (unsigned)a2);
	case MYEMU2_NR_RECVMSG:
		return sys_recvmsg((int)a0, (struct user_msghdr __user *)a1,
				   (unsigned)a2);
	case MYEMU2_NR_READ:
		if (current->pid >= 20)
			pr_emerg("MYEMU_TTYTRACE read-enter pid=%d fd=%lu count=%lu\n",
				 current->pid, a0, a2);
		if (a0 == 0) {
			struct file *file = fget((unsigned int)a0);
			ssize_t ret;
			if (file) {
				fput(file);
				if (current->pid >= 20)
					pr_emerg("MYEMU_TTYTRACE read-enter pid=%d fd=0 count=%lu\n",
						 current->pid, a2);
				ret = ksys_read((unsigned int)a0, (char __user *)a1,
						(size_t)a2);
				if (current->pid >= 20)
					pr_emerg("MYEMU_TTYTRACE read-exit pid=%d ret=%ld\n",
						 current->pid, ret);
				return ret;
			}
			ret = myemulator2_console_read_user((char __user *)a1,
							      (size_t)a2);
			if (current->pid >= 20)
				pr_emerg("MYEMU_TTYTRACE console-read-exit pid=%d ret=%ld\n",
					 current->pid, ret);
			return ret;
		}
		{
			ssize_t ret = ksys_read((unsigned int)a0, (char __user *)a1,
						(size_t)a2);
			if (current->pid >= 20)
				pr_emerg("MYEMU_TTYTRACE read-exit pid=%d ret=%ld\n",
					 current->pid, ret);
			return ret;
		}
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
	case MYEMU2_NR_WRITEV:
		/* PID 1 starts before the normal console descriptors are inherited.
		 * Match the write(2) console fallback so libc's stdio writev path is
		 * usable during early userspace startup as well. */
		if (a0 == 1 || a0 == 2) {
			struct file *file = fget((unsigned int)a0);
			if (!file) {
				struct iovec iov;
				ssize_t total = 0;
				unsigned long i;
				if (a2 > UIO_MAXIOV)
					return -EINVAL;
				for (i = 0; i < a2; i++) {
					if (copy_from_user(&iov,
							(const struct iovec __user *)a1 + i,
							sizeof(iov)))
						return -EFAULT;
					if (iov.iov_len > SSIZE_MAX - total)
						return -EINVAL;
					if (iov.iov_len &&
						myemulator2_console_write_user(
							(const char __user *)iov.iov_base,
							iov.iov_len) < 0)
						return -EFAULT;
					total += iov.iov_len;
				}
				return total;
			}
			fput(file);
		}
		return sys_writev(a0, (const struct iovec __user *)a1, a2);
	case 65: /* __NR_readv */
		if (current->pid >= 20)
			pr_emerg("MYEMU_TTYTRACE readv-enter pid=%d fd=%lu count=%lu\n",
				 current->pid, a0, a2);
		{
			long ret = sys_readv(a0, (const struct iovec __user *)a1, a2);
			if (current->pid >= 20)
				pr_emerg("MYEMU_TTYTRACE readv-exit pid=%d ret=%ld\n",
					 current->pid, ret);
			return ret;
		}
	case MYEMU2_NR_FCNTL:
		/* __NR3264_fcntl selects the 64-bit flock ABI on a 32-bit
		 * kernel.  musl's off_t is 64-bit, and SQLite relies on this
		 * for POSIX file locking. */
		return sys_fcntl64((unsigned int)a0, (unsigned int)a1, a2);
	case MYEMU2_NR_PSELECT6:
		/* Bash/readline uses pselect6 while waiting for terminal input. */
		return sys_pselect6_time32((int)a0, (fd_set __user *)a1,
			(fd_set __user *)a2, (fd_set __user *)a3,
			(struct old_timespec32 __user *)a4,
			(void __user *)a5);
	case MYEMU2_NR_OPENAT:
		return do_sys_open((int)a0, (const char __user *)a1,
				   (int)a2, (umode_t)a3);
	case MYEMU2_NR_FCHMOD:
		return sys_fchmod((unsigned int)a0, (umode_t)a1);
	case MYEMU2_NR_FCHMODAT:
		return sys_fchmodat((int)a0, (const char __user *)a1,
				    (umode_t)a2);
	case MYEMU2_NR_FACCESSAT:
		return sys_faccessat((int)a0, (const char __user *)a1, (int)a2);
	case MYEMU2_NR_FACCESSAT2:
		return sys_faccessat2((int)a0, (const char __user *)a1,
				      (int)a2, (int)a3);
	case MYEMU2_NR_CLOSE:
		return sys_close((unsigned int)a0);
	case MYEMU2_NR_FSYNC:
		return sys_fsync((unsigned int)a0);
	case 83: /* __NR_fdatasync */
		return sys_fdatasync((unsigned int)a0);
	case 46: /* __NR3264_ftruncate */
		return sys_ftruncate64((unsigned int)a0, (loff_t)a1);
	case 88: /* __NR_utimensat */
		return sys_utimensat_time32((unsigned int)a0,
					    (const char __user *)a1,
					    (struct old_timespec32 __user *)a2,
					    (int)a3);
	case MYEMU2_NR_SYNC:
		ksys_sync();
		return 0;
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
	case MYEMU2_NR_MPROTECT:
		return sys_mprotect(a0, a1, a2);
	case MYEMU2_NR_IOCTL:
		/* A serial console has no window-size negotiation with the host
		 * terminal.  Supply the documented bootstrap geometry so programs
		 * such as Kilo do not fall back to an unanswered ESC[6n query. */
		if (a1 == TIOCGWINSZ) {
			struct winsize ws = {
				.ws_row = 24,
				.ws_col = 80,
				.ws_xpixel = 0,
				.ws_ypixel = 0,
			};
			return copy_to_user((void __user *)a2, &ws, sizeof(ws))
				? -EFAULT : 0;
		}
		if (current->pid >= 20)
			pr_emerg("MYEMU_TTYTRACE ioctl pid=%d fd=%lu cmd=%08lx\n",
				 current->pid, a0, a1);
		{
			long ret = sys_ioctl((unsigned int)a0, (unsigned int)a1, a2);
			if (current->pid >= 20)
				pr_emerg("MYEMU_TTYTRACE ioctl-result pid=%d ret=%ld\n",
					 current->pid, ret);
			return ret;
		}
	case MYEMU2_NR_PPOLL:
		/* asm-generic assigns ppoll to 73 and uses the time32 ABI on
		 * 32-bit architectures.  musl's poll() wrapper calls this syscall;
		 * leaving it out makes interactive programs report ENOSYS while
		 * waiting for terminal input. */
		if (current->pid >= 20)
			pr_emerg("MYEMU_TTYTRACE ppoll-enter pid=%d nfds=%lu\n",
				 current->pid, a1);
		{
			long ret = sys_ppoll_time32((struct pollfd __user *)a0,
				(unsigned int)a1,
				(struct old_timespec32 __user *)a2,
				(const sigset_t __user *)a3, a4);
			if (current->pid >= 20)
				pr_emerg("MYEMU_TTYTRACE ppoll-exit pid=%d ret=%ld\n",
					 current->pid, ret);
			return ret;
		}
	case MYEMU2_NR_EXIT:
	case MYEMU2_NR_EXIT_GROUP:
		if (current->pid >= 20)
			pr_emerg("MYEMU_TTYTRACE exit pid=%d status=%ld\n",
				 current->pid, a0);
		do_group_exit((int)a0);
		return 0;
	case MYEMU2_NR_GETPID:
		return sys_getpid();
	case MYEMU2_NR_TIMES:
		return sys_times((struct tms __user *)a0);
	case MYEMU2_NR_GETRUSAGE:
		return sys_getrusage((int)a0, (struct rusage __user *)a1);
	case MYEMU2_NR_GETPPID:
		return sys_getppid();
	case MYEMU2_NR_GETUID:
		return sys_getuid();
	case MYEMU2_NR_GETEUID:
		return sys_geteuid();
	case MYEMU2_NR_GETGID:
		return sys_getgid();
	case MYEMU2_NR_GETEGID:
		return sys_getegid();
	case MYEMU2_NR_SETPGID:
		return sys_setpgid((pid_t)a0, (pid_t)a1);
	case MYEMU2_NR_GETPGID:
		return sys_getpgid((pid_t)a0);
	case MYEMU2_NR_GETSID:
		return sys_getsid((pid_t)a0);
	case MYEMU2_NR_SETSID:
		return sys_setsid();
	case MYEMU2_NR_SETGID:
		return sys_setgid((gid_t)a0);
	case MYEMU2_NR_SETUID:
		return sys_setuid((uid_t)a0);
	case MYEMU2_NR_SET_TID_ADDRESS:
		return sys_set_tid_address((int __user *)a0);
	case MYEMU2_NR_GETTID:
		return sys_gettid();
	case MYEMU2_NR_SET_ROBUST_LIST:
		return sys_set_robust_list((struct robust_list_head __user *)a0, a1);
	case MYEMU2_NR_RT_SIGACTION:
		return sys_rt_sigaction((int)a0,
			(const struct sigaction __user *)a1,
			(struct sigaction __user *)a2, a3);
	case MYEMU2_NR_RT_SIGPROCMASK:
		return sys_rt_sigprocmask((int)a0, (sigset_t __user *)a1,
			(sigset_t __user *)a2, a3);
	case MYEMU2_NR_KILL:
		if (a1 == SIGSEGV || a1 == SIGBUS || a1 == SIGABRT)
			pr_warn("MYEMU_SIGNAL_SYSCALL pid=%d nr=kill target=%ld sig=%ld\n",
				current->pid, a0, a1);
		return sys_kill((pid_t)a0, (int)a1);
	case MYEMU2_NR_TKILL:
		if (a1 == SIGSEGV || a1 == SIGBUS || a1 == SIGABRT)
			pr_warn("MYEMU_SIGNAL_SYSCALL pid=%d nr=tkill target=%ld sig=%ld\n",
				current->pid, a0, a1);
		return sys_tkill((pid_t)a0, (int)a1);
	case MYEMU2_NR_TGKILL:
		if (a2 == SIGSEGV || a2 == SIGBUS || a2 == SIGABRT)
			pr_warn("MYEMU_SIGNAL_SYSCALL pid=%d nr=tgkill target=%ld sig=%ld\n",
				current->pid, a1, a2);
		return sys_tgkill((pid_t)a0, (pid_t)a1, (int)a2);
	case MYEMU2_NR_SIGALTSTACK:
		return sys_sigaltstack((const struct sigaltstack __user *)a0,
			(struct sigaltstack __user *)a1);
	case MYEMU2_NR_RT_SIGSUSPEND:
		return sys_rt_sigsuspend((sigset_t __user *)a0, (size_t)a1);
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
	case MYEMU2_NR_UMASK:
		return sys_umask((int)a0);
	case 50: /* __NR_fchdir */
		return sys_fchdir((unsigned int)a0);
	case 21: /* __NR_access */
		return sys_access((const char __user *)a0, (int)a1);
	case 78: /* __NR_readlinkat */
		return sys_readlinkat((int)a0, (const char __user *)a1,
				      (char __user *)a2, (int)a3);
	case 278: /* __NR_getrandom */
		return sys_getrandom((char __user *)a0, (size_t)a1, (unsigned int)a2);
	case 261: /* __NR_prlimit64 */
		return sys_prlimit64((pid_t)a0, (unsigned int)a1,
				     (const struct rlimit64 __user *)a2,
				     (struct rlimit64 __user *)a3);
	case MYEMU2_NR_LSEEK:
		/* musl exposes a 64-bit off_t and, on 32-bit targets, maps
		 * SYS__llseek to this asm-generic __NR3264_lseek slot.  The
		 * five arguments are fd, offset high/low, result pointer and
		 * whence.  Treating this as the three-argument sys_lseek ABI
		 * silently folded the result-pointer/whence pair into the
		 * offset and made ordinary file readers fail with EINVAL. */
		return sys_llseek((unsigned int)a0, a1, a2,
				  (loff_t __user *)a3, (unsigned int)a4);
	case MYEMU2_NR_FSTATAT:
		return sys_newfstatat((int)a0, (const char __user *)a1,
				      (struct stat __user *)a2, (int)a3);
	case MYEMU2_NR_FSTAT:
		return sys_newfstat((unsigned int)a0, (struct stat __user *)a1);
	case MYEMU2_NR_UNAME:
		return sys_newuname((struct new_utsname __user *)a0);
	case MYEMU2_NR_SCHED_YIELD:
		return sys_sched_yield();
	case MYEMU2_NR_STATX:
		return sys_statx((int)a0, (const char __user *)a1,
				 (unsigned)a2, (unsigned)a3,
				 (struct statx __user *)a4);
	case MYEMU2_NR_FUTEX:
		return sys_futex((u32 __user *)a0, (int)a1, (u32)a2,
			(const struct __kernel_timespec __user *)a3,
			(u32 __user *)a4, (u32)a5);
	default:
		return -ENOSYS;
	}
}
