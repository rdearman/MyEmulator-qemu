/* The CPU has already built the 16-byte frame at SSP.  The save area is
 * below that frame; overwriting SSP would destroy the state RFE consumes.
 * The original r15 is saved before it is used as a frame pointer. */
#include <linux/linkage.h>

	.section .text,"ax"
	.align 2
	.globl myemulator2_exception_entry
	.type myemulator2_exception_entry,@function
myemulator2_exception_entry:
	sw r15, -4(sp)
	addi r15, sp, 0
	addi sp, sp, -64
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
	sw r15, 52(sp)
	sw r14, 56(sp)
	addi r1, r15, 0
	call myemulator2_exception_dispatch
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
	addi sp, sp, 64
	rfe
	.size myemulator2_exception_entry, .-myemulator2_exception_entry
