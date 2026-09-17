; Store and load a byte through an address register.
li r0,#0x42
li r1,#0x12
li r2,#0x34
lda a0,r1,r2
st r0,[a0]
ld r3,[a0]
halt
