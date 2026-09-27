#include <limits.h>
#include <stdio.h>
#include <stdlib.h>

static int
probe(const char *name)
{
    char resolved[PATH_MAX];
    return realpath(name, resolved) != NULL;
}

int
main(void)
{
    int ok = probe("/root/hello.o");
    puts(ok ? "REALPATH_OK" : "REALPATH_MISSING");
    return ok ? 0 : 1;
}
