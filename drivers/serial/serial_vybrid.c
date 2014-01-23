/*
 * Copyright 2012 Freescale Semiconductor, Inc.
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
#include <linux/compiler.h>		  /* __weak */
#include <serial.h>			  /* struct serial_device */
#include <watchdog.h>		          /* WATCHDOG_RESET() */
#include <asm/io.h>			  /* in_8(), clrbits_8(), ... */
#include <asm/arch/vybrid-regs.h>	  /* UART?_BASE */
#include <asm/arch/serial-vybrid.h>	  /* struct vybrid_uart, ... */
#include <asm/arch/clock.h>		  /* vybrid_get_uartclk() */

static void vybrid_serial_setbrg(const struct serial_device *sdev)
{
	DECLARE_GLOBAL_DATA_PTR;
	struct vybrid_uart *uart = (struct vybrid_uart *)(sdev->dev.priv);
	u32 clk = vybrid_get_clock(VYBRID_IPG_CLK);
	u16 sbr;

	sbr = (u16)(clk / (16 * gd->baudrate));
	/* ### TODO: place adjustment later - n/32 BRFA */

	uart->ubdh = sbr >> 8;
	uart->ubdl = sbr & 0xFF;
}


/*
 * Initialise the serial port with the given baudrate. The settings
 * are always 8 data bits, no parity, 1 stop bit, no start bits.
 */
static int vybrid_serial_start(const struct stdio_dev *pdev)
{
	const struct serial_device *sdev = to_serial_device(pdev);
        struct vybrid_uart *uart = (struct vybrid_uart *)(sdev->dev.priv);

	clrbits_8(&uart->uc2, UC2_RE);
	clrbits_8(&uart->uc2, ~UC2_TE);

	out_8(&uart->umodem, 0);
	out_8(&uart->uc1, 0);

	/* ### TODO: provide data bits, parity, stop bit, etc */

	vybrid_serial_setbrg(sdev);

	out_8(&uart->uc2, UC2_RE | UC2_TE);

	return 0;
}


static void vybrid_ll_putc(struct vybrid_uart *const uart, const char c)
{
	/* If \n, do \r first */
	if (c == '\n')
		vybrid_ll_putc(uart, '\r');

	/* wait for room in the tx FIFO */
	while (!(in_8(&uart->us1) & US1_TDRE))
		WATCHDOG_RESET();

	/* Send character */
	out_8(&uart->ud, c);
}

/*
 * Output a single byte to the serial port.
 */
static void vybrid_serial_putc(const struct stdio_dev *pdev, const char c)
{
	struct serial_device *sdev = to_serial_device(pdev);
	struct vybrid_uart *const uart = (struct vybrid_uart *)sdev->dev.priv;

	vybrid_ll_putc(uart, c);
}

/*
 * Output a string to the serial port.
 */
static void vybrid_serial_puts(const struct stdio_dev *pdev, const char *s)
{
	struct serial_device *sdev = to_serial_device(pdev);
	struct vybrid_uart *const uart = (struct vybrid_uart *)sdev->dev.priv;

	while (*s)
		vybrid_ll_putc(uart, *s++);
}


/*
 * Read a single byte from the serial port. 
 */
static int vybrid_serial_getc(const struct stdio_dev *pdev)
{
	struct serial_device *sdev = to_serial_device(pdev);
	struct vybrid_uart *const uart = (struct vybrid_uart *)sdev->dev.priv;

	/* Wait for character to arrive */
	while (!(in_8(&uart->us1) & US1_RDRF))
		WATCHDOG_RESET();

	setbits_8(&uart->us1, US1_RDRF);

	return in_8(&uart->ud);
}

/*
 * Test whether a character is in the RX buffer
 */
static int vybrid_serial_tstc(const struct stdio_dev *pdev)
{
	struct serial_device *sdev = to_serial_device(pdev);
	struct vybrid_uart *const uart = (struct vybrid_uart *)sdev->dev.priv;

	if (in_8(&uart->urcfifo) == 0)
		return 0;

	return 1;
}


#define INIT_VYB_SERIAL(_addr, _name, _hwname) {	\
	{       /* stdio_dev part */		\
		.name = _name,			\
		.hwname = _hwname,		\
		.flags = DEV_FLAGS_INPUT | DEV_FLAGS_OUTPUT, \
		.start = vybrid_serial_start,	\
		.stop = NULL,			\
		.getc = vybrid_serial_getc,	\
		.tstc =	vybrid_serial_tstc,	\
		.putc = vybrid_serial_putc,	\
		.puts = vybrid_serial_puts,	\
		.priv = (void *)_addr,	\
	},					\
	.setbrg = vybrid_serial_setbrg,		\
}

struct serial_device vybrid_serial_device[] = {
	INIT_VYB_SERIAL(UART0_BASE, CONFIG_SYS_SERCON_NAME "0", "vybrid_uart0"),
	INIT_VYB_SERIAL(UART1_BASE, CONFIG_SYS_SERCON_NAME "1", "vybrid_uart1"),
	INIT_VYB_SERIAL(UART2_BASE, CONFIG_SYS_SERCON_NAME "2", "vybrid_uart2"),
	INIT_VYB_SERIAL(UART3_BASE, CONFIG_SYS_SERCON_NAME "3", "vybrid_uart3"),
	INIT_VYB_SERIAL(UART4_BASE, CONFIG_SYS_SERCON_NAME "4", "vybrid_uart4"),
	INIT_VYB_SERIAL(UART5_BASE, CONFIG_SYS_SERCON_NAME "5", "vybrid_uart5"),
};

/* Get pointer to n-th serial device */
struct serial_device *get_serial_device(unsigned int n)
{
	if (n < 6)
		return &vybrid_serial_device[n];

	return NULL;
}
