// SPDX-License-Identifier: GPL-2.0
#include <linux/console.h>
#include <linux/init.h>
#include <linux/errno.h>
#include <linux/uaccess.h>
#include <asm/io.h>

#define MYEMU2_CONSOLE_DATA ((void __iomem *)0xf0000000UL)
#define MYEMU2_CONSOLE_STATUS ((void __iomem *)0xf0000001UL)
#define MYEMU2_STATUS_RX_READY 0x01

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

/* Bootstrap userspace input.  Until the full Linux tty/serial driver is
 * enabled, read(0) polls the architectural UART FIFO and returns one byte.
 * EAGAIN is intentional: userspace can retry without inventing blocking
 * semantics in the architecture port. */
ssize_t myemulator2_console_read_user(char __user *buf, size_t count)
{
	u8 value;

	if (!count)
		return 0;
	if (!(readb(MYEMU2_CONSOLE_STATUS) & MYEMU2_STATUS_RX_READY))
		return -EAGAIN;
	value = readb(MYEMU2_CONSOLE_DATA);
	return copy_to_user(buf, &value, 1) ? -EFAULT : 1;
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
