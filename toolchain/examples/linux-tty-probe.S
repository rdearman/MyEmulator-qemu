/* Probe the serial-core TTY path.  The test opens the device created by
 * serial-core, writes through VFS, waits for one received byte, echoes it,
 * then closes the file.  Syscall ABI: r1=number/result, r2-r5=args. */
	.section .text
	.global _start
_start:
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
	blt r8, r0, open_failed

	li r1, 64
	add r2, r8, r0
	lui r3, %hi(tx)
	ori r3, r3, %lo(tx)
	li r4, tx_end-tx
	syscall 0

	li r1, 63
	add r2, r8, r0
	lui r3, %hi(rx)
	ori r3, r3, %lo(rx)
	li r4, 1
	syscall 0
	blt r1, r0, read_failed

	li r1, 64
	add r2, r8, r0
	lui r3, %hi(rx)
	ori r3, r3, %lo(rx)
	li r4, 1
	syscall 0

	li r1, 57
	add r2, r8, r0
	syscall 0
	li r1, 94
	li r2, 0
	syscall 0
	halt

open_failed:
	li r1, 64
	li r2, 1
	lui r3, %hi(open_failure)
	ori r3, r3, %lo(open_failure)
	li r4, open_failure_end-open_failure
	syscall 0
	li r1, 94
	li r2, 1
	syscall 0
	halt

read_failed:
	li r1, 64
	li r2, 1
	lui r3, %hi(read_failure)
	ori r3, r3, %lo(read_failure)
	li r4, read_failure_end-read_failure
	syscall 0
	li r1, 94
	li r2, 1
	syscall 0
	halt

	.section .rodata
path:
	.asciz "/dev/ttyMY0"
banner:
	.ascii "TTY probe open/write; type one byte: "
banner_end:
tx:
	.ascii "TTY-VFS-OK\r\n"
tx_end:
open_failure:
	.ascii "TTY probe OPEN FAILED\r\n"
open_failure_end:
read_failure:
	.ascii "TTY probe READ FAILED\r\n"
read_failure_end:
	.section .bss
	.align 2
rx:
	.space 1
