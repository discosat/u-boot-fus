/*
 * Copyright (C) 2009 Sergey Kubushyn <ksi@koi8.net>
 *
 * Changes for multibus/multiadapter I2C support.
 *
 * (C) Copyright 2000
 * Paolo Scaffardi, AIRVENT SAM s.p.a - RIMINI(ITALY), arsenio@tin.it
 *
 * SPDX-License-Identifier:	GPL-2.0+
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

#if defined(CONFIG_HARD_I2C) || defined(CONFIG_SYS_I2C)
#include <i2c.h>
#endif

DECLARE_GLOBAL_DATA_PTR;

static struct stdio_dev *devs;
struct stdio_dev *stdio_devices[] = { NULL, NULL, NULL };
char *stdio_names[MAX_FILES] = { "stdin", "stdout", "stderr" };

#if defined(CONFIG_SPLASH_SCREEN) && !defined(CONFIG_SYS_DEVICE_NULLDEV)
#define	CONFIG_SYS_DEVICE_NULLDEV	1
#endif

#if CONFIG_IS_ENABLED(SYS_STDIO_DEREGISTER)
#define	CONFIG_SYS_DEVICE_NULLDEV	1
#endif

struct stdio_dev serial_dev = {
	.name = "serial",
	.flags = DEV_FLAGS_OUTPUT | DEV_FLAGS_INPUT,
	.putc = serial_putc,
	.puts = serial_puts,
	.getc = serial_getc,
	.tstc = serial_tstc
};

#ifdef CONFIG_SYS_DEVICE_NULLDEV
static void nulldev_putc(const struct stdio_dev *pdev, const char c)
{
	/* nulldev is empty! */
}

static void nulldev_puts(const struct stdio_dev *pdev, const char *s)
{
	/* nulldev is empty! */
}

static int nulldev_input(const struct stdio_dev *pdev)
{
	/* nulldev is empty! */
	return 0;
}

struct stdio_dev null_dev = {
	.name = "nulldev",
	.flags = DEV_FLAGS_OUTPUT | DEV_FLAGS_INPUT,
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

#ifdef CONFIG_DM_VIDEO
/**
 * stdio_probe_device() - Find a device which provides the given stdio device
 *
 * This looks for a device of the given uclass which provides a particular
 * stdio device. It is currently really only useful for UCLASS_VIDEO.
 *
 * Ultimately we want to be able to probe a device by its stdio name. At
 * present devices register in their probe function (for video devices this
 * is done in vidconsole_post_probe()) and we don't know what name they will
 * use until they do so.
 * TODO(sjg@chromium.org): We should be able to determine the name before
 * probing, and probe the required device.
 *
 * @name:	stdio device name (e.g. "vidconsole")
 * id:		Uclass ID of device to look for (e.g. UCLASS_VIDEO)
 * @sdevp:	Returns stdout device, if found, else NULL
 * @return 0 if found, -ENOENT if no device found with that name, other -ve
 *	   on other error
 */
static int stdio_probe_device(const char *name, enum uclass_id id,
			      struct stdio_dev **sdevp)
{
	struct stdio_dev *sdev;
	struct udevice *dev;
	int seq, ret;

	*sdevp = NULL;
	seq = trailing_strtoln(name, NULL);
	if (seq == -1)
		seq = 0;
	ret = uclass_get_device_by_seq(id, seq, &dev);
	if (ret == -ENODEV)
		ret = uclass_first_device_err(id, &dev);
	if (ret) {
		debug("No %s device for seq %d (%s)\n", uclass_get_name(id),
		      seq, name);
		return ret;
	}
	/* The device should be be the last one registered */
	sdev = list_empty(&devs.list) ? NULL :
			list_last_entry(&devs.list, struct stdio_dev, list);
	if (!sdev || strcmp(sdev->name, name)) {
		debug("Device '%s' did not register with stdio as '%s'\n",
		      dev->name, name);
		return -ENOENT;
	}
	*sdevp = sdev;

	return 0;
}
#endif

struct stdio_dev *stdio_get_by_name(const char *name)
{
	struct stdio_dev *dev = devs;

	if (name && devs) {
		do {
			if(strcmp(dev->name, name) == 0)
				return dev;
			dev = dev->next;
		} while (dev != devs);
	}
#ifdef CONFIG_DM_VIDEO
	/*
	 * We did not find a suitable stdio device. If there is a video
	 * driver with a name starting with 'vidconsole', we can try probing
	 * that in the hope that it will produce the required stdio device.
	 *
	 * This function is sometimes called with the entire value of
	 * 'stdout', which may include a list of devices separate by commas.
	 * Obviously this is not going to work, so we ignore that case. The
	 * call path in that case is console_init_r() -> search_device() ->
	 * stdio_get_by_name().
	 */
	if (!strncmp(name, "vidconsole", 10) && !strchr(name, ',') &&
	    !stdio_probe_device(name, UCLASS_VIDEO, &sdev))
		return sdev;
#endif

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
#if CONFIG_IS_ENABLED(SYS_STDIO_DEREGISTER)
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
#endif /* CONFIG_IS_ENABLED(SYS_STDIO_DEREGISTER) */

int stdio_init_tables(void)
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

	return 0;
}

int stdio_add_devices(void)
{
#ifdef CONFIG_DM_KEYBOARD
	struct udevice *dev;
	struct uclass *uc;
	int ret;

	/*
	 * For now we probe all the devices here. At some point this should be
	 * done only when the devices are required - e.g. we have a list of
	 * input devices to start up in the stdin environment variable. That
	 * work probably makes more sense when stdio itself is converted to
	 * driver model.
	 *
	 * TODO(sjg@chromium.org): Convert changing uclass_first_device() etc.
	 * to return the device even on error. Then we could use that here.
	 */
	ret = uclass_get(UCLASS_KEYBOARD, &uc);
	if (ret)
		return ret;

	/* Don't report errors to the caller - assume that they are non-fatal */
	uclass_foreach_dev(dev, uc) {
		ret = device_probe(dev);
		if (ret)
			printf("Failed to probe keyboard '%s'\n", dev->name);
	}
#endif
#ifdef CONFIG_SYS_I2C
	i2c_init_all();
#else
#if defined(CONFIG_HARD_I2C)
	i2c_init (CONFIG_SYS_I2C_SPEED, CONFIG_SYS_I2C_SLAVE);
#endif
#endif
#ifdef CONFIG_DM_VIDEO
	/*
	 * If the console setting is not in environment variables then
	 * console_init_r() will not be calling iomux_doenv() (which calls
	 * search_device()). So we will not dynamically add devices by
	 * calling stdio_probe_device().
	 *
	 * So just probe all video devices now so that whichever one is
	 * required will be available.
	 */
#ifndef CONFIG_SYS_CONSOLE_IS_IN_ENV
	struct udevice *vdev;
# ifndef CONFIG_DM_KEYBOARD
	int ret;
# endif

	for (ret = uclass_first_device(UCLASS_VIDEO, &vdev);
	     vdev;
	     ret = uclass_next_device(&vdev))
		;
	if (ret)
		printf("%s: Video device failed (ret=%d)\n", __func__, ret);
#endif /* !CONFIG_SYS_CONSOLE_IS_IN_ENV */
#else
# if defined(CONFIG_LCD)
	drv_lcd_init ();
# endif
# if defined(CONFIG_VIDEO) || defined(CONFIG_CFB_CONSOLE)
	drv_video_init ();
# endif
#endif /* CONFIG_DM_VIDEO */
#if defined(CONFIG_KEYBOARD) && !defined(CONFIG_DM_KEYBOARD)
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

	return 0;
}

int stdio_init(void)
{
	stdio_init_tables();
	stdio_add_devices();

	return 0;
}
