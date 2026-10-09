#include <unistd.h>

int main(void)
{
    static const char sequence[] = "\033[H\033[2J";
    return write(STDOUT_FILENO, sequence, sizeof(sequence) - 1) < 0;
}
