// SPDX-License-Identifier: GPL-2.0
#include <linux/delay.h>

void __delay(unsigned long loops)
{
	while (loops--)
		asm volatile("addi r0, r0, 0" ::: "memory");
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
