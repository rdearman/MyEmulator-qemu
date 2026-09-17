.include "myemulator.inc"

.section .text
.global _start

.org ROM_START

;;; ;;;;;;;;; START HERE


wait_key:
        ld   r0,[a3] 
        and  r0,r0,#CONSOLE_RX_READY
        beq  wait_key
	ld   r0,[a3-1] 		; for some reason the character is in one down from console ready. 
	cmp  r0, r3
	bne  wait_key
	ret

print_msg:
        ; A1 = address of string
        ; A0 = CONSOLE_DATA
        ; one R register = 0

.loop:
        ld  r0,[a1]        ; get next character
        cmp r0,r3          ; compare against register containing zero
        beq .done

        st  r0,[a0]        ; display it
        ada a1,#1          ; next byte
        br  .loop

.done:
        ret
	
	
	
_start:
        li  r1,#hi(CONSOLE_DATA) ; load the high part of 16 bit console data
        li  r2,#lo(CONSOLE_DATA) ; load the low part
        lda a0,r1,r2		 ; move full 16 into A0
	li r3,#0		 ; set r3 to zero for comparisions. 

	li  r1,#hi(msg0)	; move high part of message
	li  r2,#lo(msg0)	; move low part of message
	lda a1,r1,r2		; move into a1 register
	jal print_msg		; print a1

	li  r1,#hi(msg1)	; print next message
	li  r2,#lo(msg1)
	lda a1,r1,r2
	jal print_msg

        li  r1,#hi(CONSOLE_STATUS) ; move high bit into console status
        li  r2,#lo(CONSOLE_STATUS) ; move low bit into console status
        lda a3,r1,r2		   ; load into A3

	li r3, #'q'		; load q into r3 register
	jal wait_key		; jump to wait_key function (JAL has ret)
	li r3,#0		; load comparator into r3
	
	li  r1,#hi(msg2)	; print goodbye message. 
	li  r2,#lo(msg2)
	lda a1,r1,r2
	jal print_msg
	
	
        halt			; quit
	
;;; 
_catchall:
        halt

_irq1:
        br _catchall

_irq2:
        br _catchall

_irq3:
        br _catchall

_irq4:
        br _catchall

_irq5:
        br _catchall

_irq6:
        br _catchall

_irq7:
        br _catchall


.section .rodata

messages:
        .word msg0
        .word msg1

msg0:
        .asciz "RIKMON Monitor v1.0\n"

msg1:
        .asciz "Press q to quit\n"

msg2:
        .asciz "Goodbye ... \n"

.org IRQ1_VECTOR
        .word _irq1
        .word _irq2
        .word _irq3
        .word _irq4
        .word _irq5
        .word _irq6
        .word _irq7

.org INITIAL_SP_VECTOR
        .word STACK_TOP
        .word _start
