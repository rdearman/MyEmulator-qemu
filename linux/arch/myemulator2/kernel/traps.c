// SPDX-License-Identifier: GPL-2.0
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/interrupt.h>
#include <linux/irq.h>
#include <linux/ptrace.h>
#include <linux/sched.h>
#include <linux/errno.h>
#include <linux/signal.h>
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
		/*
		 * do_page_fault() calls force_sig_fault() (SIGSEGV/SIGBUS) for a
		 * genuinely bad user access. That signal is now actually
		 * delivered to a registered userspace handler (or acted on by
		 * its default disposition) by myemulator2_finish_user_exception()
		 * below, via the proper arch_do_signal_or_restart()/setup_rt_frame()
		 * implementation in signal.c -- this replaces the previous
		 * get_signal()-drain-and-drop workaround that could never
		 * actually run a handler and occasionally dequeued an unrelated
		 * pending signal (e.g. SIGCHLD) instead of the SIGSEGV/SIGBUS
		 * just queued here.
		 */
		myemulator2_finish_user_exception(regs);
		return;
	}
	/* Vectors 2/3 are INSN_ALIGN/DATA_ALIGN. A misaligned access from a
	 * user-mode process is the process's own bug (e.g. a corrupted or
	 * bogus branch target/pointer), not a kernel error: it must be
	 * reported to that process via SIGBUS, exactly like the SIGSEGV/
	 * SIGBUS path used for causes 4-9 above, instead of taking down the
	 * whole system. A misaligned access from kernel mode is still a
	 * genuine kernel bug and continues to panic below.
	 */
	if ((regs->cause == 2 || regs->cause == 3) && user_mode(regs)) {
		force_sig_fault(SIGBUS, BUS_ADRALN, (void __user *)regs->info);
		myemulator2_finish_user_exception(regs);
		return;
	}
	pr_emerg("MyEmulator2 exception: pc=%08lx sr=%08lx cause=%lu info=%08lx\n",
		regs->pc, regs->sr, regs->cause, regs->info);
	/* Dump registers and a window of user stack memory around SP/FP for
	 * any fatal, uncategorized user-mode exception, to aid diagnosing
	 * unexpected crashes without needing to reproduce them under a
	 * debugger. */
	if (user_mode(regs)) {
		unsigned long sp = regs->r[13];
		unsigned long fp = regs->r[15];
		int i;

		pr_emerg("MYEMU_FATAL_REGS pid=%d r0=%08lx r1=%08lx r2=%08lx r3=%08lx r4=%08lx r5=%08lx r6=%08lx r7=%08lx\n",
			current->pid, regs->r[0], regs->r[1], regs->r[2], regs->r[3],
			regs->r[4], regs->r[5], regs->r[6], regs->r[7]);
		pr_emerg("MYEMU_FATAL_REGS2 pid=%d r8=%08lx r9=%08lx r10=%08lx r11=%08lx r12=%08lx r13(sp)=%08lx r14(lr)=%08lx r15(fp)=%08lx\n",
			current->pid, regs->r[8], regs->r[9], regs->r[10], regs->r[11],
			regs->r[12], sp, regs->r[14], fp);
		for (i = -48; i <= 32; i++) {
			unsigned long addr = sp + (i * 4);
			unsigned int val;

			if (get_user(val, (unsigned int __user *)addr))
				continue;
			pr_emerg("MYEMU_FATAL_STACK2 pid=%d sp%+d(%08lx)=%08x\n",
				current->pid, i * 4, addr, val);
		}
	}
	panic("unhandled MyEmulator2 exception");
}

void myemulator2_finish_user_exception(struct pt_regs *regs)
{
	if (!user_mode(regs))
		return;
	arch_do_signal_or_restart(regs);
}

void show_regs(struct pt_regs *regs)
{
	pr_emerg("MyEmulator2 registers: pc=%08lx sr=%08lx sp=%08lx\n",
		regs->pc, regs->sr, regs->r[13]);
}
