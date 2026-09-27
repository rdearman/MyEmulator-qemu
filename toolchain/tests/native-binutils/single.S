	.text
	.global _start
	.type _start,@function
_start:
	/* Local branch and local call; verify initialized data and BSS. */	
	call .Lanswer
	li r2, 42
	bne r1, r2, .Lbad
	lui r6, %hi(values)
	ori r6, r6, %lo(values)
	lbu r7, 0(r6)
	li r2, 7
	bne r7, r2, .Lbad
	lw r7, 4(r6)
	li r2, 42
	bne r7, r2, .Lbad
	lui r6, %hi(scratch)
	ori r6, r6, %lo(scratch)
	li r7, 0x13579
	sw r7, 0(r6)
	lw r8, 0(r6)
	bne r7, r8, .Lbad
	li r1, 64
	li r2, 1
	lui r3, %hi(ok)
	ori r3, r3, %lo(ok)
	li r4, ok_end-ok
	syscall 0
	li r1, 94
	li r2, 0
	syscall 0
.Lbad:
	li r1, 64
	li r2, 1
	lui r3, %hi(bad)
	ori r3, r3, %lo(bad)
	li r4, bad_end-bad
	syscall 0
	li r1, 94
	li r2, 1
	syscall 0
.Lanswer:
	li r1, 42
	ret
	.size _start, .-_start

	.data
	.global values
values:
	.byte 7
	.balign 2
	.short 0x1234
	.balign 4
	.long 42

	.bss
	.balign 4
	.global scratch
scratch:
	.space 4

	.section .rodata
ok:
	.ascii "SINGLE_OK\n"
ok_end:
bad:
	.ascii "SINGLE_BAD\n"
bad_end:
