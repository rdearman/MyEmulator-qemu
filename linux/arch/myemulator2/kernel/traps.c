// SPDX-License-Identifier: GPL-2.0
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/interrupt.h>
#include <linux/irq.h>
#include <linux/ptrace.h>
#include <linux/sched.h>
#include <asm/ptrace.h>
#include <asm/unistd.h>
#include <asm/irqflags.h>

asmlinkage long myemulator2_syscall(unsigned long nr,
		unsigned long a0, unsigned long a1, unsigned long a2,
		unsigned long a3, unsigned long a4, unsigned long a5);
asmlinkage long myemulator2_rt_sigreturn(struct pt_regs *regs);
void myemulator2_timer_interrupt(void);
asmlinkage void do_page_fault(struct pt_regs *regs, unsigned long cause,
				      unsigned long address);
void arch_do_signal_or_restart(struct pt_regs *regs);

struct pt_regs *myemulator2_current_pt_regs(void)
{
	return current_thread_info()->regs;
}

void myemulator2_exception_dispatch(struct pt_regs *regs)
{
	void myemulator2_finish_user_exception(struct pt_regs *);
	/* Keep the live user frame available to copy_thread().  A timer or
	 * other supervisor exception may interrupt a syscall while it is in
	 * kernel C code; replacing this pointer with that nested frame would
	 * make a child inherit a kernel PC instead of its user continuation.
	 * Explicit exception handlers still use their regs argument directly. */
	if (!current_thread_info()->regs || user_mode(regs))
		current_thread_info()->regs = regs;
	if (regs->cause != 12)
		regs->orig_r1 = -1;
	if (regs->cause == 12) {
		/* Make the interrupted user continuation visible to clone()/execve()
		 * while they copy the current register image. */
		regs->orig_r1 = regs->r[1];
		regs->pc += 4;
		if (regs->r[1] == 139) {
			myemulator2_rt_sigreturn(regs);
			myemulator2_finish_user_exception(regs);
			return;
		}
		regs->r[1] = myemulator2_syscall(regs->r[1], regs->r[2],
			regs->r[3], regs->r[4], regs->r[5], regs->r[6], regs->r[7]);
		myemulator2_finish_user_exception(regs);
		return;
	}
	/* Vectors 16-22 represent IRQ1-IRQ7.  IRQ1 has the clockevent's
	 * architecture-specific accounting; the remaining fixed lines use the
	 * generic IRQ descriptors so serial-core handlers can run normally. */
	if (regs->cause >= 16 && regs->cause <= 22) {
		unsigned int irq = regs->cause - 15;
		unsigned long flags = arch_local_irq_save();
		if (irq == IRQ_TIMER) {
			myemulator2_timer_interrupt();
		} else {
			irq_enter();
			generic_handle_irq(irq);
			irq_exit();
		}
		arch_local_irq_restore(flags);
		myemulator2_finish_user_exception(regs);
		return;
	}
	if (regs->cause >= 4 && regs->cause <= 9) {
		do_page_fault(regs, regs->cause, regs->info);
		myemulator2_finish_user_exception(regs);
		return;
	}
	pr_emerg("MyEmulator2 exception: pc=%08lx sr=%08lx cause=%lu info=%08lx\n",
		regs->pc, regs->sr, regs->cause, regs->info);
	panic("unhandled MyEmulator2 exception");
}

void myemulator2_finish_user_exception(struct pt_regs *regs)
{
	if (!user_mode(regs))
		return;
	/* This early architecture port predates the generic entry-common hooks.
	 * Run the same arch signal hook directly while interrupts are enabled,
	 * then leave the native exception return with IRQs disabled. */
	local_irq_enable();
	arch_do_signal_or_restart(regs);
	local_irq_disable();
}

void show_regs(struct pt_regs *regs)
{
	pr_emerg("MyEmulator2 registers: pc=%08lx sr=%08lx sp=%08lx\n",
		regs->pc, regs->sr, regs->r[13]);
}
