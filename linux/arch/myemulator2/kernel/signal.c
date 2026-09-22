// SPDX-License-Identifier: GPL-2.0
#include <linux/errno.h>
#include <linux/kernel.h>
#include <linux/sched.h>
#include <linux/signal.h>
#include <linux/syscalls.h>
#include <linux/uaccess.h>
#include <linux/resume_user_mode.h>

#include <asm/ptrace.h>
#include <asm/unistd.h>
#include <asm/irqflags.h>
#include <uapi/asm/ucontext.h>

struct myemulator2_rt_sigframe {
	struct siginfo info;
	struct ucontext uc;
	/* addi r1,r0,__NR_rt_sigreturn; syscall 0 */
	__u32 retcode[2];
};

static int setup_sigcontext(struct pt_regs *regs,
				struct sigcontext __user *sc)
{
	struct user_regs_struct context;

	memset(&context, 0, sizeof(context));
	memcpy(context.r, regs->r, sizeof(context.r));
	context.pc = regs->pc;
	context.sr = regs->sr;
	context.cause = regs->cause;
	context.info = regs->info;
	return __copy_to_user(&sc->regs, &context, sizeof(context));
}

static int restore_sigcontext(struct pt_regs *regs,
				  struct sigcontext __user *sc)
{
	struct user_regs_struct context;

	current->restart_block.fn = do_no_restart_syscall;
	if (__copy_from_user(&context, &sc->regs, sizeof(context)))
		return -EFAULT;
	memcpy(regs->r, context.r, sizeof(regs->r));
	regs->pc = context.pc;
	/* User mode is always restored; supervisor mode cannot be forged. */
	regs->sr = context.sr & ~(1u << 5);
	regs->cause = context.cause;
	regs->info = context.info;
	regs->orig_r1 = -1;
	return 0;
}

asmlinkage long myemulator2_rt_sigreturn(struct pt_regs *regs)
{
	struct myemulator2_rt_sigframe __user *frame;
	sigset_t set;

	frame = (struct myemulator2_rt_sigframe __user *)regs->r[13];
	if (((unsigned long)frame & 3) || !access_ok(frame, sizeof(*frame)))
		goto badframe;
	if (__copy_from_user(&set, &frame->uc.uc_sigmask, sizeof(set)))
		goto badframe;
	set_current_blocked(&set);
	if (restore_sigcontext(regs, &frame->uc.uc_mcontext))
		goto badframe;
	if (restore_altstack(&frame->uc.uc_stack))
		goto badframe;
	return regs->r[1];

badframe:
	force_sig(SIGSEGV);
	return 0;
}

static inline void __user *get_sigframe(struct ksignal *ksig,
					struct pt_regs *regs, size_t size)
{
	unsigned long sp = sigsp(regs->r[13], ksig);

	sp = (sp - size) & ~3UL;
	if (!access_ok((void __user *)sp, size))
		return NULL;
	return (void __user *)sp;
}

static int setup_rt_frame(struct ksignal *ksig, sigset_t *set,
				  struct pt_regs *regs)
{
	struct myemulator2_rt_sigframe __user *frame;
	unsigned long return_pc;
	int err = 0;

	frame = get_sigframe(ksig, regs, sizeof(*frame));
	if (!frame)
		return -EFAULT;
	if (ksig->ka.sa.sa_flags & SA_SIGINFO)
		err |= copy_siginfo_to_user(&frame->info, &ksig->info);
	err |= __put_user(0UL, &frame->uc.uc_flags);
	err |= __put_user((struct ucontext *)0, &frame->uc.uc_link);
	err |= __save_altstack(&frame->uc.uc_stack, regs->r[13]);
	err |= setup_sigcontext(regs, &frame->uc.uc_mcontext);
	err |= __copy_to_user(&frame->uc.uc_sigmask, set, sizeof(*set));
	/* addi r1,r0,139 is 0x0420008b in little-endian memory. */
	err |= __put_user(0x0420008bU, &frame->retcode[0]);
	err |= __put_user(0x28000000U, &frame->retcode[1]);
	if (err)
		return -EFAULT;

	return_pc = (unsigned long)&frame->retcode[0];
	regs->pc = (unsigned long)ksig->ka.sa.sa_handler;
	regs->r[1] = ksig->sig;
	regs->r[2] = (unsigned long)&frame->info;
	regs->r[3] = (unsigned long)&frame->uc;
	/* r14 is this ISA's link register (see JAL/JALR codegen in
	 * translate.c); the handler's own "jr r14" return must land in the
	 * sigreturn trampoline, not wherever r14 pointed before the signal
	 * was delivered.  r4 is not part of the 3-argument handler ABI and
	 * was a leftover from a different architecture's convention. */
	regs->r[14] = return_pc;
	regs->r[13] = (unsigned long)frame;
	regs->orig_r1 = -1;
	return 0;
}

static void handle_signal(struct ksignal *ksig, struct pt_regs *regs)
{
	int ret = setup_rt_frame(ksig, sigmask_to_save(), regs);

	signal_setup_done(ret, ksig, 0);
}

void arch_do_signal_or_restart(struct pt_regs *regs)
{
	struct ksignal ksig;
	unsigned long continue_pc = regs->pc;
	unsigned long restart_pc = continue_pc - 4;
	long retval = regs->r[1];
	bool restart = false;

	/* Exception entry masks IRQs.  Keep them masked while the live
	 * exception frame is being rewritten; RFE restores the saved user SR
	 * (and therefore the user's interrupt level) atomically on return. */

	if (regs->orig_r1 >= 0) {
		switch (retval) {
		case -ERESTART_RESTARTBLOCK:
		case -ERESTARTNOHAND:
		case -ERESTARTSYS:
		case -ERESTARTNOINTR:
			restart = true;
			regs->pc = restart_pc;
			regs->r[1] = regs->orig_r1;
			break;
		}
	}

	if (get_signal(&ksig)) {
		if (restart && regs->pc == restart_pc &&
			(retval == -ERESTARTNOHAND ||
			 retval == -ERESTART_RESTARTBLOCK ||
			 (retval == -ERESTARTSYS &&
			  !(ksig.ka.sa.sa_flags & SA_RESTART)))) {
			regs->r[1] = -EINTR;
			regs->pc = continue_pc;
		}
		handle_signal(&ksig, regs);
	} else {
		restore_saved_sigmask();
		/* No user handler ran, so all restartable calls are retried. */
		if (restart && regs->pc == restart_pc)
			regs->orig_r1 = -1;
	}
}
