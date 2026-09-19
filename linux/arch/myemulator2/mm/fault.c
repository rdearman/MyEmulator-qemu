// SPDX-License-Identifier: GPL-2.0
#include <linux/kernel.h>
#include <linux/mm.h>
#include <linux/sched.h>

asmlinkage void do_page_fault(struct pt_regs *regs, unsigned long cause,
		unsigned long address)
{
	pr_emerg("MyEmulator2 page fault at 0x%08lx (cause %lu)\n",
		address, cause);
	panic("unhandled MyEmulator2 page fault");
}
