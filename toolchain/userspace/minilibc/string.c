#include <myemu/string.h>

void *memcpy(void *dst, const void *src, size_t n)
{
	unsigned char *d = dst;
	const unsigned char *s = src;
	while (n--) *d++ = *s++;
	return dst;
}

void *memmove(void *dst, const void *src, size_t n)
{
	unsigned char *d = dst;
	const unsigned char *s = src;
	if (d < s) return memcpy(dst, src, n);
	while (n) { --n; d[n] = s[n]; }
	return dst;
}

void *memset(void *dst, int value, size_t n)
{
	unsigned char *d = dst;
	while (n--) *d++ = (unsigned char)value;
	return dst;
}

int memcmp(const void *a, const void *b, size_t n)
{
	const unsigned char *x = a, *y = b;
	while (n--)
		if (*x != *y) return *x < *y ? -1 : 1;
		else { x++; y++; }
	return 0;
}

size_t myemu_strlen(const char *s)
{
	const char *p = s;
	while (*p)
		p++;
	return (size_t)(p - s);
}

size_t strlen(const char *s) { return myemu_strlen(s); }

int strcmp(const char *a, const char *b)
{
	while (*a && *a == *b) { a++; b++; }
	return (unsigned char)*a - (unsigned char)*b;
}

int strncmp(const char *a, const char *b, size_t n)
{
	while (n && *a && *a == *b) { a++; b++; n--; }
	return n ? (unsigned char)*a - (unsigned char)*b : 0;
}

char *strcpy(char *dst, const char *src)
{
	char *out = dst;
	while ((*dst++ = *src++)) { }
	return out;
}

char *strncpy(char *dst, const char *src, size_t n)
{
	char *out = dst;
	while (n && (*dst++ = *src++)) n--;
	while (n--) *dst++ = 0;
	return out;
}

char *strchr(const char *s, int c)
{
	while (*s) { if (*s == c) return (char *)s; s++; }
	return c == 0 ? (char *)s : (char *)0;
}
