/*
 * (C) Copyright 2002
 * Gary Jennejohn, DENX Software Engineering, <garyj@denx.de>
 *
 * (C) Copyright 2008
 * Guennadi Liakhovetki, DENX Software Engineering, <lg@denx.de>
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
#include <linux/compiler.h>		  /* barrier, __weak */
#include <serial.h>			  /* struct serial_device */
#include <asm/arch/s3c64xx-regs.h>

/*
 * The coefficient, used to calculate the baudrate on S3C6400 UARTs is
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

static void s3c64xx_serial_setbrg(const struct serial_device *sdev)
{
	DECLARE_GLOBAL_DATA_PTR;
	int port = (int)sdev->serpriv;
	s3c64xx_uart *const uart = s3c64xx_get_base_uart(port);
	u32 pclk = get_PCLK();
	u32 baudrate = gd->baudrate;
	int i;

	pclk /= baudrate;
	uart->UBRDIV = pclk / 16 - 1;
	uart->UDIVSLOT = udivslot[pclk % 16];

	for (i = 0; i < 100; i++)
		barrier();
}

/*
 * Initialise the serial port with the given baudrate. The settings
 * are always 8 data bits, no parity, 1 stop bit, no start bits.
 */
static int s3c64xx_serial_start(const struct stdio_dev *pdev)
{
	struct serial_device *sdev = pdev->priv;
	int port = (int)sdev->serpriv;
	s3c64xx_uart *const uart = s3c64xx_get_base_uart(port);

	/* reset and enable FIFOs, set triggers to the maximum */
	uart->UFCON = 0xff;
	uart->UMCON = 0;
	/* 8N1 */
	uart->ULCON = 3;
	/* No interrupts, no DMA, pure polling, PCLK as clock source */
	uart->UCON = 0x0805;

	s3c64xx_serial_setbrg(sdev);

	return 0;
}

static void s3c64xx_ll_putc(s3c64xx_uart *const uart, const char c)
{
	/* wait for room in the tx FIFO */
	while (!(uart->UTRSTAT & 0x2))
		;

	uart->UTXH = c;

	/* If \n, also do \r */
	if (c == '\n')
		s3c64xx_ll_putc(uart, '\r');
}

/*
 * Output a single byte to the serial port.
 */
void s3c64xx_serial_putc(const struct stdio_dev *pdev, const char c)
{
	struct serial_device *sdev = pdev->priv;
	int port = (int)sdev->serpriv;
	s3c64xx_uart *const uart = s3c64xx_get_base_uart(port);

	s3c64xx_ll_putc(uart, c);
}

/*
 * Output a string to the serial port.
 */
static void s3c64xx_serial_puts(const struct stdio_dev *pdev, const char *s)
{
	struct serial_device *sdev = pdev->priv;
	int port = (int)sdev->serpriv;
	s3c64xx_uart *const uart = s3c64xx_get_base_uart(port);

	while (*s)
		s3c64xx_ll_putc(uart, *s++);
}

/*
 * Read a single byte from the serial port. 
 */
static int s3c64xx_serial_getc(const struct stdio_dev *pdev)
{
	struct serial_device *sdev = pdev->priv;
	int port = (int)sdev->serpriv;
	s3c64xx_uart *const uart = s3c64xx_get_base_uart(port);

	/* wait for character to arrive */
	while (!(uart->UTRSTAT & 0x1))
		;

	return uart->URXH & 0xff;
}

/*
 * Test whether a character is in the RX buffer
 */
static int s3c64xx_serial_tstc(const struct stdio_dev *pdev)
{
	struct serial_device *sdev = pdev->priv;
	int port = (int)sdev->serpriv;
	s3c64xx_uart *const uart = s3c64xx_get_base_uart(port);

	return uart->UTRSTAT & 0x1;
}

#define INIT_S3C64XX_SERIAL_STRUCTURE(_port, _name, _hwname) {	\
	{       /* stdio_dev part */		\
		.name = _name,			\
		.hwname = _hwname,		\
		.flags = DEV_FLAGS_INPUT | DEV_FLAGS_OUTPUT, \
		.start = s3c64xx_serial_start,	\
		.stop = NULL,			\
		.getc = s3c64xx_serial_getc,	\
		.tstc =	s3c64xx_serial_tstc,	\
		.putc = s3c64xx_serial_putc,	\
		.puts = s3c64xx_serial_puts,	\
		.priv = &s3c64xx_serial_device[_port],  \
	},					\
	.setbrg = s3c64xx_serial_setbrg,	\
	.serpriv = (void *)_port		\
}

struct serial_device s3c64xx_serial_device[] = {
	INIT_S3C64XX_SERIAL_STRUCTURE(0, CONFIG_SYS_SERCON_NAME "0", "s3c_uart0"),
	INIT_S3C64XX_SERIAL_STRUCTURE(1, CONFIG_SYS_SERCON_NAME "1", "s3c_uart1"),
	INIT_S3C64XX_SERIAL_STRUCTURE(2, CONFIG_SYS_SERCON_NAME "2", "s3c_uart2"),
	INIT_S3C64XX_SERIAL_STRUCTURE(3, CONFIG_SYS_SERCON_NAME "3", "s3c_uart3"),
};

__weak struct serial_device *default_serial_console(void)
{
#if defined(CONFIG_SERIAL0)
	return &s3c64xx_serial_device[0];
#elif defined(CONFIG_SERIAL1)
	return &s3c64xx_serial_device[1];
#elif defined(CONFIG_SERIAL2)
	return &s3c64xx_serial_device[2];
#elif defined(CONFIG_SERIAL3)
	return &s3c64xx_serial_device[3];
#else
#error "CONFIG_SERIAL? missing."
#endif
}

/* Register the given serial port; use new name if name != NULL */
void s3c64xx_serial_register(int port, const char *name)
{
	if (port < 4) {
		struct serial_device *sdev = &s3c64xx_serial_device[port];

		if (name)
			strcpy(sdev->dev.name, name);
		serial_register(sdev);
	}
}
