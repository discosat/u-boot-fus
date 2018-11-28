/*
 * (C) Copyright 2000
 * Paolo Scaffardi, AIRVENT SAM s.p.a - RIMINI(ITALY), arsenio@tin.it
 *
 * SPDX-License-Identifier:	GPL-2.0+
 */

#ifndef _STDIO_DEV_H_
#define _STDIO_DEV_H_

#include <linux/list.h>

/*
 * STDIO DEVICES
 */

#define DEV_FLAGS_INPUT	 0x00000001	/* Device can be used as input	console */
#define DEV_FLAGS_OUTPUT 0x00000002	/* Device can be used as output console */
#define DEV_FLAGS_SYSTEM 0x80000000	/* Device is a system device		*/
#define DEV_EXT_VIDEO	 0x00000001	/* Video extensions supported		*/

#define DEV_NAME_SIZE 16

/* Device information */
typedef struct stdio_dev stdio_dev_t;

struct stdio_dev {
	int	flags;			/* Device flags: input/output/system */
	char	name[DEV_NAME_SIZE];	/* Device name, e.g. ttySAC1 */
	char	hwname[DEV_NAME_SIZE];	/* Hardware name, e.g. s5p_uart1 */

/* GENERAL functions */
	/* To start the device */
	int (*start) (const struct stdio_dev *pdev);

	/* To stop the device */
	int (*stop) (const struct stdio_dev *pdev);

/* OUTPUT functions */
	/* To put a char */
	void (*putc) (const struct stdio_dev *pdev, const char c);

	/* To put a string (accelerator) */
	void (*puts) (const struct stdio_dev *pdev, const char *s);

/* INPUT functions */
	/* To test if a char is ready */
	int (*tstc) (const struct stdio_dev *pdev);

	/* To get that char */
	int (*getc) (const struct stdio_dev *pdev);

/* Other functions */

	void *priv;			/* Private extensions */
//###	int	ext;			/* Supported extensions */

	/* The stdio devices are stored in a ring structure. So the last
	   element points with its next pointer back again to the first
	   element and the first element points with its prev pointer back
	   again to the last element. The beginning of the ring is stored in
	   variable devs. */
	struct stdio_dev *next;		/* Next device */
	struct stdio_dev *prev;		/* Next device */
#ifdef CONFIG_CONSOLE_MUX
	/* Each of these pointers is part of a linked list; the head of the
	   list is in variable stdio_devices[i]. */
	struct stdio_dev *file_next[MAX_FILES];	/* Next device in file list */
#endif
};

/*
 * VIDEO EXTENSIONS
 */
#define VIDEO_FORMAT_RGB_INDEXED	0x0000
#define VIDEO_FORMAT_RGB_DIRECTCOLOR	0x0001
#define VIDEO_FORMAT_YUYV_4_4_4		0x0010
#define VIDEO_FORMAT_YUYV_4_2_2		0x0011

typedef struct {
	void *address;			/* Address of framebuffer		*/
	ushort	width;			/* Horizontal resolution		*/
	ushort	height;			/* Vertical resolution			*/
	uchar	format;			/* Format				*/
	uchar	colors;			/* Colors number or color depth		*/
	void (*setcolreg) (int, int, int, int);
	void (*getcolreg) (int, void *);
} video_ext_t;

/*
 * VARIABLES
 */
extern struct stdio_dev *stdio_devices[];
extern char *stdio_names[MAX_FILES];

/*
 * PROTOTYPES
 */
int	stdio_register (struct stdio_dev * dev);
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
#ifdef CONFIG_SYS_STDIO_DEREGISTER
int	stdio_deregister(const char *devname);
#endif
struct stdio_dev *stdio_get_list(void);
struct stdio_dev* stdio_get_by_name(const char* name);
struct stdio_dev* stdio_clone(struct stdio_dev *dev);

#ifdef CONFIG_LCD
void	drv_lcd_init(void);
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
