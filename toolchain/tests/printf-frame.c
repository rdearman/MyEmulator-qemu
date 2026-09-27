#include <stdarg.h>
#include <stdio.h>

__attribute__((noinline)) static int
forward_printf(const char *format, ...)
{
    va_list ap;
    int result;
    va_start(ap, format);
    result = vprintf(format, ap);
    va_end(ap);
    return result;
}

int
main(void)
{
    return forward_printf("PRINTF_FRAME_OK %d %s\n", 42, "REM") < 0;
}
