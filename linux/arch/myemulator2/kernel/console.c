// SPDX-License-Identifier: GPL-2.0
#include <linux/console.h>
#include <linux/init.h>
#include <linux/uaccess.h>
#include <asm/io.h>

#define MYEMU2_CONSOLE_DATA ((void __iomem *)0xf0000000UL)

static struct console myemulator2_console;

static void myemulator2_console_write(struct console *console,
                                      const char *str, unsigned int count)
{
	while (count--) {
		if (*str == '\n')
			writeb('\r', MYEMU2_CONSOLE_DATA);
		writeb(*str++, MYEMU2_CONSOLE_DATA);
	}
}

/* Bootstrap userspace has no tty driver yet.  Provide the initial Linux
 * write(1/2, ...) path through the same UART used by printk; this is kept
 * separate from the console callback so the eventual tty driver can replace
 * it without changing the syscall ABI. */
ssize_t myemulator2_console_write_user(const char __user *buf, size_t count)
{
	char tmp[64];
	size_t done = 0;

	while (done < count) {
		size_t n = min_t(size_t, sizeof(tmp), count - done);
		if (copy_from_user(tmp, buf + done, n))
			return done ? (ssize_t)done : -EFAULT;
		myemulator2_console_write(&myemulator2_console, tmp, n);
		done += n;
	}
	return (ssize_t)done;
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
