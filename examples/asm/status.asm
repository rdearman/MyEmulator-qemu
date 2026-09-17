; Copy flags to a data register and restore them from a data register.
li r0,#255
add r0,r0,#1
gf r1
sf r1
halt
