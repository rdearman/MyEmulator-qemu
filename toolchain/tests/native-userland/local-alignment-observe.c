#include <stdio.h>
int main(void) {
    _Alignas(16) char values[16];
    volatile char pad;
    pad = 1;
    printf("values=%p alignment=%u pad=%u\n",
           (void *)values, (unsigned long)(void *)values & 15, pad);
    return 0;
}
