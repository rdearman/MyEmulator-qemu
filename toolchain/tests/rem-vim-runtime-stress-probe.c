/*
 * Small static REM probe for isolating Vim's pre-screen user fault.
 * Run phases independently so GCC and chibicc output can be compared against
 * the same musl archive and the same kernel/rootfs tuple.
 */
#define _GNU_SOURCE
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <unistd.h>

#define FILE_PATH "/usr/bin/vim"
#define READ_CAP (16U * 1024U * 1024U)

static void say(const char *s)
{
	write(STDOUT_FILENO, s, strlen(s));
	write(STDOUT_FILENO, "\n", 1);
}

static int fail(const char *s)
{
	say(s);
	return 1;
}

static int phase_heap(void)
{
	unsigned char *p;
	unsigned char *q;
	size_t i;
	unsigned round;
	say("heap: malloc 4 MiB");
	p = malloc(4U * 1024U * 1024U);
	if (!p)
		return fail("FAIL heap malloc");
	for (i = 0; i < 4U * 1024U * 1024U; i++)
		p[i] = (unsigned char)((i * 37U + 11U) & 255U);
	say("heap: realloc to 16 MiB");
	q = realloc(p, READ_CAP);
	if (!q)
		return fail("FAIL heap realloc");
	p = q;
	for (i = 0; i < 4U * 1024U * 1024U; i++) {
		if (p[i] != (unsigned char)((i * 37U + 11U) & 255U))
			return fail("FAIL realloc content changed");
	}
	for (i = 4U * 1024U * 1024U; i < READ_CAP; i++)
		p[i] = (unsigned char)((i * 13U + 7U) & 255U);
	for (i = 0; i < READ_CAP; i += 4093U) {
		unsigned char want = (i < 4U * 1024U * 1024U)
			? (unsigned char)((i * 37U + 11U) & 255U)
			: (unsigned char)((i * 13U + 7U) & 255U);
		if (p[i] != want)
			return fail("FAIL large heap buffer changed");
	}
	free(p);
	for (round = 1; round <= 512; round++) {
		size_t size = (round * 131U) % 65536U + 1U;
		p = malloc(size);
		if (!p)
			return fail("FAIL repeated malloc");
		p[0] = (unsigned char)round;
		p[size - 1] = (unsigned char)(round ^ 0xa5U);
		q = realloc(p, size + 97U);
		if (!q)
			return fail("FAIL repeated realloc");
		if (q[0] != (unsigned char)round ||
			q[size - 1] != (unsigned char)(round ^ 0xa5U))
			return fail("FAIL repeated realloc content");
		free(q);
	}
	say("PASS heap");
	return 0;
}

static int phase_strings(void)
{
	char src[128];
	char dst[128];
	char suffix[] = "_rem_runtime_probe";
	unsigned n;
	for (n = 0; n < 5000; n++) {
		unsigned x = n;
		unsigned pos = 0;
		char digits[16];
		unsigned nd = 0;
		src[pos++] = 'n';
		do {
			digits[nd++] = (char)('0' + (x % 10U));
			x /= 10U;
		} while (x);
		while (nd)
			src[pos++] = digits[--nd];
		src[pos] = 0;
		strcpy(dst, src);
		strcat(dst, suffix);
		if (strcmp(dst, src) == 0 || strlen(dst) != strlen(src) + strlen(suffix))
			return fail("FAIL string operation");
		if (!strstr(dst, suffix) || !strchr(dst, '_'))
			return fail("FAIL string lookup");
	}
	say("PASS strings");
	return 0;
}

static int phase_file(void)
{
	int fd;
	ssize_t nread;
	size_t used = 0;
	unsigned char *buf;
	unsigned long checksum = 0;
	size_t i;
	say("file: open /usr/bin/vim");
	fd = open(FILE_PATH, O_RDONLY);
	if (fd < 0)
		return fail("FAIL open");
	buf = malloc(READ_CAP);
	if (!buf) {
		close(fd);
		return fail("FAIL file malloc");
	}
	say("file: read to EOF");
	while (used < READ_CAP) {
		size_t ask = READ_CAP - used;
		if (ask > 4096U)
			ask = 4096U;
		nread = read(fd, buf + used, ask);
		if (nread < 0) {
			free(buf);
			close(fd);
			return fail("FAIL read");
		}
		if (nread == 0)
			break;
		used += (size_t)nread;
	}
	if (used == READ_CAP) {
		free(buf);
		close(fd);
		return fail("FAIL input exceeded read cap");
	}
	if (used < 4 || buf[0] != 0x7f || buf[1] != 'E' ||
		buf[2] != 'L' || buf[3] != 'F') {
		free(buf);
		close(fd);
		return fail("FAIL ELF contents");
	}
	for (i = 0; i < used; i++)
		checksum = (checksum * 33UL) ^ buf[i];
	if (close(fd) != 0) {
		free(buf);
		return fail("FAIL close");
	}
	free(buf);
	if (!checksum)
		return fail("FAIL file checksum");
	say("PASS file read/close/checksum");
	return 0;
}

static int phase_mmap(void)
{
	int fd;
	unsigned char *filemap;
	unsigned char *anonmap;
	volatile unsigned char value = 0;
	size_t i;
	say("mmap: open Vim");
	fd = open(FILE_PATH, O_RDONLY);
	if (fd < 0)
		return fail("FAIL mmap open");
	filemap = mmap(0, 4U * 1024U * 1024U, PROT_READ, MAP_PRIVATE, fd, 0);
	if (filemap == MAP_FAILED) {
		close(fd);
		return fail("FAIL file mmap");
	}
	if (filemap[0] != 0x7f || filemap[1] != 'E' ||
		filemap[2] != 'L' || filemap[3] != 'F')
		return fail("FAIL mmap contents");
	for (i = 0; i < 4U * 1024U * 1024U; i += 4093U)
		value ^= filemap[i];
	if (munmap(filemap, 4U * 1024U * 1024U) != 0)
		return fail("FAIL file munmap");
	if (close(fd) != 0)
		return fail("FAIL mmap close");
	anonmap = mmap(0, 8U * 1024U * 1024U, PROT_READ | PROT_WRITE,
		MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
	if (anonmap == MAP_FAILED)
		return fail("FAIL anonymous mmap");
	for (i = 0; i < 8U * 1024U * 1024U; i += 4096U)
		anonmap[i] = (unsigned char)(i >> 12);
	for (i = 0; i < 8U * 1024U * 1024U; i += 4096U)
		value ^= anonmap[i];
	if (munmap(anonmap, 8U * 1024U * 1024U) != 0)
		return fail("FAIL anonymous munmap");
	(void)value;
	say("PASS file/anonymous mmap");
	return 0;
}

static int phase_maps(void)
{
	char buf[512];
	ssize_t nread;
	int fd = open("/proc/self/maps", O_RDONLY);
	if (fd < 0)
		return fail("FAIL open /proc/self/maps");
	say("maps: /proc/self/maps follows");
	while ((nread = read(fd, buf, sizeof(buf))) > 0)
		write(STDOUT_FILENO, buf, (size_t)nread);
	close(fd);
	if (nread < 0)
		return fail("FAIL read /proc/self/maps");
	say("PASS maps");
	return 0;
}

int main(int argc, char **argv)
{
	int rc = 0;
	if (argc < 2 || strcmp(argv[1], "all") == 0 || strcmp(argv[1], "heap") == 0)
		rc |= phase_heap();
	if (argc < 2 || strcmp(argv[1], "all") == 0 || strcmp(argv[1], "strings") == 0)
		rc |= phase_strings();
	if (argc < 2 || strcmp(argv[1], "all") == 0 || strcmp(argv[1], "file") == 0)
		rc |= phase_file();
	if (argc < 2 || strcmp(argv[1], "all") == 0 || strcmp(argv[1], "mmap") == 0)
		rc |= phase_mmap();
	if (argc >= 2 && strcmp(argv[1], "maps") == 0)
		rc |= phase_maps();
	if (rc)
		return 1;
	say("ALL REQUESTED PHASES PASS");
	return 0;
}
