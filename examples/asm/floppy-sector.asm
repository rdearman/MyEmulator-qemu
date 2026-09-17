; Example sector 0 loaded by the built-in floppy bootstrap.
; The visible result is R0=0x2a before HALT.
li r0,#0x2a
gf r1
halt
