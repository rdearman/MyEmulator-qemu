/* __clone(func, stack, flags, arg, ptid, tls, ctid)
 *
 * C arguments are passed in r1-r7.  The Linux clone syscall receives
 * flags,newsp,parent_tid,child_tid,tls in r2-r6 after r1 is loaded with the
 * syscall number.  The child starts with r13=newsp, so its trampoline record
 * must be written to the aligned top of the caller-supplied stack.
 */
.section .text
.global __clone
.hidden __clone
.type __clone,@function
__clone:
	/* posix_spawn may supply a temporary child stack in the caller's own
	 * frame while the parent is suspended by vfork.  Keep our saved parent
	 * registers well below that record so the child cannot overwrite them. */
	subi r13, r13, 128
	sw r1, 0(r13)
	sw r2, 4(r13)
	sw r3, 8(r13)
	sw r4, 12(r13)
	sw r5, 16(r13)
	sw r6, 20(r13)
	sw r7, 24(r13)
	sw r14, 28(r13)
	sw r15, 32(r13)
	sw r8, 36(r13)
	sw r9, 40(r13)
	sw r10, 44(r13)
	sw r11, 48(r13)
	sw r12, 52(r13)

	/* Keep the caller-supplied child stack unchanged.  The vfork child
	 * shares the parent's address space and musl deliberately points this
	 * at a temporary stack in the posix_spawn frame.  Writing a trampoline
	 * record below it can overwrite the suspended parent's frame.  Preserve
	 * the function and argument in registers instead; copy_thread() copies
	 * those registers into the child, while the parent path restores them
	 * from our save area. */
	lw r2, 4(r13)
	/* REM's compiler reserves a 4092-byte positive-offset area in each
	 * frame before its ordinary local frame.  A vfork child receives the
	 * caller's temporary stack base, so move the child SP below that area;
	 * otherwise the child's first prologue writes into the suspended
	 * parent's posix_spawn frame. */
	lui r11, 1
	sub r3, r2, r11
	lw r12, 0(r13)
	lw r7, 12(r13)

	/* clone(flags, newsp, parent_tid, child_tid, tls). */
	lw r2, 8(r13)
	lw r4, 16(r13)
	lw r5, 20(r13)
	lw r6, 24(r13)
	li r1, 220
	syscall 0
	bne r1, r0, 1f

	/* Child: kernel switched r13 to newsp and preserved r12/r7. */
	add r1, r7, r0
	jalr r12
	add r2, r1, r0
	li r1, 93
	syscall 0
	j 2f

1:
	/* Restore the caller's argument registers and stack. */
	/* Keep r1 as the kernel's child PID return value.  The syscall number
	 * saved at 0(r13) is not part of the caller-visible result; restoring it
	 * here made vfork/posix_spawn pass the child function address or 220 to
	 * wait4 instead of the actual PID. */
	lw r2, 4(r13)
	lw r3, 8(r13)
	lw r4, 12(r13)
	lw r5, 16(r13)
	lw r6, 20(r13)
	lw r7, 24(r13)
	lw r14, 28(r13)
	lw r15, 32(r13)
	lw r8, 36(r13)
	lw r9, 40(r13)
	lw r10, 44(r13)
	lw r11, 48(r13)
	lw r12, 52(r13)
	addi r13, r13, 128
	jr r14

2:
	j 2b
