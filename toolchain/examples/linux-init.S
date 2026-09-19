/* Minimal Linux 6.12 MyEmulator2 initramfs program.
 * Linux syscall ABI for the bring-up port: r1=syscall number, r2-r4 args. */
	.section .text
	.global _start
	.type _start,@function
_start:
	lui r3, %hi(message)
	ori r3, r3, %lo(message)
	li r1, 64              /* __NR_write */
	li r2, 1               /* stdout */
	li r4, message_end-message
	syscall 0
	li r1, 93              /* __NR_exit */
	li r2, 0
1:
	j 1b

	.section .rodata
message:
	.ascii "MyEmulator2 userspace /init reached\n"
message_end:
