#ifndef _ASM_MYEMULATOR2_THREAD_INFO_H
#define _ASM_MYEMULATOR2_THREAD_INFO_H
#define THREAD_SIZE_ORDER 1
#include <asm/page.h>
#define THREAD_SIZE (PAGE_SIZE << THREAD_SIZE_ORDER)
#define TIF_NOTIFY_SIGNAL 0
#define TIF_SYSCALL_TRACE 0
#define TIF_NEED_RESCHED 1
#define TIF_SIGPENDING 2
#define TIF_MEMDIE 3
#define _TIF_NEED_RESCHED (1 << TIF_NEED_RESCHED)
#define _TIF_WORK_MASK (_TIF_NEED_RESCHED | (1 << TIF_SIGPENDING))
#ifndef __ASSEMBLY__
struct thread_info {
	unsigned long flags;
	int preempt_count;
	struct task_struct *task;
	struct pt_regs *regs;
};
#define INIT_THREAD_INFO(tsk) { .flags = 0, .preempt_count = INIT_PREEMPT_COUNT, .task = &tsk, .regs = NULL }
static inline struct thread_info *current_thread_info(void)
{
	register unsigned long sp asm("sp");
	return (struct thread_info *)(sp & ~(THREAD_SIZE - 1));
}
#endif
#endif
