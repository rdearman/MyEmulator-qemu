// SPDX-License-Identifier: GPL-2.0
#include <linux/elf.h>
#include <linux/ptrace.h>
#include <linux/regset.h>
#include <linux/sched.h>
#include <linux/sched/task_stack.h>
#include <linux/uaccess.h>
#include <asm/ptrace.h>
#include <asm/elf.h>

static int myemu_regs_get(struct task_struct *target,
		const struct user_regset *regset, struct membuf to)
{
	return membuf_write(&to, task_pt_regs(target), sizeof(struct pt_regs));
}

static int myemu_regs_set(struct task_struct *target,
		const struct user_regset *regset, unsigned int pos,
		unsigned int count, const void *kbuf, const void __user *ubuf)
{
	return user_regset_copyin(&pos, &count, &kbuf, &ubuf,
		 task_pt_regs(target), 0, sizeof(struct pt_regs));
}

static const struct user_regset myemu_regsets[] = {
	{
		.core_note_type = NT_PRSTATUS,
		.n = sizeof(struct pt_regs) / sizeof(unsigned long),
		.size = sizeof(unsigned long),
		.align = sizeof(unsigned long),
		.regset_get = myemu_regs_get,
		.set = myemu_regs_set,
	},
};

static const struct user_regset_view myemu_user_view = {
	.name = "myemulator2",
	.e_machine = ELF_ARCH,
	.regsets = myemu_regsets,
	.n = ARRAY_SIZE(myemu_regsets),
};

const struct user_regset_view *task_user_regset_view(struct task_struct *task)
{
	return &myemu_user_view;
}

void ptrace_disable(struct task_struct *child)
{
	(void)child;
}

long arch_ptrace(struct task_struct *child, long request,
		unsigned long addr, unsigned long data)
{
	return ptrace_request(child, request, addr, data);
}
