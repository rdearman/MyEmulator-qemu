#include <stdio.h>
#include <string.h>
int main(void) {
    char output[32];
    if (snprintf(output, sizeof(output), "%.2f", 1.5) != 4) return 1;
    return strcmp(output, "1.50") ? 2 : 0;
}
