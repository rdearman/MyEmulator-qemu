// SPDX-License-Identifier: GPL-2.0
#include <linux/init.h>
#include <linux/mm.h>
#include <linux/memblock.h>
#include <linux/overflow.h>
#include <asm/io.h>
#include <asm/pgtable.h>
#include <asm/sections.h>
#include <asm/setup.h>

pgd_t swapper_pg_dir[PTRS_PER_PGD] __aligned(PAGE_SIZE);
bool myemulator2_paging_ready;

/* The boot image and its initial stack are linked at physical addresses.
 * Keep an identity map only through the image plus one table-allocation
 * window. The size is computed from the linked image, not from RAM size. */
static pte_t mmio_pte[PTRS_PER_PTE] __aligned(PAGE_SIZE);
static pte_t *bootstrap_pte;
static pte_t *direct_pte;
static unsigned int bootstrap_pgd_count;
unsigned long myemulator2_bootstrap_identity_end;

pgprot_t vm_get_page_prot(unsigned long vm_flags)
{
	pgprot_t prot = _PAGE_PRESENT;
	if (vm_flags & VM_READ)
		prot |= _PAGE_READ;
	/* A writable private file mapping must remain write-protected until the
	 * generic MM performs COW.  Anonymous write faults install writable PTEs
	 * in do_anonymous_page(); shared mappings can be writable immediately. */
	if ((vm_flags & (VM_WRITE | VM_SHARED)) == (VM_WRITE | VM_SHARED))
		prot |= _PAGE_WRITE;
	if (vm_flags & VM_EXEC)
		prot |= _PAGE_EXEC;
	return prot | _PAGE_USER;
}

static unsigned long myemulator2_table_phys(const void *table)
{
	return __pa(table) & PAGE_MASK;
}

pgd_t *pgd_alloc(struct mm_struct *mm)
{
	pgd_t *pgd = (pgd_t *)get_zeroed_page(GFP_KERNEL);
	pte_t *user_pte;
	const unsigned int kernel_flags = _PAGE_PRESENT | _PAGE_READ |
		_PAGE_WRITE | _PAGE_EXEC;
	const unsigned int user_flags = kernel_flags | _PAGE_USER;
	unsigned int i;

	if (!pgd)
		return NULL;
	memcpy(pgd, swapper_pg_dir, sizeof(swapper_pg_dir));

	/* A userspace fault must never replace a shared supervisor identity PTE.
	 * Give each mm private copies only for the boot image's low identity map. */
	for (i = 0; i < bootstrap_pgd_count; i++) {
		pte_t *private_pte;

		if (i >= (MYEMU_USER_IMAGE_BASE >> PGDIR_SHIFT) || !pgd[i].pgd)
			continue;
		private_pte = (pte_t *)get_zeroed_page(GFP_KERNEL);
		if (!private_pte)
			goto fail_identity;
		memcpy(private_pte, bootstrap_pte + i * PTRS_PER_PTE, PAGE_SIZE);
		pgd[i] = __pgd(myemulator2_table_phys(private_pte) | kernel_flags);
	}

	user_pte = (pte_t *)get_zeroed_page(GFP_KERNEL);
	if (!user_pte)
		goto fail_identity;
	pgd[MYEMU_USER_IMAGE_BASE >> PGDIR_SHIFT] =
		__pgd(myemulator2_table_phys(user_pte) | user_flags);
	mm_inc_nr_ptes(mm);
	return pgd;

fail_identity:
	while (i) {
		--i;
		if (i < (MYEMU_USER_IMAGE_BASE >> PGDIR_SHIFT) &&
		    pgd[i].pgd && pgd[i].pgd != swapper_pg_dir[i].pgd)
			free_page((unsigned long)__va(pgd[i].pgd & PAGE_MASK));
	}
	free_page((unsigned long)pgd);
	return NULL;
}

void pgd_free(struct mm_struct *mm, pgd_t *pgd)
{
	unsigned int i;
	(void)mm;

	if (!pgd)
		return;
	for (i = 0; i < bootstrap_pgd_count &&
	     i < (MYEMU_USER_IMAGE_BASE >> PGDIR_SHIFT); i++) {
		if (pgd[i].pgd && pgd[i].pgd != swapper_pg_dir[i].pgd)
			free_page((unsigned long)__va(pgd[i].pgd & PAGE_MASK));
	}
	i = MYEMU_USER_IMAGE_BASE >> PGDIR_SHIFT;
	if (pgd[i].pgd)
		free_page((unsigned long)__va(pgd[i].pgd & PAGE_MASK));
	free_page((unsigned long)pgd);
}

void __init paging_init(void)
{
	unsigned long max_zone_pfns[MAX_NR_ZONES] = { 0 };
	const unsigned long kernel_flags = _PAGE_PRESENT | _PAGE_READ |
		_PAGE_WRITE | _PAGE_EXEC;
	const unsigned long direct_pgd_count = DIV_ROUND_UP(memory_end,
								 PGDIR_SIZE);
	const unsigned long image_end = __pa_symbol(_end);
	const unsigned long bootstrap_end = ALIGN(image_end, PGDIR_SIZE) +
		PGDIR_SIZE;
	const unsigned long identity_count = DIV_ROUND_UP(bootstrap_end,
								 PGDIR_SIZE);
	const unsigned long pte_pages = identity_count + direct_pgd_count;
	phys_addr_t pte_phys;
	unsigned long pte_bytes;
	unsigned long i, j;
	unsigned long identity_limit;

	if (direct_pgd_count > (PTRS_PER_PGD - (PAGE_OFFSET >> PGDIR_SHIFT)))
		panic("MyEmulator2 RAM exceeds the 32-bit direct-map window");
	if (check_mul_overflow(pte_pages, (unsigned long)PAGE_SIZE, &pte_bytes))
		panic("MyEmulator2 page-table size overflow");
	identity_limit = min(memory_end, bootstrap_end);
	pte_phys = memblock_phys_alloc_range(pte_bytes, PAGE_SIZE,
					    memory_start, identity_limit);
	if (!pte_phys)
		panic("MyEmulator2 cannot allocate RAM-scaled page tables");
	bootstrap_pte = (pte_t *)(unsigned long)pte_phys;
	direct_pte = bootstrap_pte + identity_count * PTRS_PER_PTE;
	bootstrap_pgd_count = identity_count;
	myemulator2_bootstrap_identity_end = bootstrap_end;

	memset(swapper_pg_dir, 0, sizeof(swapper_pg_dir));
	memset((void *)(unsigned long)pte_phys, 0, pte_bytes);

	/* Temporary identity map for executing the linked image and constructing
	 * the final map. Allocated table storage is constrained to this window. */
	for (i = 0; i < identity_count; i++) {
		pte_t *table = bootstrap_pte + i * PTRS_PER_PTE;

		for (j = 0; j < PTRS_PER_PTE; j++) {
			unsigned long phys = i * PGDIR_SIZE + j * PAGE_SIZE;
			if (phys < bootstrap_end)
				table[j] = __pte(phys | kernel_flags);
		}
		swapper_pg_dir[i] = __pgd(myemulator2_table_phys(table) |
					   kernel_flags);
	}

	/* Map each RAM PFN at PAGE_OFFSET + physical address. The number of PTE
	 * pages is derived from the machine-provided RAM extent; no fixed RAM
	 * mapping limit or fixed-size PTE array is involved. */
	for (i = 0; i < direct_pgd_count; i++) {
		pte_t *table = direct_pte + i * PTRS_PER_PTE;
		unsigned long remaining = memory_end - i * PGDIR_SIZE;
		unsigned long entries = min_t(unsigned long, PTRS_PER_PTE,
					      DIV_ROUND_UP(remaining, PAGE_SIZE));
		for (j = 0; j < entries; j++)
			table[j] = __pte((i * PGDIR_SIZE + j * PAGE_SIZE) |
					 kernel_flags);
		swapper_pg_dir[(PAGE_OFFSET >> PGDIR_SHIFT) + i] =
			__pgd(myemulator2_table_phys(table) | kernel_flags);
	}

	/* The machine's UART and block controller have stable physical bus
	 * addresses and fixed aliases in the vmalloc region. */
	mmio_pte[0] = __pte(0xf0000000UL | kernel_flags);
	mmio_pte[0x100] = __pte(0xf0100000UL | kernel_flags);
	mmio_pte[0x200] = __pte(0xf0200000UL | kernel_flags);
	mmio_pte[0x300] = __pte(0xf0300000UL | kernel_flags);
	swapper_pg_dir[MYEMU2_UART_VIRT >> PGDIR_SHIFT] =
		__pgd(myemulator2_table_phys(mmio_pte) | kernel_flags);

	init_mm.pgd = swapper_pg_dir;
	max_zone_pfns[ZONE_NORMAL] = memory_end >> PAGE_SHIFT;
	asm volatile("mtsr ptbr, %0\n\tmtsr mmcr, %1" ::
			"r"(__pa(swapper_pg_dir)), "r"(1) : "memory");
	myemulator2_paging_ready = true;
	/* free_area_init() can now safely access allocator metadata through the
	 * direct map, including when the mem_map spans beyond the bootstrap map. */
	free_area_init(max_zone_pfns);
	pr_info("MyEmulator2 maps RAM dynamically: %lu MiB, %lu PTE pages; "
		"bootstrap identity end 0x%08lx\n",
		memory_end >> 20, direct_pgd_count, bootstrap_end);
}

void __init mem_init(void)
{
	memblock_free_all();
}
