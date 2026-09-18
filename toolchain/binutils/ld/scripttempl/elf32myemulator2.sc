cat <<EOF
/* Default MyEmulator2 bare-metal linker script. */
OUTPUT_FORMAT("$OUTPUT_FORMAT")
OUTPUT_ARCH($ARCH)
ENTRY(_start)

SECTIONS
{
  . = 0x00100000;
  __text_start = .;
  .text ALIGN(4) :
  {
    *(.text .text.*)
  }
  __text_end = .;

  . = ALIGN(16);
  __rodata_start = .;
  .rodata :
  {
    *(.rodata .rodata.*)
  }
  __rodata_end = .;

  . = ALIGN(16);
  __data_start = .;
  .data :
  {
    *(.data .data.*)
  }
  __data_end = .;

  . = ALIGN(16);
  __bss_start = .;
  .bss (NOLOAD) :
  {
    *(.bss .bss.* COMMON)
  }
  __bss_end = .;
  __image_end = .;
}
EOF
