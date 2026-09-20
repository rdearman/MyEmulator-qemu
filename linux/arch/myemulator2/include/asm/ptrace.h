#ifndef _ASM_MYEMULATOR2_PTRACE_H
#define _ASM_MYEMULATOR2_PTRACE_H
#include <uapi/asm/ptrace.h>
#ifndef __ASSEMBLY__
struct pt_regs;
struct pt_regs *myemulator2_current_pt_regs(void);
#define current_pt_regs() myemulator2_current_pt_regs()
struct pt_regs {
	unsigned long r[16];
	unsigned long pc;
	unsigned long sr;
	unsigned long cause;
	unsigned long info;
};
#define user_mode(regs) (!((regs)->sr & (1u << 5)))
#define instruction_pointer(regs) ((regs)->pc)
#define user_stack_pointer(regs) ((regs)->r[13])
#define profile_pc(regs) instruction_pointer(regs)
#endif
#endif
