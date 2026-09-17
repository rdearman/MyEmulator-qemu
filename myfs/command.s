.include "../rikmon/include/myemulator.inc"

; COMMAND.COM for MyFS v1.  This is a raw .COM image loaded at 0400.
; It uses 0300-03ff as a sector buffer and e000-e1ff as workspace.

.equ FS_BUFFER,   0x0300
.equ PROGRAM_LOAD,0x2000
.equ INPUT,       0xE000
.equ NAME,        0xE080
.equ FS_START,    0xE100
.equ FS_SIZE,     0xE102
.equ HEX_TEMP,    0xE104
.equ HEX_LOW,     0xE105
.equ LINE_LIMIT,  63

.org 0x0400
entry:
        la   a1,#msg_banner,r2,r3
        call print_string

command_loop:
        la   a1,#msg_prompt,r2,r3
        call print_string
        call read_line

        la   a1,#INPUT,r2,r3
        la   a0,#word_dir,r2,r3
        call match_word
        or   r0,r0
        bne  dispatch_dir
        la   a1,#INPUT,r2,r3
        la   a0,#word_type,r2,r3
        call match_word
        or   r0,r0
        bne  dispatch_type
        la   a1,#INPUT,r2,r3
        la   a0,#word_run,r2,r3
        call match_word
        or   r0,r0
        bne  dispatch_run
        la   a1,#INPUT,r2,r3
        la   a0,#word_cls,r2,r3
        call match_word
        or   r0,r0
        bne  dispatch_cls
        la   a1,#INPUT,r2,r3
        la   a0,#word_ver,r2,r3
        call match_word
        or   r0,r0
        bne  dispatch_ver
        la   a1,#INPUT,r2,r3
        la   a0,#word_help,r2,r3
        call match_word
        or   r0,r0
        bne  dispatch_help
        la   a1,#INPUT,r2,r3
        ld   r0,[a1]
        li   r1,#0
        cmp  r0,r1
        beq  restart
        la   a1,#msg_bad_command,r2,r3
        call print_string
        br   restart

dispatch_dir:   call command_dir_impl
                la a2,#command_loop,r0,r1
                ja a2
dispatch_type:  call command_type_impl
                la a2,#command_loop,r0,r1
                ja a2
dispatch_run:   call command_run_impl
                la a2,#command_loop,r0,r1
                ja a2
dispatch_cls:   call command_cls_impl
                la a2,#command_loop,r0,r1
                ja a2
dispatch_ver:   call command_ver_impl
                la a2,#command_loop,r0,r1
                ja a2
dispatch_help:  call command_help_impl
                la a2,#command_loop,r0,r1
                ja a2

restart:
        la   a2,#command_loop,r0,r1
        ja   a2

command_dir_impl:
        call find_directory
        la   a1,#FS_BUFFER,r2,r3
dir_loop:
        ld   r0,[a1]
        or   r0,r0
        beq  restart
        mva  a2,a1
        mva  a1,a2
        la   a0,#CONSOLE_DATA,r2,r3
        li   r3,#11
dir_name:
        ld   r0,[a1]
        st   r0,[a0]
        ada  a1,#1
        sub  r3,r3,#1
        bne  dir_name
        li   r0,#' '
        st   r0,[a0]
        ; Save the next entry pointer before calls reuse A2 as their
        ; scratch address register.
        mva  a2,a1
        ada  a2,#5
        gta  r1,r0,a2
        la   a0,#FS_SIZE,r2,r3
        st   r0,[a0]
        st   r1,[a0+1]
        ; A1 is at entry+11; the start sector is at entry+12 and the
        ; exact size is at entry+14.
        ada  a1,#1
        ld   r2,[a1]
        ld   r3,[a1+1]
        ada  a1,#2
        ld   r0,[a1]
        ld   r1,[a1+1]
        ; Keep the low byte separate: print_hex_byte uses HEX_TEMP itself.
        la   a0,#HEX_LOW,r2,r3
        st   r0,[a0]
        add  r0,r1,#0
        call print_hex_byte
        la   a0,#HEX_LOW,r2,r3
        ld   r0,[a0]
        call print_hex_byte
        la   a1,#msg_nl,r0,r1
        call print_string
        la   a0,#FS_SIZE,r0,r1
        ld   r0,[a0]
        ld   r1,[a0+1]
        lda  a1,r1,r0
        ld   r0,[a1]
        or   r0,r0
        bne  dir_loop
        la   a2,#command_loop,r0,r1
        ja   a2

command_type_impl:
        la   a1,#INPUT,r2,r3
        ada  a1,#5
        call make_name
        call find_file
        or   r0,r0
        beq  command_not_found
        call output_file
        la   a2,#command_loop,r0,r1
        ja   a2

command_run_impl:
        la   a1,#INPUT,r2,r3
        ada  a1,#4
        call make_name
        call find_file
        or   r0,r0
        beq  command_not_found
        call load_file
        push {lr}
        la   a2,#PROGRAM_LOAD,r0,r1
        jla  a2
        pop  {lr}
        la   a2,#command_loop,r0,r1
        ja   a2

command_cls_impl:
        la   a1,#msg_cls,r2,r3
        call print_string
        la   a2,#command_loop,r0,r1
        ja   a2
command_ver_impl:
        la   a1,#msg_version,r2,r3
        call print_string
        la   a2,#command_loop,r0,r1
        ja   a2
command_help_impl:
        la   a1,#msg_help,r2,r3
        call print_string
        la   a2,#command_loop,r0,r1
        ja   a2
command_not_found:
        la   a1,#msg_not_found,r2,r3
        call print_string
        la   a2,#command_loop,r0,r1
        ja   a2

; Read and uppercase one bounded line.  LF or CR terminates the line.
read_line:
        la   a0,#CONSOLE_DATA,r2,r3
        la   a3,#CONSOLE_STATUS,r2,r3
        la   a2,#INPUT,r2,r3
        la   a0,#CONSOLE_DATA,r2,r3
        la   a3,#CONSOLE_STATUS,r2,r3
        li   r2,#0
read_wait:
        ld   r0,[a3]
        and  r0,r0,#CONSOLE_RX_READY
        beq  read_wait
        ld   r0,[a0]
        li   r1,#10
        cmp  r0,r1
        beq  read_done
        li   r1,#13
        cmp  r0,r1
        beq  read_done
        li   r1,#8
        cmp  r0,r1
        beq  read_backspace
        li   r1,#127
        cmp  r0,r1
        beq  read_backspace
        li   r1,#'a'
        cmp  r0,r1
        bltu read_store
        li   r1,#'{'
        cmp  r0,r1
        bge  read_store
        sub  r0,r0,#32
read_store:
        li   r1,#LINE_LIMIT
        cmp  r2,r1
        bge  read_wait
        st   r0,[a2]
        st   r0,[a0]
        ada  a2,#1
        add  r2,r2,#1
        br   read_wait
read_backspace:
        or   r2,r2
        beq  read_wait
        ada  a2,#-1
        sub  r2,r2,#1
        li   r0,#8
        st   r0,[a0]
        li   r0,#' '
        st   r0,[a0]
        li   r0,#8
        st   r0,[a0]
        br   read_wait
read_done:
        li   r0,#0
        st   r0,[a2]
        li   r0,#10
        st   r0,[a0]
        ret

; Compare an upper-case command word.  A1 is input and A0 is NUL-terminated
; word.  Return R0=1 on a complete word, otherwise R0=0.
match_word:
match_loop:
        ld   r1,[a0]
        or   r1,r1
        beq  match_end
        ld   r0,[a1]
        cmp  r0,r1
        bne  match_fail
        ada  a1,#1
        ada  a0,#1
        br   match_loop
match_end:
        ld   r0,[a1]
        or   r0,r0
        beq  match_yes
        li   r1,#' '
        cmp  r0,r1
        bne  match_fail
match_yes:
        li   r0,#1
        ret
match_fail:
        li   r0,#0
        ret

; Convert the argument at A1 to an upper-case, space-padded 8.3 name.
make_name:
        la   a2,#NAME,r2,r3
        li   r2,#11
make_fill:
        li   r0,#' '
        st   r0,[a2]
        ada  a2,#1
        sub  r2,r2,#1
        bne  make_fill
        la   a2,#NAME,r2,r3
        li   r2,#8
make_base:
        ld   r0,[a1]
        or   r0,r0
        beq  make_done
        li   r1,#' '
        cmp  r0,r1
        beq  make_done
        li   r1,#'.'
        cmp  r0,r1
        beq  make_ext
        st   r0,[a2]
        ada  a1,#1
        ada  a2,#1
        sub  r2,r2,#1
        bne  make_base
make_skip_base:
        ld   r0,[a1]
        or   r0,r0
        beq  make_done
        li   r1,#'.'
        cmp  r0,r1
        beq  make_ext
        ada  a1,#1
        br   make_skip_base
make_ext:
        ada  a1,#1
        la   a2,#NAME+8,r2,r3
        li   r2,#3
make_ext_loop:
        ld   r0,[a1]
        or   r0,r0
        beq  make_done
        li   r1,#' '
        cmp  r0,r1
        beq  make_done
        st   r0,[a2]
        ada  a1,#1
        ada  a2,#1
        sub  r2,r2,#1
        bne  make_ext_loop
make_done:
        ret

; Read one 256-byte sector.  R0:R1 is the sector and A1 is the destination.
read_sector:
        la   a3,#FLOPPY_COMMAND,r2,r3
        st   r0,[a3+2]
        st   r1,[a3+3]
        li   r0,#FLOPPY_CMD_READ
        st   r0,[a3]
read_sector_wait:
        ld   r0,[a3+1]
        and  r0,r0,#FLOPPY_STATUS_ERROR
        bne  read_sector_fail
        ld   r0,[a3+1]
        and  r0,r0,#FLOPPY_STATUS_READY
        beq  read_sector_wait
        li   r1,#0
read_sector_copy:
        ld   r0,[a3+4]
        st   r0,[a1]
        ada  a1,#1
        add  r1,r1,#1
        bne  read_sector_copy
        ret
read_sector_fail:
        la   a1,#msg_disk_error,r0,r1
        call print_string
        la   a2,#command_loop,r0,r1
        ja   a2

find_directory:
        la   a1,#FS_BUFFER,r0,r1
        li   r0,#2
        li   r1,#0
        call read_sector
        ret

; Find NAME in the fixed directory.  Save start and exact length in scratch.
find_file:
        call find_directory
        la   a1,#FS_BUFFER,r0,r1
        li   r2,#16
find_entry:
        ld   r0,[a1]
        or   r0,r0
        beq  find_no
        mva  a2,a1
        la   a0,#NAME,r0,r1
        li   r3,#11
find_name:
        ld   r0,[a1]
        ld   r1,[a0]
        cmp  r0,r1
        bne  find_next
        ada  a1,#1
        ada  a0,#1
        sub  r3,r3,#1
        bne  find_name
        mva  a1,a2
        ada  a1,#12
        la   a0,#FS_START,r0,r1
        ld   r2,[a1]
        ld   r3,[a1+1]
        st   r2,[a0]
        st   r3,[a0+1]
        ada  a1,#2
        la   a0,#FS_SIZE,r0,r1
        ld   r2,[a1]
        ld   r3,[a1+1]
        st   r2,[a0]
        st   r3,[a0+1]
        li   r0,#1
        ret
find_next:
        mva  a1,a2
        ada  a1,#16
        sub  r2,r2,#1
        bne  find_entry
find_no:
        li   r0,#0
        ret

load_file:
        la   a1,#PROGRAM_LOAD,r2,r3
        la   a0,#FS_SIZE,r2,r3
        ld   r0,[a0]
        ld   r1,[a0+1]
        li   r2,#0
        lda  a0,r2,r1
        or   r0,r0
        beq  load_sector_start
        ada  a0,#1
load_count:
load_sector_start:
        la   a2,#FS_START,r2,r3
        ld   r0,[a2]
        ld   r1,[a2+1]
        gta  r2,r3,a0
        or   r2,r3
        beq  load_done
        call read_sector
        la   a2,#FS_START,r2,r3
        ld   r0,[a2]
        ld   r1,[a2+1]
        add  r0,r0,#1
        bne  load_store_sector
        add  r1,r1,#1
load_store_sector:
        st   r0,[a2]
        st   r1,[a2+1]
load_sector:
        ada  a0,#-1
        br   load_count
load_done:
        ret

; Output exactly FS_SIZE bytes, using one sector buffer at a time.
output_file:
        la   a0,#FS_START,r2,r3
        ld   r0,[a0]
        ld   r1,[a0+1]
output_sector:
        la   a1,#FS_BUFFER,r2,r3
        call read_sector
        la   a2,#FS_START,r2,r3
        ld   r0,[a2]
        ld   r1,[a2+1]
        add  r0,r0,#1
        bne  output_store_sector
        add  r1,r1,#1
output_store_sector:
        st   r0,[a2]
        st   r1,[a2+1]
        la   a0,#FS_SIZE,r2,r3
        la   a1,#FS_BUFFER,r0,r1
        la   a0,#CONSOLE_DATA,r0,r1
        la   a2,#FS_SIZE,r0,r1
        ld   r2,[a2]
        ld   r3,[a2+1]
        li   r0,#0
output_byte:
        or   r2,r3
        beq  output_done
        ld   r1,[a1]
        st   r1,[a0]
        ada  a1,#1
        add  r0,r0,#1
        or   r2,r2
        beq  output_borrow
        sub  r2,r2,#1
        br   output_byte_more
output_borrow:
        sub  r3,r3,#1
        li   r2,#255
output_byte_more:
        or   r0,r0
        bne  output_byte
        st   r2,[a2]
        st   r3,[a2+1]
        or   r2,r3
        beq  output_done
        la   a0,#FS_START,r1,r2
        ld   r0,[a0]
        ld   r1,[a0+1]
        br   output_sector
output_done:
        ret

print_char:
        la   a0,#CONSOLE_DATA,r2,r3
        st   r0,[a0]
        ret

print_string:
print_string_loop:
        ld   r0,[a1]
        or   r0,r0
        beq  print_string_done
        call print_char
        ada  a1,#1
        br   print_string_loop
print_string_done:
        ret

print_hex_nibble:
        li   r1,#10
        cmp  r0,r1
        bltu print_hex_digit
        add  r0,r0,#7
print_hex_digit:
        add  r0,r0,#48
        call print_char
        ret

print_hex_byte:
        la   a1,#HEX_TEMP,r2,r3
        st   r0,[a1]
        shr  r0,r0,#4
        call print_hex_nibble
        la   a1,#HEX_TEMP,r2,r3
        ld   r0,[a1]
        and  r0,r0,#15
        call print_hex_nibble
        ret

msg_banner:      .asciz "MyEmulator 1.0\nMyFS 1.0\n"
msg_prompt:      .asciz "A:\\> "
msg_bad_command: .asciz "Bad command\n"
msg_not_found:   .asciz "File not found\n"
msg_disk_error:  .asciz "Disk error\n"
msg_nl:          .asciz "\n"
msg_version:     .asciz "MyEmulator 1.0\nMyFS 1.0\n"
msg_help:        .asciz "DIR TYPE RUN CLS VER HELP\n"
msg_cls:         .asciz "\x1b[2J\x1b[H"
word_dir:        .asciz "DIR"
word_type:       .asciz "TYPE"
word_run:        .asciz "RUN"
word_cls:        .asciz "CLS"
word_ver:        .asciz "VER"
word_help:       .asciz "HELP"
