; Immediate arithmetic and status flags.
li r0,#7
add r0,r0,#5
sub r0,r0,#2
after_sub:
gf r1
halt
