; Built-in MyEmulator boot ROM, assembled at 0x0180.
; It reads sector 0 through the fictional floppy controller, copies the
; 256-byte buffer to 0x0100, and branches to the loaded sector.

FDC_COMMAND = 0xf000
FDC_STATUS = 0xf001
FDC_SECTOR_LO = 0xf002
FDC_DATA = 0xf004

.org 0x0180

start:
    li r0,#0
    la a0,#FDC_SECTOR_LO,r2,r3
    st r0,[a0]
    ada a0,#1
    st r0,[a0]

    ada a0,#-3
    li r0,#1
    st r0,[a0]
    ada a0,#1

wait_ready:
    ld r1,[a0]
    and r1,r1,#2
    beq wait_ready

    la a0,#FDC_DATA,r2,r3
    la a1,#0x0100,r2,r3
    li r2,#0

copy_sector:
    ld r0,[a0]
    st r0,[a1]
    ada a1,#1
    add r2,r2,#1
    bne copy_sector

    br 0x0100
