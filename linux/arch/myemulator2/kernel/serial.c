// SPDX-License-Identifier: GPL-2.0
#include <linux/console.h>
#include <linux/interrupt.h>
#include <linux/io.h>
#include <linux/init.h>
#include <linux/platform_device.h>
#include <linux/serial_core.h>
#include <linux/tty_flip.h>
#include <asm/io.h>

#define MYEMU_UART_BASE 0xf0000000UL
#define MYEMU_UART_SIZE 0x10
#define MYEMU_UART_IRQ 4
#define MYEMU_UART_DATA 0
#define MYEMU_UART_STATUS 1
#define MYEMU_UART_CONTROL 2
#define MYEMU_UART_STATUS_RX_READY 0x01
#define MYEMU_UART_STATUS_TX_READY 0x02
#define MYEMU_UART_CONTROL_RX_IRQ_ENABLE 0x01

static struct uart_driver myemu_uart_driver;
static struct uart_port myemu_uart_port;
static struct platform_device *myemu_uart_pdev;

static unsigned int myemu_tx_empty(struct uart_port *port)
{
	return readb(port->membase + MYEMU_UART_STATUS) &
		MYEMU_UART_STATUS_TX_READY ? TIOCSER_TEMT : 0;
}

static void myemu_set_mctrl(struct uart_port *port, unsigned int mctrl) { }
static unsigned int myemu_get_mctrl(struct uart_port *port) { return TIOCM_CAR | TIOCM_DSR | TIOCM_CTS; }

static void myemu_stop_tx(struct uart_port *port)
{
	(void)port;
}

static void myemu_start_tx(struct uart_port *port)
{
	unsigned char ch;

	uart_port_tx(port, ch,
		     readb(port->membase + MYEMU_UART_STATUS) &
		     MYEMU_UART_STATUS_TX_READY,
		     writeb(ch, port->membase + MYEMU_UART_DATA));
}

static void myemu_stop_rx(struct uart_port *port)
{
	writeb(0, port->membase + MYEMU_UART_CONTROL);
}

static void myemu_enable_ms(struct uart_port *port) { }
static void myemu_break_ctl(struct uart_port *port, int break_state) { }

static irqreturn_t myemu_uart_irq(int irq, void *dev_id)
{
	struct uart_port *port = dev_id;
	struct tty_port *tty = &port->state->port;
	unsigned int status;
	int received = 0;

	while ((status = readb(port->membase + MYEMU_UART_STATUS)) &
	       MYEMU_UART_STATUS_RX_READY) {
		unsigned char ch = readb(port->membase + MYEMU_UART_DATA);
		uart_insert_char(port, 0, 0, ch, TTY_NORMAL);
		port->icount.rx++;
		received = 1;
	}
	if (received)
		tty_flip_buffer_push(tty);
	return IRQ_HANDLED;
}

static int myemu_startup(struct uart_port *port)
{
	int ret = request_irq(MYEMU_UART_IRQ, myemu_uart_irq, 0,
			      "myemulator2-uart", port);
	if (ret)
		return ret;
	writeb(MYEMU_UART_CONTROL_RX_IRQ_ENABLE,
	       port->membase + MYEMU_UART_CONTROL);
	return 0;
}

static void myemu_shutdown(struct uart_port *port)
{
	writeb(0, port->membase + MYEMU_UART_CONTROL);
	free_irq(MYEMU_UART_IRQ, port);
}

static void myemu_set_termios(struct uart_port *port, struct ktermios *termios,
			      const struct ktermios *old)
{
	uart_update_timeout(port, termios->c_cflag, 115200);
}

static const char *myemu_type(struct uart_port *port) { return "myemulator2"; }
static void myemu_release_port(struct uart_port *port) { }
static int myemu_request_port(struct uart_port *port) { return 0; }
static void myemu_config_port(struct uart_port *port, int flags) { port->type = PORT_16550A; }
static int myemu_verify_port(struct uart_port *port, struct serial_struct *ser) { return 0; }

static const struct uart_ops myemu_uart_ops = {
	.tx_empty = myemu_tx_empty,
	.set_mctrl = myemu_set_mctrl,
	.get_mctrl = myemu_get_mctrl,
	.stop_tx = myemu_stop_tx,
	.start_tx = myemu_start_tx,
	.stop_rx = myemu_stop_rx,
	.enable_ms = myemu_enable_ms,
	.break_ctl = myemu_break_ctl,
	.startup = myemu_startup,
	.shutdown = myemu_shutdown,
	.set_termios = myemu_set_termios,
	.type = myemu_type,
	.release_port = myemu_release_port,
	.request_port = myemu_request_port,
	.config_port = myemu_config_port,
	.verify_port = myemu_verify_port,
};

static struct uart_driver myemu_uart_driver = {
	.owner = THIS_MODULE,
	.driver_name = "myemulator2-uart",
	.dev_name = "ttyMY",
	/* Reserve a project-local character major so a tiny initramfs can create
	 * /dev/ttyMY0 without depending on devtmpfs being mounted first. */
	.major = 240,
	.minor = 0,
	.nr = 1,
};

static int __init myemu_uart_init(void)
{
	int ret;

	myemu_uart_pdev = platform_device_register_simple("myemulator2-uart",
							 -1, NULL, 0);
	if (IS_ERR(myemu_uart_pdev))
		return PTR_ERR(myemu_uart_pdev);

	myemu_uart_port.mapbase = MYEMU_UART_BASE;
	myemu_uart_port.membase = (void __iomem *)MYEMU_UART_BASE;
	myemu_uart_port.dev = &myemu_uart_pdev->dev;
	myemu_uart_port.iotype = UPIO_MEM;
	myemu_uart_port.irq = MYEMU_UART_IRQ;
	myemu_uart_port.fifosize = 1;
	myemu_uart_port.flags = UPF_BOOT_AUTOCONF;
	myemu_uart_port.line = 0;
	myemu_uart_port.ops = &myemu_uart_ops;
	ret = uart_register_driver(&myemu_uart_driver);
	if (ret)
		return ret;
	ret = uart_add_one_port(&myemu_uart_driver, &myemu_uart_port);
	if (ret) {
		uart_unregister_driver(&myemu_uart_driver);
		platform_device_unregister(myemu_uart_pdev);
	}
	return ret;
}
device_initcall(myemu_uart_init);
