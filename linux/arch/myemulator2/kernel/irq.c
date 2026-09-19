// SPDX-License-Identifier: GPL-2.0
#include <linux/init.h>
#include <linux/interrupt.h>
#include <linux/irq.h>
#include <linux/irqdesc.h>
#include <asm/irq.h>

void __init init_IRQ(void)
{
	/* The initial platform wires the CPU's architectural IRQ inputs directly
	 * to the machine devices; no irqdomain is needed for this fixed layout. */
}

void myemulator2_exception_dispatch(struct pt_regs *regs);
