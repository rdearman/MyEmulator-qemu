; Minimal IRQ4 console test. The handler consumes one byte and stores it at
; 0x0200 before returning with RTI.
CONSOLE_DATA = 0xf010
CONSOLE_CONTROL = 0xf012

la a0,#CONSOLE_CONTROL,r2,r3
li r0,#1
st r0,[a0]
halt

.org 0x0100
console_irq4:
    la a0,#CONSOLE_DATA,r2,r3
    ld r0,[a0]
    la a1,#0x0200,r2,r3
    st r0,[a1]
    rti

.org 0xfff4
.word console_irq4
