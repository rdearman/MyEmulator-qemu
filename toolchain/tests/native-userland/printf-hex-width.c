#include <stdio.h>
#include <stdlib.h>
#include <string.h>
int main(void) {
    char text[80];
    snprintf(text, sizeof(text), "%x %llx",
             0x123U, 0x1122334455667788ULL);
    puts(text);
    if (strcmp(text, "123 1122334455667788")) return 1;
    snprintf(text, sizeof(text), "%p", (void *)0x1234);
    puts(text);
    if (text[0] != '0' || text[1] != 'x') return 2;
    return strtoul(text + 2, 0, 16) != 0x1234;
}
