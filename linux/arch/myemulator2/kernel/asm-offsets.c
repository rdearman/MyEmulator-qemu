// SPDX-License-Identifier: GPL-2.0
#include <linux/const.h>
#include <linux/kbuild.h>
#include <asm/ptrace.h>

void foo(void)
{
	DEFINE(PT_REGS_PC, offsetof(struct pt_regs, pc));
	DEFINE(PT_REGS_SR, offsetof(struct pt_regs, sr));
	DEFINE(PT_REGS_SIZE, sizeof(struct pt_regs));
}
