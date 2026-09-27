/* Standalone console and UNIX98 PTY probe for a REM Linux guest. */
#define _GNU_SOURCE
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/ioctl.h>
#include <sys/mount.h>
#include <sys/syscall.h>
#include <termios.h>
#include <unistd.h>

static void report(const char *name, long rc, int error)
{
	printf("%s=%ld errno=%d\n", name, rc, error);
	fflush(stdout);
}

static void probe_tty(const char *name, int fd)
{
	struct termios term;
	struct winsize size;
	pid_t foreground;
	int rc;

	errno = 0;
	rc = isatty(fd);
	report(name, rc, errno);
	errno = 0;
	rc = tcgetattr(fd, &term);
	report("tcgetattr", rc, errno);
	errno = 0;
	memset(&size, 0, sizeof(size));
	rc = ioctl(fd, TIOCGWINSZ, &size);
	report("ioctl_TIOCGWINSZ", rc, errno);
	if (rc == 0)
		printf("winsize_rows=%u cols=%u\n", size.ws_row, size.ws_col);
	errno = 0;
	rc = ioctl(fd, TIOCGPGRP, &foreground);
	report("ioctl_TIOCGPGRP", rc, errno);
	if (rc == 0)
		printf("foreground_pgrp=%ld\n", (long)foreground);
	fflush(stdout);
}

static void probe_pty_io(int master, int slave)
{
	struct termios term;
	char input[] = "pty-input-line\n";
	char output[] = "pty-output-line\n";
	char buffer[64] = { 0 };
	ssize_t rc;
	if (tcgetattr(slave, &term) == 0) {
		term.c_lflag &= (tcflag_t)~ECHO;
		errno = 0;
		rc = tcsetattr(slave, TCSANOW, &term);
		report("pty_disable_echo", rc, errno);
	}
	errno = 0;
	rc = write(master, input, sizeof(input) - 1);
	report("pty_write_master", rc, errno);
	errno = 0;
	rc = read(slave, buffer, sizeof(buffer) - 1);
	report("pty_read_slave", rc, errno);
	if (rc >= 0) {
		buffer[rc] = 0;
		printf("pty_slave_received=%s", buffer);
		fflush(stdout);
	}
	memset(buffer, 0, sizeof(buffer));
	errno = 0;
	rc = write(slave, output, sizeof(output) - 1);
	report("pty_write_slave", rc, errno);
	errno = 0;
	rc = read(master, buffer, sizeof(buffer) - 1);
	report("pty_read_master", rc, errno);
	if (rc >= 0) {
		buffer[rc] = 0;
		printf("pty_master_received=%s", buffer);
		fflush(stdout);
	}
}

int main(void)
{
	int master, slave, rc;
	char path[128];
	const char device_path[] = "/tmp/rem-pty-node-probe";

	(void)unlink(device_path);
	errno = 0;
	rc = (int)syscall(33, AT_FDCWD, device_path,
			  S_IFCHR | 0600, (5 << 8) | 2);
	report("raw_mknodat_33_ptmx", rc, errno);

	errno = 0;
	rc = mkdir("/dev/pts", 0755);
	if (rc < 0 && errno == EEXIST)
		report("mkdir_devpts", 0, 0);
	else
		report("mkdir_devpts", rc, errno);
	errno = 0;
	rc = mount("devpts", "/dev/pts", "devpts", 0, NULL);
	report("mount_devpts", rc, errno);

	probe_tty("stdin_isatty", STDIN_FILENO);
	errno = 0;
	master = open("/dev/ptmx", O_RDWR | O_NOCTTY);
	report("open_ptmx", master, errno);
	if (master < 0)
		return 0;

	errno = 0;
	rc = grantpt(master);
	report("grantpt", rc, errno);
	errno = 0;
	rc = unlockpt(master);
	report("unlockpt", rc, errno);
	errno = 0;
	rc = ptsname_r(master, path, sizeof(path));
	report("ptsname_r", rc, rc);
	if (rc != 0) {
		close(master);
		return 0;
	}
	printf("pty_slave=%s\n", path);
	fflush(stdout);

	errno = 0;
	slave = open(path, O_RDWR | O_NOCTTY);
	report("open_pty_slave", slave, errno);
	if (slave >= 0) {
		probe_tty("pty_slave_isatty", slave);
		probe_pty_io(master, slave);
		close(slave);
	}
	close(master);
	return 0;
}
