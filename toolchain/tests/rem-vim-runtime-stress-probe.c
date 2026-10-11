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

#define FILE_PATH "/bin/busybox"
#define READ_CAP (16U * 1024U * 1024U)
#define FILE_MAP_LEN (64U * 1024U)

static void say(const char *s)
{
	size_t len = 0;
	while (s[len])
		len++;
	write(STDOUT_FILENO, s, len);
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
	say("heap: malloc 16 MiB");
	p = malloc(READ_CAP);
	if (!p)
		return fail("FAIL heap malloc");
	say("heap: malloc returned");
	p[0] = 0x5a;
	p[4096] = 0xa5;
	p[8U * 1024U * 1024U] = 0x39;
	p[READ_CAP - 1U] = 0xc7;
	if (p[0] != 0x5a || p[4096] != 0xa5 ||
		p[8U * 1024U * 1024U] != 0x39 || p[READ_CAP - 1U] != 0xc7)
		return fail("FAIL heap first byte");
	say("PASS heap large-buffer endpoints");
	free(p);
	q = malloc(4096U);
	if (!q)
		return fail("FAIL small malloc");
	q[0] = 0x17;
	q[4095] = 0x42;
	say("heap: realloc to 16 KiB");
	p = realloc(q, 16384U);
	if (!p)
		return fail("FAIL heap realloc");
	if (p[0] != 0x17 || p[4095] != 0x42)
		return fail("FAIL realloc content changed");
	p[16383] = 0x69;
	if (p[16383] != 0x69)
		return fail("FAIL realloc extended endpoint");
	free(p);
	say("PASS heap");
	return 0;
}

static int phase_strings(void)
{
	char src[128];
	char dst[128];
	char suffix[] = "_rem_runtime_probe";
	unsigned n;
	for (n = 0; n < 1; n++) {
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
		say("strings: strcpy passed");
		strcat(dst, suffix);
		say("strings: strcat passed");
		if (strcmp(dst, src) == 0)
			return fail("FAIL string operation");
		if (strlen(dst) != strlen(src) + strlen(suffix))
			return fail("FAIL string length");
		say("strings: strcmp/strlen passed");
		say("strings: before strstr");
		if (!strstr(dst, suffix))
			return fail("FAIL string lookup");
		say("strings: strstr passed");
		if (!strchr(dst, '_'))
			return fail("FAIL string character lookup");
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
	say("file: open /bin/busybox");
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
		if (ask > 65536U)
			ask = 65536U;
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
	for (i = 0; i < used; i += 257U)
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
	say("mmap: open /bin/busybox");
	fd = open(FILE_PATH, O_RDONLY);
	if (fd < 0)
		return fail("FAIL mmap open");
	filemap = mmap(0, FILE_MAP_LEN, PROT_READ, MAP_PRIVATE, fd, 0);
	if (filemap == MAP_FAILED) {
		close(fd);
		return fail("FAIL file mmap");
	}
	if (filemap[0] != 0x7f || filemap[1] != 'E' ||
		filemap[2] != 'L' || filemap[3] != 'F')
		return fail("FAIL mmap contents");
	for (i = 0; i < FILE_MAP_LEN; i += 4093U * 3U)
		value ^= filemap[i];
	if (munmap(filemap, FILE_MAP_LEN) != 0)
		return fail("FAIL file munmap");
	if (close(fd) != 0)
		return fail("FAIL mmap close");
	anonmap = mmap(0, 8U * 1024U * 1024U, PROT_READ | PROT_WRITE,
		MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
	if (anonmap == MAP_FAILED)
		return fail("FAIL anonymous mmap");
	for (i = 0; i < 8U * 1024U * 1024U; i += 4093U * 257U)
		anonmap[i] = (unsigned char)(i >> 12);
	for (i = 0; i < 8U * 1024U * 1024U; i += 4093U * 257U)
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

/* Compile once per phase with -DREM_PROBE_PHASE=1..5.  Keep main(void)
 * argument-free so this probe does not depend on argv startup handling. */
#ifndef REM_PROBE_PHASE
#define REM_PROBE_PHASE 0
#endif

int main(void)
{
	int rc = 0;
#if REM_PROBE_PHASE == 0 || REM_PROBE_PHASE == 1
	rc |= phase_heap();
#endif
#if REM_PROBE_PHASE == 0 || REM_PROBE_PHASE == 2
	rc |= phase_strings();
#endif
#if REM_PROBE_PHASE == 0 || REM_PROBE_PHASE == 3
	rc |= phase_file();
#endif
#if REM_PROBE_PHASE == 0 || REM_PROBE_PHASE == 4
	rc |= phase_mmap();
#endif
#if REM_PROBE_PHASE == 0 || REM_PROBE_PHASE == 5
	rc |= phase_maps();
#endif
	if (rc)
		return 1;
	say("ALL REQUESTED PHASES PASS");
	return 0;
}
