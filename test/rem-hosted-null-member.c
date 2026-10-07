/* Regression: the REM32 native preprocessor must preserve include guards and
 * NULL expansion while compiling a pointer-member store in the hosted
 * chibicc header environment.  This was the smallest Level 7 reproducer. */
#include "../chibicc.h"

void rem_hosted_null_member(StringArray *arr) {
  arr->data[0] = NULL;
}

int main(void) { return 0; }
