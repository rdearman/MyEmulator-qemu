; Console newline test: guest LF becomes host CRLF; guest CR is unchanged.
CONSOLE_DATA = 0xf010

la a0,#CONSOLE_DATA,r2,r3
li r0,#'A'
st r0,[a0]
li r0,#0x0a
st r0,[a0]
li r0,#'B'
st r0,[a0]
li r0,#0x0d
st r0,[a0]
li r0,#'C'
st r0,[a0]
li r0,#0x0a
st r0,[a0]
li r0,#'D'
st r0,[a0]
halt
