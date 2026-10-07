/* REM32 regression: #if expressions must not truncate UINT64_MAX to long. */
#include <stdint.h>

#if UINTPTR_MAX == UINT64_MAX
#error ILP32 preprocessor selected the LP64 inttypes branch
#endif

int main(void) { return 0; }
