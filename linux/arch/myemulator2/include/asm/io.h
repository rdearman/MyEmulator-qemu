#ifndef _ASM_MYEMULATOR2_IO_H
#define _ASM_MYEMULATOR2_IO_H
#include <asm-generic/io.h>
#define __io(p) ((void __iomem *)(unsigned long)(p))

/* MyEmulator2 currently uses a flat physical kernel mapping.  Keep the
 * Linux I/O mapping API explicit so generic kernel code can build while the
 * eventual platform MMIO mapping is supplied by the architecture VM setup. */
static inline void __iomem *ioremap(phys_addr_t offset, size_t size)
{
	(void)size;
	return __io(offset);
}

static inline void iounmap(volatile void __iomem *addr)
{
	(void)addr;
}
#endif
