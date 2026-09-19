// SPDX-License-Identifier: GPL-2.0
#include <linux/delay.h>

void __delay(unsigned long loops)
{
	while (loops--)
		asm volatile("addi r0, r0, 0" ::: "memory");
}
