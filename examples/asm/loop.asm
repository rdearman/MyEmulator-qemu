; Count down from ten. The final SUB sets ZF for BEQ.
li r0,#10
loop:
sub r0,r0,#1
bne loop
halt
