/*
 * (C) Copyright 2000
 * Paolo Scaffardi, AIRVENT SAM s.p.a - RIMINI(ITALY), arsenio@tin.it
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

#include <config.h>
#include <common.h>
#include <stdarg.h>
#include <malloc.h>
#include <stdio_dev.h>
#include <serial.h>
#ifdef CONFIG_LOGBUFFER
#include <logbuff.h>
#endif
#if defined(CONFIG_HARD_I2C) || defined(CONFIG_SOFT_I2C)
#include <i2c.h>
#endif

DECLARE_GLOBAL_DATA_PTR;

static struct stdio_dev *devs;
struct stdio_dev *stdio_devices[] = { NULL, NULL, NULL };
char *stdio_names[MAX_FILES] = { "stdin", "stdout", "stderr" };

#if defined(CONFIG_SPLASH_SCREEN) && !defined(CONFIG_SYS_DEVICE_NULLDEV)
#define	CONFIG_SYS_DEVICE_NULLDEV	1
#endif


struct stdio_dev serial_dev = {
	.name = "serial",
	.flags = DEV_FLAGS_OUTPUT | DEV_FLAGS_INPUT | DEV_FLAGS_SYSTEM,
	.putc = serial_putc,
	.puts = serial_puts,
	.getc = serial_getc,
	.tstc = serial_tstc
};

#ifdef CONFIG_SYS_DEVICE_NULLDEV
void nulldev_putc(const device_t *pdev, const char c)
{
	/* nulldev is empty! */
}

void nulldev_puts(const device_t *pdev, const char *s)
{
	/* nulldev is empty! */
}

int nulldev_input(const device_t *pdev)
{
	/* nulldev is empty! */
	return 0;
}

struct stdio_dev null_dev = {
	.name = "nulldev",
	.flags = DEV_FLAGS_OUTPUT | DEV_FLAGS_INPUT | DEV_FLAGS_SYSTEM,
	.putc = nulldev_putc,
	.puts = nulldev_puts,
	.getc = nulldev_input,
	.tstc = nulldev_input
};
#endif

/**************************************************************************
 * SYSTEM DRIVERS
 **************************************************************************
 */

static void drv_system_init (void)
{
	stdio_register(&serial_dev);

#ifdef CONFIG_SYS_DEVICE_NULLDEV
	stdio_register(&null_dev);
#endif
}

/**************************************************************************
 * DEVICES
 **************************************************************************
 */
struct stdio_dev *stdio_get_list(void)
{
	return devs;
}

struct stdio_dev* stdio_get_by_name(const char *name)
{
	struct stdio_dev *dev = devs;

	if (name && devs) {
		do {
			if(strcmp(dev->name, name) == 0)
				return dev;
			dev = dev->next;
		} while (dev != devs);
	}

	return NULL;
}

int stdio_register (struct stdio_dev *dev)
{
#ifdef CONFIG_CONSOLE_MUX
	int i;

	/* Clear all links that represent lists */
	for (i = 0; i < MAX_FILES; i++)
		dev->file_next[i] = NULL;
#endif

	if (!devs) {
		/* First element, loop back to itself */
		dev->next = dev;
		dev->prev = dev;
		devs = dev;
	} else {
		/* Add at end; devs points to the first element in the device
		   ring, so devs->prev points to the last element */
		dev->prev = devs->prev;
		dev->prev->next = dev;
		dev->next = devs;
		dev->next->prev = dev;
	}
	return 0;
}

/* deregister the device "devname".
 * returns 0 if success, -1 if device is assigned and 1 if devname not found
 */
#ifdef	CONFIG_SYS_STDIO_DEREGISTER
int stdio_deregister(const char *devname)
{
	int i;
	struct stdio_dev *dev;
	struct stdio_dev *tmp;

	dev = stdio_get_by_name(devname);
	if (!dev) /* device not found */
		return -1;

	/* Check if device is assigned */
	for (i=0 ; i< MAX_FILES; i++) {
#ifdef CONFIG_CONSOLE_MUX
		tmp = stdio_devices[i];

		while (tmp) {
			if (tmp == dev)
				return -1; /* in use */
			tmp = tmp->file_next[i];
		}
#else
		if (stdio_devices[i] == dev)
			return -1;	  /* in use */
#endif
	}

	/* The device is not part of any of the active stdio devices. Now
	   find device in the device list */
	tmp = devs;
	while (tmp != dev)
		tmp = tmp->next;

	/* Unlink device from device ring list */
	if (tmp->next == tmp)
		devs = NULL;		  /* It was the last device */
	else {
		/* Update head if this is the first element */
		if (tmp == devs)
			devs = tmp->next;

		tmp->prev->next = tmp->next;
		tmp->next->prev = tmp->prev;

	}

	return 0;
}
#endif	/* CONFIG_SYS_STDIO_DEREGISTER */

int stdio_init (void)
{
#if defined(CONFIG_NEEDS_MANUAL_RELOC)
	/* already relocated for current ARM implementation */
	ulong relocation_offset = gd->reloc_off;
	int i;

	/* relocate device name pointers */
	for (i = 0; i < (sizeof (stdio_names) / sizeof (char *)); ++i) {
		stdio_names[i] = (char *) (((ulong) stdio_names[i]) +
						relocation_offset);
	}
#endif /* CONFIG_NEEDS_MANUAL_RELOC */

	devs = NULL;

#ifdef CONFIG_ARM_DCC_MULTI
	drv_arm_dcc_init ();
#endif
#if defined(CONFIG_HARD_I2C) || defined(CONFIG_SOFT_I2C)
	i2c_init (CONFIG_SYS_I2C_SPEED, CONFIG_SYS_I2C_SLAVE);
#endif
#if defined(CONFIG_LCD) || defined(CONFIG_CMD_LCD)
	drv_lcd_init ();
#endif
#if defined(CONFIG_VIDEO) || defined(CONFIG_CFB_CONSOLE)
	drv_video_init ();
#endif
#ifdef CONFIG_KEYBOARD
	drv_keyboard_init ();
#endif
#ifdef CONFIG_LOGBUFFER
	drv_logbuff_init ();
#endif
	drv_system_init ();
	serial_stdio_init ();
#ifdef CONFIG_USB_TTY
	drv_usbtty_init ();
#endif
#ifdef CONFIG_NETCONSOLE
	drv_nc_init ();
#endif
#ifdef CONFIG_JTAG_CONSOLE
	drv_jtag_console_init ();
#endif
#ifdef CONFIG_CBMEM_CONSOLE
	cbmemc_init();
#endif
	return (0);
}
