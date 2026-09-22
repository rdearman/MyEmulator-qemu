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

	.global myemu_openat
	.type myemu_openat,@function
myemu_openat:
	addi r13, r13, -16
	sw r5, 0(r13)
	add r5, r4, r0
	add r4, r3, r0
	add r3, r2, r0
	add r2, r1, r0
	li r1, 56
	syscall 0
	lw r5, 0(r13)
	addi r13, r13, 16
	ret

	.global myemu_close
	.type myemu_close,@function
myemu_close:
	add r2, r1, r0
	li r1, 57
	syscall 0
	ret

	.global myemu_fsync
	.type myemu_fsync,@function
myemu_fsync:
	add r2, r1, r0
	li r1, 74
	syscall 0
	ret

	.global myemu_execve
	.type myemu_execve,@function
myemu_execve:
	add r4, r3, r0
	add r3, r2, r0
	add r2, r1, r0
	li r1, 221
	syscall 0
	ret

	.global myemu_lseek
	.type myemu_lseek,@function
myemu_lseek:
	add r4, r3, r0
	add r3, r2, r0
	add r2, r1, r0
	li r1, 62
	syscall 0
	ret

	.global myemu_fstat
	.type myemu_fstat,@function
myemu_fstat:
	add r3, r2, r0
	add r2, r1, r0
	li r1, 80
	syscall 0
	ret

	.global myemu_fstatat
	.type myemu_fstatat,@function
myemu_fstatat:
	addi r13, r13, -16
	sw r5, 0(r13)
	add r5, r4, r0
	add r4, r3, r0
	add r3, r2, r0
	add r2, r1, r0
	li r1, 79
	syscall 0
	lw r5, 0(r13)
	addi r13, r13, 16
	ret

	.global myemu_getdents64
	.type myemu_getdents64,@function
myemu_getdents64:
	add r4, r3, r0
	add r3, r2, r0
	add r2, r1, r0
	li r1, 61
	syscall 0
	ret

	.global myemu_getcwd
	.type myemu_getcwd,@function
myemu_getcwd:
	add r3, r2, r0
	add r2, r1, r0
	li r1, 17
	syscall 0
	ret

	.global myemu_mkdirat
	.type myemu_mkdirat,@function
myemu_mkdirat:
	add r4, r3, r0
	add r3, r2, r0
	add r2, r1, r0
	li r1, 34
	syscall 0
	ret

	.global myemu_unlinkat
	.type myemu_unlinkat,@function
myemu_unlinkat:
	add r4, r3, r0
	add r3, r2, r0
	add r2, r1, r0
	li r1, 35
	syscall 0
	ret

	.global myemu_renameat
	.type myemu_renameat,@function
myemu_renameat:
	addi r13, r13, -16
	sw r5, 0(r13)
	add r5, r4, r0
	add r4, r3, r0
	add r3, r2, r0
	add r2, r1, r0
	li r1, 38
	syscall 0
	lw r5, 0(r13)
	addi r13, r13, 16
	ret

	.global myemu_chdir
	.type myemu_chdir,@function
myemu_chdir:
	add r2, r1, r0
	li r1, 49
	syscall 0
	ret

	.global myemu_uname
	.type myemu_uname,@function
myemu_uname:
	add r2, r1, r0
	li r1, 160
	syscall 0
	ret

	.global myemu_sched_yield
	.type myemu_sched_yield,@function
myemu_sched_yield:
	li r1, 124
	syscall 0
	ret

	.global myemu_nanosleep
	.type myemu_nanosleep,@function
myemu_nanosleep:
	add r3, r2, r0
	add r2, r1, r0
	li r1, 101
	syscall 0
	ret

	.global myemu_clock_gettime
	.type myemu_clock_gettime,@function
myemu_clock_gettime:
	add r3, r2, r0
	add r2, r1, r0
	li r1, 403
	syscall 0
	ret

	.global myemu_brk
	.type myemu_brk,@function
myemu_brk:
	add r2, r1, r0
	li r1, 214
	syscall 0
	ret

	.global myemu_mmap
	.type myemu_mmap,@function
myemu_mmap:
	addi r13, r13, -16
	sw r5, 0(r13)
	sw r6, 4(r13)
	sw r7, 8(r13)
	lw r7, 52(sp)
	lw r6, 48(sp)
	add r5, r4, r0
	add r4, r3, r0
	add r3, r2, r0
	add r2, r1, r0
	li r1, 222
	syscall 0
	lw r5, 0(r13)
	lw r6, 4(r13)
	lw r7, 8(r13)
	addi r13, r13, 16
	ret

	.global myemu_munmap
	.type myemu_munmap,@function
myemu_munmap:
	add r3, r2, r0
	add r2, r1, r0
	li r1, 215
	syscall 0
	ret

	.global myemu_clone
	.type myemu_clone,@function
myemu_clone:
	addi r13, r13, -16
	sw r5, 0(r13)
	sw r6, 4(r13)
	lw r6, 48(sp)
	add r5, r4, r0
	add r4, r3, r0
	add r3, r2, r0
	add r2, r1, r0
	li r1, 220
	syscall 0
	lw r5, 0(r13)
	lw r6, 4(r13)
	addi r13, r13, 16
	ret

	.global myemu_wait4
	.type myemu_wait4,@function
myemu_wait4:
	addi r13, r13, -16
	sw r5, 0(r13)
	add r5, r4, r0
	add r4, r3, r0
	add r3, r2, r0
	add r2, r1, r0
	li r1, 260
	syscall 0
	lw r5, 0(r13)
	addi r13, r13, 16
	ret

	.global myemu_exit
	.type myemu_exit,@function
myemu_exit:
	add r2, r1, r0
	li r1, 94
	syscall 0
1:
	j 1b
