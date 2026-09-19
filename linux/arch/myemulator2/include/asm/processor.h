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
struct thread_struct { unsigned long sp; unsigned long pc; unsigned long tp; };
#define INIT_THREAD { .sp = 0, .pc = 0, .tp = 0 }
#define cpu_relax() barrier()
#define task_pt_regs(task) ((struct pt_regs *)(task_stack_page(task) + THREAD_SIZE) - 1)
extern void start_thread(struct pt_regs *, unsigned long, unsigned long);
extern unsigned long __get_wchan(struct task_struct *task);
#endif
#endif
