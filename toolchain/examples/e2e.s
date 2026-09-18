.text
.global _start
_start:
        addi sp, sp, -16
        sw   lr, 12(sp)
        li r1, 5
        li r2, 7
        call add_values
        lw   lr, 12(sp)
        addi sp, sp, 16
        li r5, answer
        sw r3, 0(r5)
        lw r4, 0(r5)
        beq r4, r3, done
        halt
done:
        halt

add_values:
        add r3, r1, r2
        ret

.data
.align 4
answer:
        .word 0

.section .rodata
message:
        .asciz "MyEmulator2\n"
