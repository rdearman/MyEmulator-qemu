// SPDX-License-Identifier: GPL-2.0
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/ptrace.h>
#include <linux/sched.h>
#include <asm/ptrace.h>

void myemulator2_exception_dispatch(unsigned long *frame)
{
	pr_emerg("MyEmulator2 exception: pc=%08lx sr=%08lx cause=%lu info=%08lx\n",
		frame[0], frame[1], frame[2], frame[3]);
	panic("unhandled MyEmulator2 exception");
}

void show_regs(struct pt_regs *regs)
{
	pr_emerg("MyEmulator2 registers: pc=%08lx sr=%08lx sp=%08lx\n",
		regs->pc, regs->sr, regs->r[13]);
}
