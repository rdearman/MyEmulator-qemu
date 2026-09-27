#ifndef _ASM_MYEMULATOR2_PTRACE_H
#define _ASM_MYEMULATOR2_PTRACE_H
#include <uapi/asm/ptrace.h>
#ifndef __ASSEMBLY__
struct task_struct;
struct pt_regs;
struct pt_regs *myemulator2_current_pt_regs(void);
void myemulator2_debug_trap(struct pt_regs *regs, int *code);
#define current_pt_regs() myemulator2_current_pt_regs()
struct pt_regs {
	unsigned long r[16];
	unsigned long pc;
	unsigned long sr;
	unsigned long cause;
	unsigned long info;
	/* Syscall number captured before r1 is replaced by its result. */
	long orig_r1;
};
#define user_mode(regs) (!((regs)->sr & (1u << 5)))
#define instruction_pointer(regs) ((regs)->pc)
#define user_stack_pointer(regs) ((regs)->r[13])
#define profile_pc(regs) instruction_pointer(regs)
#define arch_has_single_step() 1
void user_enable_single_step(struct task_struct *task);
void user_disable_single_step(struct task_struct *task);
#endif
#endif
