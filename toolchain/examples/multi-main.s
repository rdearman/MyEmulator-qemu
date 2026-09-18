.text
.global _start
.extern increment

_start:
        li      r1, value
        lw      r1, 0(r1)
        call    increment
        halt

.data
.align 4
value:
        .word   41

.bss
.align 4
scratch:
        .space  4
