#ifndef _ASM_MYEMULATOR2_IO_H
#define _ASM_MYEMULATOR2_IO_H
#include <asm-generic/io.h>
#define __io(p) ((void __iomem *)(unsigned long)(p))
#endif
