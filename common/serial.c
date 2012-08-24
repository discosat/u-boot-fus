/*
 * (C) Copyright 2004
 * Wolfgang Denk, DENX Software Engineering, wd@denx.de.
 *
 * See file CREDITS for list of people who contributed to this
 * project.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of
 * the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston,
 * MA 02111-1307 USA
 */

#include <common.h>
#include <serial.h>
#include <stdio_dev.h>
#include <post.h>
#include <linux/compiler.h>

DECLARE_GLOBAL_DATA_PTR;

static struct serial_device *serial_devices;
static struct serial_device *serial_current;

void serial_register(struct serial_device *sdev)
{
#ifdef CONFIG_NEEDS_MANUAL_RELOC
	sdev->dev.start += gd->reloc_off;
	sdev->dev.getc += gd->reloc_off;
	sdev->dev.tstc += gd->reloc_off;
	sdev->dev.putc += gd->reloc_off;
	sdev->dev.puts += gd->reloc_off;
	sdev->setbrg += gd->reloc_off;
#endif
	if (!serial_devices) {
		/* First element, loop back to itself */
		sdev->next = sdev;
		sdev->prev = sdev;
		serial_devices = sdev;
	} else {
		/* Add at end; serial_devices points to the first element in
		   the serial devices ring, so serial_devices->prev points to
		   the last element. */
		sdev->prev = serial_devices->prev;
		sdev->prev->next = sdev;
		sdev->next = serial_devices;
		sdev->next->prev = sdev;
	}
}

static int __board_serial_init(void)
{
	return -1;
}
int board_serial_init(void) __attribute__((weak, alias("__board_serial_init")));

void serial_initialize(void)
{
	/* The board may initialize only a few ports of some SOC. */
	if (board_serial_init() < 0) {
#if defined(CONFIG_8xx_CONS_SMC1) || defined(CONFIG_8xx_CONS_SMC2)
		serial_register(&serial_smc_device);
#endif
#if	defined(CONFIG_8xx_CONS_SCC1) || defined(CONFIG_8xx_CONS_SCC2) || \
	defined(CONFIG_8xx_CONS_SCC3) || defined(CONFIG_8xx_CONS_SCC4)
		serial_register(&serial_scc_device);
#endif

#if defined(CONFIG_SYS_NS16550_SERIAL)
#if defined(CONFIG_SYS_NS16550_COM1)
		serial_register(&eserial1_device);
#endif
#if defined(CONFIG_SYS_NS16550_COM2)
		serial_register(&eserial2_device);
#endif
#if defined(CONFIG_SYS_NS16550_COM3)
		serial_register(&eserial3_device);
#endif
#if defined(CONFIG_SYS_NS16550_COM4)
		serial_register(&eserial4_device);
#endif
#endif /* CONFIG_SYS_NS16550_SERIAL */
#if defined(CONFIG_FFUART)
		serial_register(&serial_ffuart_device);
#endif
#if defined(CONFIG_BTUART)
		serial_register(&serial_btuart_device);
#endif
#if defined(CONFIG_STUART)
		serial_register(&serial_stuart_device);
#endif
#if defined(CONFIG_S3C2410)
		serial_register(&s3c24xx_serial0_device);
		serial_register(&s3c24xx_serial1_device);
		serial_register(&s3c24xx_serial2_device);
#endif
#if defined(CONFIG_S5P)
		s5p_serial_register(0, NULL);
		s5p_serial_register(1, NULL);
		s5p_serial_register(2, NULL);
		s5p_serial_register(3, NULL);
#endif
#if defined(CONFIG_MPC512X)
#if defined(CONFIG_SYS_PSC1)
		serial_register(&serial1_device);
#endif
#if defined(CONFIG_SYS_PSC3)
		serial_register(&serial3_device);
#endif
#if defined(CONFIG_SYS_PSC4)
		serial_register(&serial4_device);
#endif
#if defined(CONFIG_SYS_PSC6)
		serial_register(&serial6_device);
#endif
#endif
#if defined(CONFIG_SYS_BFIN_UART)
		serial_register_bfin_uart();
#endif
#if defined(CONFIG_XILINX_UARTLITE)
# ifdef XILINX_UARTLITE_BASEADDR
		serial_register(&uartlite_serial0_device);
# endif /* XILINX_UARTLITE_BASEADDR */
# ifdef XILINX_UARTLITE_BASEADDR1
		serial_register(&uartlite_serial1_device);
# endif /* XILINX_UARTLITE_BASEADDR1 */
# ifdef XILINX_UARTLITE_BASEADDR2
		serial_register(&uartlite_serial2_device);
# endif /* XILINX_UARTLITE_BASEADDR2 */
# ifdef XILINX_UARTLITE_BASEADDR3
		serial_register(&uartlite_serial3_device);
# endif /* XILINX_UARTLITE_BASEADDR3 */
#endif /* CONFIG_XILINX_UARTLITE */
	}

	serial_current = default_serial_console();
}

void serial_stdio_init(void)
{
	struct serial_device *sdev = serial_devices;

	/* Register a stdio_dev for every serial_device */
	if (sdev) {
		do {
			stdio_register(&sdev->dev);
			sdev = sdev->next;
		} while (sdev != serial_devices);
	}
}

int serial_assign(const char *name)
{
	struct serial_device *sdev = serial_devices;

	if (sdev) {
		do {
			if (strcmp(sdev->dev.name, name) == 0) {
				serial_current = sdev;
				return 0;
			}
			sdev = sdev->next;
		} while (sdev != serial_devices);
	}

	return 1;
}

void serial_reinit_all(void)
{
	struct serial_device *sdev = serial_devices;

	if (sdev) {
		do {
			sdev->dev.start(&sdev->dev);
			sdev = sdev->next;
		} while (sdev != serial_devices);
	}
}

static struct serial_device *get_current(void)
{
	struct serial_device *sdev;

	if (!(gd->flags & GD_FLG_RELOC) || !serial_current) {
		sdev = default_serial_console();

		/* We must have a console device */
		if (!sdev) 
			panic("Cannot find console");
	} else
		sdev = serial_current;

	return sdev;
}

int serial_init(void)
{
	const struct serial_device *sdev = get_current();

	return sdev->dev.start(&sdev->dev);
}

void serial_setbrg(void)
{
	const struct serial_device *sdev = get_current();

	sdev->setbrg(sdev);
}

int serial_getc(const struct stdio_dev *pdev)
{
	const struct serial_device *sdev = get_current();

	return sdev->dev.getc(&sdev->dev);
}

int serial_tstc(const struct stdio_dev *pdev)
{
	const struct serial_device *sdev = get_current();

	return sdev->dev.tstc(&sdev->dev);
}

void serial_putc(const struct stdio_dev *pdev, const char c)
{
	const struct serial_device *sdev = get_current();

	sdev->dev.putc(&sdev->dev, c);
}

void serial_puts(const struct stdio_dev *pdev, const char *s)
{
	const struct serial_device *sdev = get_current();
	
	sdev->dev.puts(&sdev->dev, s);
}

#if CONFIG_POST & CONFIG_SYS_POST_UART
static const int bauds[] = CONFIG_SYS_BAUDRATE_TABLE;

/* Mark weak until post/cpu/.../uart.c migrate over */
__weak
int uart_post_test(int flags)
{
	unsigned char c;
	int ret, saved_baud, b;
	struct serial_device *saved_dev, *sdev;
	bd_t *bd = gd->bd;

	/* Save current serial state */
	ret = 0;
	saved_dev = serial_current;
	saved_baud = bd->bi_baudrate;

	sdev = serial_devices;
	if (!sdev)
		goto done;

	do {
		/* If this driver doesn't support loop back, skip it */
		if (!sdev->loop)
			continue;

		/* Test the next device */
		serial_current = sdev;

		ret = serial_init();
		if (ret)
			goto done;

		/* Consume anything that happens to be queued */
		while (serial_tstc(&sdev->dev))
			serial_getc(&sdev->dev);

		/* Enable loop back */
		sdev->loop(sdev, 1);

		/* Test every available baud rate */
		for (b = 0; b < ARRAY_SIZE(bauds); ++b) {
			bd->bi_baudrate = bauds[b];
			serial_setbrg();

			/*
			 * Stick to printable chars to avoid issues:
			 *  - terminal corruption
			 *  - serial program reacting to sequences and sending
			 *    back random extra data
			 *  - most serial drivers add in extra chars (like \r\n)
			 */
			for (c = 0x20; c < 0x7f; ++c) {
				/* Send it out */
				serial_putc(&sdev->dev, c);

				/* Make sure it's the same one */
				ret = (c != serial_getc(&sdev->dev));
				if (ret) {
					sdev->loop(sdev, 0);
					goto done;
				}

				/* Clean up the output in case it was sent */
				serial_putc(&sdev->dev, '\b');
				ret = ('\b' != serial_getc(&sdev->dev));
				if (ret) {
					sdev->loop(sdev, 0);
					goto done;
				}
			}
		}

		/* Disable loop back */
		sdev->loop(sdev, 0);
	} while (sdev != serial_devices);

 done:
	/* Restore previous serial state */
	serial_current = saved_dev;
	bd->bi_baudrate = saved_baud;
	serial_reinit_all();
	serial_setbrg();

	return ret;
}
#endif
