	.text
	.global _start
	.type _start,@function
	.extern value_a
	.extern value_b
_start:
	call value_a
	add r5, r1, r0
	call value_b
	add r1, r5, r1
	li r2, 42
	bne r1, r2, .Lchanged
	li r1, 64
	li r2, 1
	lui r3, %hi(version_one)
	ori r3, r3, %lo(version_one)
	li r4, version_one_end-version_one
	syscall 0
	li r1, 94
	li r2, 0
	syscall 0
.Lchanged:
	li r1, 64
	li r2, 1
	lui r3, %hi(version_two)
	ori r3, r3, %lo(version_two)
	li r4, version_two_end-version_two
	syscall 0
	li r1, 94
	li r2, 0
	syscall 0
	.section .rodata
version_one:
	.ascii "ARCHIVE_42\n"
version_one_end:
version_two:
	.ascii "ARCHIVE_43\n"
version_two_end:
