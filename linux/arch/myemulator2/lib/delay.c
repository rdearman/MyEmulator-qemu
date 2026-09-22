// SPDX-License-Identifier: GPL-2.0
#include <linux/delay.h>

void __delay(unsigned long loops)
{
	if (!loops)
		return;
	/* Keep the decrement explicit: the C post-decrement is lowered to
	 * the signed-immediate encoding (4095 == -1), which is needlessly
	 * expensive in the tiny target's delay path. */
	asm volatile(
		"1: subi %0, %0, 1\n"
		"bne %0, r0, 1b\n"
		: "+r"(loops) :: "memory");
}

void __const_udelay(unsigned long xloops)
{
	__delay(xloops >> 8);
}

void __udelay(unsigned long usecs)
{
	__const_udelay(usecs * 0x10c7UL);
}

void __ndelay(unsigned long nsecs)
{
	__const_udelay(nsecs * 0x5UL);
}

void calibrate_delay(void)
{
}
