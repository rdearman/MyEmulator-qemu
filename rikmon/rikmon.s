.include "myemulator.inc"

.section .text
.global _start

.org ROM_START

; RIKMON workspace. The input area is deliberately in RAM, not ROM.
.equ INPUT_BUFFER, 0xEF00
.equ INPUT_MAX,    63
.equ BOOT_BUFFER,  0x0200

; A3 points at CONSOLE_STATUS and A0 points at CONSOLE_DATA.
read_char:
        ld   r0,[a3]
        and  r0,r0,#CONSOLE_RX_READY
        beq  read_char
        ld   r0,[a3-1]
        ret

; Read a bounded, zero-terminated line into A2.
; LF/CR finish the line. Backspace removes one character and erases it.
read_line:
        push {lr}
        li   r2,#0
read_line_loop:
        bl  read_char
        li   r1,#0x0A
        cmp  r0,r1
        beq  read_line_done
        li   r1,#0x0D
        cmp  r0,r1
        beq  read_line_done
        li   r1,#0x08
        cmp  r0,r1
        beq  read_line_backspace
        li   r1,#0x7F
        cmp  r0,r1
        beq  read_line_backspace
        li   r1,#INPUT_MAX
        cmp  r2,r1
        bge  read_line_full
        st   r0,[a2]
        st   r0,[a0]
        ada  a2,#1
        add  r2,r2,#1
        br   read_line_loop
read_line_backspace:
        li   r1,#0
        cmp  r2,r1
        beq  read_line_loop
        ada  a2,#-1
        sub  r2,r2,#1
        li   r0,#0x08
        st   r0,[a0]
        li   r0,#' '
        st   r0,[a0]
        li   r0,#0x08
        st   r0,[a0]
        br   read_line_loop
read_line_full:
        li   r0,#0x07
        st   r0,[a0]
        br   read_line_loop
read_line_done:
        li   r1,#0
        st   r1,[a2]
        li   r0,#0x0A
        st   r0,[a0]
        pop  {lr}
        ret

; Convert ASCII A-Z in the input buffer to lower case in place.
normalize_line:
        push {lr}
        li   r1,#'A'
        li   r2,#'['
normalize_loop:
        ld   r0,[a1]
        cmp  r0,r3
        beq  normalize_done
        cmp  r0,r1
        bltu normalize_next
        cmp  r0,r2
        bge  normalize_next
        or   r0,r0,#0x20
        st   r0,[a1]
normalize_next:
        ada  a1,#1
        br   normalize_loop
normalize_done:
        pop  {lr}
        ret

; Print a zero-terminated string at A1. R3 must contain zero.
print_msg:
        ld   r0,[a1]
        cmp  r0,r3
        beq  print_msg_done
        st   r0,[a0]
        ada  a1,#1
        br   print_msg
print_msg_done:
        ret

; Match a command word at A1 against the zero-terminated word at A2.
match_word:
        ld   r0,[a2]
        cmp  r0,r3
        beq  match_word_end
        ld   r1,[a1]
        cmp  r1,r0
        bne  match_word_fail
        ada  a1,#1
        ada  a2,#1
        br   match_word
match_word_end:
        ld   r1,[a1]
        cmp  r1,r3
        beq  match_word_ok
        li   r2,#' '
        cmp  r1,r2
        beq  match_word_ok
match_word_fail:
        li   r0,#0
        ret
match_word_ok:
        li   r0,#1
        ret

print_msg_start:
        push {lr}
        bl  print_msg
        pop  {lr}
        ret

_start:
        li   r1,#hi(CONSOLE_DATA)
        li   r2,#lo(CONSOLE_DATA)
        lda  a0,r1,r2
        li   r3,#0
        li   r1,#hi(msg0)
        li   r2,#lo(msg0)
        lda  a1,r1,r2
        bl  print_msg_start
        li   r1,#hi(msg1)
        li   r2,#lo(msg1)
        lda  a1,r1,r2
        bl  print_msg_start
        li   r1,#hi(CONSOLE_STATUS)
        li   r2,#lo(CONSOLE_STATUS)
        lda  a3,r1,r2
        br   start_show_prompt

read_char_command:
        ld   r0,[a3]
        and  r0,r0,#CONSOLE_RX_READY
        beq  read_char_command
        ld   r0,[a3-1]
        ret

read_line_command:
        push {lr}
        li   r2,#0
read_line_command_loop:
        bl  read_char_command
        li   r1,#0x0A
        cmp  r0,r1
        beq  read_line_command_done
        li   r1,#0x0D
        cmp  r0,r1
        beq  read_line_command_done
        li   r1,#0x08
        cmp  r0,r1
        beq  read_line_command_backspace
        li   r1,#0x7F
        cmp  r0,r1
        beq  read_line_command_backspace
        li   r1,#INPUT_MAX
        cmp  r2,r1
        bge  read_line_command_full
        st   r0,[a2]
        st   r0,[a0]
        ada  a2,#1
        add  r2,r2,#1
        br   read_line_command_loop
read_line_command_backspace:
        li   r1,#0
        cmp  r2,r1
        beq  read_line_command_loop
        ada  a2,#-1
        sub  r2,r2,#1
        li   r0,#0x08
        st   r0,[a0]
        li   r0,#' '
        st   r0,[a0]
        li   r0,#0x08
        st   r0,[a0]
        br   read_line_command_loop
read_line_command_full:
        li   r0,#0x07
        st   r0,[a0]
        br   read_line_command_loop
read_line_command_done:
        li   r1,#0
        st   r1,[a2]
        li   r0,#0x0A
        st   r0,[a0]
        pop  {lr}
        ret

normalize_line_command:
        push {lr}
        li   r1,#'A'
        li   r2,#'['
normalize_command_loop:
        ld   r0,[a1]
        cmp  r0,r3
        beq  normalize_command_done
        cmp  r0,r1
        bltu normalize_command_next
        cmp  r0,r2
        bge  normalize_command_next
        or   r0,r0,#0x20
        st   r0,[a1]
normalize_command_next:
        ada  a1,#1
        br   normalize_command_loop
normalize_command_done:
        pop  {lr}
        ret

match_word_command:
        ld   r0,[a2]
        cmp  r0,r3
        beq  match_word_command_end
        ld   r1,[a1]
        cmp  r1,r0
        bne  match_word_command_fail
        ada  a1,#1
        ada  a2,#1
        br   match_word_command
match_word_command_end:
        ld   r1,[a1]
        cmp  r1,r3
        beq  match_word_command_ok
        li   r2,#' '
        cmp  r1,r2
        beq  match_word_command_ok
match_word_command_fail:
        li   r0,#0
        ret
match_word_command_ok:
        li   r0,#1
        ret

start_show_prompt: br show_prompt

command_loop:
        li   r1,#hi(INPUT_BUFFER)
        li   r2,#lo(INPUT_BUFFER)
        lda  a2,r1,r2
        bl  read_line_command
        li   r1,#hi(INPUT_BUFFER)
        li   r2,#lo(INPUT_BUFFER)
        lda  a1,r1,r2
        bl  normalize_line_command
        li   r1,#hi(INPUT_BUFFER)
        li   r2,#lo(INPUT_BUFFER)
        lda  a1,r1,r2
        li   r1,#hi(cmd_help)
        li   r2,#lo(cmd_help)
        lda  a2,r1,r2
        bl  match_word_command
        li   r1,#1
        cmp  r0,r1
        beq  dispatch_help
        li   r1,#hi(INPUT_BUFFER)
        li   r2,#lo(INPUT_BUFFER)
        lda  a1,r1,r2
        li   r1,#hi(cmd_boot)
        li   r2,#lo(cmd_boot)
        lda  a2,r1,r2
        bl  match_word_command
        li   r1,#1
        cmp  r0,r1
        beq  dispatch_boot
        li   r1,#hi(INPUT_BUFFER)
        li   r2,#lo(INPUT_BUFFER)
        lda  a1,r1,r2
        ld   r0,[a1]
        li   r1,#'q'
        cmp  r0,r1
        beq  dispatch_quit
        li   r1,#'m'
        cmp  r0,r1
        beq  dispatch_memory
        li   r1,#'d'
        cmp  r0,r1
        beq  dispatch_dump
        li   r1,#'f'
        cmp  r0,r1
        beq  dispatch_fill
        li   r1,#'r'
        cmp  r0,r1
        beq  dispatch_registers
        li   r1,#'g'
        cmp  r0,r1
        beq  dispatch_go
        br   command_error

show_prompt:
        li   r3,#0
        li   r1,#hi(prompt)
        li   r2,#lo(prompt)
        lda  a1,r1,r2
        bl  print_msg_prompt
        br   command_loop

print_msg_prompt:
        ld   r0,[a1]
        cmp  r0,r3
        beq  print_msg_prompt_done
        st   r0,[a0]
        ada  a1,#1
        br   print_msg_prompt
print_msg_prompt_done:
        ret

dispatch_help: br command_help
dispatch_boot: br boot_bridge_a
dispatch_quit: br command_quit
dispatch_memory: ada a1,#1
        br memory_bridge_a
dispatch_dump: ada a1,#1
        br dump_bridge_a
dispatch_fill: ada a1,#1
        br fill_bridge_a
dispatch_registers: br registers_bridge_a
dispatch_go: ada a1,#1
        br go_bridge_a

boot_bridge_a: br boot_bridge_a_mid
dump_bridge_a: br dump_bridge_a_mid
fill_bridge_a: br fill_bridge_a_mid
memory_bridge_a: br memory_bridge_a_mid
registers_bridge_a: br registers_bridge_a_mid
go_bridge_a: br go_bridge_a_mid

command_help:
        li   r3,#0
        li   r1,#hi(msg_help)
        li   r2,#lo(msg_help)
        lda  a1,r1,r2
        bl  print_msg_monitor
        br   show_prompt
command_error:
        li   r3,#0
        li   r1,#hi(msg_error)
        li   r2,#lo(msg_error)
        lda  a1,r1,r2
        bl  print_msg_monitor
        br   show_prompt
command_quit:
        li   r3,#0
        li   r1,#hi(msg_goodbye)
        li   r2,#lo(msg_goodbye)
        lda  a1,r1,r2
        bl  print_msg_monitor
        halt

print_msg_monitor:
        ld   r0,[a1]
        cmp  r0,r3
        beq  print_msg_monitor_done
        st   r0,[a0]
        ada  a1,#1
        br   print_msg_monitor
print_msg_monitor_done:
        ret

boot_bridge_a_mid: br boot_bridge_a_mid2
dump_bridge_a_mid: br dump_bridge_a_mid2
fill_bridge_a_mid: br fill_bridge_a_mid2
memory_bridge_a_mid: br memory_bridge_a_mid2
registers_bridge_a_mid: br registers_bridge_a_mid2
go_bridge_a_mid: br go_bridge_a_mid2
syntax_prompt_bridge: br show_prompt
done_bridge_0: br show_prompt

print_hex_nibble:
        li   r1,#10
        cmp  r0,r1
        bltu print_hex_digit
        add  r0,r0,#55
        br   print_hex_nibble_out
print_hex_digit:
        add  r0,r0,#48
print_hex_nibble_out:
        st   r0,[a0]
        ret

print_hex_byte:
        push {lr}
        add  r1,r0,#0
        shr  r0,r1,#4
        bl  print_hex_nibble
        and  r0,r1,#0x0F
        bl  print_hex_nibble
        pop  {lr}
        ret

print_hex_word:
        push {lr}
        gta  r0,r1,a2
        push {r1}
        bl  print_hex_byte
        pop  {r1}
        add  r0,r1,#0
        bl  print_hex_byte
        pop  {lr}
        ret

; Parse one hexadecimal value after A1, skipping spaces. On success A2 is
; the value, A1 points at the delimiter, and R0 is one.
parse_hex:
        li   r1,#' '
parse_hex_spaces:
        ld   r0,[a1]
        cmp  r0,r1
        bne  parse_hex_first
        ada  a1,#1
        br   parse_hex_spaces
parse_hex_first:
        li   r1,#0
        li   r2,#0
        lda  a2,r1,r2
        ld   r0,[a1]
        li   r1,#'0'
        cmp  r0,r1
        bltu parse_hex_fail
        li   r1,#':'
        cmp  r0,r1
        bltu parse_hex_digit_first
        li   r1,#'a'
        cmp  r0,r1
        bltu parse_hex_fail
        li   r1,#'g'
        cmp  r0,r1
        bltu parse_hex_letter_first
        br   parse_hex_fail
parse_hex_digit_first:
        sub  r0,r0,#'0'
        br   parse_hex_have_first
parse_hex_letter_first:
        sub  r0,r0,#87
parse_hex_have_first:
        add  r3,r0,#0
        br   parse_hex_accumulate
parse_hex_loop:
        ada  a1,#1
        ld   r0,[a1]
        li   r1,#'0'
        cmp  r0,r1
        bltu parse_hex_done
        li   r1,#':'
        cmp  r0,r1
        bltu parse_hex_digit
        li   r1,#'a'
        cmp  r0,r1
        bltu parse_hex_done
        li   r1,#'g'
        cmp  r0,r1
        bltu parse_hex_letter
        br   parse_hex_done
parse_hex_digit:
        sub  r0,r0,#'0'
        br   parse_hex_have_digit
parse_hex_letter:
        sub  r0,r0,#87
parse_hex_have_digit:
        add  r3,r0,#0
parse_hex_accumulate:
        gta  r1,r2,a2
        shl  r2,r2,#1
        gf   r0
        and  r0,r0,#S0_CF
        shr  r0,r0,#2
        shl  r1,r1,#1
        or   r1,r0
        shl  r2,r2,#1
        gf   r0
        and  r0,r0,#S0_CF
        shr  r0,r0,#2
        shl  r1,r1,#1
        or   r1,r0
        shl  r2,r2,#1
        gf   r0
        and  r0,r0,#S0_CF
        shr  r0,r0,#2
        shl  r1,r1,#1
        or   r1,r0
        shl  r2,r2,#1
        gf   r0
        and  r0,r0,#S0_CF
        shr  r0,r0,#2
        shl  r1,r1,#1
        or   r1,r0
        add  r2,r3
        lda  a2,r1,r2
        br   parse_hex_loop
parse_hex_done:
        li   r3,#0
        li   r0,#1
        ret
parse_hex_fail:
        li   r3,#0
        li   r0,#0
        ret

parse_hex_bridge:
        push {lr}
        bl  parse_hex
        pop  {lr}
        ret

boot_bridge_a_mid2: br boot_bridge_b
dump_bridge_a_mid2: br dump_bridge_b
fill_bridge_a_mid2: br fill_bridge_b
memory_bridge_a_mid2: br memory_bridge_b
registers_bridge_a_mid2: br registers_bridge_b
go_bridge_a_mid2: br go_bridge_b

command_syntax:
        pop  {lr}
        li   r3,#0
        li   r1,#hi(msg_syntax)
        li   r2,#lo(msg_syntax)
        lda  a1,r1,r2
        bl  print_msg_syntax
        br   syntax_prompt_bridge
done_bridge_1: br done_bridge_0

print_msg_syntax:
        ld   r0,[a1]
        cmp  r0,r3
        beq  print_msg_syntax_done
        st   r0,[a0]
        ada  a1,#1
        br   print_msg_syntax
print_msg_syntax_done:
        ret

print_hex_byte_local:
        push {lr}
        add  r1,r0,#0
        shr  r0,r1,#4
        li   r2,#10
        cmp  r0,r2
        bltu print_hex_local_hi_digit
        add  r0,r0,#55
        br   print_hex_local_hi_out
print_hex_local_hi_digit:
        add  r0,r0,#48
print_hex_local_hi_out:
        st   r0,[a0]
        and  r0,r1,#0x0F
        li   r2,#10
        cmp  r0,r2
        bltu print_hex_local_lo_digit
        add  r0,r0,#55
        br   print_hex_local_lo_out
print_hex_local_lo_digit:
        add  r0,r0,#48
print_hex_local_lo_out:
        st   r0,[a0]
        pop  {lr}
        ret

print_hex_word_local:
        push {lr}
        gta  r0,r1,a2
        push {r1}
        bl  print_hex_byte_local
        pop  {r1}
        add  r0,r1,#0
        bl  print_hex_byte_local
        pop  {lr}
        ret

parse_hex_local:
        push {lr}
        bl  parse_hex_bridge
        pop  {lr}
        ret

go_parse_mid:
        push {lr}
        bl   parse_hex_local
        pop  {lr}
        ret

; M address [value ...]. Without values, examine one byte.
boot_bridge_b: br boot_bridge_c
dump_bridge_b: br dump_bridge_c
fill_bridge_b: br fill_bridge_c
memory_bridge_b: br command_memory
registers_bridge_b: br registers_bridge_c
go_bridge_b: br go_bridge_c
command_memory:
        push {lr}
        bl  parse_hex_local
        li   r1,#1
        cmp  r0,r1
        bne  command_syntax
        mva  lr,a2
        ld   r0,[a1]
        cmp  r0,r3
        beq  memory_examine
memory_values:
        push {lr}
        bl  parse_hex_local
        pop  {lr}
        li   r1,#1
        cmp  r0,r1
        bne  memory_values_done
        gta  r0,r1,a2
        mva  a2,lr
        st   r1,[a2]
        ada  a2,#1
        mva  lr,a2
        br   memory_values
memory_values_done:
        ld   r0,[a1]
        cmp  r0,r3
        bne  command_syntax
        br   command_done
memory_examine:
        mva  a2,lr
        bl  print_hex_word_local
        li   r0,#':'
        st   r0,[a0]
        li   r0,#' '
        st   r0,[a0]
        ld   r0,[a2]
        bl  print_hex_byte_local
        li   r0,#0x0A
        st   r0,[a0]
        br   command_done

; D start [end], inclusive. A missing end defaults to 15 more bytes.
done_bridge_2: br done_bridge_1
go_parse_mid2: br go_parse_mid
go_syntax_mid: br syntax_fill_mid
syntax_fill_mid: br command_syntax
boot_bridge_c: br boot_bridge_d
dump_bridge_c: br command_dump
fill_bridge_c: br command_fill
registers_bridge_c: br registers_bridge_d
go_bridge_c: br go_bridge_d
command_dump:
        push {lr}
        bl  parse_hex_local
        li   r1,#1
        cmp  r0,r1
        bne  command_syntax
        mva  a3,a2
        push {lr}
        bl  parse_hex_local
        pop  {lr}
        li   r1,#1
        cmp  r0,r1
        beq  dump_have_end
        mva  a2,a3
        ada  a2,#15
        mva  lr,a2
        mva  a2,a3
dump_have_end:
        mva  lr,a2
        mva  a2,a3
dump_loop:
        mva  a3,lr
        gta  r0,r1,a2
        gta  r2,r3,a3
        cmp  r0,r2
        bltu dump_emit
        bne  command_done
        cmp  r1,r3
        bltu dump_emit
        bne  command_done
dump_emit:
        push {lr}
        bl  print_hex_word_local
        pop  {lr}
        li   r0,#':'
        st   r0,[a0]
        li   r0,#' '
        st   r0,[a0]
        ld   r0,[a2]
        push {lr}
        bl  print_hex_byte_local
        pop  {lr}
        li   r0,#0x0A
        st   r0,[a0]
        ada  a2,#1
        br   dump_loop

; F start end value, inclusive.
boot_bridge_d: br boot_bridge_e
syntax_fill_bridge: br syntax_fill_mid
command_fill:
        push {lr}
        bl  parse_hex_local
        li   r1,#1
        cmp  r0,r1
        bne  syntax_fill_bridge
        mva  lr,a2
        push {lr}
        bl  parse_hex_local
        pop  {lr}
        li   r1,#1
        cmp  r0,r1
        bne  syntax_fill_bridge
        mva  a3,a2
        push {lr}
        bl  parse_hex_local
        pop  {lr}
        li   r1,#1
        cmp  r0,r1
        bne  syntax_fill_bridge
        li   r1,#hi(INPUT_BUFFER)
        li   r2,#lo(INPUT_BUFFER)
        lda  a1,r1,r2
        gta  r0,r1,a2
        st   r1,[a1]
        mva  a2,lr
fill_loop:
        gta  r1,r2,a2
        gta  r0,r3,a3
        cmp  r1,r0
        bltu fill_emit
        bne  command_done
        cmp  r2,r3
        bltu fill_emit
        bne  command_done
fill_emit:
        ld   r0,[a1]
        st   r0,[a2]
        ada  a2,#1
        br   fill_loop

done_bridge_3: br done_bridge_2
command_done:
        pop  {lr}
        li   r1,#hi(CONSOLE_STATUS)
        li   r2,#lo(CONSOLE_STATUS)
        lda  a3,r1,r2
        br   done_bridge_3

command_return:
        li   r1,#hi(CONSOLE_STATUS)
        li   r2,#lo(CONSOLE_STATUS)
        lda  a3,r1,r2
        br   done_bridge_3

registers_bridge_d: br command_registers
go_bridge_d: br command_go
go_parse_near: br go_parse_mid1
go_parse_mid1: br go_parse_mid2
go_syntax_near: br go_syntax_mid

print_msg_late:
        ld   r0,[a1]
        cmp  r0,r3
        beq  print_msg_late_done
        st   r0,[a0]
        ada  a1,#1
        br   print_msg_late
print_msg_late_done:
        ret

print_hex_byte_late:
        push {lr}
        add  r1,r0,#0
        shr  r0,r1,#4
        li   r2,#10
        cmp  r0,r2
        bltu print_hex_late_hi_digit
        add  r0,r0,#55
        br   print_hex_late_hi_out
print_hex_late_hi_digit:
        add  r0,r0,#48
print_hex_late_hi_out:
        st   r0,[a0]
        and  r0,r1,#0x0F
        li   r2,#10
        cmp  r0,r2
        bltu print_hex_late_lo_digit
        add  r0,r0,#55
        br   print_hex_late_lo_out
print_hex_late_lo_digit:
        add  r0,r0,#48
print_hex_late_lo_out:
        st   r0,[a0]
        pop  {lr}
        ret

print_hex_word_late:
        push {lr}
        gta  r0,r1,a2
        push {r1}
        bl  print_hex_byte_late
        pop  {r1}
        add  r0,r1,#0
        bl  print_hex_byte_late
        pop  {lr}
        ret

registers_return_bridge: br command_return

command_go:
        push {lr}
        bl   go_parse_near
        li   r1,#1
        cmp  r0,r1
        bne  go_syntax_near
        ld   r0,[a1]
        cmp  r0,r3
        bne  go_syntax_near
        ; parse_hex_local leaves the destination in A2. JA does not alter LR.
        ja   a2

; BOOT reads raw sector zero into RAM and transfers control to it.
boot_bridge_e: br command_boot
command_boot:
        li   r1,#hi(FLOPPY_COMMAND)
        li   r2,#lo(FLOPPY_COMMAND)
        lda  a1,r1,r2
        li   r0,#0
        st   r0,[a1+2]
        st   r0,[a1+3]
        li   r0,#FLOPPY_CMD_READ
        st   r0,[a1]
boot_wait:
        ld   r0,[a1+1]
        and  r0,r0,#FLOPPY_STATUS_ERROR
        bne  boot_error
        ld   r0,[a1+1]
        and  r0,r0,#FLOPPY_STATUS_READY
        beq  boot_wait
        li   r1,#hi(BOOT_BUFFER)
        li   r2,#lo(BOOT_BUFFER)
        lda  a2,r1,r2
        li   r2,#0
        li   r3,#0
boot_copy:
        ld   r0,[a1+4]
        st   r0,[a2]
        ada  a2,#1
        add  r2,r2,#1
        cmp  r2,r3
        bne  boot_copy
        li   r1,#hi(BOOT_BUFFER)
        li   r2,#lo(BOOT_BUFFER)
        lda  a2,r1,r2
        ja   a2
boot_error:
        li   r3,#0
        li   r1,#hi(msg_boot_error)
        li   r2,#lo(msg_boot_error)
        lda  a1,r1,r2
        bl  print_msg_late
        br   command_return

print_hex_word_regs:
        push {lr}
        gta  r0,r1,a2
        push {r1}
        bl  print_hex_byte_regs
        pop  {r1}
        add  r0,r1,#0
        bl  print_hex_byte_regs
        pop  {lr}
        ret

print_hex_byte_regs:
        push {lr}
        add  r1,r0,#0
        shr  r0,r1,#4
        li   r2,#10
        cmp  r0,r2
        bltu print_hex_regs_hi_digit
        add  r0,r0,#55
        br   print_hex_regs_hi_out
print_hex_regs_hi_digit:
        add  r0,r0,#48
print_hex_regs_hi_out:
        st   r0,[a0]
        and  r0,r1,#0x0F
        li   r2,#10
        cmp  r0,r2
        bltu print_hex_regs_lo_digit
        add  r0,r0,#55
        br   print_hex_regs_lo_out
print_hex_regs_lo_digit:
        add  r0,r0,#48
print_hex_regs_lo_out:
        st   r0,[a0]
        pop  {lr}
        ret

print_msg_regs:
        ld   r0,[a1]
        cmp  r0,r3
        beq  print_msg_regs_done
        st   r0,[a0]
        ada  a1,#1
        br   print_msg_regs
print_msg_regs_done:
        ret

registers_return_mid: br registers_return_bridge
command_registers:
        push {r0,r1,r2,r3}
        li   r0,#'A'
        st   r0,[a0]
        li   r0,#'2'
        st   r0,[a0]
        li   r0,#'='
        st   r0,[a0]
        bl  print_hex_word_regs
        li   r0,#' '
        st   r0,[a0]
        mva  a2,a0
        li   r0,#'A'
        st   r0,[a0]
        li   r0,#'0'
        st   r0,[a0]
        li   r0,#'='
        st   r0,[a0]
        mva  a2,a0
        bl  print_hex_word_regs
        li   r0,#' '
        st   r0,[a0]
        mva  a2,a1
        li   r0,#'A'
        st   r0,[a0]
        li   r0,#'1'
        st   r0,[a0]
        li   r0,#'='
        st   r0,[a0]
        mva  a2,a1
        bl  print_hex_word_regs
        li   r0,#' '
        st   r0,[a0]
        mva  a2,a3
        li   r0,#'A'
        st   r0,[a0]
        li   r0,#'3'
        st   r0,[a0]
        li   r0,#'='
        st   r0,[a0]
        mva  a2,a3
        bl  print_hex_word_regs
        li   r0,#0x0A
        st   r0,[a0]
        mva  a2,sp
        ada  a2,#4
        li   r0,#'S'
        st   r0,[a0]
        li   r0,#'P'
        st   r0,[a0]
        li   r0,#'='
        st   r0,[a0]
        bl  print_hex_word_regs
        li   r0,#0x0A
        st   r0,[a0]
        mva  a2,sp
        li   r0,#'R'
        st   r0,[a0]
        li   r0,#'0'
        st   r0,[a0]
        li   r0,#'='
        st   r0,[a0]
        ld   r0,[a2+3]
        bl  print_hex_byte_regs
        li   r0,#' '
        st   r0,[a0]
        li   r0,#'R'
        st   r0,[a0]
        li   r0,#'1'
        st   r0,[a0]
        li   r0,#'='
        st   r0,[a0]
        ld   r0,[a2+2]
        bl  print_hex_byte_regs
        li   r0,#' '
        st   r0,[a0]
        li   r0,#'R'
        st   r0,[a0]
        li   r0,#'2'
        st   r0,[a0]
        li   r0,#'='
        st   r0,[a0]
        ld   r0,[a2+1]
        bl  print_hex_byte_regs
        li   r0,#' '
        st   r0,[a0]
        li   r0,#'R'
        st   r0,[a0]
        li   r0,#'3'
        st   r0,[a0]
        li   r0,#'='
        st   r0,[a0]
        ld   r0,[a2]
        bl  print_hex_byte_regs
        li   r0,#0x0A
        st   r0,[a0]
        gf   r0
        li   r1,#'S'
        st   r1,[a0]
        li   r1,#'0'
        st   r1,[a0]
        li   r1,#'='
        st   r1,[a0]
        bl  print_hex_byte_regs_tail
        li   r0,#0x0A
        st   r0,[a0]
        li   r3,#0
        li   r1,#hi(msg_register_limit)
        li   r2,#lo(msg_register_limit)
        lda  a1,r1,r2
        bl  print_msg_regs
        pop  {r0,r1,r2,r3}
        br   registers_return_mid

print_hex_byte_regs_tail:
        push {lr}
        add  r1,r0,#0
        shr  r0,r1,#4
        li   r2,#10
        cmp  r0,r2
        bltu print_hex_tail_hi_digit
        add  r0,r0,#55
        br   print_hex_tail_hi_out
print_hex_tail_hi_digit:
        add  r0,r0,#48
print_hex_tail_hi_out:
        st   r0,[a0]
        and  r0,r1,#0x0F
        li   r2,#10
        cmp  r0,r2
        bltu print_hex_tail_lo_digit
        add  r0,r0,#55
        br   print_hex_tail_lo_out
print_hex_tail_lo_digit:
        add  r0,r0,#48
print_hex_tail_lo_out:
        st   r0,[a0]
        pop  {lr}
        ret

_alignment:
        li   r1,#hi(CONSOLE_DATA)
        li   r2,#lo(CONSOLE_DATA)
        lda  a0,r1,r2
        li   r1,#hi(msg_alignment)
        li   r2,#lo(msg_alignment)
        lda  a1,r1,r2
        li   r3,#0
alignment_print:
        ld   r0,[a1]
        cmp  r0,r3
        beq  alignment_halt
        st   r0,[a0]
        ada  a1,#1
        br   alignment_print
alignment_halt:
        halt
_catchall: halt
_irq1: br _catchall
_irq2: br _catchall
_irq3: br _catchall
_irq4: br _catchall
_irq5: br _catchall
_irq6: br _catchall
_irq7: br _catchall

.section .rodata
cmd_help: .asciz "help"
cmd_boot: .asciz "boot"
msg0: .asciz "RIKMON Monitor v1.0\n"
msg1: .asciz "Type HELP for commands\n"
prompt: .asciz "> "
msg_goodbye: .asciz "Goodbye ...\n"
msg_help: .asciz "M addr [value...]  D start [end]  F start end value\nR  G addr  BOOT  HELP  Q\n"
msg_error: .asciz "? unknown command\n"
msg_syntax: .asciz "? syntax\n"
msg_registers: .asciz "Registers: R0-R3 and A0-A3 are available\n"
msg_register_limit: .asciz "LR/SP/PC require debugger inspection in ISA 1.0\n"
msg_alignment: .asciz "Alignment exception\n"
msg_boot_error: .asciz "BOOT error: no readable sector 0\n"

.org ALIGNMENT_VECTOR
        .word _alignment
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
