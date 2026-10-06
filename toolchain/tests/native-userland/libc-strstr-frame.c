#include <string.h>

int main(void) {
    static const char text[] = "0123456789needle and some more text";
    return strstr(text, "needle") == text + 10 ? 0 : 1;
}
