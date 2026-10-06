#include <stdarg.h>
static int value(int named, ...) {
    volatile char scratch[4096];
    scratch[0] = 1;
    va_list ap;
    va_start(ap, named);
    int result = va_arg(ap, int);
    va_end(ap);
    return result;
}
int main(void) {
    return value(0, 42) == 42 ? 0 : 1;
}
