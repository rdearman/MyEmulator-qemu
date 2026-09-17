; BL writes LR; RET returns to the instruction after the call.
bl twice
halt
twice:
li r0,#2
add r0,r0,#2
ret
