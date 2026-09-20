#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
#include <myemu/syscall.h>

struct MYEMU_FILE { int fd; int owned; };
FILE myemu_stdin = {0, 0};
FILE myemu_stdout = {1, 0};
FILE myemu_stderr = {2, 0};

static int emit(FILE *stream, const char *s, size_t n)
{
	long ret = write(stream->fd, s, n);
	return ret < 0 ? -1 : (int)ret;
}

int fputc(int c, FILE *stream) { unsigned char ch = (unsigned char)c; return emit(stream, (char *)&ch, 1) < 0 ? EOF : ch; }
int putchar(int c) { return fputc(c, stdout); }
int fputs(const char *s, FILE *stream) { return emit(stream, s, strlen(s)) < 0 ? EOF : 0; }
int puts(const char *s) { return fputs(s, stdout) == EOF || fputc('\n', stdout) == EOF ? EOF : 0; }
int fflush(FILE *stream) { (void)stream; return 0; }
size_t fread(void *p, size_t size, size_t count, FILE *stream) { long n = read(stream->fd, p, size * count); return n < 0 ? 0 : (size ? (size_t)n / size : 0); }
size_t fwrite(const void *p, size_t size, size_t count, FILE *stream) { long n = write(stream->fd, p, size * count); return n < 0 ? 0 : (size ? (size_t)n / size : 0); }

static int number(char *out, size_t cap, unsigned long value, int base, int negative)
{
	char temp[32]; size_t n = 0, i = 0;
	const char *digits = "0123456789abcdef";
	if (!value) temp[n++] = '0';
	while (value) { temp[n++] = digits[value % (unsigned)base]; value /= (unsigned)base; }
	if (negative && i < cap) out[i++] = '-';
	while (n && i < cap) out[i++] = temp[--n];
	return (int)i;
}

static int format(char *out, size_t cap, const char *fmt, va_list ap)
{
	size_t used = 0;
	#define PUT(ch) do { if (used < cap) out[used] = (ch); used++; } while (0)
	while (*fmt) {
		if (*fmt != '%') { PUT(*fmt++); continue; }
		fmt++;
		if (*fmt == '%') { PUT('%'); fmt++; continue; }
		if (*fmt == 's') { const char *s = va_arg(ap, const char *); while (*s) PUT(*s++); fmt++; continue; }
		if (*fmt == 'c') { PUT((char)va_arg(ap, int)); fmt++; continue; }
		if (*fmt == 'd' || *fmt == 'i' || *fmt == 'u' || *fmt == 'x' || *fmt == 'p') {
			unsigned long v; int neg = 0, base = *fmt == 'x' ? 16 : 10;
			if (*fmt == 'd' || *fmt == 'i') { long x = va_arg(ap, long); neg = x < 0; v = neg ? (unsigned long)(-x) : (unsigned long)x; }
			else if (*fmt == 'p') { v = (unsigned long)va_arg(ap, void *); base = 16; PUT('0'); PUT('x'); }
			else v = va_arg(ap, unsigned long);
			char tmp[40]; int n = number(tmp, sizeof(tmp), v, base, neg); for (int i = 0; i < n; i++) PUT(tmp[i]); fmt++; continue;
		}
		PUT('%');
	}
	#undef PUT
	if (cap) out[used < cap ? used : cap - 1] = 0;
	return used > 0x7fffffffU ? -1 : (int)used;
}

int snprintf(char *out, size_t size, const char *fmt, ...)
{
	va_list ap; int ret; va_start(ap, fmt); ret = format(out, size, fmt, ap); va_end(ap); return ret;
}

int fprintf(FILE *stream, const char *fmt, ...)
{
	char buf[512]; va_list ap; int n;
	va_start(ap, fmt); n = format(buf, sizeof(buf), fmt, ap); va_end(ap);
	return n < 0 || emit(stream, buf, strlen(buf)) < 0 ? -1 : n;
}

int printf(const char *fmt, ...)
{
	char buf[512]; va_list ap; int n;
	va_start(ap, fmt); n = format(buf, sizeof(buf), fmt, ap); va_end(ap);
	return n < 0 || emit(stdout, buf, strlen(buf)) < 0 ? -1 : n;
}

FILE *fopen(const char *path, const char *mode)
{
	int flags = O_RDONLY;
	if (mode[0] == 'w') flags = O_WRONLY | O_CREAT | O_TRUNC;
	else if (mode[0] == 'a') flags = O_WRONLY | O_CREAT | O_APPEND;
	else if (mode[0] == 'r' && mode[1] == '+') flags = O_RDWR;
	int fd = open(path, flags, 0666);
	if (fd < 0) return 0;
	FILE *f = malloc(sizeof(*f));
	if (!f) { close(fd); return 0; }
	f->fd = fd; f->owned = 1; return f;
}

int fclose(FILE *stream)
{
	int ret = stream->owned ? close(stream->fd) : 0;
	if (stream->owned) free(stream);
	return ret;
}
