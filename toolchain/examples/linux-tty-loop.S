/* Exercise the real Linux serial-core/TTY path with twenty canonical reads.
 * Each input line is echoed back before the next read begins. */
	.section .text
	.global _start
_start:
	li r7, 20
	li r1, 64
	li r2, 1
	lui r3, %hi(banner)
	ori r3, r3, %lo(banner)
	li r4, banner_end-banner
	syscall 0
	li r1, 56
	li r2, -100
	lui r3, %hi(path)
	ori r3, r3, %lo(path)
	li r4, 2
	li r5, 0
	syscall 0
	add r8, r1, r0
	blt r8, r0, failed

1:
	li r1, 63
	add r2, r8, r0
	lui r3, %hi(buffer)
	ori r3, r3, %lo(buffer)
	li r4, 64
	syscall 0
	blt r1, r0, failed
	add r6, r1, r0
	li r1, 64
	li r2, 1
	lui r3, %hi(buffer)
	ori r3, r3, %lo(buffer)
	add r4, r6, r0
	syscall 0
	subi r7, r7, 1
	bne r7, r0, 1b

	li r1, 94
	li r2, 0
	syscall 0
	halt

failed:
	li r1, 64
	li r2, 1
	lui r3, %hi(failure)
	ori r3, r3, %lo(failure)
	li r4, failure_end-failure
	syscall 0
	li r1, 94
	li r2, 1
	syscall 0
	halt

	.section .rodata
path:
	.asciz "/dev/ttyMY0"
banner:
	.ascii "TTY loop ready\r\n"
banner_end:
failure:
	.ascii "TTY loop FAILED\r\n"
failure_end:
	.section .bss
	.align 2
buffer:
	.space 64
