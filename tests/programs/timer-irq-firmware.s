.include "myemulator.inc"
.org ROM_START
start:
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
li   r0,#(TIMER_ENABLE | TIMER_IRQ_ENABLE)
st   r0,[a0]
irq_loop:
br   irq_loop
irq1_handler:
li   r0,#0xa5
li   r1,#hi(0xE000)
li   r2,#lo(0xE000)
lda  a0,r1,r2
st   r0,[a0]
li   r1,#hi(TIMER_STATUS)
li   r2,#lo(TIMER_STATUS)
lda  a0,r1,r2
li   r0,#TIMER_EXPIRED
st   r0,[a0]
rti
.org ALIGNMENT_VECTOR
.word start
.org IRQ1_VECTOR
.word irq1_handler
.org INITIAL_SP_VECTOR
.word STACK_TOP
.word start
