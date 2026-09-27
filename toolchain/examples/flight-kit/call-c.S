	.text
	.global _start
_start:
	jal answer
	/* exit_group(0 if answer() was 42, otherwise 1) */
	li r2, 1
	li r3, 42
	bne r1, r3, 1f
	li r2, 0
1:
	li r1, 94
	syscall 0
