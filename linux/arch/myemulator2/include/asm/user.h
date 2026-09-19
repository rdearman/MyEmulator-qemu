/* SPDX-License-Identifier: GPL-2.0 */
#ifndef _ASM_MYEMULATOR2_USER_H
#define _ASM_MYEMULATOR2_USER_H

#include <linux/ptrace.h>

/* Minimal legacy core-dump view.  The architecture currently has no
 * floating-point register file. */
struct user {
	unsigned long regs[16];
	unsigned long u_tsize;
	unsigned long u_dsize;
	unsigned long u_ssize;
	unsigned long start_code;
	unsigned long start_data;
	unsigned long start_stack;
	long signal;
	unsigned long u_ar0;
	unsigned long magic;
	char u_comm[32];
};

#endif
