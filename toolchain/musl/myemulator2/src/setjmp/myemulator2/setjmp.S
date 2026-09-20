	.section .text
	.global __setjmp
	.global _setjmp
	.global setjmp
	.type __setjmp,@function
	.type _setjmp,@function
	.type setjmp,@function
__setjmp:
_setjmp:
setjmp:
	sw r5, 0(r1)
	sw r6, 4(r1)
	sw r7, 8(r1)
	sw r8, 12(r1)
	sw r9, 16(r1)
	sw r10, 20(r1)
	sw r11, 24(r1)
	sw r12, 28(r1)
	sw r13, 32(r1)
	sw r14, 36(r1)
	sw r15, 40(r1)
	mfsr r3, tp
	sw r3, 44(r1)
	addi r1, r0, 0
	jr r14
