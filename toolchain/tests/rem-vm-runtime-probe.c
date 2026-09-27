/* Standalone mmap, pthread, timer, and signal probe for a REM Linux guest. */
#define _GNU_SOURCE
#include <errno.h>
#include <fcntl.h>
#include <pthread.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/syscall.h>
#include <time.h>
#include <unistd.h>

static volatile sig_atomic_t handled_signal;

static void on_signal(int signo)
{
	handled_signal = signo;
}

static void *thread_entry(void *value)
{
	return value;
}

static void report(const char *name, long rc, int error)
{
	printf("%s=%ld errno=%d\n", name, rc, error);
	fflush(stdout);
}

static void probe_mmap(void)
{
	const size_t length = 4096;
	char path[] = "/tmp/rem-map-probe-XXXXXX";
	int fd = mkstemp(path);
	char *memory;
	char input[] = "mapped-file-sentinel";
	ssize_t n;
	if (fd < 0) {
		report("mkstemp", -1, errno);
		return;
	}
	(void)unlink(path);
	n = write(fd, input, sizeof(input));
	report("write_map_seed", n, errno);
	errno = 0;
	memory = mmap(NULL, length, PROT_READ | PROT_WRITE,
		      MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
	report("mmap_anon", (long)memory, errno);
	if (memory != MAP_FAILED) {
		memcpy(memory, input, sizeof(input));
		printf("mmap_anon_readback=%s\n", memory);
		errno = 0;
		n = mprotect(memory, length, PROT_READ);
		report("mprotect_readonly", n, errno);
		errno = 0;
		n = munmap(memory, length);
		report("munmap_anon", n, errno);
	}
	errno = 0;
	memory = mmap(NULL, length, PROT_READ, MAP_PRIVATE, fd, 0);
	report("mmap_file_readonly", (long)memory, errno);
	if (memory != MAP_FAILED) {
		printf("mmap_file_readback=%s\n", memory);
		errno = 0;
		n = msync(memory, length, MS_SYNC);
		report("msync_file", n, errno);
		errno = 0;
		n = munmap(memory, length);
		report("munmap_file_readonly", n, errno);
	}
	close(fd);
}

static void probe_signals(void)
{
	struct sigaction action;
	sigset_t blocked, previous, waitmask;
	int rc;
	memset(&action, 0, sizeof(action));
	action.sa_handler = on_signal;
	sigemptyset(&action.sa_mask);
	errno = 0;
	rc = sigaction(SIGUSR1, &action, NULL);
	report("sigaction_usr1", rc, errno);
	sigemptyset(&blocked);
	sigaddset(&blocked, SIGUSR1);
	errno = 0;
	rc = sigprocmask(SIG_BLOCK, &blocked, &previous);
	report("sigprocmask_block", rc, errno);
	errno = 0;
	rc = kill(getpid(), SIGUSR1);
	report("kill_self_usr1", rc, errno);
	waitmask = previous;
	errno = 0;
	rc = sigsuspend(&waitmask);
	report("sigsuspend_after_usr1", rc, errno);
	printf("signal_handler_value=%d\n", (int)handled_signal);
	fflush(stdout);
}

static void probe_timer(void)
{
	struct sigevent event;
	struct itimerspec value;
	sigset_t set;
	struct timespec timeout = { .tv_sec = 0, .tv_nsec = 250000000 };
	siginfo_t info;
	timer_t timer;
	int rc;
	sigemptyset(&set);
	sigaddset(&set, SIGUSR2);
	errno = 0;
	rc = sigprocmask(SIG_BLOCK, &set, NULL);
	report("sigprocmask_block_timer_signal", rc, errno);
	memset(&event, 0, sizeof(event));
	event.sigev_notify = SIGEV_SIGNAL;
	event.sigev_signo = SIGUSR2;
	errno = 0;
	rc = timer_create(CLOCK_MONOTONIC, &event, &timer);
	report("timer_create_sigev_signal", rc, errno);
	if (rc == 0) {
		memset(&value, 0, sizeof(value));
		value.it_value.tv_nsec = 50000000;
		errno = 0;
		rc = timer_settime(timer, 0, &value, NULL);
		report("timer_settime", rc, errno);
		if (rc == 0) {
			errno = 0;
			rc = sigtimedwait(&set, &info, &timeout);
			report("sigtimedwait_timer", rc, errno);
			if (rc >= 0)
				printf("timer_signal=%d code=%d\n", info.si_signo,
				       info.si_code);
		}
		errno = 0;
		rc = timer_delete(timer);
		report("timer_delete", rc, errno);
	} else {
		long raw_rc;
		int raw_error;
		long raw_settime_rc;
		int raw_settime_error;
		errno = 0;
		raw_rc = syscall(107, CLOCK_MONOTONIC, NULL, &timer);
		raw_error = errno;
		printf("raw_timer_create_107=%ld errno=%d\n", raw_rc,
		       raw_error);
		memset(&value, 0, sizeof(value));
		value.it_value.tv_nsec = 50000000;
		errno = 0;
		raw_settime_rc = syscall(409, 1, 0, &value, NULL);
		raw_settime_error = errno;
		printf("raw_timer_settime_409=%ld errno=%d\n",
		       raw_settime_rc, raw_settime_error);
	}
}

int main(void)
{
	pthread_t thread;
	void *thread_result = NULL;
	int rc;
	probe_mmap();
	errno = 0;
	rc = pthread_create(&thread, NULL, thread_entry, (void *)0x1234);
	printf("pthread_create=%d errno=%d\n", rc, errno);
	fflush(stdout);
	if (rc == 0) {
		rc = pthread_join(thread, &thread_result);
		printf("pthread_join=%d errno=%d result=%p\n", rc, errno,
		       thread_result);
		fflush(stdout);
	}
	probe_signals();
	probe_timer();
	return 0;
}
