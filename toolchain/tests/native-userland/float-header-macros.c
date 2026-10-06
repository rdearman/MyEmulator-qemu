#include <float.h>

#if !defined(FLT_DECIMAL_DIG) || !defined(DBL_DECIMAL_DIG) || !defined(LDBL_DECIMAL_DIG)
#error "float.h lacks mandatory C11 decimal round-trip macros"
#endif
#if !defined(FLT_HAS_SUBNORM) || !defined(DBL_HAS_SUBNORM) || !defined(LDBL_HAS_SUBNORM)
#error "float.h lacks mandatory C11 subnormal macros"
#endif

_Static_assert(FLT_DECIMAL_DIG >= FLT_DIG, "float round-trip precision");
_Static_assert(DBL_DECIMAL_DIG >= DBL_DIG, "double round-trip precision");
_Static_assert(LDBL_DECIMAL_DIG >= LDBL_DIG, "long double round-trip precision");
_Static_assert(DECIMAL_DIG >= LDBL_DECIMAL_DIG, "combined round-trip precision");

int main(void) { return 0; }
