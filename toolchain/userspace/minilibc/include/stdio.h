#ifndef MYEMU_STDIO_H
#define MYEMU_STDIO_H
#include <stddef.h>
#include <stdarg.h>
typedef struct MYEMU_FILE FILE;
#define EOF (-1)
#define stdin (&myemu_stdin)
#define stdout (&myemu_stdout)
#define stderr (&myemu_stderr)
extern FILE myemu_stdin, myemu_stdout, myemu_stderr;
int putchar(int c);
int fputc(int c, FILE *stream);
int puts(const char *s);
int fputs(const char *s, FILE *stream);
size_t fread(void *ptr, size_t size, size_t count, FILE *stream);
size_t fwrite(const void *ptr, size_t size, size_t count, FILE *stream);
int fflush(FILE *stream);
int printf(const char *format, ...);
int fprintf(FILE *stream, const char *format, ...);
int snprintf(char *out, size_t size, const char *format, ...);
FILE *fopen(const char *path, const char *mode);
int fclose(FILE *stream);
#endif
