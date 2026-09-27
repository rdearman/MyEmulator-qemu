	.section .text
	.global _start
	.type _start,@function
_start:
	li r1, 220
	li r2, 17
	li r3, 0
	li r4, 0
	li r5, 0
	syscall 0
	beq r1, r0, child
	add r8, r1, r0
	li r1, 260
	add r2, r8, r0
	li r3, 0
	li r4, 0
	li r5, 0
	syscall 0
	li r1, 64
	li r2, 1
	lui r3, %hi(done_msg)
	ori r3, r3, %lo(done_msg)
	li r4, done_msg_end-done_msg
	syscall 0
	li r1, 94
	li r2, 0
	syscall 0
child:
	li r1, 221
	lui r2, %hi(cat_path)
	ori r2, r2, %lo(cat_path)
	lui r3, %hi(cat_argv)
	ori r3, r3, %lo(cat_argv)
	lui r4, %hi(empty_env)
	ori r4, r4, %lo(empty_env)
	syscall 0
	li r1, 94
	li r2, 127
	syscall 0
	.section .rodata
cat_path:
	.asciz "/usr/bin/cat"
input_path:
	.asciz "/tmp/cat-input"
	.align 2
cat_argv:
	.word cat_path
	.word input_path
	.word 0
empty_env:
	.word 0
done_msg:
	.ascii "CLONE_EXEC_CAT_OK\n"
done_msg_end:
