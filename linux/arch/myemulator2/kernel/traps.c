// SPDX-License-Identifier: GPL-2.0
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/interrupt.h>
#include <linux/irq.h>
#include <linux/ptrace.h>
#include <linux/sched.h>
#include <asm/ptrace.h>
#include <asm/unistd.h>

asmlinkage long myemulator2_syscall(unsigned long nr,
		unsigned long a0, unsigned long a1, unsigned long a2,
		unsigned long a3);
void myemulator2_timer_interrupt(void);
asmlinkage void do_page_fault(struct pt_regs *regs, unsigned long cause,
				      unsigned long address);

void myemulator2_exception_dispatch(struct pt_regs *regs)
{
	if (regs->cause == 12) {
		regs->r[1] = myemulator2_syscall(regs->r[1], regs->r[2],
			regs->r[3], regs->r[4], regs->r[5]);
		/* SYSCALL is a synchronous exception and the architectural frame
		 * contains the address of the SYSCALL instruction itself. */
		regs->pc += 4;
		return;
	}
	/* Vectors 16-22 represent IRQ1-IRQ7.  IRQ1 has the clockevent's
	 * architecture-specific accounting; the remaining fixed lines use the
	 * generic IRQ descriptors so serial-core handlers can run normally. */
	if (regs->cause >= 16 && regs->cause <= 22) {
		unsigned int irq = regs->cause - 15;
		if (irq == IRQ_TIMER) {
			myemulator2_timer_interrupt();
		} else {
			irq_enter();
			generic_handle_irq(irq);
			irq_exit();
		}
		return;
	}
	if (regs->cause >= 4 && regs->cause <= 9) {
		do_page_fault(regs, regs->cause, regs->info);
		return;
	}
	pr_emerg("MyEmulator2 exception: pc=%08lx sr=%08lx cause=%lu info=%08lx\n",
		regs->pc, regs->sr, regs->cause, regs->info);
	panic("unhandled MyEmulator2 exception");
}

void show_regs(struct pt_regs *regs)
{
	pr_emerg("MyEmulator2 registers: pc=%08lx sr=%08lx sp=%08lx\n",
		regs->pc, regs->sr, regs->r[13]);
}
