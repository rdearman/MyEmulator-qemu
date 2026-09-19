/* MyEmulator2 Linux kernel context switch.
 *
 * Arguments are pointers to prev->thread and next->thread.  The function
 * saves the caller's resume address, stack, TP and ABI callee-saved registers
 * and returns through the saved resume address of the next task. */
#include <linux/linkage.h>

	.section .text,"ax"
	.align 2
	.globl myemulator2_switch_to
	.type myemulator2_switch_to,@function
myemulator2_switch_to:
	/* struct thread_struct offsets: sp=0, pc=4, tp=8,
	 * callee_saved[0..7]=12..40, frame=44. */
	sw r13, 0(r1)
	sw r14, 4(r1)
	mfsr r3, tp
	sw r3, 8(r1)
	sw r5, 12(r1)
	sw r6, 16(r1)
	sw r7, 20(r1)
	sw r8, 24(r1)
	sw r9, 28(r1)
	sw r10, 32(r1)
	sw r11, 36(r1)
	sw r12, 40(r1)
	sw r15, 44(r1)

	lw r13, 0(r2)
	lw r5, 12(r2)
	lw r6, 16(r2)
	lw r7, 20(r2)
	lw r8, 24(r2)
	lw r9, 28(r2)
	lw r10, 32(r2)
	lw r11, 36(r2)
	lw r12, 40(r2)
	lw r15, 44(r2)
	lw r3, 8(r2)
	mtsr tp, r3
	lw r14, 4(r2)
	jr r14
	.size myemulator2_switch_to, .-myemulator2_switch_to

	.globl ret_from_fork
	.type ret_from_fork,@function
ret_from_fork:
	/* r1 is the previous task, as required by schedule_tail(). */
	call schedule_tail
	call myemulator2_fork_entry

	/* User return: r1 points to the child's software pt_regs.  Build the
	 * CPU's native 16-byte frame at the current supervisor SP, restore the
	 * general registers, then RFE switches to the child's USP. */
	addi r15, r1, 0
	lw r3, 64(r15)
	sw r3, 0(r13)
	lw r3, 68(r15)
	sw r3, 4(r13)
	sw r0, 8(r13)
	sw r0, 12(r13)

	lw r3, 52(r15)
	mtsr usp, r3
	lw r2, 8(r15)
	lw r3, 12(r15)
	lw r4, 16(r15)
	lw r5, 20(r15)
	lw r6, 24(r15)
	lw r7, 28(r15)
	lw r8, 32(r15)
	lw r9, 36(r15)
	lw r10, 40(r15)
	lw r11, 44(r15)
	lw r12, 48(r15)
	lw r14, 56(r15)
	lw r1, 4(r15)
	lw r15, 60(r15)
	rfe
	.size ret_from_fork, .-ret_from_fork
