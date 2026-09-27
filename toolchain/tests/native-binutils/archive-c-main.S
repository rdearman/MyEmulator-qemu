	.text
	.global _start
	.type _start,@function
	.extern archive_result
_start:
	call archive_result
	li r2, 42
	bne r1, r2, .Lbad
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
	.section .rodata
ok:
	.ascii "C_ARCHIVE_OK\n"
ok_end:
bad:
	.ascii "C_ARCHIVE_BAD\n"
bad_end:
