#ifndef _UAPI_ASM_MYEMULATOR2_SIGCONTEXT_H
#define _UAPI_ASM_MYEMULATOR2_SIGCONTEXT_H
#include <asm/ptrace.h>
struct sigcontext { struct user_regs_struct regs; };
#endif
