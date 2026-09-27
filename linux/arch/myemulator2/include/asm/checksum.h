/* SPDX-License-Identifier: GPL-2.0 */
#ifndef __ASM_MYEMULATOR2_CHECKSUM_H
#define __ASM_MYEMULATOR2_CHECKSUM_H

#include <linux/in6.h>

extern __wsum csum_partial(const void *buf, int len, __wsum sum);

static __always_inline __sum16 csum_fold(__wsum sum)
{
    u32 v = (__force u32)sum;
    v = (v & 0xffff) + (v >> 16);
    v = (v & 0xffff) + (v >> 16);
    return (__force __sum16)~v;
}

static __always_inline __sum16 ip_fast_csum(const void *iph, unsigned int ihl)
{
    return csum_fold(csum_partial(iph, (int)ihl * 4, 0));
}

static __always_inline __wsum csum_tcpudp_nofold(__be32 saddr, __be32 daddr,
                                        __u32 len, __u8 proto,
                                        __wsum sum)
{
    __be32 lp = cpu_to_be32(len + proto);
    sum = csum_partial(&saddr, sizeof(saddr), sum);
    sum = csum_partial(&daddr, sizeof(daddr), sum);
    return csum_partial(&lp, sizeof(lp), sum);
}

static __always_inline __sum16 csum_tcpudp_magic(__be32 saddr, __be32 daddr,
                                        __u32 len, __u8 proto,
                                        __wsum sum)
{
    return csum_fold(csum_tcpudp_nofold(saddr, daddr, len, proto, sum));
}

static __always_inline __sum16 ip_compute_csum(const void *buf, int len)
{
    return csum_fold(csum_partial(buf, len, 0));
}

#endif
