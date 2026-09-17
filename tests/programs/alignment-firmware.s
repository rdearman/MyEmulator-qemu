; Infrastructure-only alignment-exception fixture. Not RIKMON.
.org 0xF100
reset:
    li r0,#0
    li r1,#1
    lda a0,r0,r1
    ja a0
    halt

.org 0xF110
alignment_handler:
    halt

.org 0xFFEC
    .word alignment_handler
.org 0xFFFC
    .word 0xF000
    .word reset
