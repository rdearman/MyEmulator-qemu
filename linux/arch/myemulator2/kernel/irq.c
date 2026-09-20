// SPDX-License-Identifier: GPL-2.0
#include <linux/init.h>
#include <linux/interrupt.h>
#include <linux/irq.h>
#include <linux/irqdesc.h>
#include <asm/irq.h>

static void myemulator2_irq_noop(struct irq_data *data)
{
	(void)data;
}

static struct irq_chip myemulator2_irq_chip = {
	.name = "myemulator2-fixed",
	.irq_ack = myemulator2_irq_noop,
	.irq_mask = myemulator2_irq_noop,
	.irq_unmask = myemulator2_irq_noop,
};

void __init init_IRQ(void)
{
	unsigned int irq;

	/* The CPU exposes fixed, level-sensitive IRQ inputs directly to the
	 * kernel.  There is no programmable interrupt controller or irqdomain;
	 * install descriptors so request_irq() and generic_handle_irq() can use
	 * the architectural lines.  Device handlers clear their own level source.
	 */
	for (irq = 0; irq < NR_IRQS; irq++)
		irq_set_chip_and_handler(irq, &myemulator2_irq_chip,
					 handle_level_irq);
}

void myemulator2_exception_dispatch(struct pt_regs *regs);
