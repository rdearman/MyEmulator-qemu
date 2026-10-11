#include <errno.h>
#include <stdio.h>
#include <sys/mman.h>

int main(void)
{
    void *mem;
    puts("MMAP_ERROR_BEFORE");
    errno = 0;
    mem = mmap(0, 0, PROT_READ | PROT_WRITE,
               MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    printf("MMAP_ERROR_RESULT failed=%d errno=%d\n", mem == MAP_FAILED,
           errno);
    if (mem != MAP_FAILED)
        munmap(mem, 0);
    puts("MMAP_ERROR_PASS");
    return mem == MAP_FAILED ? 0 : 1;
}
