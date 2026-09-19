#ifndef _ASM_MYEMULATOR2_VMALLOC_H
#define _ASM_MYEMULATOR2_VMALLOC_H
#include <asm/page.h>
#include <asm/pgtable.h>
#define VMALLOC_TOTAL (VMALLOC_END - VMALLOC_START)
#define arch_vmap_pmd_supported(addr) false
#define arch_vmap_pud_supported(addr) false
#define arch_vmap_p4d_supported(addr) false
#define arch_vmap_pgprot_tagged(prot) (prot)
#endif
