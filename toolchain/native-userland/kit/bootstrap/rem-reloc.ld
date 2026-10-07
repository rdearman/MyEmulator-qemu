SECTIONS
{
  .text 0 : { *(.text .text.*) }
  .rodata 0 : { *(.rodata .rodata.*) }
  .data 0 : { *(.data .data.*) }
  .bss 0 : { *(.bss .bss.*) *(COMMON) }
}
