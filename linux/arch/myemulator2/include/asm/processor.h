#ifndef _ASM_MYEMULATOR2_PROCESSOR_H
#define _ASM_MYEMULATOR2_PROCESSOR_H
#include <asm/ptrace.h>
#define STACK_TOP TASK_SIZE
#define STACK_TOP_MAX STACK_TOP
#define THREAD_SIZE_ORDER 1
#define THREAD_SIZE (PAGE_SIZE << THREAD_SIZE_ORDER)
#define KSTK_ESP(task) (task_pt_regs(task)->r[13])
#define KSTK_EIP(task) (task_pt_regs(task)->pc)
#ifndef __ASSEMBLY__
/* State which is not part of a task's exception frame.  The switch
 * backend saves the ABI callee-saved registers (r5-r12), the kernel
 * stack, the resume PC and the architectural thread pointer here. */
struct thread_struct {
	unsigned long sp;
	unsigned long pc;
	unsigned long tp;
	unsigned long callee_saved[8];
	/* GCC uses r15 as its fixed frame-base register.  It is not an
	 * allocatable ABI register, but a scheduler context switch must still
	 * preserve it. */
	unsigned long frame;
	unsigned long fn;
	unsigned long fn_arg;
};
#define INIT_THREAD { 0 }
#define cpu_relax() barrier()
#define task_pt_regs(task) ((struct pt_regs *)(task_stack_page(task) + THREAD_SIZE) - 1)
extern void start_thread(struct pt_regs *, unsigned long, unsigned long);
extern unsigned long __get_wchan(struct task_struct *task);
#endif
#endif
