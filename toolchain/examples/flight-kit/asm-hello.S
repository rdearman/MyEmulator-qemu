	.text
	.global _start
_start:
	li r1, 64
	li r2, 1
	lui r3, %hi(message)
	ori r3, r3, %lo(message)
	li r4, message_end-message
	syscall 0
	li r1, 94
	li r2, 0
	syscall 0
	.section .rodata
message:
	.ascii "Hello from REM assembly\n"
message_end:
