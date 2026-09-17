; Polling console echo test.
CONSOLE_DATA = 0xf010
CONSOLE_STATUS = 0xf011

la a0,#CONSOLE_DATA,r2,r3
la a1,#CONSOLE_STATUS,r2,r3

poll:
    ld r1,[a1]
    and r1,r1,#1
    beq poll
    ld r0,[a0]
    st r0,[a0]
    br poll
