.include "myemulator.inc"

.section .text
.global _start

.org ROM_START

; Temporary command-line storage in the top 256 bytes of RAM.  The first
; version of read_line intentionally does not enforce INPUT_MAX yet.
.equ INPUT_BUFFER, 0xEF00
.equ INPUT_MAX,    63

;;; ;;;;;;;;; START HERE


; Read one byte from the console. A3 points at CONSOLE_STATUS and the DATA
; register is at A3-1. The returned character is in R0.
read_char:
        ld   r0,[a3]
        and  r0,r0,#CONSOLE_RX_READY
        beq  read_char
        ld   r0,[a3-1]
        ret

; Read a line into the zero-terminated buffer at A2.
;
; A2 is advanced as characters are stored.  LF and CR both terminate the
; line.  Ordinary characters are echoed and stored in RAM.  The caller's LR
; is saved because this routine calls read_char with JAL.
read_line:
        push {lr}

read_line_loop:
        jal  read_char

        li   r1,#0x0A
        cmp  r0,r1
        beq  read_line_done
        li   r1,#0x0D
        cmp  r0,r1
        beq  read_line_done

        st   r0,[a2]
        st   r0,[a0]
        ada  a2,#1
        br   read_line_loop

read_line_done:
        li   r1,#0
        st   r1,[a2]
        li   r0,#0x0A
        st   r0,[a0]
        pop  {lr}
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

        br show_prompt

; The command loop currently implements q (quit) and reserves b for the
; future floppy boot command. Unknown lines are ignored.
command_loop:
        li  r1,#hi(INPUT_BUFFER)
        li  r2,#lo(INPUT_BUFFER)
        lda  a2,r1,r2
        jal  read_line

        li  r1,#hi(INPUT_BUFFER)
        li  r2,#lo(INPUT_BUFFER)
        lda  a1,r1,r2
        ld   r0,[a1]

        li  r1,#'q'
        cmp r0,r1
        beq quit_command

        li  r1,#'b'
        cmp r0,r1
        beq boot_command

        br show_prompt

show_prompt:
        li  r1,#hi(prompt)
        li  r2,#lo(prompt)
        lda a1,r1,r2
        jal print_msg
        br command_loop

quit_command:
        li  r1,#hi(msg2)
        li  r2,#lo(msg2)
        lda a1,r1,r2
        jal print_msg
        halt

boot_command:
        ; Placeholder only: the real floppy BOOT command will be written in
        ; RIKMON later.
        li  r1,#hi(msg3)
        li  r2,#lo(msg3)
        lda a1,r1,r2
        jal print_msg
        br show_prompt
	
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
        .asciz "Press q to quit; b to boot\n"

prompt:
        .asciz "> "

msg2:
        .asciz "Goodbye ...\n"

msg3:
        .asciz "BOOT not implemented\n"

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
