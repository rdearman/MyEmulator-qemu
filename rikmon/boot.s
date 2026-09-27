; Minimal MyEmulator boot sector
; RIKMON loads this binary at 0x0200.

start:
        ; Establish our own console DATA address: A0 = 0xF010
        li   r1,#0xf0
        li   r2,#0x10
        lda  a0,r1,r2

        li   r0,#'B'
        st   r0,[a0]
        li   r0,#'O'
        st   r0,[a0]
        li   r0,#'O'
        st   r0,[a0]
        li   r0,#'T'
        st   r0,[a0]
        li   r0,#'\n'
        st   r0,[a0]

        halt
	
