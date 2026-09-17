.include "myemulator.inc"

; Timer smoke fixture: program a short one-shot and keep the CPU running.
li   r0,#2
li   r1,#hi(TIMER_RELOAD_LO)
li   r2,#lo(TIMER_RELOAD_LO)
lda  a0,r1,r2
st   r0,[a0]
li   r0,#0
st   r0,[a0+1]
li   r1,#hi(TIMER_CONTROL)
li   r2,#lo(TIMER_CONTROL)
lda  a0,r1,r2
li   r0,#TIMER_ENABLE
st   r0,[a0]
timer_loop:
br   timer_loop
