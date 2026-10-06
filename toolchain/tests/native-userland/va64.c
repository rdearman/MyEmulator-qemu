#include <stdarg.h>
static unsigned long long value(int named, ...) {
    va_list ap;
    va_start(ap, named);
    unsigned long long result = va_arg(ap, unsigned long long);
    va_end(ap);
    return result;
}
int main(void) {
    return value(0, 0x1122334455667788ULL) == 0x1122334455667788ULL ? 0 : 1;
}
