// SPDX-License-Identifier: GPL-2.0
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/mm.h>
#include <linux/vmalloc.h>

/* Exercise the hardware page-table path used by vzalloc before userspace
 * starts.  In particular, touch both sides of a page boundary and the end
 * of a multi-page mapping rather than testing only its first byte. */
static int __init myemulator2_vmalloc_selftest(void)
{
	unsigned char *p;
	unsigned int i;

	p = vzalloc(3 * PAGE_SIZE + 17);
	if (!p)
		return -ENOMEM;
	for (i = 0; i < 3 * PAGE_SIZE + 17; i++)
		if (p[i] != 0)
			goto fail;
	p[0] = 0x11;
	p[PAGE_SIZE - 1] = 0x22;
	p[PAGE_SIZE] = 0x33;
	p[2 * PAGE_SIZE] = 0x44;
	p[3 * PAGE_SIZE + 16] = 0x55;
	if (p[0] != 0x11 || p[PAGE_SIZE - 1] != 0x22 ||
	    p[PAGE_SIZE] != 0x33 || p[2 * PAGE_SIZE] != 0x44 ||
	    p[3 * PAGE_SIZE + 16] != 0x55)
		goto fail;
	vfree(p);
	pr_info("REM vmalloc selftest: PASS\n");
	return 0;
fail:
	vfree(p);
	pr_err("REM vmalloc selftest: FAIL\n");
	return -EIO;
}
late_initcall(myemulator2_vmalloc_selftest);
