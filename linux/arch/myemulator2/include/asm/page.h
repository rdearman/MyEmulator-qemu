#ifndef _ASM_MYEMULATOR2_PAGE_H
#define _ASM_MYEMULATOR2_PAGE_H
#define PAGE_SHIFT 12
#ifndef __ASSEMBLY__
typedef unsigned int pgprot_t;
#endif
#define PAGE_SIZE (1UL << PAGE_SHIFT)
#define PAGE_MASK (~(PAGE_SIZE - 1))
#ifndef PAGE_ALIGN
#define PAGE_ALIGN(addr) (((addr) + PAGE_SIZE - 1) & PAGE_MASK)
#endif
#define PAGE_OFFSET 0x80000000UL
#define TASK_SIZE 0x40000000UL
#define TASK_UNMAPPED_BASE 0x10000000UL
#define MYEMU_USER_IMAGE_BASE 0x02000000UL
#define VMALLOC_START 0x40000000UL
#define VMALLOC_END PAGE_OFFSET
#define ARCH_PFN_OFFSET 0
#define __pa(x) (((unsigned long)(x) >= PAGE_OFFSET) ? \
	((unsigned long)(x) - PAGE_OFFSET) : (unsigned long)(x))
#define __va(x) ((void *)(PAGE_OFFSET + (unsigned long)(x)))
#define virt_to_phys(x) __pa(x)
#define phys_to_virt(x) __va(x)
#define pfn_to_phys(pfn) ((phys_addr_t)(pfn) << PAGE_SHIFT)
#define phys_to_pfn(phys) ((unsigned long)(phys) >> PAGE_SHIFT)
#define virt_to_pfn(vaddr) phys_to_pfn(__pa(vaddr))
#ifndef __ASSEMBLY__
extern unsigned long max_low_pfn;
extern unsigned long myemulator2_bootstrap_identity_end;
static inline int myemulator2_virt_addr_valid(const void *address)
{
	unsigned long va = (unsigned long)address;
	unsigned long max_phys = max_low_pfn << PAGE_SHIFT;

	/* Only the linked kernel and early boot allocations retain an identity
	 * alias. Normal RAM is accessed through the scalable direct map. */
	if (va < myemulator2_bootstrap_identity_end)
		return va < max_phys;
	return va >= PAGE_OFFSET && va - PAGE_OFFSET < max_phys;
}
#define virt_addr_valid(kaddr) myemulator2_virt_addr_valid((const void *)(kaddr))
#endif
#define pfn_valid(pfn) ((pfn) < max_low_pfn)
#include <asm-generic/getorder.h>
#ifndef __ASSEMBLY__
#include <linux/string.h>
struct page;
extern struct page *mem_map;
#include <asm-generic/memory_model.h>
static inline void clear_page(void *page) { memset(page, 0, PAGE_SIZE); }
static inline void copy_page(void *to, const void *from) { memcpy(to, from, PAGE_SIZE); }
static inline void clear_user_page(void *page, unsigned long vaddr,
					struct page *pg) { clear_page(page); }
static inline void copy_user_page(void *to, const void *from,
					unsigned long vaddr, struct page *pg)
{ copy_page(to, from); }
#endif
#endif
