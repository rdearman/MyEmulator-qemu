; Infrastructure-only test image. This is not RIKMON.
.org 0xF100
reset:
    li r0,#42
    halt

.org 0xFFFC
    .word 0xF000
    .word reset
