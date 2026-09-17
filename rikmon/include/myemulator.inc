/*
 * MyEmulator 1.0 hardware definitions
 */

; ------------------------------------------------------------
; Memory map
; ------------------------------------------------------------

.equ RAM_START,              0x0000
.equ RAM_END,                0xEFFF
.equ STACK_TOP,              0xF000

.equ MMIO_START,             0xF000
.equ MMIO_END,               0xF0FF

.equ ROM_START,              0xF100
.equ ROM_END,                0xFFFF
.equ ROM_SIZE,               0x0F00


; ------------------------------------------------------------
; Floppy controller
; ------------------------------------------------------------

.equ FLOPPY_COMMAND,         0xF000
.equ FLOPPY_STATUS,          0xF001
.equ FLOPPY_SECTOR_LO,       0xF002
.equ FLOPPY_SECTOR_HI,       0xF003
.equ FLOPPY_DATA,            0xF004
.equ FLOPPY_ERROR,           0xF005

; COMMAND values

.equ FLOPPY_CMD_READ,        0x01
.equ FLOPPY_CMD_WRITE,       0x02

; STATUS bits

.equ FLOPPY_STATUS_BUSY,     0x01
.equ FLOPPY_STATUS_READY,    0x02
.equ FLOPPY_STATUS_ERROR,    0x04


; ------------------------------------------------------------
; Console controller
; ------------------------------------------------------------

.equ CONSOLE_DATA,           0xF010
.equ CONSOLE_STATUS,         0xF011
.equ CONSOLE_CONTROL,        0xF012
.equ CONSOLE_IRQ_STATUS,     0xF013
.equ CONSOLE_RX_READY, 	     0x01
.equ CONSOLE_TX_READY, 	     0x02

; 16-bit virtual-time countdown timer, one millisecond per count.
.equ TIMER_CONTROL,          0xF020
.equ TIMER_STATUS,           0xF021
.equ TIMER_COUNT_LO,         0xF022
.equ TIMER_COUNT_HI,         0xF023
.equ TIMER_RELOAD_LO,        0xF024
.equ TIMER_RELOAD_HI,        0xF025
.equ TIMER_ENABLE,           0x01
.equ TIMER_PERIODIC,         0x02
.equ TIMER_IRQ_ENABLE,       0x04
.equ TIMER_EXPIRED,          0x01
.equ TIMER_RUNNING,          0x02

; ------------------------------------------------------------
; Reserved MMIO
; ------------------------------------------------------------

.equ MMIO_RESERVED_START,    0xF026
.equ MMIO_RESERVED_END,      0xF0FF


; ------------------------------------------------------------
; CPU status register S0
; ------------------------------------------------------------

.equ S0_ZF,                  0x01
.equ S0_NF,                  0x02
.equ S0_CF,                  0x04
.equ S0_OF,                  0x08

.equ S0_IPL_MASK,            0x70


; ------------------------------------------------------------
; Interrupt vectors
; ------------------------------------------------------------

.equ ALIGNMENT_VECTOR,       0xFFEC
.equ IRQ1_VECTOR,            0xFFEE
.equ IRQ2_VECTOR,            0xFFF0
.equ IRQ3_VECTOR,            0xFFF2
.equ IRQ4_VECTOR,            0xFFF4
.equ IRQ5_VECTOR,            0xFFF6
.equ IRQ6_VECTOR,            0xFFF8
.equ IRQ7_VECTOR,            0xFFFA

.equ INITIAL_SP_VECTOR,      0xFFFC
.equ RESET_VECTOR,           0xFFFE
