.text
.global _start
_start:
        li r1, 5
        li r2, 7
        add r3, r1, r2
        beq r3, r0, _start
        halt
