// SPDX-License-Identifier: GPL-2.0
#include <linux/kernel.h>
#include <linux/sched.h>
#include <linux/sched/task_stack.h>
#include <linux/sched/debug.h>
#include <linux/elfcore.h>
#include <linux/uaccess.h>
#include <asm/processor.h>

unsigned long init_stack[THREAD_SIZE / sizeof(unsigned long)]
	__aligned(THREAD_SIZE);

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
	regs->sr = (1u << 5);
}

int elf_core_copy_task_fpregs(struct task_struct *task, elf_fpregset_t *fpu)
{
	(void)task;
	(void)fpu;
	return 0;
}

int copy_thread(struct task_struct *p, const struct kernel_clone_args *args)
{
	return -ENOSYS;
}
