// SPDX-License-Identifier: GPL-2.0
#include <linux/kernel.h>
#include <linux/sched.h>
#include <linux/sched/task_stack.h>
#include <linux/uaccess.h>
#include <asm/processor.h>

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

int copy_thread(struct task_struct *p, const struct kernel_clone_args *args)
{
	return -ENOSYS;
}
