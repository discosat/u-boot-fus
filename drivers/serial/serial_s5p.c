/*
 * (C) Copyright 2009 SAMSUNG Electronics
 * Minkyu Kang <mk7.kang@samsung.com>
 * Heungjun Kim <riverful.kim@samsung.com>
 *
 * based on drivers/serial/s3c64xx.c
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 */

#include <common.h>
#include <linux/compiler.h>
#include <asm/io.h>
#include <asm/arch/uart.h>
#include <asm/arch/clk.h>
#include <serial.h>
#include <asm/arch/cpu.h>

DECLARE_GLOBAL_DATA_PTR;

static inline struct s5p_uart *s5p_get_base_uart(int port)
{
	return ((struct s5p_uart *)samsung_get_base_uart()) + port;
}

/*
 * The coefficient, used to calculate the baudrate on S5P UARTs is
 * calculated as
 * C = UBRDIV * 16 + number_of_set_bits_in_UDIVSLOT
 * however, section 31.6.11 of the datasheet doesn't recomment using 1 for 1,
 * 3 for 2, ... (2^n - 1) for n, instead, they suggest using these constants:
 */
static const int udivslot[] = {
	0,
	0x0080,
	0x0808,
	0x0888,
	0x2222,
	0x4924,
	0x4a52,
	0x54aa,
	0x5555,
	0xd555,
	0xd5d5,
	0xddd5,
	0xdddd,
	0xdfdd,
	0xdfdf,
	0xffdf,
};

static void s5p_serial_setbrg(const struct serial_device *sdev)
{
	int port = (int)sdev->serpriv;
	struct s5p_uart *const uart = s5p_get_base_uart(port);
	u32 uclk = get_uart_clk(port);
	u32 baudrate = gd->baudrate;
	u32 val;

	val = uclk / baudrate;

	writel(val / 16 - 1, &uart->ubrdiv);

	if (s5p_uart_divslot())
		writew(udivslot[val % 16], &uart->rest.slot);
	else
		writeb(val % 16, &uart->rest.value);
}

/*
 * Initialise the serial port with the given baudrate. The settings
 * are always 8 data bits, no parity, 1 stop bit, no start bits.
 */
int s5p_serial_start(const struct stdio_dev *pdev)
{
	struct serial_device *sdev = pdev->priv;
	int port = (int)sdev->serpriv;
	struct s5p_uart *const uart = s5p_get_base_uart(port);

	/* reset and enable FIFOs, set triggers to the maximum */
	writel(7, &uart->ufcon);
	writel(0, &uart->umcon);
	/* 8N1 */
	writel(0x3, &uart->ulcon);
	/* No interrupts, no DMA, pure polling, PCLK as clock source */
	writel(0x245, &uart->ucon);

	s5p_serial_setbrg(sdev);

	return 0;
}

static void s5p_ll_putc(struct s5p_uart *const uart, const char c)
{
	/* wait for room in the tx FIFO */
	while (!(readl(&uart->utrstat) & 0x2)) {
		/* Check for break */
		if (readl(&uart->uerstat) & 0x8)
			return;
	}

	writeb(c, &uart->utxh);

	/* If \n, also do \r */
	if (c == '\n')
		s5p_ll_putc(uart, '\r');
}

/*
 * Output a single byte to the serial port.
 */
static void s5p_serial_putc(const struct stdio_dev *pdev, const char c)
{
	struct serial_device *sdev = pdev->priv;
	int port = (int)sdev->serpriv;
	struct s5p_uart *const uart = s5p_get_base_uart(port);

	s5p_ll_putc(uart, c);
}

/*
 * Output a string to the serial port.
 */
static void s5p_serial_puts(const struct stdio_dev *pdev, const char *s)
{
	struct serial_device *sdev = pdev->priv;
	int port = (int)sdev->serpriv;
	struct s5p_uart *const uart = s5p_get_base_uart(port);

	while (*s)
		s5p_ll_putc(uart, *s++);
}

/*
 * Read a single byte from the serial port. Returns 1 on success, 0
 * otherwise. When the function is successful, the character read is
 * written into its argument c.
 */
static int s5p_serial_getc(const struct stdio_dev *pdev)
{
	struct serial_device *sdev = pdev->priv;
	int port = (int)sdev->serpriv;
	struct s5p_uart *const uart = s5p_get_base_uart(port);

	/* wait for character to arrive */
	while (!(readl(&uart->utrstat) & 0x1)) {
		/* Check for break, Frame Err, Parity Err, Overrun Err */
		if (readl(&uart->uerstat) & 0xF)
			return 0;
	}

	return (int)(readb(&uart->urxh) & 0xff);
}

/*
 * Test whether a character is in the RX buffer
 */
static int s5p_serial_tstc(const struct stdio_dev *pdev)
{
	struct serial_device *sdev = pdev->priv;
	int port = (int)sdev->serpriv;
	struct s5p_uart *const uart = s5p_get_base_uart(port);

	return (int)(readl(&uart->utrstat) & 0x1);
}

#define INIT_S5P_SERIAL_STRUCTURE(_port, _name) {	\
	{       /* stdio_dev part */		\
		.name = _name,			\
		.flags = DEV_FLAGS_INPUT | DEV_FLAGS_OUTPUT, \
		.start = s5p_serial_start,	\
		.stop = NULL,			\
		.getc = s5p_serial_getc,	\
		.tstc =	s5p_serial_tstc,	\
		.putc = s5p_serial_putc,	\
		.puts = s5p_serial_puts,	\
		.priv = &s5p_serial_device[_port],  \
	},					\
	.setbrg = s5p_serial_setbrg,		\
	.serpriv = (void *)_port		\
}

struct serial_device s5p_serial_device[] = {
	INIT_S5P_SERIAL_STRUCTURE(0, "s5pser0"),
	INIT_S5P_SERIAL_STRUCTURE(1, "s5pser1"),
	INIT_S5P_SERIAL_STRUCTURE(2, "s5pser2"),
	INIT_S5P_SERIAL_STRUCTURE(3, "s5pser3"),
};

__weak struct serial_device *default_serial_console(void)
{
#if defined(CONFIG_SERIAL0)
	return &s5p_serial_device[0];
#elif defined(CONFIG_SERIAL1)
	return &s5p_serial_device[1];
#elif defined(CONFIG_SERIAL2)
	return &s5p_serial_device[2];
#elif defined(CONFIG_SERIAL3)
	return &s5p_serial_device[3];
#else
#error "CONFIG_SERIAL? missing."
#endif
}

/* Register the given serial port; use new name if name != NULL */
void s5p_serial_register(int port, const char *name)
{
	if (port < 4) {
		struct serial_device *sdev = &s5p_serial_device[port];

		if (name)
			strcpy(sdev->dev.name, name);
		serial_register(sdev);
	}
}
