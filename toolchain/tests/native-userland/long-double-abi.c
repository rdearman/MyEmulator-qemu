#include <float.h>
int main(void) {
    if (LDBL_MANT_DIG == DBL_MANT_DIG && LDBL_MAX_EXP == DBL_MAX_EXP)
        return sizeof(long double) == sizeof(double) ? 0 : 1;
    return 0;
}
