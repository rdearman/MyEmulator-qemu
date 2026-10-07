#include <stdarg.h>

/* A variadic callee homes r1-r4 in the caller's outgoing area even when the
   call has no stack-passed arguments.  This is the smallest regression for
   calls such as fatal("open %s:", path) and printf("%d", value). */
static int
take_one(const char *tag, ...)
{
	va_list ap;
	va_start(ap, tag);
	int value = va_arg(ap, int);
	va_end(ap);
	return value;
}

int
main(void)
{
	return take_one("value", 42) == 42 ? 0 : 1;
}
