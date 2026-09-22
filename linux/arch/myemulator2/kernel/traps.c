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
void myemulator2_timer_interrupt(void);
asmlinkage void do_page_fault(struct pt_regs *regs, unsigned long cause,
				      unsigned long address);

struct pt_regs *myemulator2_current_pt_regs(void)
{
	return current_thread_info()->regs;
}

static unsigned int myemu_trace_user_faults;
static int myemu_last_fault_pid;
static unsigned long myemu_last_fault_cause;
static unsigned long myemu_last_fault_pc;
static unsigned long myemu_last_fault_r14;

void myemulator2_exception_dispatch(struct pt_regs *regs)
{
	/* Keep the live user frame available to copy_thread().  A timer or
	 * other supervisor exception may interrupt a syscall while it is in
	 * kernel C code; replacing this pointer with that nested frame would
	 * make a child inherit a kernel PC instead of its user continuation.
	 * Explicit exception handlers still use their regs argument directly. */
	if (!current_thread_info()->regs || user_mode(regs))
		current_thread_info()->regs = regs;
	if (regs->cause == 12) {
		if (regs->r[1] == 214 || regs->r[1] == 220 || regs->r[1] == 221 ||
		    regs->r[1] == 222 || regs->r[1] == 260)
			pr_warn("MYEMU_SYSCALL_ENTER pid=%d nr=%lu pc=%08lx r1=%08lx r2=%08lx r3=%08lx r4=%08lx r5=%08lx r13=%08lx r14=%08lx r15=%08lx\n",
				current->pid, regs->r[1], regs->pc, regs->r[1],
				regs->r[2], regs->r[3], regs->r[4], regs->r[5],
				regs->r[13], regs->r[14], regs->r[15]);
		/* Make the interrupted user continuation visible to clone()/execve()
		 * while they copy the current register image. */
		regs->pc += 4;
		{
			unsigned long nr = regs->r[1];
			unsigned long a0 = regs->r[2], a1 = regs->r[3], a2 = regs->r[4];
			unsigned long a3 = regs->r[5], a4 = regs->r[6], a5 = regs->r[7];

			regs->r[1] = myemulator2_syscall(nr, a0, a1, a2, a3, a4, a5);
			/*
			 * This port has no return-to-userspace signal delivery
			 * path (no do_signal()/get_signal() call anywhere in
			 * entry.S or here, confirmed by inspection). A blocking
			 * syscall such as wait4() can legitimately return one of
			 * the kernel-internal -ERESTARTSYS/-ERESTARTNOINTR/
			 * -ERESTARTNOHAND/-ERESTART_RESTARTBLOCK codes when a
			 * signal becomes pending while it slept; those values
			 * are only meaningful to the (missing) signal-delivery
			 * step that is supposed to consume the pending signal
			 * and either restart the syscall or hand control to a
			 * userspace handler. Without that step the raw code
			 * leaked straight into userspace as literal -512..-516,
			 * which musl's syscall wrapper turned into a bogus
			 * errno; this was observed cascading into an eventual
			 * fatal "Attempted to kill init!" panic when pid 1's own
			 * wait4() was affected.
			 *
			 * Minimal fix: when a restart code comes back, actually
			 * dequeue the pending signal via get_signal(). If it was
			 * handled internally (ignored, or fatal-default causing
			 * do_exit()/do_group_exit() from within get_signal()
			 * itself) it returns false and the interrupted syscall is
			 * simply retried, matching normal ERESTARTNOHAND/
			 * ERESTARTSYS semantics for the common case (e.g.
			 * default-ignored SIGCHLD waking wait4()). If a real
			 * userspace handler is registered, get_signal() returns
			 * true, but this port still has no setup_rt_frame()/
			 * sigreturn trampoline to invoke it; in that case fall
			 * back to -EINTR rather than looping forever or leaking
			 * the restart code. This is a known remaining gap, not
			 * full POSIX signal support.
			 */
			{
			unsigned int restart_guard = 0;
			while ((regs->r[1] == (unsigned long)-ERESTARTSYS ||
			       regs->r[1] == (unsigned long)-ERESTARTNOINTR ||
			       regs->r[1] == (unsigned long)-ERESTARTNOHAND ||
			       regs->r[1] == (unsigned long)-ERESTART_RESTARTBLOCK) &&
			       ++restart_guard < 1000) {
				struct ksignal ksig;

				if (signal_pending(current) && get_signal(&ksig)) {
					pr_warn("MYEMU_SIGNAL_NO_HANDLER pid=%d sig=%d: no sigreturn support, forcing -EINTR\n",
						current->pid, ksig.sig);
					regs->r[1] = (unsigned long)-EINTR;
					break;
				}
				/* Signal queue drained (ignored/default-handled, or
				 * nothing left pending); retry the original syscall. */
				regs->r[1] = myemulator2_syscall(nr, a0, a1, a2, a3, a4, a5);
			}
			}
		}
		if (regs->pc && (regs->r[1] == 214 || regs->r[1] == 222 ||
			regs->r[1] < 0 || regs->r[1] > 0) &&
			(regs->pc >= 0x02000000 && regs->pc < 0x10000000))
			pr_warn("MYEMU_SYSCALL_EXIT pid=%d pc=%08lx ret=%08lx r13=%08lx r14=%08lx r15=%08lx\n",
				current->pid, regs->pc, regs->r[1], regs->r[13],
				regs->r[14], regs->r[15]);
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
		return;
	}
	if (regs->cause >= 4 && regs->cause <= 9) {
		unsigned int fault_no = myemu_trace_user_faults++;
		/* TEMPORARY diagnostic: remember the last page fault's pid/pc/
		 * cause/r14 so a later unrelated-looking align fault can be
		 * correlated with what immediately preceded it. */
		myemu_last_fault_pid = current->pid;
		myemu_last_fault_cause = regs->cause;
		myemu_last_fault_pc = regs->pc;
		myemu_last_fault_r14 = regs->r[14];
		if (fault_no < 64 || (fault_no & 1023) == 0)
			pr_warn("MYEMU_USER_FAULT n=%u pid=%d cause=%08lx pc=%08lx info=%08lx sp=%08lx r1=%08lx r2=%08lx r14=%08lx r15=%08lx pgd=%08lx\\n",
				fault_no, current->pid, regs->cause,
				regs->pc, regs->info, regs->r[13], regs->r[1], regs->r[2],
				regs->r[14], regs->r[15],
				current->mm ? (unsigned long)current->mm->pgd : 0);
		do_page_fault(regs, regs->cause, regs->info);
		/*
		 * do_page_fault() calls force_sig_fault() (SIGSEGV/SIGBUS) for a
		 * genuinely bad user access. That only queues the signal; as with
		 * the cause==12 syscall path above, this port has no
		 * return-to-userspace signal delivery step to dequeue it. Left
		 * unhandled, we simply `rfe` straight back to the same faulting
		 * instruction and fault on it again forever -- this is the
		 * MYEMU_USER_FAULT storm reported in the original bug (observed
		 * here as an unbounded instruction-fetch refault loop at a fixed
		 * pc after a forked child's exec()). Dequeue via get_signal():
		 * for the default SIGSEGV/SIGBUS disposition this terminates the
		 * faulting process from within get_signal() itself (no return
		 * here in that case). get_signal() also dequeues whichever signal
		 * is *first* in priority order, which is not necessarily the
		 * SIGSEGV/SIGBUS just queued above -- e.g. an interactive shell's
		 * own previously-pending SIGCHLD can be dequeued here instead.
		 * There is still no setup_rt_frame()/sigreturn support to hand
		 * control to a real userspace handler in that case; killing the
		 * process outright (previously attempted here) is worse than the
		 * bug it replaces, since it can take down pid 1 for an unrelated
		 * signal such as SIGCHLD whose handler we simply cannot invoke.
		 * Instead, log and drop the undeliverable handler invocation:
		 * this clears the signal's pending state (allowing forward
		 * progress / other pending signals such as the real SIGSEGV to
		 * be considered on the next fault or syscall) without
		 * terminating an unrelated process.
		 */
		if (user_mode(regs) && signal_pending(current)) {
			struct ksignal ksig;

			if (get_signal(&ksig)) {
				pr_warn_ratelimited("MYEMU_SIGNAL_NO_HANDLER pid=%d sig=%d: no sigreturn support, dropping\n",
					current->pid, ksig.sig);
			}
		}
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
		/* TEMPORARY diagnostic (Bug #4 follow-up): dump the full
		 * register file to help identify whether r14 (or another
		 * register) was clobbered by something other than the
		 * faulting instruction itself. Remove once root-caused. */
		pr_warn("MYEMU_USER_ALIGN pid=%d cause=%lu pc=%08lx info=%08lx sp=%08lx\n",
			current->pid, regs->cause, regs->pc, regs->info, regs->r[13]);
		pr_warn("MYEMU_USER_ALIGN_REGS pid=%d r0=%08lx r1=%08lx r2=%08lx r3=%08lx r4=%08lx r5=%08lx r6=%08lx r7=%08lx\n",
			current->pid, regs->r[0], regs->r[1], regs->r[2], regs->r[3],
			regs->r[4], regs->r[5], regs->r[6], regs->r[7]);
		pr_warn("MYEMU_USER_ALIGN_REGS2 pid=%d r8=%08lx r9=%08lx r10=%08lx r11=%08lx r12=%08lx r13=%08lx r14=%08lx r15=%08lx sr=%08lx\n",
			current->pid, regs->r[8], regs->r[9], regs->r[10], regs->r[11],
			regs->r[12], regs->r[13], regs->r[14], regs->r[15], regs->sr);
		pr_warn("MYEMU_USER_ALIGN_PREV last_fault_pid=%d last_fault_cause=%lu last_fault_pc=%08lx last_fault_r14=%08lx\n",
			myemu_last_fault_pid, myemu_last_fault_cause,
			myemu_last_fault_pc, myemu_last_fault_r14);
		force_sig_fault(SIGBUS, BUS_ADRALN, (void __user *)regs->info);
		if (signal_pending(current)) {
			struct ksignal ksig;

			if (get_signal(&ksig)) {
				pr_warn_ratelimited("MYEMU_SIGNAL_NO_HANDLER pid=%d sig=%d: no sigreturn support, dropping\n",
					current->pid, ksig.sig);
			}
		}
		return;
	}
	pr_emerg("MyEmulator2 exception: pc=%08lx sr=%08lx cause=%lu info=%08lx\n",
		regs->pc, regs->sr, regs->cause, regs->info);
	/* TEMPORARY diagnostic (Bug #6 follow-up): dump registers and a
	 * window of user stack memory around SP/FP at the moment of a fatal,
	 * uncategorized user-mode exception, to look for stack corruption
	 * (e.g. a return address overwritten by string data) without
	 * modifying the userspace test binary itself. Remove once
	 * root-caused. */
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

void show_regs(struct pt_regs *regs)
{
	pr_emerg("MyEmulator2 registers: pc=%08lx sr=%08lx sp=%08lx\n",
		regs->pc, regs->sr, regs->r[13]);
}
