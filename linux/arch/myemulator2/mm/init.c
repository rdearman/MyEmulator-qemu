// SPDX-License-Identifier: GPL-2.0
#include <linux/init.h>
#include <linux/mm.h>
#include <linux/memblock.h>
#include <asm/pgtable.h>
#include <asm/setup.h>

pgd_t swapper_pg_dir[PTRS_PER_PGD] __aligned(PAGE_SIZE);
static pte_t swapper_pte[4][PTRS_PER_PTE] __aligned(PAGE_SIZE);
static pte_t swapper_pte_mmio[PTRS_PER_PTE] __aligned(PAGE_SIZE);

pgprot_t vm_get_page_prot(unsigned long vm_flags)
{
	pgprot_t prot = _PAGE_PRESENT;
	if (vm_flags & VM_READ)
		prot |= _PAGE_READ;
	if (vm_flags & VM_WRITE)
		prot |= _PAGE_WRITE;
	if (vm_flags & VM_EXEC)
		prot |= _PAGE_EXEC;
	/* This helper describes user VMAs.  Kernel mappings use PAGE_KERNEL. */
	prot |= _PAGE_USER;
	return prot;
}

pgd_t *pgd_alloc(struct mm_struct *mm)
{
	pgd_t *pgd = (pgd_t *)get_zeroed_page(GFP_KERNEL);
	pte_t *user_pte;
	unsigned int flags = _PAGE_PRESENT | _PAGE_USER | _PAGE_READ |
		_PAGE_WRITE | _PAGE_EXEC;
	if (pgd)
		memcpy(pgd, swapper_pg_dir, sizeof(swapper_pg_dir));
	if (!pgd)
		return NULL;
	user_pte = (pte_t *)get_zeroed_page(GFP_KERNEL);
	if (user_pte) {
		memcpy(user_pte, swapper_pte[1], PAGE_SIZE);
		user_pte[256] = __pte(0);
		pgd[1] = __pgd((unsigned long)user_pte | flags);
	}
	return pgd;
}

void __init paging_init(void)
{
	unsigned long max_zone_pfns[MAX_NR_ZONES] = { 0 };
	unsigned int i, j;
	unsigned long flags = _PAGE_PRESENT | _PAGE_READ |
		_PAGE_WRITE | _PAGE_EXEC;

	/* The kernel is linked at its physical addresses.  Install an identity
	 * map for the first 16 MiB so enabling the hardware MMU does not disturb
	 * supervisor execution, while leaving user mappings to Linux's normal
	 * page-fault path. */
	memset(swapper_pg_dir, 0, sizeof(swapper_pg_dir));
	for (i = 0; i < 4; i++) {
		for (j = 0; j < PTRS_PER_PTE; j++)
			swapper_pte[i][j] = __pte(i * PGDIR_SIZE + j * PAGE_SIZE |
				flags);
		swapper_pg_dir[i] = __pgd((unsigned long)swapper_pte[i] | flags);
	}
	/* The first test userspace image is linked at 0x00500000.  Expose only
	 * that page to User mode; the rest of the kernel identity map remains
	 * Supervisor-only. */
	/* Keep the architected UART visible at its fixed virtual/physical
	 * address once the MMU is enabled. */
	swapper_pte_mmio[0] = __pte(0xf0000000UL | flags);
	swapper_pg_dir[0x3c0] = __pgd((unsigned long)swapper_pte_mmio | flags);
	init_mm.pgd = swapper_pg_dir;

	/* The initial port uses a flat physical kernel mapping.  Still create
	 * Linux's normal zone/page allocator metadata so boot-time slab users can
	 * obtain pages from the memblock-described RAM. */
	/* setup_arch() establishes the platform RAM extent.  The generic
	 * memblock query is not populated by this minimal platform's boot
	 * path, so use the verified extent directly. */
	max_zone_pfns[ZONE_NORMAL] = memory_end >> PAGE_SHIFT;
	free_area_init(max_zone_pfns);
	asm volatile("mtsr ptbr, %0\n\tmtsr mmcr, %1" ::
			"r"((unsigned long)swapper_pg_dir), "r"(1) : "memory");
}

void __init mem_init(void)
{
	memblock_free_all();
}
