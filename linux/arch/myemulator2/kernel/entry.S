/* The CPU has already built the 16-byte frame at SSP.  Linux presents a
 * complete software pt_regs image to C below that native frame.  Keeping the
 * native frame intact is essential: RFE consumes it after C has returned. */
#include <linux/linkage.h>

	.section .text,"ax"
	.align 2
	.globl myemulator2_exception_entry
	.type myemulator2_exception_entry,@function
myemulator2_exception_entry:
	sw r15, -4(sp)
	addi r15, sp, 0
	/* 84 bytes of pt_regs plus 12 bytes padding keeps the C stack aligned. */
	addi sp, sp, -96
	sw r0, 0(sp)
	sw r1, 4(sp)
	sw r2, 8(sp)
	sw r3, 12(sp)
	sw r4, 16(sp)
	sw r5, 20(sp)
	sw r6, 24(sp)
	sw r7, 28(sp)
	sw r8, 32(sp)
	sw r9, 36(sp)
	sw r10, 40(sp)
	sw r11, 44(sp)
	sw r12, 48(sp)
	mfsr r3, usp
	sw r3, 52(sp)
	lw r3, -4(r15)
	sw r3, 60(sp)
	sw r14, 56(sp)
	lw r3, 0(r15)
	sw r3, 64(sp)
	lw r3, 4(r15)
	sw r3, 68(sp)
	lw r3, 8(r15)
	sw r3, 72(sp)
	lw r3, 12(r15)
	sw r3, 76(sp)
	addi r1, sp, 0
	call myemulator2_exception_dispatch
	/* Reconstruct the native-frame pointer; C is allowed to use r15. */
	addi r15, sp, 96
	lw r3, 64(sp)
	sw r3, 0(r15)
	lw r3, 68(sp)
	sw r3, 4(r15)
	/* The saved user stack is restored by RFE selecting USP. */
	lw r3, 52(sp)
	mtsr usp, r3
	lw r0, 0(sp)
	lw r1, 4(sp)
	lw r2, 8(sp)
	lw r3, 12(sp)
	lw r4, 16(sp)
	lw r5, 20(sp)
	lw r6, 24(sp)
	lw r7, 28(sp)
	lw r8, 32(sp)
	lw r9, 36(sp)
	lw r10, 40(sp)
	lw r11, 44(sp)
	lw r12, 48(sp)
	lw r14, 56(sp)
	lw r15, 60(sp)
	addi sp, sp, 96
	rfe
	.size myemulator2_exception_entry, .-myemulator2_exception_entry
