#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/utsname.h>
#include <time.h>

static int fail(const char *what)
{
	fprintf(stderr, "FAIL: %s\n", what);
	return 1;
}

int main(void)
{
	char buf[128], path[32];
	struct utsname name;
	struct stat st;
	puts("libc-start");
	char *p = malloc(32);
	if (!p) return fail("malloc");
	memset(p, 'A', 31); p[31] = 0;
	if (strlen(p) != 31 || strcmp(p, "AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA")) return fail("string");
	char *q = realloc(p, 64);
	if (!q || q[0] != 'A') return fail("realloc");
	if (calloc(4, 4) == 0) return fail("calloc");
	if (snprintf(buf, sizeof(buf), "value=%d hex=%x text=%s", -17, 0xbeefUL, "ok") < 1)
		return fail("snprintf");
	printf("libc: %s\n", buf);
	if (uname(&name) < 0 || strcmp(name.sysname, "Linux")) return fail("uname");
	int fd = open("/test.txt", O_RDONLY, 0);
	if (fd < 0 || fstat(fd, &st) < 0) return fail("open/fstat");
	long n = read(fd, buf, sizeof(buf) - 1);
	if (n <= 0) return fail("read");
	buf[n] = 0;
	printf("file: %s", buf);
	if (close(fd) < 0) return fail("close");
	if (mkdir("/work", 0755) < 0) return fail("mkdir");
	fd = open("/work/output", O_WRONLY | O_CREAT | O_TRUNC, 0644);
	if (fd < 0 || write(fd, "written\n", 8) != 8) return fail("write");
	close(fd);
	if (stat("/work/output", &st) < 0 || st.st_size != 8) return fail("stat");
	struct timespec ts = {0, 1000};
	if (nanosleep(&ts, 0) < 0) return fail("nanosleep");
	puts("LIBC REGRESSION PASS");
	return 0;
}
