	.text
	.global bitcount32
	.type bitcount32,@function
bitcount32:
	li r2, 0
	li r3, 32
.Lbitcount_loop:
	andi r4, r1, 1
	add r2, r2, r4
	srli r1, r1, 1
	addi r3, r3, -1
	bne r3, r0, .Lbitcount_loop
	add r1, r2, r0
	ret
