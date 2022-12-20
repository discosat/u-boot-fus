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
#include <asm/global_data.h>		  /* DECLARE_GLOBAL_DATA_PTR */
#include <asm/arch/vybrid-regs.h>	  /* UART?_BASE */
#include <asm/arch/serial-vybrid.h>	  /* struct vybrid_uart, ... */
#include <asm/arch/clock.h>		  /* vybrid_get_uartclk() */

static void vybrid_serial_setbrg(const struct serial_device *sdev)
{
	DECLARE_GLOBAL_DATA_PTR;
	struct vybrid_uart *uart = (struct vybrid_uart *)(sdev->priv);
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
static int vybrid_serial_start(const struct serial_device *sdev)
{
        struct vybrid_uart *uart = (struct vybrid_uart *)(sdev->priv);

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
static void vybrid_serial_putc(const struct serial_device *sdev, const char c)
{
	struct vybrid_uart *const uart = (struct vybrid_uart *)sdev->priv;

	vybrid_ll_putc(uart, c);
}

/*
 * Output a string to the serial port.
 */
static void vybrid_serial_puts(const struct serial_device *sdev, const char *s)
{
	struct vybrid_uart *const uart = (struct vybrid_uart *)sdev->priv;

	while (*s)
		vybrid_ll_putc(uart, *s++);
}


/*
 * Read a single byte from the serial port. 
 */
static int vybrid_serial_getc(const struct serial_device *sdev)
{
	struct vybrid_uart *const uart = (struct vybrid_uart *)sdev->priv;

	/* Wait for character to arrive */
	while (!(in_8(&uart->us1) & US1_RDRF))
		WATCHDOG_RESET();

	setbits_8(&uart->us1, US1_RDRF);

	return in_8(&uart->ud);
}

/*
 * Test whether a character is in the RX buffer
 */
static int vybrid_serial_tstc(const struct serial_device *sdev)
{
	struct vybrid_uart *const uart = (struct vybrid_uart *)sdev->priv;

	if (in_8(&uart->urcfifo) == 0)
		return 0;

	return 1;
}


#define INIT_VYB_SERIAL(_addr, _name) { \
	.name = _name,			\
	.start = vybrid_serial_start,	\
	.stop = NULL,			\
	.setbrg = vybrid_serial_setbrg,	\
	.getc = vybrid_serial_getc,	\
	.tstc =	vybrid_serial_tstc,	\
	.putc = vybrid_serial_putc,	\
	.puts = vybrid_serial_puts,	\
	.priv = (void *)_addr,		\
}

static struct serial_device vybrid_serial_devices[] = {
	INIT_VYB_SERIAL(UART0_BASE, "ttyLP0"),
	INIT_VYB_SERIAL(UART1_BASE, "ttyLP1"),
	INIT_VYB_SERIAL(UART2_BASE, "ttyLP2"),
	INIT_VYB_SERIAL(UART3_BASE, "ttyLP3"),
	INIT_VYB_SERIAL(UART4_BASE, "ttyLP4"),
	INIT_VYB_SERIAL(UART5_BASE, "ttyLP5"),
};

__weak ulong board_serial_base(void)
{
	return CONFIG_MXC_UART_BASE;
}

struct serial_device *default_serial_console(void)
{
	void *addr = (void *)board_serial_base();
	int port = ARRAY_SIZE(vybrid_serial_devices);

	do {
		port--;
		if (addr == vybrid_serial_devices[port].priv)
			break;
	} while (port > 0);

	return &vybrid_serial_devices[port];
}

/* Register the default serial port */
void vybrid_serial_initialize(void)
{
	serial_register(default_serial_console());
}
