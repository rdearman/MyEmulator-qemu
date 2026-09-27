/* __clone(func, stack, flags, arg, ptid, tls, ctid)
 *
 * MyEmulator2 Linux uses the generic 32-bit clone ABI:
 *   clone(flags, newsp, parent_tid, child_tid, tls)
 * Syscall number and arguments are passed in r1..r6.  The kernel installs
 * newsp as the child's user stack pointer, so the function/argument pair is
 * written at the top of that stack before entering the syscall.
 */
.section .text
.global __clone
.hidden __clone
.type __clone,@function
__clone:
	subi r13, r13, 48
	sw r5, 0(r13)
	sw r6, 4(r13)
	sw r7, 8(r13)
	sw r8, 12(r13)
	sw r9, 16(r13)
	sw r10, 20(r13)
	sw r11, 24(r13)
	sw r12, 28(r13)
	sw r14, 32(r13)
	sw r15, 36(r13)
	/* The seventh C argument (ctid) is in the outgoing stack area at
	 * 32(original_sp).  The MyEmulator2 C ABI puts func, stack, flags,
	 * arg, ptid, and tls in r1-r6 respectively. */
	addi r15, r13, 48
	lw r12, 32(r15)       /* child_tid */
	add r9, r3, r0        /* clone flags */
	add r8, r1, r0        /* function */
	/* Keep the child entry stack 16-byte aligned at its first C call. */
	addi r2, r2, -16      /* child stack record */
	sw r8, 0(r2)
	sw r4, 4(r2)          /* function argument */

	/* Arrange the Linux clone(flags,newsp,parent_tid,child_tid,tls)
	 * syscall arguments. */
	li r1, 220           /* __NR_clone */
	add r3, r2, r0        /* newsp */
	add r2, r9, r0        /* flags */
	add r4, r5, r0        /* parent_tid */
	add r5, r12, r0
	add r6, r6, r0        /* tls */
	syscall 0
	bne r1, r0, 1f       /* parent: return child pid/error */

	/* Child: kernel switched r13 to newsp. */
	lw r3, 0(r13)        /* function */
	lw r1, 4(r13)        /* function argument */
	jalr r3
	add r2, r1, r0       /* preserve function return value */
	li r1, 93            /* __NR_exit */
	syscall 0

1:
	lw r5, 0(r13)
	lw r6, 4(r13)
	lw r7, 8(r13)
	lw r8, 12(r13)
	lw r9, 16(r13)
	lw r10, 20(r13)
	lw r11, 24(r13)
	lw r12, 28(r13)
	lw r14, 32(r13)
	lw r15, 36(r13)
	addi r13, r13, 48
	jr r14
