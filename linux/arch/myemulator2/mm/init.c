// SPDX-License-Identifier: GPL-2.0
#include <linux/init.h>
#include <linux/mm.h>
#include <linux/memblock.h>
#include <asm/pgtable.h>

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
	/* Initial bring-up runs with the MMU disabled; page-table integration is
	 * added after the physical console and exception path are validated. */
}

void __init mem_init(void)
{
}
