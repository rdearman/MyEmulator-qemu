// SPDX-License-Identifier: GPL-2.0
#include <linux/kernel.h>
#include <linux/sched.h>
#include <linux/sched/task_stack.h>
#include <linux/sched/debug.h>
#include <linux/elfcore.h>
#include <linux/uaccess.h>
#include <linux/sched/task_stack.h>
#include <asm/processor.h>

unsigned long init_stack[THREAD_SIZE / sizeof(unsigned long)]
	__aligned(THREAD_SIZE);

asmlinkage void ret_from_fork(void);

/* Keep pt_regs field accesses based on the pointer argument.  Folding
 * task_pt_regs(task) into a stack-base plus 8176-byte displacement is not
 * representable by the ISA's signed 12-bit load/store displacement. */
noinline __used void
myemulator2_init_user_regs(struct pt_regs *regs, unsigned long pc,
				   unsigned long usp)
{
	regs->pc = pc;
	regs->r[13] = usp;
	regs->sr = 0;
	regs->cause = 0;
	regs->info = 0;
}

void arch_cpu_idle(void)
{
	for (;;) {
		asm volatile("halt" ::: "memory");
	}
}

void machine_restart(char *cmd)
{
	pr_emerg("MyEmulator2 restart requested\n");
	for (;;)
		asm volatile("halt" ::: "memory");
}

void machine_halt(void)
{
	for (;;)
		asm volatile("halt" ::: "memory");
}

void machine_power_off(void)
{
	machine_halt();
}

void flush_thread(void) { }

unsigned long __get_wchan(struct task_struct *task)
{
	(void)task;
	return 0;
}

void show_stack(struct task_struct *task, unsigned long *sp, const char *loglvl)
{
	(void)task;
	(void)sp;
	(void)loglvl;
}

void start_thread(struct pt_regs *regs, unsigned long pc, unsigned long usp)
{
	memset(regs, 0, sizeof(*regs));
	regs->pc = pc;
	regs->r[13] = usp;
	/* start_thread prepares a User-mode image; ret_from_fork will place
	 * this SR in the native exception frame consumed by RFE. */
	regs->sr = 0;
	/* kernel_execve runs in a kernel thread.  Keep the new image in task
	 * state as well as the transient pt_regs slot: scheduling or the return
	 * from the kernel-thread entry function must not restore the old image. */
	current->thread.exec_pc = pc;
	current->thread.exec_sp = usp;
	current->thread.exec_pending = 1;
}

asmlinkage struct pt_regs *myemulator2_fork_entry(void)
{
	struct task_struct *task = current;
	int (*fn)(void *) = (void *)task->thread.fn;
	/* TP is a real architectural special register, not a GPR. */
	asm volatile("mtsr tp, %0" :: "r"(task->thread.tp) : "memory");

	if (fn) {
		int ret = fn((void *)task->thread.fn_arg);

		/* kernel_execve() is allowed to return after replacing the
		 * current task's saved image with a user image.  In that case
		 * ret_from_fork must perform the user RFE path instead of
		 * terminating the task as an ordinary kernel thread. */
		if (task->thread.exec_pending) {
			struct pt_regs *regs = task_pt_regs(task);
			myemulator2_init_user_regs(regs, task->thread.exec_pc,
						   task->thread.exec_sp);
			task->thread.exec_pending = 0;
			return regs;
		}
		if (!user_mode(task_pt_regs(task)))
			do_exit(ret);
	}

	return task_pt_regs(task);
}

/* Write the native exception frame from C immediately before the assembly
 * return path.  This keeps the frame construction tied to the active SSP;
 * the returned pt_regs image is still the source for general registers. */
asmlinkage void myemulator2_prepare_user_frame(struct pt_regs *regs,
						unsigned long frame)
{
	((u32 *)frame)[0] = regs->pc;
	((u32 *)frame)[1] = regs->sr;
	((u32 *)frame)[2] = 0;
	((u32 *)frame)[3] = 0;
}

int elf_core_copy_task_fpregs(struct task_struct *task, elf_fpregset_t *fpu)
{
	(void)task;
	(void)fpu;
	return 0;
}

int copy_thread(struct task_struct *p, const struct kernel_clone_args *args)
{
	struct pt_regs *childregs = task_pt_regs(p);
	unsigned long *child_frame = (unsigned long *)childregs - 4;

	memset(&p->thread, 0, sizeof(p->thread));
	p->thread.sp = (unsigned long)child_frame;
	p->thread.pc = (unsigned long)ret_from_fork;

	if (unlikely(args->fn)) {
		/* Kernel threads start in ret_from_fork and never use a user
		 * exception frame unless they later exec a user program. */
		memset(childregs, 0, sizeof(*childregs));
		childregs->sr = (1u << 5);
		p->thread.fn = (unsigned long)args->fn;
		p->thread.fn_arg = (unsigned long)args->fn_arg;
		return 0;
	}

	/* A userspace child inherits the parent's software register image. */
	*childregs = *current_pt_regs();
	childregs->r[1] = 0;
	if (args->stack)
		childregs->r[13] = args->stack;
	if (args->flags & CLONE_SETTLS)
		p->thread.tp = args->tls;
	else
		p->thread.tp = current->thread.tp;
	return 0;
}
