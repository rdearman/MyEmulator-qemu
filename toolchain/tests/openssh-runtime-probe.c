#define _GNU_SOURCE
#include <errno.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/mount.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <termios.h>
#include <unistd.h>

static volatile sig_atomic_t caught_signal;

static void on_signal(int signo)
{
	(void)signo;
	caught_signal = 1;
}

static void result(const char *name, int ok, int error)
{
	printf("%s=%s errno=%d\n", name, ok ? "PASS" : "FAIL", ok ? 0 : error);
}

static int socket_roundtrip(void)
{
	int server = -1, client = -1, accepted = -1, ok = 1;
	struct sockaddr_in addr = { .sin_family = AF_INET,
		.sin_addr.s_addr = htonl(INADDR_ANY), .sin_port = 0 };
	socklen_t addrlen = sizeof(addr);
	int error = 0, flags;

	server = socket(AF_INET, SOCK_STREAM, 0);
	if (server < 0) { error = errno; result("SOCKET_CREATE", 0, error); ok = 0; goto out; }
	result("SOCKET_CREATE", 1, 0);
	if (bind(server, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
		error = errno; result("SOCKET_BIND", 0, error); ok = 0; goto out;
	}
	result("SOCKET_BIND", 1, 0);
	if (listen(server, 1) < 0) { error = errno; result("SOCKET_LISTEN", 0, error); ok = 0; goto out; }
	result("SOCKET_LISTEN", 1, 0);
	if (getsockname(server, (struct sockaddr *)&addr, &addrlen) < 0) {
		error = errno; result("SOCKET_GETSOCKNAME", 0, error); ok = 0; goto out;
	}
	result("SOCKET_GETSOCKNAME", 1, 0);
	client = socket(AF_INET, SOCK_STREAM, 0);
	if (client < 0) {
		error = errno; result("SOCKET_CONNECT", 0, error); ok = 0;
	} else {
		addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
		flags = fcntl(client, F_GETFL);
		if (flags < 0 || fcntl(client, F_SETFL, flags | O_NONBLOCK) < 0) {
			error = errno; result("SOCKET_CONNECT", 0, error); ok = 0;
		} else if (connect(client, (struct sockaddr *)&addr, sizeof(addr)) == 0) {
			result("SOCKET_CONNECT", 1, 0);
		} else if (errno == EINPROGRESS || errno == EALREADY) {
			result("SOCKET_CONNECT_INIT", 1, 0);
		} else {
			error = errno; result("SOCKET_CONNECT", 0, error); ok = 0;
		}
	}
	flags = fcntl(server, F_GETFL);
	if (flags < 0 || fcntl(server, F_SETFL, flags | O_NONBLOCK) < 0) {
		error = errno; result("SOCKET_ACCEPT", 0, error); ok = 0;
	} else {
		accepted = accept(server, NULL, NULL);
		if (accepted >= 0) {
			result("SOCKET_ACCEPT", 1, 0);
		} else if (errno == EAGAIN || errno == EWOULDBLOCK) {
			/* The syscall works; no peer completed a TCP connection. */
			result("SOCKET_ACCEPT", 1, 0);
		} else {
			error = errno; result("SOCKET_ACCEPT", 0, error); ok = 0;
		}
	}
out:
	if (accepted >= 0) close(accepted);
	if (client >= 0) close(client);
	if (server >= 0) close(server);
	result("SOCKET_API_PROBES", ok, error);
	return ok;
}

static int exec_wait(void)
{
	pid_t child = fork();
	int status = 0, error = 0;
	if (child < 0) { error = errno; result("FORK_EXEC_WAIT", 0, error); return 0; }
	if (child == 0) {
		char *const argv[] = { (char *)"busybox", (char *)"true", NULL };
		execv("/bin/busybox", argv);
		_exit(127);
	}
	if (waitpid(child, &status, 0) != child || !WIFEXITED(status) || WEXITSTATUS(status)) {
		error = errno ? errno : ECHILD;
		result("FORK_EXEC_WAIT", 0, error);
		return 0;
	}
	result("FORK_EXEC_WAIT", 1, 0);
	return 1;
}

static int signal_delivery(void)
{
	struct sigaction action = { .sa_handler = on_signal };
	pid_t child;
	int status = 0, error = 0;
	sigemptyset(&action.sa_mask);
	if (sigaction(SIGUSR1, &action, NULL) < 0) {
		error = errno; result("SIGNAL_DELIVERY", 0, error); return 0;
	}
	child = fork();
	if (child < 0) { error = errno; result("SIGNAL_DELIVERY", 0, error); return 0; }
	if (child == 0) {
		while (!caught_signal) pause();
		_exit(0);
	}
	if (kill(child, SIGUSR1) < 0 || waitpid(child, &status, 0) != child ||
	    !WIFEXITED(status) || WEXITSTATUS(status)) {
		error = errno ? errno : ECHILD;
		result("SIGNAL_DELIVERY", 0, error);
		return 0;
	}
	result("SIGNAL_DELIVERY", 1, 0);
	return 1;
}

static int private_mode(void)
{
	const char *path = "/tmp/rem-openssh-private-probe";
	struct stat st;
	mode_t old = umask(077);
	int fd = open(path, O_CREAT | O_EXCL | O_WRONLY, 0666);
	int error = 0, ok = 0;
	(void)umask(old);
	if (fd < 0) error = errno;
	else {
		if (fstat(fd, &st) == 0 && (st.st_mode & 0777) == 0600) ok = 1;
		else error = errno ? errno : EACCES;
		close(fd);
		unlink(path);
	}
	result("PRIVATE_FILE_MODE", ok, error);
	return ok;
}

static int pty_allocation(void)
{
	int master = -1, slave = -1, error = 0, number = -1, ok = 0;
	char path[64];
	struct termios term;
	if (mkdir("/dev/pts", 0755) < 0 && errno != EEXIST) {
		error = errno; goto out;
	}
	if (mount("devpts", "/dev/pts", "devpts", 0, "") < 0 && errno != EBUSY) {
		error = errno; goto out;
	}
	master = open("/dev/ptmx", O_RDWR | O_NOCTTY);
	if (master < 0) { error = errno; goto out; }
	if (grantpt(master) < 0 || unlockpt(master) < 0 ||
	    ioctl(master, TIOCGPTN, &number) < 0) { error = errno; goto out; }
	if (snprintf(path, sizeof(path), "/dev/pts/%d", number) >= (int)sizeof(path)) {
		error = ENAMETOOLONG; goto out;
	}
	slave = open(path, O_RDWR | O_NOCTTY);
	if (slave < 0) { error = errno; goto out; }
	if (tcgetattr(slave, &term) < 0) { error = errno; goto out; }
	ok = 1;
out:
	if (slave >= 0) close(slave);
	if (master >= 0) close(master);
	result("UNIX98_PTY_MASTER_SLAVE", ok, error);
	return ok;
}

int main(void)
{
	int ok = 1;
	ok &= socket_roundtrip();
	ok &= exec_wait();
	ok &= signal_delivery();
	ok &= private_mode();
	ok &= pty_allocation();
	return ok ? 0 : 1;
}
