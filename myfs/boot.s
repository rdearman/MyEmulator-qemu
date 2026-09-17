.include "../rikmon/include/myemulator.inc"

; MyFS v1 sector-0 loader.  RIKMON copies this sector to 0200 and jumps to it.
; The loader deliberately knows only the fixed header and directory format:
; it finds COMMAND.COM by its directory name, rather than by a fixed sector.

.equ FS_BUFFER,       0x0300
.equ COMMAND_LOAD,    0x0400
.equ COMMAND_LIMIT_HI,0x1c       ; COMMAND.COM must fit below 2000

.org 0x0200
boot_start:
        la   a3,#FLOPPY_COMMAND,r2,r3
        la   a1,#FS_BUFFER,r2,r3
        li   r2,#1
        li   r3,#0
        bl   boot_read_sector

        ; Validate the signature and version in the header.  V1 fixes the
        ; directory at sector 2, so the boot sector need not parse metadata.
        la   a0,#FS_BUFFER,r2,r3
        la   a1,#boot_magic,r2,r3
        li   r2,#5
boot_header_loop:
        ld   r0,[a0]
        ld   r1,[a1]
        cmp  r0,r1
        bne  boot_error
        ada  a0,#1
        ada  a1,#1
        sub  r2,r2,#1
        bne  boot_header_loop

        ; Read the fixed directory sector into the sector buffer.
        la   a1,#FS_BUFFER,r0,r1
        li   r2,#2
        li   r3,#0
        bl   boot_read_sector

        ; Scan fixed 16-byte entries for canonical COMMAND COM.
        la   a0,#boot_name,r2,r3
        la   a1,#FS_BUFFER,r2,r3
boot_scan:
        ld   r0,[a1]
        or   r0,r0
        beq  boot_no_command
        mva  a2,a1
        la   a0,#boot_name,r2,r3
        li   r2,#11
boot_name_loop:
        ld   r0,[a1]
        ld   r1,[a0]
        cmp  r0,r1
        bne  boot_next_entry
        ada  a1,#1
        ada  a0,#1
        sub  r2,r2,#1
        bne  boot_name_loop
        br   boot_found
boot_next_entry:
        mva  a1,a2
        ada  a1,#16
        br   boot_scan

boot_found:
        mva  a1,a2
        ada  a1,#12
        ld   r2,[a1]
        ld   r3,[a1+1]
        lda  a0,r3,r2
        ada  a1,#2
        ld   r0,[a1]
        ld   r1,[a1+1]
        ; Keep the shell below 2000, leaving the transient area free.
        li   r2,#COMMAND_LIMIT_HI
        cmp  r1,r2
        bge  boot_command_large
        ; Keep the file start sector in A0.  A2 is the remaining-sector
        ; counter so the la pseudo-operation below can use R2/R3 as scratch
        ; without destroying the source sector.
        li   r2,#0
        lda  a2,r2,r1
        or   r0,r0
        beq  boot_size_even
        ada  a2,#1
boot_size_even:
        la   a1,#COMMAND_LOAD,r2,r3
        gta  r3,r2,a0
boot_load_loop:
        gta  r0,r1,a2
        or   r0,r1
        beq  boot_loaded
        bl   boot_read_sector
        add  r2,r2,#1
        bne  boot_sector_no_carry
        add  r3,r3,#1
boot_sector_no_carry:
        ada  a2,#-1
        br   boot_load_loop
boot_loaded:
        la   a2,#COMMAND_LOAD,r0,r1
        ja   a2

; Read one complete sector.  A1 is the destination and R2:R3 the sector.
boot_read_sector:
        push {lr}
        st   r2,[a3+2]
        st   r3,[a3+3]
        li   r0,#FLOPPY_CMD_READ
        st   r0,[a3]
boot_wait:
        ld   r0,[a3+1]
        and  r0,r0,#FLOPPY_STATUS_ERROR
        bne  boot_io_error
        ld   r0,[a3+1]
        and  r0,r0,#FLOPPY_STATUS_READY
        beq  boot_wait
        li   r1,#0
boot_copy:
        ld   r0,[a3+4]
        st   r0,[a1]
        ada  a1,#1
        add  r1,r1,#1
        bne  boot_copy
        pop  {lr}
        ret

boot_no_command:
boot_command_large:
boot_io_error:
        la   a0,#CONSOLE_DATA,r2,r3
        li   r0,#'E'
        st   r0,[a0]
        li   r0,#'\n'
        st   r0,[a0]
        halt

boot_error:     br   boot_no_command
boot_magic:     .ascii "MYFS\x01"
boot_name:      .ascii "COMMAND COM"
