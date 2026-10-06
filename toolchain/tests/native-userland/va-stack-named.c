#include <stdarg.h>
static int value(int a, int b, int c, int d, int e, ...) {
    va_list ap;
    va_start(ap, e);
    int result = va_arg(ap, int);
    va_end(ap);
    return result;
}
int main(void) {
    return value(1, 2, 3, 4, 5, 42) == 42 ? 0 : 1;
}
