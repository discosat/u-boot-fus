/*
 * (C) Copyright 2000
 * Rob Taylor, Flying Pig Systems. robt@flyingpig.com.
 *
 * SPDX-License-Identifier:	GPL-2.0+
 */

#include <common.h>
#include <linux/compiler.h>

#include <ns16550.h>
#ifdef CONFIG_NS87308
#include <ns87308.h>
#endif

#include <serial.h>

#ifndef CONFIG_NS16550_MIN_FUNCTIONS

DECLARE_GLOBAL_DATA_PTR;

#if !defined(CONFIG_CONS_INDEX)
#elif (CONFIG_CONS_INDEX < 1) || (CONFIG_CONS_INDEX > 6)
#error	"Invalid console index value."
#endif

#if CONFIG_CONS_INDEX == 1 && !defined(CONFIG_SYS_NS16550_COM1)
#error	"Console port 1 defined but not configured."
#elif CONFIG_CONS_INDEX == 2 && !defined(CONFIG_SYS_NS16550_COM2)
#error	"Console port 2 defined but not configured."
#elif CONFIG_CONS_INDEX == 3 && !defined(CONFIG_SYS_NS16550_COM3)
#error	"Console port 3 defined but not configured."
#elif CONFIG_CONS_INDEX == 4 && !defined(CONFIG_SYS_NS16550_COM4)
#error	"Console port 4 defined but not configured."
#elif CONFIG_CONS_INDEX == 5 && !defined(CONFIG_SYS_NS16550_COM5)
#error	"Console port 5 defined but not configured."
#elif CONFIG_CONS_INDEX == 6 && !defined(CONFIG_SYS_NS16550_COM6)
#error	"Console port 6 defined but not configured."
#endif


static int calc_divisor (struct NS16550 *uart)
{
#ifdef CONFIG_OMAP1510
	/* If can't cleanly clock 115200 set div to 1 */
	if ((CONFIG_SYS_NS16550_CLK == 12000000) && (gd->baudrate == 115200)) {
		uart->osc_12m_sel = OSC_12M_SEL;	/* enable 6.5 * divisor */
		return (1);				/* return 1 for base divisor */
	}
	uart->osc_12m_sel = 0;			/* clear if previsouly set */
#endif
#ifdef CONFIG_OMAP1610
	/* If can't cleanly clock 115200 set div to 1 */
	if ((CONFIG_SYS_NS16550_CLK == 48000000) && (gd->baudrate == 115200)) {
		return (26);		/* return 26 for base divisor */
	}
#endif

#define MODE_X_DIV 16
	/* Compute divisor value. Normally, we should simply return:
	 *   CONFIG_SYS_NS16550_CLK) / MODE_X_DIV / gd->baudrate
	 * but we need to round that value by adding 0.5.
	 * Rounding is especially important at high baud rates.
	 */
	return (CONFIG_SYS_NS16550_CLK + (gd->baudrate * (MODE_X_DIV / 2))) /
		(MODE_X_DIV * gd->baudrate);
}

static void eserial_setbrg (const struct serial_device *sdev)
{
	struct NS16550 *uart = (struct NS16550 *)sdev->dev.priv;
	int clock_divisor;

	clock_divisor = calc_divisor(uart);
	NS16550_reinit(uart, clock_divisor);
}


static int eserial_start(const struct stdio_dev *pdev)
{
	eserial_setbrg(to_serial_device(pdev));

	return 0;
}


static void eserial_ll_putc(struct NS16550 *const uart, const char c)
{
	/* If \n, do \r first */
	if (c == '\n')
		NS16550_putc(uart, '\r');

	NS16550_putc(uart, c);
}

/*
 * Output a single byte to the serial port.
 */
static void eserial_putc(const struct stdio_dev *pdev, const char c)
{
	struct serial_device *sdev = to_serial_device(pdev);
	struct NS16550 *const uart = (struct NS16550 *)sdev->dev.priv;

	eserial_ll_putc(uart, c);
}

/*
 * Output a string to the serial port.
 */
static void eserial_puts(const struct stdio_dev *pdev, const char *s)
{
	struct serial_device *sdev = to_serial_device(pdev);
	struct NS16550 *const uart = (struct NS16550 *)sdev->dev.priv;

	while (*s)
		eserial_ll_putc(uart, *s++);
}

/*
 * Read a single byte from the serial port. 
 */
static int eserial_getc(const struct stdio_dev *pdev)
{
	struct serial_device *sdev = to_serial_device(pdev);
	struct NS16550 *const uart = (struct NS16550 *)sdev->dev.priv;

	return NS16550_getc(uart);
}

/*
 * Test whether a character is in the RX buffer
 */
static int eserial_tstc(const struct stdio_dev *pdev)
{
	struct serial_device *sdev = to_serial_device(pdev);
	struct NS16550 *const uart = (struct NS16550 *)sdev->dev.priv;

	return NS16550_tstc(uart);
}

/* Serial device descriptor */
#define INIT_ESERIAL(_addr, _name, _hwname) {		\
	{	/* stdio_dev part */		\
		.name = _name,			\
		.hwname = _hwname,		\
		.flags = DEV_FLAGS_INPUT | DEV_FLAGS_OUTPUT, \
		.start = eserial_start,		\
		.stop = NULL,			\
		.getc = eserial_getc,		\
		.tstc =	eserial_tstc,		\
		.putc = eserial_putc,		\
		.puts = eserial_puts,		\
		.priv = (void *)_addr,		\
	},					\
	.setbrg = eserial_setbrg,		\
}

struct serial_device eserial_device[] = {
	INIT_ESERIAL(CONFIG_SYS_NS16550_COM1, CONFIG_SYS_SERCON_NAME "0", "eserial0"),
	INIT_ESERIAL(CONFIG_SYS_NS16550_COM2, CONFIG_SYS_SERCON_NAME "1", "eserial1"),
	INIT_ESERIAL(CONFIG_SYS_NS16550_COM3, CONFIG_SYS_SERCON_NAME "2", "eserial2"),
	INIT_ESERIAL(CONFIG_SYS_NS16550_COM4, CONFIG_SYS_SERCON_NAME "3", "eserial3"),
	INIT_ESERIAL(CONFIG_SYS_NS16550_COM5, CONFIG_SYS_SERCON_NAME "4", "eserial4"),
	INIT_ESERIAL(CONFIG_SYS_NS16550_COM6, CONFIG_SYS_SERCON_NAME "5", "eserial5"),
};

__weak struct serial_device *default_serial_console(void)
{
#if CONFIG_CONS_INDEX == 1
	return &eserial_device[0];
#elif CONFIG_CONS_INDEX == 2
	return &eserial_device[1];
#elif CONFIG_CONS_INDEX == 3
	return &eserial_device[2];
#elif CONFIG_CONS_INDEX == 4
	return &eserial_device[3];
#elif CONFIG_CONS_INDEX == 5
	return &eserial_device[4];
#elif CONFIG_CONS_INDEX == 6
	return &eserial_device[5];
#else
#error "Bad CONFIG_CONS_INDEX."
#endif
}

/* Get pointer to n-th serial device */
struct serial_device *get_serial_device(unsigned int n)
{
	if (n < 6)
		return &eserial_device[n];

	return NULL;
}

/* Register all serial ports; if you only want to register a subset, implement
   function board_serial_init() and call serial_register() there. */
void ns16550_serial_initialize(void)
{
#if defined(CONFIG_SYS_NS16550_COM1)
	serial_register(&eserial_device[0]);
#endif
#if defined(CONFIG_SYS_NS16550_COM2)
	serial_register(&eserial_device[1]);
#endif
#if defined(CONFIG_SYS_NS16550_COM3)
	serial_register(&eserial_device[2]);
#endif
#if defined(CONFIG_SYS_NS16550_COM4)
	serial_register(&eserial_device[3]);
#endif
#if defined(CONFIG_SYS_NS16550_COM5)
	serial_register(&eserial_device[4]);
#endif
#if defined(CONFIG_SYS_NS16550_COM6)
	serial_register(&eserial_device[5]);
#endif
}

#endif /* !CONFIG_NS16550_MIN_FUNCTIONS */
