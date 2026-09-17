; Minimal console hardware test: writes RIK and halts.
CONSOLE_DATA = 0xf010

la a0,#CONSOLE_DATA,r2,r3
li r0,#'R'
st r0,[a0]
li r0,#'I'
st r0,[a0]
li r0,#'K'
st r0,[a0]
halt
