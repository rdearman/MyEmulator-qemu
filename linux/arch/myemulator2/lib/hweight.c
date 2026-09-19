// SPDX-License-Identifier: GPL-2.0
#include <linux/types.h>

unsigned int __sw_hweight32(unsigned int w)
{
	w -= (w >> 1) & 0x55555555;
	w = (w & 0x33333333) + ((w >> 2) & 0x33333333);
	return (((w + (w >> 4)) & 0x0f0f0f0f) * 0x01010101) >> 24;
}

unsigned int __sw_hweight16(unsigned int w)
{
	return __sw_hweight32(w & 0xffff);
}

unsigned int __sw_hweight8(unsigned int w)
{
	return __sw_hweight32(w & 0xff);
}

unsigned long __sw_hweight64(__u64 w)
{
	return __sw_hweight32((u32)(w >> 32)) + __sw_hweight32((u32)w);
}
