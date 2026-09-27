#ifndef _UAPI_ASM_MYEMULATOR2_PTRACE_H
#define _UAPI_ASM_MYEMULATOR2_PTRACE_H
#include <linux/types.h>
#define PTRACE_GETREGS 12
#define PTRACE_SETREGS 13
struct user_regs_struct { __u32 r[16]; __u32 pc; __u32 sr; __u32 cause; __u32 info; };
#endif
