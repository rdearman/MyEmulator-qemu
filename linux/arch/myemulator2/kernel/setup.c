// SPDX-License-Identifier: GPL-2.0
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/memblock.h>
#include <linux/mm.h>
#include <linux/console.h>
#include <linux/bug.h>
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
	volatile u32 *boot_ram_size = (volatile u32 *)0x00000408UL;
	u32 configured_ram;

	console_verbose();
	if (IS_ENABLED(CONFIG_CMDLINE_FORCE))
		strscpy(boot_command_line, CONFIG_CMDLINE, COMMAND_LINE_SIZE);
	*cmdline_p = boot_command_line;

	/* Describe RAM from the first usable machine page.  The complete
	 * linked image is reserved below; bottom-up memblock allocation then
	 * starts after its actual configuration-dependent end. */
	memory_start = 0x00101000;
	configured_ram = READ_ONCE(*boot_ram_size);
	/* The machine reset ABI publishes the actual RAM extent.  A missing
	 * descriptor is a boot protocol error; silently inventing a RAM size
	 * makes the allocator and page tables disagree with QEMU. */
	if (configured_ram <= memory_start)
		panic("MyEmulator2 invalid RAM size descriptor: 0x%08x",
		      configured_ram);
	memory_end = configured_ram;
	memblock_add(memory_start, memory_end - memory_start);
	min_low_pfn = PFN_UP(memory_start);
	max_low_pfn = PFN_DOWN(memory_end);
	set_max_mapnr(max_low_pfn - ARCH_PFN_OFFSET);
	/* At the 2 GiB physical limit this exclusive end is 4 GiB and wraps to
	 * NULL in the 32-bit direct-map representation.  The architecture's
	 * virt_addr_valid() uses PFNs and does not infer validity from this value. */
	high_memory = (void *)((unsigned long)PAGE_OFFSET +
			       PFN_PHYS(max_low_pfn));
	memblock_reserve(__pa_symbol(__vectors_start),
			__pa_symbol(_end) - __pa_symbol(__vectors_start));
	/* head.S switches from QEMU's reset stack to the linked init stack before
	 * entering C, so no RAM-top stack page needs to be reserved here. */
	/* Allocate FLATMEM metadata from the low end, where the bootstrap mapping
	 * can access it while paging_init() constructs the full direct map. */
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
