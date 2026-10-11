#ifndef _ASM_MYEMULATOR2_IO_H
#define _ASM_MYEMULATOR2_IO_H
#include <asm-generic/io.h>
#define __io(p) ((void __iomem *)(unsigned long)(p))

#define MYEMU2_UART_VIRT  0x7fc00000UL
#define MYEMU2_BLOCK_VIRT 0x7fd00000UL
#define MYEMU2_VIRTIO_NET_VIRT 0x7fe00000UL
#define MYEMU2_POWER_VIRT 0x7ff00000UL

/* The machine's MMIO bus is physical. These fixed virtual aliases are
 * installed alongside the RAM direct map during paging_init(). */
static inline void __iomem *ioremap(phys_addr_t offset, size_t size)
{
	(void)size;
	if (offset == 0xf0000000UL)
		return __io(MYEMU2_UART_VIRT);
	if (offset == 0xf0100000UL)
		return __io(MYEMU2_BLOCK_VIRT);
	if (offset == 0xf0200000UL)
		return __io(MYEMU2_VIRTIO_NET_VIRT);
	if (offset == 0xf0300000UL)
		return __io(MYEMU2_POWER_VIRT);
	return NULL;
}

static inline void iounmap(volatile void __iomem *addr)
{
	(void)addr;
}
#endif
