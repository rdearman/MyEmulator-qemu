#define TC_MYEMULATOR2 1
#define TARGET_BYTES_BIG_ENDIAN 0
#define WORKING_DOT_WORD
#define TARGET_FORMAT "elf32-littlemyemulator2"
#define TARGET_ARCH bfd_arch_myemulator2
#define md_undefined_symbol(NAME) 0
#define md_estimate_size_before_relax(A, B) 0
#define md_convert_frag(B, S, F) as_fatal (_("unexpected relaxation"))
#define md_section_align(SEGMENT, SIZE) (SIZE)
