/* Bootstrap MyEmulator2 Linux console echo test.
 * ABI: r1=syscall number, r2-r4=arguments, r1=result. */
	.section .text
	.global _start
	.type _start,@function
_start:
	li r1, 64              /* write */
	li r2, 1              /* stdout */
	lui r3, %hi(prompt)
	ori r3, r3, %lo(prompt)
	li r4, prompt_end-prompt
	syscall 0

	/* Retry read(0, buffer, 1) until a host character is available. */
1:
	li r1, 63              /* read */
	li r2, 0              /* stdin */
	lui r3, %hi(buffer)
	ori r3, r3, %lo(buffer)
	li r4, 1
	syscall 0
	blt r1, r0, 1b

	li r1, 64              /* write */
	li r2, 1              /* stdout */
	add r3, r3, r0
	li r4, 1
	syscall 0
	j _start

	.section .rodata
prompt:
	.ascii "myemu-echo> "
prompt_end:

	.section .bss
	.align 2
buffer:
	.space 1
