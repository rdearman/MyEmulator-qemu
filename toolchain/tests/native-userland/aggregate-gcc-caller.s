	.file	"aggregate-abi.c"
# GCC-generated REM caller fixture: every aggregate return uses hidden sret
# pointer in r1; aggregate argument pointer follows in r2.
	.text
	.p2align	2
	.global	main
	.type	main, @function
main:
	add r12, r15, r0
	add r15, sp, r0
	addi sp, sp, -4
	sw r5, 0(sp)
	subi sp, sp, 224
	sw lr, 64(sp)
	sw r12, 68(sp)
	li r1, 11
	sw r1, -36(r15)
	li r1, 22
	sw r1, -32(r15)
	subi r3, r15, 44
	lw r1, -36(r15)
	sw r1, -92(r15)
	lw r1, -32(r15)
	sw r1, -88(r15)
	subi r1, r15, 92
	add r2, r1, r0
	add r1, r3, r0
	jal small_copy
	li r1, 1410065408
	sw r1, -60(r15)
	li r1, 2
	sw r1, -56(r15)
	li r1, 7
	sw r1, -52(r15)
	li r1, 123
	sw r1, -48(r15)
	subi r5, r15, 76
	lw r1, -60(r15)
	sw r1, -108(r15)
	lw r1, -56(r15)
	sw r1, -104(r15)
	lw r1, -52(r15)
	sw r1, -100(r15)
	lw r1, -48(r15)
	sw r1, -96(r15)
	li r1, 4
	sw r1, 36(sp)
	subi r1, r15, 108
	sw r1, 32(sp)
	li r4, 3
	li r3, 2
	li r2, 1
	add r1, r5, r0
	jal large_copy
	lw r2, -32(r15)
	li r1, 22
	beq r2, r1, .Lmyemu_cbranch_skip52
	li r12, .L2
	jr r12
.Lmyemu_cbranch_skip52: 
	lw r2, -44(r15)
	li r1, 11
	beq r2, r1, .Lmyemu_cbranch_skip56
	li r12, .L2
	jr r12
.Lmyemu_cbranch_skip56: 
	lw r2, -40(r15)
	li r1, 23
	bne r2, r1, .Lmyemu_cbranch_skip60
	li r12, .L3
	jr r12
.Lmyemu_cbranch_skip60: 
.L2:
	li r1, 1
	j .L11
.L3:
	lw r2, -60(r15)
	lw r3, -56(r15)
	li r1, 1410065408
	beq r2, r1, .Lmyemu_cbranch_skip72
	li r12, .L5
	jr r12
.Lmyemu_cbranch_skip72: 
	li r1, 2
	beq r3, r1, .Lmyemu_cbranch_skip75
	li r12, .L5
	jr r12
.Lmyemu_cbranch_skip75: 
	lw r2, -48(r15)
	li r1, 123
	bne r2, r1, .Lmyemu_cbranch_skip79
	li r12, .L6
	jr r12
.Lmyemu_cbranch_skip79: 
.L5:
	li r1, 2
	j .L11
.L6:
	lw r2, -76(r15)
	lw r3, -72(r15)
	li r1, 1410065418
	beq r2, r1, .Lmyemu_cbranch_skip91
	li r12, .L7
	jr r12
.Lmyemu_cbranch_skip91: 
	li r1, 2
	beq r3, r1, .Lmyemu_cbranch_skip94
	li r12, .L7
	jr r12
.Lmyemu_cbranch_skip94: 
	lw r2, -68(r15)
	li r1, 7
	beq r2, r1, .Lmyemu_cbranch_skip98
	li r12, .L7
	jr r12
.Lmyemu_cbranch_skip98: 
	lw r2, -64(r15)
	li r1, 124
	bne r2, r1, .Lmyemu_cbranch_skip102
	li r12, .L8
	jr r12
.Lmyemu_cbranch_skip102: 
.L7:
	li r1, 3
	j .L11
.L8:
	subi r3, r15, 28
	lw r1, -36(r15)
	sw r1, -92(r15)
	lw r1, -32(r15)
	sw r1, -88(r15)
	subi r1, r15, 92
	add r2, r1, r0
	add r1, r3, r0
	jal small_copy
	lw r2, -24(r15)
	li r1, 23
	bne r2, r1, .Lmyemu_cbranch_skip121
	li r12, .L9
	jr r12
.Lmyemu_cbranch_skip121: 
	li r1, 4
	j .L11
.L9:
	subi r5, r15, 20
	lw r1, -60(r15)
	sw r1, -108(r15)
	lw r1, -56(r15)
	sw r1, -104(r15)
	lw r1, -52(r15)
	sw r1, -100(r15)
	lw r1, -48(r15)
	sw r1, -96(r15)
	li r1, 4
	sw r1, 36(sp)
	subi r1, r15, 108
	sw r1, 32(sp)
	li r4, 3
	li r3, 2
	li r2, 1
	add r1, r5, r0
	jal large_copy
	lw r2, -8(r15)
	li r1, 124
	bne r2, r1, .Lmyemu_cbranch_skip148
	li r12, .L10
	jr r12
.Lmyemu_cbranch_skip148: 
	li r1, 5
	j .L11
.L10:
	li r1, 0
.L11:
	add r12, r15, r0
	subi r12, r12, 164
	lw lr, (r12)
	lw r11, 4(r12)
	add r12, r15, r0
	subi r12, r12, 4
	lw r5, 0(r12)
	addi r12, r12, 4
	add sp, r15, r0
	add r15, r11, r0
	jr lr
	.size	main, .-main
	.ident	"GCC: (GNU) 15.2.0"
