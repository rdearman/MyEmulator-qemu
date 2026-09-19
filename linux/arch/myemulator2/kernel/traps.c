// SPDX-License-Identifier: GPL-2.0
#include <linux/kernel.h>
#include <linux/init.h>
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
		return;
	}
	/* IRQ1 is vector 16 (vectors 16-22 represent IRQ1-IRQ7). */
	if (regs->cause == 16) {
		myemulator2_timer_interrupt();
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
