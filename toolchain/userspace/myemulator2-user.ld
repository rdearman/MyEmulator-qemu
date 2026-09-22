/*
 * Linker script for statically linked MyEmulator2 Linux programs.
 *
 * The ELF loader publishes AT_PHDR only when the program-header table is
 * covered by a PT_LOAD segment.  A bare -Ttext link normally starts the
 * first load segment at file offset 0x1000, leaving the ELF headers outside
 * the mapping.  That is valid for simple freestanding programs, but it
 * breaks musl startup and TLS initialization, which requires AT_PHDR.
 */
OUTPUT_FORMAT("elf32-littlemyemulator2")
OUTPUT_ARCH(myemulator2)
ENTRY(_start)

PHDRS
{
  text PT_LOAD FILEHDR PHDRS FLAGS(7);
  stack PT_GNU_STACK FLAGS(7);
}

SECTIONS
{
  . = 0x02000000 + SIZEOF_HEADERS;

  .text :
  {
    *(.text.startup)
    *(.text .text.*)
  } :text

  .fini : { *(.fini) } :text
  .rodata : { *(.rodata .rodata.*) } :text
  .eh_frame : { *(.eh_frame) } :text
  .preinit_array : { KEEP(*(.preinit_array)) } :text
  .init_array : { KEEP(*(.init_array)) } :text
  .fini_array : { KEEP(*(.fini_array)) } :text
  .data : { *(.data .data.*) } :text
  .data.rel.ro : { *(.data.rel.ro .data.rel.ro.*) } :text
  .sdata : { *(.sdata .sdata.*) } :text
  .got : { *(.got .got.*) } :text
  .bss : { *(.bss .bss.* COMMON) } :text
  .sbss : { *(.sbss .sbss.*) } :text

  . = ALIGN(16);
}
