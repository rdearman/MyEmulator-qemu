#ifndef _ASM_MYEMULATOR2_SWITCH_TO_H
#define _ASM_MYEMULATOR2_SWITCH_TO_H
#include <asm/processor.h>
#include <linux/printk.h>
extern void myemulator2_switch_to(struct thread_struct *prev,
		struct thread_struct *next, struct task_struct *prev_task);

#define switch_to(prev, next, last) do { \
	if ((prev)->pid == 1 || (next)->pid == 1 || (next)->pid >= 20) \
		pr_emerg("MYEMU_TTYTRACE switch prev=%d next=%d\n", \
			 (prev)->pid, (next)->pid); \
	myemulator2_switch_to(&(prev)->thread, &(next)->thread, (prev)); \
	(last) = (prev); \
} while (0)
#endif
