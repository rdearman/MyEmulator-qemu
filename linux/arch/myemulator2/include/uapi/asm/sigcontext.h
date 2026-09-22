#ifndef _UAPI_ASM_MYEMULATOR2_SIGCONTEXT_H
#define _UAPI_ASM_MYEMULATOR2_SIGCONTEXT_H
#include <asm/ptrace.h>
struct sigcontext { struct user_regs_struct regs; };
#endif
#ifndef _UAPI_ASM_MYEMULATOR2_SIGCONTEXT_H
#define _UAPI_ASM_MYEMULATOR2_SIGCONTEXT_H

#include <asm/ptrace.h>

/* The rt signal frame saves the complete software register image.  The
 * trailing word keeps the mcontext layout in sync with musl's 21-word
 * MyEmulator2 gregset and is retained for ABI compatibility. */
struct sigcontext {
	struct user_regs_struct regs;
	unsigned long oldmask;
};

#endif
