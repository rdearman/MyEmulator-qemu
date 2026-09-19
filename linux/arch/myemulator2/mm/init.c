// SPDX-License-Identifier: GPL-2.0
#include <linux/init.h>
#include <linux/mm.h>
#include <linux/memblock.h>
#include <asm/pgtable.h>

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
