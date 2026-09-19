/* MyEmulator2 reset entry.  QEMU supplies SSP and PC through the
 * architected physical reset words; this code only establishes the Linux
 * vector base and clears BSS before entering generic startup. */
#include <linux/linkage.h>

	.section .text.head,"ax"
	.align 2
	.section .vectors,"ax"
	.align 10
	.globl __vectors_start
__vectors_start:
	.rept 256
	.long myemulator2_exception_entry
	.endr

	.section .text.head,"ax"
	.align 2
	.globl _start
	.type _start,@function
_start:
	/* The reset stack is only a machine boot stack.  Linux's current()
	 * implementation derives the initial task from init_thread_union, so
	 * establish the top of the linked init_stack before entering C. */
	li r13, __end_init_stack
	li r1, __vectors_start
	mtsr vbr, r1

	li r1, __bss_start
	li r2, __bss_end
	beq r1, r2, 2f
1:
	sw r0, 0(r1)
	addi r1, r1, 4
	bne r1, r2, 1b
2:
	call myemulator2_start_kernel
3:
	halt
	j 3b
	.size _start, .-_start
