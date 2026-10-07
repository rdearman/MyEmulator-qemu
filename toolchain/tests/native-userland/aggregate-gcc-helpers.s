	.file	"aggregate-abi.c"
	.text
	.p2align	2
	.global	small_copy
	.type	small_copy, @function
small_copy:
	add r12, r15, r0
	add r15, sp, r0
	addi sp, sp, -4
	sw r5, 0(sp)
	subi sp, sp, 96
	sw lr, 32(sp)
	sw r12, 36(sp)
	sw r1, 16(r15)
	add r5, r2, r0
	lw r1, (r5)
	sw r1, -12(r15)
	lw r1, 4(r5)
	sw r1, -8(r15)
	lw r1, -8(r15)
	addi r1, r1, 1
	sw r1, -8(r15)
	lw r1, 16(r15)
	lw r2, -12(r15)
	sw r2, (r1)
	lw r2, -8(r15)
	sw r2, 4(r1)
	lw r1, 16(r15)
	add r12, r15, r0
	subi r12, r12, 68
	lw lr, (r12)
	lw r11, 4(r12)
	add r12, r15, r0
	subi r12, r12, 4
	lw r5, 0(r12)
	addi r12, r12, 4
	add sp, r15, r0
	add r15, r11, r0
	jr lr
	.size	small_copy, .-small_copy
	.p2align	2
	.global	large_copy
	.type	large_copy, @function
large_copy:
	add r12, r15, r0
	add r15, sp, r0
	addi sp, sp, -4
	sw r5, 0(sp)
	addi sp, sp, -4
	sw r6, 0(sp)
	addi sp, sp, -4
	sw r7, 0(sp)
	addi sp, sp, -4
	sw r8, 0(sp)
	addi sp, sp, -4
	sw r9, 0(sp)
	subi sp, sp, 80
	sw lr, 32(sp)
	sw r12, 36(sp)
	sw r1, 16(r15)
	sw r2, 20(r15)
	sw r3, 24(r15)
	sw r4, 28(r15)
	lw r5, 32(r15)
	lw r6, (r5)
	lw r7, 4(r5)
	lw r2, 20(r15)
	lw r1, 24(r15)
	add r2, r2, r1
	lw r1, 28(r15)
	add r2, r2, r1
	lw r1, 36(r15)
	add r1, r2, r1
	add r8, r1, r0
	srai r1, r1, 31
	add r9, r1, r0
	add r2, r6, r8
	sltu r1, r2, r6
	add r3, r7, r9
	add r1, r1, r3
	add r3, r1, r0
	add r6, r2, r0
	add r7, r3, r0
	sw r6, (r5)
	sw r7, 4(r5)
	lw r1, 12(r5)
	addi r1, r1, 1
	sw r1, 12(r5)
	lw r1, 16(r15)
	lw r2, (r5)
	sw r2, (r1)
	lw r2, 4(r5)
	sw r2, 4(r1)
	lw r2, 8(r5)
	sw r2, 8(r1)
	lw r2, 12(r5)
	sw r2, 12(r1)
	lw r1, 16(r15)
	add r12, r15, r0
	subi r12, r12, 68
	lw lr, (r12)
	lw r11, 4(r12)
	add r12, r15, r0
	subi r12, r12, 20
	lw r9, 0(r12)
	addi r12, r12, 4
	lw r8, 0(r12)
	addi r12, r12, 4
	lw r7, 0(r12)
	addi r12, r12, 4
	lw r6, 0(r12)
	addi r12, r12, 4
	lw r5, 0(r12)
	addi r12, r12, 4
	add sp, r15, r0
	add r15, r11, r0
	jr lr
	.size	large_copy, .-large_copy
	.ident	"GCC: (GNU) 15.2.0"
