#ifndef MYEMU_STDLIB_H
#define MYEMU_STDLIB_H
#include <stddef.h>
void *malloc(size_t size);
void *calloc(size_t count, size_t size);
void *realloc(void *ptr, size_t size);
void free(void *ptr);
void abort(void);
void exit(int status);
long strtol(const char *s, char **end, int base);
#endif
