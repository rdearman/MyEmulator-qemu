/* Minimal Linux syscall wrappers for the MyEmulator2 six-register syscall ABI.
 * r1 contains the syscall number and result; r2-r7 contain arguments. */
	.section .text
	.align 2
	.global myemu_read
	.type myemu_read,@function
myemu_read:
	add r4, r3, r0
	add r3, r2, r0
	add r2, r1, r0
	li r1, 63
	syscall 0
	ret

	.global myemu_write
	.type myemu_write,@function
myemu_write:
	add r4, r3, r0
	add r3, r2, r0
	add r2, r1, r0
	li r1, 64
	syscall 0
	ret

	.global myemu_getpid
	.type myemu_getpid,@function
myemu_getpid:
	li r1, 172
	syscall 0
	ret

	.global myemu_exit
	.type myemu_exit,@function
myemu_exit:
	add r2, r1, r0
	li r1, 94
	syscall 0
1:
	j 1b
