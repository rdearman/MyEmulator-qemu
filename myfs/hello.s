.include "../rikmon/include/myemulator.inc"

.org 0x2000
hello:
        la   a1,#message,r2,r3
hello_loop:
        ld   r0,[a1]
        or   r0,r0
        beq  hello_done
        la   a0,#CONSOLE_DATA,r2,r3
        st   r0,[a0]
        ada  a1,#1
        br   hello_loop
hello_done:
        ret
message: .asciz "Hello from MyEmulator!\n"
