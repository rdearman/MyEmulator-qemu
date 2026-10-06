#include <float.h>
int main(void) {
    volatile long double one = 1.0L;
    return LDBL_MANT_DIG == 53 && one + 0x1p-63L > one ? 1 : 0;
}
