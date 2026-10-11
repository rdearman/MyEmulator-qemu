// SPDX-License-Identifier: GPL-2.0
#include <linux/export.h>
#include <linux/types.h>

__wsum csum_partial(const void *buf, int len, __wsum sum)
{
    const unsigned char *p = buf;
    u64 acc = (__force u32)sum;

    while (len >= 2) {
        acc += (u32)p[0] | ((u32)p[1] << 8);
        p += 2;
        len -= 2;
    }
    if (len)
        acc += (u32)p[0];
    while (acc >> 16)
        acc = (acc & 0xffff) + (acc >> 16);
    return (__force __wsum)acc;
}
EXPORT_SYMBOL(csum_partial);
