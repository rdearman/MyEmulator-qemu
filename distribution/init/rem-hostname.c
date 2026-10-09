#include <stdio.h>
#include <string.h>
#include <unistd.h>

int main(int argc, char **argv)
{
    if (argc != 2) {
        fprintf(stderr, "usage: hostname NAME\n");
        return 2;
    }
    return sethostname(argv[1], strlen(argv[1])) ? 1 : 0;
}
