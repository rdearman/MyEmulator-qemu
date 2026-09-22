/* Linux process-management regression for REM.
 *
 * The test exercises the real clone -> child userspace -> exit -> wait4
 * path.  Linux syscall arguments use r2-r5 and the syscall number/result
 * uses r1, as documented by the architecture port.
 */
	.section .text
	.global _start
	.type _start,@function
_start:
	li r1, 64
	li r2, 1
	lui r3, start_msg
	ori r3, r3, start_msg
	li r4, start_msg_end-start_msg
	syscall 0

	li r1, 220             /* clone */
	li r2, 17              /* SIGCHLD */
	li r3, 0               /* child stack: inherit */
	li r4, 0               /* parent_tid */
	li r5, 0               /* child_tid */
	syscall 0
	beq r1, r0, child
	add r8, r1, r0         /* parent keeps the child PID */

	li r1, 260             /* wait4 */
	add r2, r8, r0
	li r3, 0               /* status */
	li r4, 0               /* options */
	li r5, 0               /* rusage */
	syscall 0

	li r1, 64
	li r2, 1
	lui r3, parent_msg
	ori r3, r3, parent_msg
	li r4, parent_msg_end-parent_msg
	syscall 0
	li r1, 94              /* exit_group */
	li r2, 0
	syscall 0

child:
	li r1, 64
	li r2, 1
	lui r3, child_msg
	ori r3, r3, child_msg
	li r4, child_msg_end-child_msg
	syscall 0
	li r1, 94              /* exit_group */
	li r2, 0
	syscall 0

1:
	j 1b

	.section .rodata
start_msg:
	.ascii "process test start\n"
start_msg_end:
child_msg:
	.ascii "process child\n"
child_msg_end:
parent_msg:
	.ascii "process parent waited\n"
parent_msg_end:
