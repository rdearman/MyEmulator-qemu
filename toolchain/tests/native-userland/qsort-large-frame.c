#include <stdlib.h>
static int compare(const void *a, const void *b) {
    volatile char scratch[4096];
    scratch[0] = 1;
    return *(const int *)a - *(const int *)b;
}
int main(void) {
    int values[] = {3, 1, 2};
    qsort(values, 3, sizeof(values[0]), compare);
    return values[0] == 1 && values[1] == 2 && values[2] == 3 ? 0 : 1;
}
