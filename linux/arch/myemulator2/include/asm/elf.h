#ifndef _ASM_MYEMULATOR2_ELF_H
#define _ASM_MYEMULATOR2_ELF_H
#include <uapi/asm/elf.h>
#define ELF_CLASS ELFCLASS32
#define elf_check_arch(x) ((x)->e_machine == EM_MYEMULATOR2)
#define ELF_EXEC_PAGESIZE 4096
#define ELF_PLATFORM NULL

typedef unsigned long elf_greg_t;
typedef elf_greg_t elf_gregset_t[20];
typedef unsigned long elf_fpregset_t;
#endif
