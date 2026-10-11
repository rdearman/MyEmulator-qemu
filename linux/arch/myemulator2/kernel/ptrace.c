// SPDX-License-Identifier: GPL-2.0
#include <linux/elf.h>
#include <linux/errno.h>
#include <linux/mm.h>
#include <linux/ptrace.h>
#include <linux/printk.h>
#include <linux/regset.h>
#include <linux/sched.h>
#include <linux/sched/task_stack.h>
#include <linux/thread_info.h>
#include <linux/uaccess.h>
#include <asm/cacheflush.h>
#include <asm/ptrace.h>
#include <asm/elf.h>
#include <asm/processor.h>

#define MYEMU_BREAK_INSN 0x25000000u

static struct pt_regs *myemu_task_regs(struct task_struct *task)
{
	struct pt_regs *regs = task_thread_info(task)->regs;
	return regs ? regs : task_pt_regs(task);
}

static void regs_to_user(struct user_regs_struct *u, const struct pt_regs *r)
{
	int i;
	for (i = 0; i < 16; i++)
		u->r[i] = r->r[i];
	u->pc = r->pc;
	u->sr = r->sr & ~(1u << 5); /* never expose supervisor mode to userspace */
	u->cause = r->cause;
	u->info = r->info;
}

static void user_to_regs(struct pt_regs *r, const struct user_regs_struct *u)
{
	int i;
	for (i = 0; i < 16; i++)
		r->r[i] = u->r[i];
	r->pc = u->pc;
	r->sr = u->sr & ~(1u << 5);
	r->cause = u->cause;
	r->info = u->info;
	r->orig_r1 = -1;
}

static int myemu_regs_get(struct task_struct *target,
		const struct user_regset *regset, struct membuf to)
{
	struct user_regs_struct u;

	regs_to_user(&u, myemu_task_regs(target));
	return membuf_write(&to, &u, sizeof(u));
}

static int myemu_regs_set(struct task_struct *target,
		const struct user_regset *regset, unsigned int pos,
		unsigned int count, const void *kbuf, const void __user *ubuf)
{
	struct user_regs_struct u;
	int ret;

	regs_to_user(&u, myemu_task_regs(target));
	ret = user_regset_copyin(&pos, &count, &kbuf, &ubuf,
			 &u, 0, sizeof(u));
	if (ret)
		return ret;
	if (u.pc & 3)
		return -EINVAL;
	user_to_regs(myemu_task_regs(target), &u);
	return 0;
}

static const struct user_regset myemu_regsets[] = {
	{
		.core_note_type = NT_PRSTATUS,
		.n = sizeof(struct user_regs_struct) / sizeof(unsigned long),
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

static int debug_word(struct task_struct *task, unsigned long addr,
		      unsigned long *word, bool write)
{
	int n = ptrace_access_vm(task, addr, word, sizeof(*word),
				 FOLL_FORCE | (write ? FOLL_WRITE : 0));
	return n == sizeof(*word) ? 0 : -EIO;
}

static void cancel_step(struct task_struct *task)
{
	unsigned long insn;
	unsigned long addr = task->thread.debug_step_addr;

	if (task->thread.debug_step_pending &&
	    !debug_word(task, addr, &insn, false)) {
		insn = task->thread.debug_step_insn;
		if (!debug_word(task, addr, &insn, true))
			flush_icache_range(addr, addr + sizeof(unsigned long));
	}
	task->thread.debug_step_pending = 0;
	task->thread.debug_step_active = 0;
	task->thread.debug_step_link = 0;
}

void myemulator2_debug_trap(struct pt_regs *regs, int *code)
{
	if (current->thread.debug_step_pending &&
	    regs->pc == current->thread.debug_step_addr) {
		unsigned long insn = current->thread.debug_step_insn;
		unsigned long addr = current->thread.debug_step_addr;
		/* This handler runs inside the tracee.  ptrace_access_vm() is for
		 * access by a tracer and rejects this self-access; use the process VM
		 * accessor so the temporary BREAK can be removed from the live mm. */
		int write_ret = access_process_vm(current, addr, &insn, sizeof(insn),
						 FOLL_FORCE | FOLL_WRITE) == sizeof(insn) ?
			0 : -EIO;
		if (!write_ret)
			flush_icache_range(addr, addr + sizeof(unsigned long));
		/* A self-targeting JAL/JALR is intercepted at its own PC.  Reproduce
		 * its only other architectural effect (the link register) at the stop. */
		if (current->thread.debug_step_link)
			regs->r[14] = current->thread.debug_step_link_value;
		current->thread.debug_step_pending = 0;
		current->thread.debug_step_active = 0;
		current->thread.debug_step_link = 0;
		*code = TRAP_TRACE;
	} else {
		*code = TRAP_BRKPT;
	}
}

void ptrace_disable(struct task_struct *child)
{
	cancel_step(child);
}

void user_enable_single_step(struct task_struct *task)
{
	task->thread.debug_step_active = 1;
}

void user_disable_single_step(struct task_struct *task)
{
	cancel_step(task);
}

static int32_t sign_extend(unsigned int v, unsigned int bits)
{
	unsigned int sign = 1u << (bits - 1);
	return (int32_t)((v ^ sign) - sign);
}

static int prepare_single_step(struct task_struct *child)
{
	struct pt_regs *r = myemu_task_regs(child);
	unsigned long raw, next;
	unsigned int op, ra, rb, cond;
	int take, link_on_self = 0;
	int32_t disp;

	if (r->pc & 3)
		return -EIO;
	if (debug_word(child, r->pc, &raw, false))
		return -EIO;
	op = (raw >> 26) & 63;
	next = r->pc + 4;
	if (op == 4) {
		cond = (raw >> 23) & 7;
		ra = (raw >> 18) & 31;
		rb = (raw >> 13) & 31;
		if (cond > 5 || ra > 15 || rb > 15)
			return -EIO;
		switch (cond) {
		case 0: take = r->r[ra] == r->r[rb]; break;
		case 1: take = r->r[ra] != r->r[rb]; break;
		case 2: take = (long)r->r[ra] < (long)r->r[rb]; break;
		case 3: take = (long)r->r[ra] >= (long)r->r[rb]; break;
		case 4: take = r->r[ra] < r->r[rb]; break;
		default: take = r->r[ra] >= r->r[rb]; break;
		}
		if (take) {
			disp = sign_extend(raw & 0x1fff, 13);
			next += disp * 4;
		}
	} else if (op == 5 || op == 6) {
		disp = sign_extend(raw & 0x3ffffff, 26);
		next += disp * 4;
		if (op == 6)
			link_on_self = 1;
	} else if (op == 7) {
		ra = (raw >> 20) & 31;
		if (ra > 15 || (raw & 0xfffff))
			return -EIO;
		next = r->r[ra];
		link_on_self = (raw >> 25) & 1;
	} else if (op == 9 && ((raw >> 22) & 15) == 3) {
		return -EIO; /* HALT has no sequential successor. */
	}
	/* A branch back to itself has no architectural side effect unless it
	 * links.  Planting the temporary BREAK at the current PC therefore gives
	 * the debugger a single-step stop without executing a second instruction.
	 * For a self-targeting JAL/JALR, the trap handler reproduces the link
	 * register update before exposing the stop. */
	if ((next & 3) ||
	    (next == r->pc && !(op == 4 || op == 5 ||
			       op == 6 || op == 7)))
		return -EIO;
	if (child->thread.debug_step_pending)
		cancel_step(child);
	if (debug_word(child, next, &raw, false))
		return -EIO;
	child->thread.debug_step_addr = next;
	child->thread.debug_step_insn = raw;
	child->thread.debug_step_pending = 1;
	child->thread.debug_step_link = next == r->pc && link_on_self;
	child->thread.debug_step_link_value = r->pc + 4;
	raw = MYEMU_BREAK_INSN;
	if (debug_word(child, next, &raw, true)) {
		child->thread.debug_step_pending = 0;
		return -EIO;
	}
	flush_icache_range(next, next + sizeof(unsigned long));
	return 0;
}

long arch_ptrace(struct task_struct *child, long request,
		unsigned long addr, unsigned long data)
{
	if (request == PTRACE_GETREGS || request == PTRACE_SETREGS) {
		struct user_regs_struct u;
		struct pt_regs *r = myemu_task_regs(child);

		if (request == PTRACE_GETREGS) {
			regs_to_user(&u, r);
			return copy_to_user((void __user *)data, &u, sizeof(u)) ?
				-EFAULT : 0;
		}
		if (copy_from_user(&u, (void __user *)data, sizeof(u)))
			return -EFAULT;
		if (u.pc & 3)
			return -EINVAL;
		user_to_regs(r, &u);
		return 0;
	}
	if (request == PTRACE_SINGLESTEP) {
		int ret = prepare_single_step(child);
		if (ret)
			return ret;
		ret = ptrace_request(child, request, addr, data);
		if (ret)
			cancel_step(child);
		return ret;
	}
	if (request == PTRACE_CONT || request == PTRACE_SYSCALL ||
	    request == PTRACE_DETACH)
		cancel_step(child);
	{
		long ret = ptrace_request(child, request, addr, data);

		/* access_process_vm() can update executable guest memory through a
		 * kernel mapping.  That bypasses the emulator's translated-store
		 * invalidation, so explicitly invalidate code after successful
		 * POKETEXT/POKEDATA requests. */
		if (!ret && (request == PTRACE_POKETEXT ||
			     request == PTRACE_POKEDATA)) {
			flush_icache_range(addr, addr + sizeof(unsigned long));
		}

		return ret;
	}
}
