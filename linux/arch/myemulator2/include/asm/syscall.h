#ifndef _ASM_MYEMULATOR2_SYSCALL_H
#define _ASM_MYEMULATOR2_SYSCALL_H
#include <linux/sched.h>
#include <asm/ptrace.h>
static inline int syscall_get_nr(struct task_struct *task, struct pt_regs *regs) { return regs->r[1]; }
static inline void syscall_rollback(struct task_struct *task, struct pt_regs *regs) { }
static inline long syscall_get_return_value(struct task_struct *task, struct pt_regs *regs) { return regs->r[1]; }
static inline long syscall_get_error(struct task_struct *task, struct pt_regs *regs) { return regs->r[1] < 0 ? regs->r[1] : 0; }
static inline void syscall_set_return_value(struct task_struct *task, struct pt_regs *regs, int error, long val) { regs->r[1] = error ? error : val; }
static inline void syscall_get_arguments(struct task_struct *task, struct pt_regs *regs, unsigned long *args) { for (int i = 0; i < 6; i++) args[i] = regs->r[i + 2]; }
static inline int syscall_get_arch(struct task_struct *task) { return 0; }
#endif
