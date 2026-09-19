#ifndef _ASM_MYEMULATOR2_MMU_H
#define _ASM_MYEMULATOR2_MMU_H
#include <asm/pgtable.h>
typedef struct { unsigned long pgd; } mm_context_t;
static inline void update_mmu_cache(struct vm_area_struct *vma,
				    unsigned long address, pte_t *ptep)
{ (void)vma; (void)address; (void)ptep; }
static inline bool arch_vma_access_permitted(struct vm_area_struct *vma,
					     bool write, bool execute, bool foreign)
{ (void)vma; (void)write; (void)execute; (void)foreign; return true; }
#endif
