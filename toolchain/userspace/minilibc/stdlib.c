#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <myemu/syscall.h>

int errno;
static unsigned char *heap_end;

static size_t align16(size_t n) { return (n + 15u) & ~15u; }

void *malloc(size_t size)
{
	unsigned char *old, *next;
	if (!size) size = 1;
	size = align16(size);
	if (!heap_end) {
		long base = myemu_brk((void *)0);
		if (base < 0) { errno = (int)-base; return 0; }
		heap_end = (unsigned char *)(unsigned long)base;
	}
	old = heap_end;
	next = old + size;
	if (myemu_brk(next) != (long)(unsigned long)next) {
		errno = ENOMEM;
		return 0;
	}
	heap_end = next;
	return old;
}

void *calloc(size_t count, size_t size)
{
	if (size && count > (size_t)-1 / size) { errno = ENOMEM; return 0; }
	void *p = malloc(count * size);
	return p ? memset(p, 0, count * size) : 0;
}

void *realloc(void *ptr, size_t size)
{
	void *p;
	if (!ptr) return malloc(size);
	if (!size) { free(ptr); return 0; }
	/* This bump allocator cannot shrink or recover old blocks.  Copying into
	 * a fresh block is correct and keeps the contract useful to applications. */
	p = malloc(size);
	if (p) memcpy(p, ptr, size);
	return p;
}

void free(void *ptr) { (void)ptr; }

void abort(void) { myemu_exit(134); for (;;) { } }
void exit(int status) { myemu_exit(status); for (;;) { } }

long strtol(const char *s, char **end, int base)
{
	long value = 0, sign = 1;
	if (base == 0) base = (s[0] == '0' && (s[1] == 'x' || s[1] == 'X')) ? 16 : 10;
	if (*s == '-') { sign = -1; s++; }
	while (*s) {
		int digit = *s >= '0' && *s <= '9' ? *s - '0' :
			(*s >= 'a' && *s <= 'z' ? *s - 'a' + 10 : *s - 'A' + 10);
		if (digit < 0 || digit >= base) break;
		value = value * base + digit;
		s++;
	}
	if (end) *end = (char *)s;
	return value * sign;
}
