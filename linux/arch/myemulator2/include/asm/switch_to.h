#ifndef _ASM_MYEMULATOR2_SWITCH_TO_H
#define _ASM_MYEMULATOR2_SWITCH_TO_H
#include <asm/processor.h>
static inline void myemulator2_switch_to(struct task_struct *prev,
		struct task_struct *next)
{
	if (next->thread.sp)
		asm volatile("addi sp, %0, 0" :: "r"(next->thread.sp) : "memory");
}
#define switch_to(prev, next, last) do { myemulator2_switch_to(prev, next); (last) = (prev); } while (0)
#endif
