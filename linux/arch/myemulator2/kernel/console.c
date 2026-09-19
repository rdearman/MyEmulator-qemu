// SPDX-License-Identifier: GPL-2.0
#include <linux/console.h>
#include <linux/init.h>
#include <asm/io.h>

#define MYEMU2_CONSOLE_DATA ((void __iomem *)0xf0000000UL)

static void myemulator2_console_write(struct console *console,
                                      const char *str, unsigned int count)
{
	while (count--) {
		if (*str == '\n')
			writeb('\r', MYEMU2_CONSOLE_DATA);
		writeb(*str++, MYEMU2_CONSOLE_DATA);
	}
}

static struct console myemulator2_console = {
	.name = "myemu",
	.write = myemulator2_console_write,
	.flags = CON_PRINTBUFFER | CON_ENABLED,
	.index = 0,
};

static int __init myemulator2_console_init(void)
{
	register_console(&myemulator2_console);
	return 0;
}
console_initcall(myemulator2_console_init);
