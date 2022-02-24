/* SPDX-License-Identifier: GPL-2.0+ */
/*
 * (C) Copyright 2000
 * Paolo Scaffardi, AIRVENT SAM s.p.a - RIMINI(ITALY), arsenio@tin.it
 */

#ifndef _STDIO_DEV_H_
#define _STDIO_DEV_H_

#include <stdio.h>
#include <linux/list.h>

/*
 * STDIO DEVICES
 */

#define DEV_FLAGS_INPUT	 0x00000001	/* Device can be used as input	console */
#define DEV_FLAGS_OUTPUT 0x00000002	/* Device can be used as output console */
#define DEV_FLAGS_DM     0x00000004	/* Device priv is a struct udevice * */

#define DEV_NAME_SIZE 32

/* Device information */
typedef struct stdio_dev stdio_dev_t;

struct stdio_dev {
	int	flags;			/* Device flags: input/output/system */
//###	int	ext;			/* Supported extensions */
	char	name[DEV_NAME_SIZE];	/* Device name */

/* GENERAL functions */
	/* To start the device */
	int (*start) (const struct stdio_dev *dev);

	/* To stop the device */
	int (*stop) (const struct stdio_dev *dev);

/* OUTPUT functions */
	/* To put a char */
	void (*putc) (const struct stdio_dev *dev, const char c);

	/* To put a string (accelerator) */
	void (*puts) (const struct stdio_dev *dev, const char *s);

/* INPUT functions */
	/* To test if a char is ready */
	int (*tstc) (const struct stdio_dev *dev);

	/* To get that char */
	int (*getc) (const struct stdio_dev *dev);

/* Other functions */

	void *priv;			/* Private extensions */
	struct list_head list;
};

/*
 * VARIABLES
 */
extern struct stdio_dev *stdio_devices[];
extern char *stdio_names[MAX_FILES];

/*
 * PROTOTYPES
 */
int stdio_register(const struct stdio_dev * dev);
int stdio_register_dev(const struct stdio_dev *dev, struct stdio_dev **devp);

/**
 * stdio_init_tables() - set up stdio tables ready for devices
 *
 * This does not add any devices, but just prepares stdio for use.
 */
int stdio_init_tables(void);

/**
 * stdio_add_devices() - Add stdio devices to the table
 *
 * This makes calls to all the various subsystems that use stdio, to make
 * them register with stdio.
 */
int stdio_add_devices(void);

/**
 * stdio_init() - Sets up stdio ready for use
 *
 * This calls stdio_init_tables() and stdio_add_devices()
 */
int	stdio_init (void);

void	stdio_print_current_devices(void);
#if CONFIG_IS_ENABLED(SYS_STDIO_DEREGISTER)
int stdio_deregister(const char *devname, int force);
int stdio_deregister_dev(struct stdio_dev *dev, int force);
#endif
struct list_head* stdio_get_list(void);
struct stdio_dev* stdio_get_by_name(const char* name);
struct stdio_dev* stdio_clone(const struct stdio_dev *dev);

#ifdef CONFIG_LCD
int	drv_lcd_init (void);
#endif
#if defined(CONFIG_VIDEO) || defined(CONFIG_CFB_CONSOLE)
int	drv_video_init (void);
#endif
#ifdef CONFIG_KEYBOARD
int	drv_keyboard_init (void);
#endif
#ifdef CONFIG_USB_TTY
int	drv_usbtty_init (void);
#endif
#ifdef CONFIG_NETCONSOLE
int	drv_nc_init (void);
#endif
#ifdef CONFIG_JTAG_CONSOLE
int drv_jtag_console_init (void);
#endif
#ifdef CONFIG_CBMEM_CONSOLE
int cbmemc_init(void);
#endif

#endif
