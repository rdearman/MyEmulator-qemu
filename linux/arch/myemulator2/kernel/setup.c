// SPDX-License-Identifier: GPL-2.0
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/memblock.h>
#include <linux/mm.h>
#include <linux/console.h>
#include <linux/initrd.h>
#include <linux/seq_file.h>
#include <asm/sections.h>
#include <asm/setup.h>

extern void start_kernel(void);
extern void paging_init(void);
extern char __vectors_start[];

unsigned long memory_start;
unsigned long memory_end;

const struct seq_operations cpuinfo_op = { };

static void __init myemulator2_install_vectors(void)
{
	/* The vector table is relocated to the linked kernel table by head.S. */
	pr_info("MyEmulator2 vectors at %px\n", (void *)__vectors_start);
}

void __init setup_arch(char **cmdline_p)
{
	console_verbose();
	if (IS_ENABLED(CONFIG_CMDLINE_FORCE))
		strscpy(boot_command_line, CONFIG_CMDLINE, COMMAND_LINE_SIZE);
	*cmdline_p = boot_command_line;

	/* Keep the reset/vector and kernel-load area out of Linux's general
	 * allocator.  The linked image begins at 0x000ffc00 and the vectors
	 * occupy 0x00100000-0x001003ff. */
	memory_start = 0x00101000;
	memory_end = memblock_end_of_DRAM();
	if (!memory_end)
		memory_end = 16 * 1024 * 1024;
	memblock_add(memory_start, memory_end - memory_start);
	min_low_pfn = PFN_UP(memory_start);
	max_low_pfn = PFN_DOWN(memory_end);
	set_max_mapnr(max_low_pfn - ARCH_PFN_OFFSET);
	high_memory = (void *)__va(PFN_PHYS(max_low_pfn));
	memblock_reserve(__pa_symbol(_stext), _end - _stext);
	/* QEMU resets SSP to the top of the page immediately above this
	 * one, and the architectural stack grows downward.  Keep the
	 * initial supervisor stack page out of early allocations until the
	 * normal per-task stacks exist. */
	if (memory_end >= 2 * PAGE_SIZE)
		memblock_reserve(memory_end - 2 * PAGE_SIZE, PAGE_SIZE);
	/* The minimal machine has no high-memory window.  Allocate early
	 * metadata from the low end so FLATMEM's struct page array cannot land at
	 * the exclusive end address of RAM. */
	memblock_set_bottom_up(true);
	setup_initial_init_mm(_stext, _etext, _edata, _end);
	paging_init();
	myemulator2_install_vectors();
	pr_info("MyEmulator2 RAM: 0x%08lx-0x%08lx\n",
		memory_start, memory_end);
}

void __init myemulator2_start_kernel(void)
{
	start_kernel();
}
