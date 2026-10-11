#include <errno.h>
#include <stdio.h>
#include <sys/mman.h>

int main(void)
{
    volatile unsigned int *p;
    void *mem;

    puts("MMAP_BEFORE");
    errno = 0;
    mem = mmap(0, 4096, PROT_READ | PROT_WRITE,
               MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    if (mem == MAP_FAILED) {
        printf("MMAP_FAIL errno=%d\n", errno);
        return 1;
    }
    puts("MMAP_RETURNED");
    p = (volatile unsigned int *)mem;
    p[0] = 0x52454d35u;
    printf("MMAP_VALUE=%08x\n", p[0]);
    puts("MMAP_BEFORE_UNMAP");
    if (munmap((void *)mem, 4096) != 0) {
        printf("MUNMAP_FAIL errno=%d\n", errno);
        return 2;
    }
    puts("MMAP_PASS");
    return 0;
}
