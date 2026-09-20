	.section .text
	.global _start
	.type _start,@function
_start:
	call main
	add r2, r1, r0
	li r1, 94
	syscall 0
1:
	j 1b
