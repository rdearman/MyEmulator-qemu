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
#define PAGE_OFFSET 0x00000000UL
#define TASK_SIZE 0x80000000UL
#define TASK_UNMAPPED_BASE 0x10000000UL
#define VMALLOC_START 0x80000000UL
#define VMALLOC_END 0xf0000000UL
#define ARCH_PFN_OFFSET 0
#define __pa(x) ((unsigned long)(x))
#define __va(x) ((void *)((unsigned long)(x)))
#define virt_to_phys(x) __pa(x)
#define phys_to_virt(x) __va(x)
#define pfn_to_phys(pfn) ((phys_addr_t)(pfn) << PAGE_SHIFT)
#define phys_to_pfn(phys) ((unsigned long)(phys) >> PAGE_SHIFT)
#define virt_to_pfn(vaddr) phys_to_pfn(__pa(vaddr))
#define virt_addr_valid(kaddr) ((unsigned long)(kaddr) < 0x01000000UL)
#define pfn_valid(pfn) ((pfn) < (0x01000000UL >> PAGE_SHIFT))
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
