// SPDX-License-Identifier: GPL-2.0
#include <linux/init.h>
#include <linux/mm.h>
#include <linux/memblock.h>
#include <asm/pgtable.h>
#include <asm/setup.h>

pgd_t swapper_pg_dir[PTRS_PER_PGD] __aligned(PAGE_SIZE);

pgprot_t vm_get_page_prot(unsigned long vm_flags)
{
	pgprot_t prot = _PAGE_PRESENT;
	if (vm_flags & VM_READ)
		prot |= _PAGE_READ;
	if (vm_flags & VM_WRITE)
		prot |= _PAGE_WRITE;
	if (vm_flags & VM_EXEC)
		prot |= _PAGE_EXEC;
	if (vm_flags & VM_MAYSHARE)
		prot |= _PAGE_USER;
	return prot;
}

pgd_t *pgd_alloc(struct mm_struct *mm)
{
	(void)mm;
	return (pgd_t *)get_zeroed_page(GFP_KERNEL);
}

void __init paging_init(void)
{
	unsigned long max_zone_pfns[MAX_NR_ZONES] = { 0 };

	/* The initial port uses a flat physical kernel mapping.  Still create
	 * Linux's normal zone/page allocator metadata so boot-time slab users can
	 * obtain pages from the memblock-described RAM. */
	/* setup_arch() establishes the platform RAM extent.  The generic
	 * memblock query is not populated by this minimal platform's boot
	 * path, so use the verified extent directly. */
	max_zone_pfns[ZONE_NORMAL] = memory_end >> PAGE_SHIFT;
	free_area_init(max_zone_pfns);
}

void __init mem_init(void)
{
	memblock_free_all();
}
