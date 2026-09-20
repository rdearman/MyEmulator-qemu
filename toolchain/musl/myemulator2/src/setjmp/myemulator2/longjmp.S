	.section .text
	.global __longjmp
	.global _longjmp
	.global longjmp
	.type __longjmp,@function
	.type _longjmp,@function
	.type longjmp,@function
__longjmp:
_longjmp:
longjmp:
	lw r5, 0(r1)
	lw r6, 4(r1)
	lw r7, 8(r1)
	lw r8, 12(r1)
	lw r9, 16(r1)
	lw r10, 20(r1)
	lw r11, 24(r1)
	lw r12, 28(r1)
	lw r13, 32(r1)
	lw r14, 36(r1)
	lw r15, 40(r1)
	mtsr tp, r15
	seq r1, r2, r0
	add r1, r1, r2
	jr r14
