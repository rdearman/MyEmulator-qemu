/* Tiny REM32 program for exercising remdbg's GDB command language. */
	.section .text
	.global _start
	.type _start,@function
_start:
	li r1, 1
	li r2, 2
	li r3, 3
	jal add3
	addi r5, r1, 0
	li r6, 7
	add r5, r5, r6
	addi r1, r5, 0
	li r5, 93              /* exit */
	addi r2, r1, 0
	addi r1, r5, 0
	syscall 0
1:
	j 1b

	.global add3
	.type add3,@function
add3:
	add r1, r1, r2
	add r1, r1, r3
	addi r1, r1, 29
	jr r14
