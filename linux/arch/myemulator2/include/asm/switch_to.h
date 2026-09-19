#ifndef _ASM_MYEMULATOR2_SWITCH_TO_H
#define _ASM_MYEMULATOR2_SWITCH_TO_H
#include <asm/processor.h>
extern void myemulator2_switch_to(struct thread_struct *prev,
		struct thread_struct *next);

#define switch_to(prev, next, last) do { \
	myemulator2_switch_to(&(prev)->thread, &(next)->thread); \
	(last) = (prev); \
} while (0)
#endif
