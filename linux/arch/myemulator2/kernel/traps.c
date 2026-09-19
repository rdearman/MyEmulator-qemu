// SPDX-License-Identifier: GPL-2.0
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/ptrace.h>
#include <linux/sched.h>
#include <asm/ptrace.h>

void myemulator2_exception_dispatch(void)
{
	pr_emerg("MyEmulator2 exception\n");
	panic("unhandled MyEmulator2 exception");
}

void show_regs(struct pt_regs *regs)
{
	pr_emerg("MyEmulator2 registers: pc=%08lx sr=%08lx sp=%08lx\n",
		regs->pc, regs->sr, regs->r[13]);
}
