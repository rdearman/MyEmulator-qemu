.text
.global _start

_start:
        lui     r5, data_word
        ori     r5, r5, data_word
        lw      r1, 0(r5)
        beq     r1, r0, backwards
        jal     forward
backwards:
        j       done
forward:
        addi    r1, r1, 1
        ret
done:
        halt

.rodata
message:
        .asciz  "relocations"

.data
data_word:
        .word   message + 4
