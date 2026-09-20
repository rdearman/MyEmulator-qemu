/* Minimal bootstrap shell using the temporary UART polling ABI.
 * r1=syscall number/result, r2-r4=syscall arguments. */
	.section .text
	.global _start
	.type _start,@function
_start:
	/* The assembly shell deliberately uses no stack, so it also serves as a
	 * small userspace test while the hosted C stack ABI is being completed. */
	li r1, 64
	li r2, 1
	lui r3, %hi(banner)
	ori r3, r3, %lo(banner)
	li r4, banner_end-banner
	syscall 0

main_loop:
	li r1, 64
	li r2, 1
	lui r3, %hi(prompt)
	ori r3, r3, %lo(prompt)
	li r4, prompt_end-prompt
	syscall 0
	li r5, 0

read_line:
	lui r3, %hi(line)
	ori r3, r3, %lo(line)
	add r3, r3, r5
	li r1, 63
	li r2, 0
	li r4, 1
	syscall 0
	blt r1, r0, read_line
	lbu r6, 0(r3)
	li r7, 10
	beq r6, r7, line_done
	li r7, 13
	beq r6, r7, line_done
	li r7, 8
	beq r6, r7, read_line
	li r7, 127
	beq r6, r7, read_line
	li r1, 64
	li r2, 1
	li r4, 1
	syscall 0
	addi r5, r5, 1
	li r7, 126
	bltu r7, r5, read_line
	j read_line

line_done:
	sb r0, 0(r3)
	li r1, 64
	li r2, 1
	lui r3, %hi(newline)
	ori r3, r3, %lo(newline)
	li r4, newline_end-newline
	syscall 0

	/* Dispatch on the first two bytes; accepted commands are exact enough
	 * for this bootstrap shell and avoid a libc dependency. */
	lui r3, %hi(line)
	ori r3, r3, %lo(line)
	lbu r6, 0(r3)
	li r7, 'h'
	beq r6, r7, command_help
	li r7, 'u'
	beq r6, r7, command_uname
	li r7, 'c'
	beq r6, r7, command_clear
	li r7, 'e'
	beq r6, r7, command_e
	beq r6, r0, main_loop

command_unknown:
	li r1, 64
	li r2, 1
	lui r3, %hi(unknown)
	ori r3, r3, %lo(unknown)
	li r4, unknown_end-unknown
	syscall 0
	j main_loop

command_help:
	li r1, 64
	li r2, 1
	lui r3, %hi(help_text)
	ori r3, r3, %lo(help_text)
	li r4, help_text_end-help_text
	syscall 0
	j main_loop

command_uname:
	li r1, 64
	li r2, 1
	lui r3, %hi(uname_text)
	ori r3, r3, %lo(uname_text)
	li r4, uname_text_end-uname_text
	syscall 0
	j main_loop

command_clear:
	li r1, 64
	li r2, 1
	lui r3, %hi(clear_text)
	ori r3, r3, %lo(clear_text)
	li r4, clear_text_end-clear_text
	syscall 0
	j main_loop

command_e:
	lbu r6, 1(r3)
	li r7, 'x'
	beq r6, r7, command_exit
	li r7, 'c'
	bne r6, r7, command_unknown
	lbu r6, 2(r3)
	li r7, 'h'
	bne r6, r7, command_unknown
	lbu r6, 3(r3)
	li r7, 'o'
	bne r6, r7, command_unknown
	lbu r6, 4(r3)
	li r7, ' '
	beq r6, r7, command_echo_text
	beq r6, r0, command_echo_empty
	j command_unknown

command_echo_text:
	li r5, 5
	add r3, r3, r5
	li r6, 0
echo_length:
	lbu r7, 0(r3)
	beq r7, r0, echo_write
	addi r3, r3, 1
	addi r6, r6, 1
	j echo_length
echo_write:
	lui r3, %hi(line)
	ori r3, r3, %lo(line)
	addi r3, r3, 5
	li r1, 64
	li r2, 1
	add r4, r6, r0
	syscall 0

command_echo_empty:
	li r1, 64
	li r2, 1
	lui r3, %hi(newline)
	ori r3, r3, %lo(newline)
	li r4, newline_end-newline
	syscall 0
	j main_loop

command_exit:
	li r1, 94
	li r2, 0
	syscall 0
	halt

	.section .rodata
banner:
	.ascii "MyEmulator2 bootstrap shell\r\n"
banner_end:
prompt:
	.ascii "myemu> "
prompt_end:
help_text:
	.ascii "help echo uname clear exit\r\n"
help_text_end:
uname_text:
	.ascii "MyEmulator2 Linux 6.12.1\r\n"
uname_text_end:
clear_text:
	.ascii "\033[2J\033[H"
clear_text_end:
unknown:
	.ascii "unknown command\r\n"
unknown_end:
newline:
	.ascii "\r\n"
newline_end:

	.section .bss
	.align 2
line:
	.space 128
