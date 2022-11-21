/*
 * Generic LCD declarations
 *
 * (C) Copyright 2012
 * Hartmut Keller, F&S Elektronik Systeme GmbH, keller@fs-net.de
 *
 * SPDX-License-Identifier:	GPL-2.0+
 */

#ifndef _CMD_LCD_H_
#define _CMD_LCD_H_

/* Define some types to be used with displays */
typedef unsigned int COLOR32;
typedef unsigned int RGBA;
typedef unsigned char VID;
typedef unsigned char WINDOW;
typedef unsigned char PIX;
typedef int XYPOS;

#include <xlcd_panels.h>		  /* lcdinfo_t */
#include "stdio_dev.h"			  /* struct stdio_dev */

/************************************************************************/
/* DEFINITIONS								*/
/************************************************************************/

/* Supported draw functions, combine with | in CONFIG_XLCD_DRAW */
#define XLCD_DRAW_PIXEL  0x0001		  /* pixel */
#define XLCD_DRAW_LINE   0x0002		  /* line */
#define XLCD_DRAW_RECT   0x0004		  /* frame, rect */
#define XLCD_DRAW_CIRC   0x0008		  /* circle, disc, rframe, rrect */
#define XLCD_DRAW_TEXT   0x0010		  /* text */
#define XLCD_DRAW_BITMAP 0x0020		  /* bm */
#define XLCD_DRAW_TURTLE 0x0040		  /* turtle */
#define XLCD_DRAW_FILL   0x0080		  /* fill, clear */
#define XLCD_DRAW_PROG   0x0100		  /* pbr, pbt, prog */
#define XLCD_DRAW_TEST   0x0200		  /* test */
#define XLCD_DRAW_ALL    0xFFFF		  /* All of the above */

/* Supported test images, combine with | in CONFIG_XLCD_TEST */
#define XLCD_TEST_GRID   0x01		  /* Color grid */
#define XLCD_TEST_COLORS 0x02		  /* Eight basic colors */
#define XLCD_TEST_D2B    0x04		  /* All colors dark to bright */
#define XLCD_TEST_GRAD   0x08		  /* Hue at edges to gray in center */
#define XLCD_TEST_ALL    0xFF		  /* all of the above */

/* Pixel info flags */
#define PIF_TRUECOL   0x00		  /* Pixel format is true color */
#define PIF_CMAP      0x01		  /* Pixel format uses a color map */
#define PIF_NO_ALPHA  0x00		  // Pixel has no alpha value
#define PIF_ALPHA     0x02		  /* Pixel has alpha value */

/* Draw attributes */
#define ATTR_HLEFT    0x0000		  /* Available for text + pbt + bm */
#define ATTR_HCENTER  0x0001		  /* Available for text + pbt + bm */
#define ATTR_HRIGHT   0x0002		  /* Available for text + pbt + bm */
#define ATTR_HRIGHT1  0x0003		  /* Available for text + bm */
#define ATTR_HFOLLOW  0x0003		  /* Available for pbt */
#define ATTR_HMASK    0x0003		  /* Horizontal reference point mask */
#define ATTR_VTOP     0x0000		  /* Available for text + pbt + bm */
#define ATTR_VCENTER  0x0004		  /* Available for text + pbt + bm */
#define ATTR_VBOTTOM  0x0008		  /* Available for text + pbt + bm */
#define ATTR_VBOTTOM1 0x000C		  /* Available for text + bm */
#define ATTR_NO_TEXT  0x000C		  /* Available for pbt */
#define ATTR_VMASK    0x000C		  /* Vertical reference point mask */
#define ATTR_HSINGLE  0x0000		  /* Available for text + pbt + bm */
#define ATTR_HDOUBLE  0x0010		  /* Available for text + pbt + bm */
#define ATTR_HTRIPLE  0x0020		  /* Available for text + pbt + bm */
#define ATTR_HQUAD    0x0030		  /* Available for text + pbt + bm */
#define ATTR_HS_MASK  0x0030		  /* Horizontal scaling mask */
#define ATTR_VSINGLE  0x0000		  /* Available for text + pbt + bm */
#define ATTR_VDOUBLE  0x0040		  /* Available for text + pbt + bm */
#define ATTR_VTRIPLE  0x0080		  /* Available for text + pbt + bm */
#define ATTR_VQUAD    0x00C0		  /* Available for text + pbt + bm */
#define ATTR_VS_MASK  0x00C0		  /* Vertical scaling mask */
#define ATTR_BOLD     0x0100		  /* Available for text + pbt */
#define ATTR_INVERSE  0x0200		  /* Available for text + pbt */
#define ATTR_UNDERL   0x0400		  /* Available for text + pbt */
#define ATTR_STRIKE   0x0800		  /* Available for text + pbt */
#define ATTR_NO_BG    0x1000		  /* Available for text + pbt */

#define ATTR_ALPHA    0x8000		  /* Available for all functions */

/* PWM value for maximum voltage */
#define MAX_PWM 4096

/* These appear so often that a macro seems appropriate */
#define lcd_set_fg(pwi, rgba) lcd_set_col(pwi, rgba, &pwi->fg)
#define lcd_set_bg(pwi, rgba) lcd_set_col(pwi, rgba, &pwi->bg)


/************************************************************************/
/* TYPES AND STRUCTURES							*/
/************************************************************************/

/* Forward declaration */
typedef struct wininfo wininfo_t;
typedef struct vidinfo vidinfo_t;

/* Structure for holding color info, incl. alpha pre-multiply data */
typedef struct colinfo {
	COLOR32 col;			  /* Color as COLOR32 */
	RGBA rgba;			  /* Color as RGBA */
	RGBA RA1;			  /* Red * (Alpha+1) (16 bits) */
	RGBA GA1;			  /* Green * (Alpha+1) (16 bits) */
	RGBA BA1;			  /* Blue * (Alpha+1) (16 bits) */
	RGBA A256;			  /* 256-Alpha (8 bits) */
} colinfo_t;

/* Structure to store window specific alpha information; from, to and now are
   only valid if time != 0. */
typedef struct alphainfo {
	RGBA alpha;			  /* Current alpha value */
	RGBA from;			  /* Alpha to fade from */
	RGBA to;			  /* Alpha to fade to */
	int time;			  /* Total fading time (in ms) */
	int now;			  /* Current fading time (in ms) */
} alphainfo_t;

/* Pixel format information */
typedef struct PIXEL_INFO {
	char *name;			/* Format description */
	u_char depth;			/* Actually used bits for the color */
	u_char bpp_shift;		/* Bits per pixel as power of 2;
					   0: 1 bpp, 1: 2 bpp, .. 5: 32bpp */
	u_char flags;			/* Bit 0: 0: true color, 1: palettized
					   Bit 1: 0: no alpha, 1: alpha */
	u_char priv;			/* Private byte for the driver */

	/* Function to convert RGBA to COLOR32 */
	COLOR32 (*rgba2col)(const wininfo_t *pwi, RGBA rgba);

	/* Function to convert COLOR32 to RGBA */
	RGBA (*col2rgba)(const wininfo_t *pwi, COLOR32 color);

#ifdef CONFIG_CMD_ADRAW
	/* Function to apply Alpha and pre-multiplied pixel to COLOR32 value */
	COLOR32 (*apply_alpha)(const wininfo_t *pwi, const colinfo_t *pci,
			       COLOR32 oldcol);
#endif
} pixinfo_t;

/* Console information */
typedef struct coninfo {
	u_short x;			  /* Current writing position */
	u_short y;			  /* (aligned to characters) */
	COLOR32 fg;			  /* Foreground and background color */
	COLOR32 bg;
} coninfo_t;

/* Progress bar information */
typedef struct pbinfo {
	/* pbr info */
	XYPOS x1;			  /* Bar position and size */
	XYPOS y1;
	XYPOS x2;
	XYPOS y2;
	colinfo_t rect_fg;		  /* Color of progress bar */
	colinfo_t rect_bg;		  /* Color if background */
	/* pbt info */
	colinfo_t text_fg;		  /* Color of percentage text */
	colinfo_t text_bg;
	u_int attr;			  /* Attribute of percentage text */
	/* prog info */
	u_int prog;			  /* Percentage */
} pbinfo_t;


/* Video/LCD device info */
struct vidinfo
{
	/* Driver specific display info */
	char *driver_name;		  /* Name of display driver */
	char name[12];			  /* "lcd0" or "lcd" */
	wininfo_t *pwi;			  /* Pointer to info about windows */
	WINDOW wincount;		  /* Number of available windows */
	WINDOW win_sel;			  /* Currently selected window */
	PIX pixcount;			  /* Number of av. pixel formats */
	VID vid;			  /* Current display number */
	u_char is_enabled;		  /* Flag if LCD is switched on */

	/* Extra settings */
	u_char	frc;		/* Dither mode (FRC) #### */
	u_char	drive;		/* Drive strength: 0=2mA, 1=4mA, 2=7mA, 3=9mA */
	u_char	reserved;	/* (needed for alignment anyway) */

	/* Driver independent LCD panel info */
	lcdinfo_t lcd;			  /* Info about lcd panel */

	/* Function to get a pointer to the info for pixel format pix */
	const pixinfo_t *(*get_pixinfo_p)(WINDOW win, PIX pix);

	/* Return maximum horizontal framebuffer resolution for this window */
	XYPOS (*get_fbmaxhres)(WINDOW win, PIX pix);

	/* Return maximum vertical framebuffer resolution for this window */
	XYPOS (*get_fbmaxvres)(WINDOW win, PIX pix);

	/* Align horizontal resolution to next word boundary (round up) */
	XYPOS (*align_hres)(WINDOW win, PIX pix, XYPOS hres);

	/* Align horizontal offset to current word boundary (round down) */
	XYPOS (*align_hoffs)(const wininfo_t *pwi, XYPOS hoffs);

	/* Function to update controller hardware with new vidinfo; vidinfo
	   is updated with actually used hardware settings */
	void (*set_vidinfo)(vidinfo_t *pvi);

	/* Function to update controller hardware with new wininfo; wininfo
	   is not changed */
	void (*set_wininfo)(const wininfo_t *pwi);

	/* Function to switch LCD on */
	void (*enable)(const vidinfo_t *pvi);

	/* Function to switch LCD off */
	void (*disable)(const vidinfo_t *pvi);
};


/* Common window information */
struct wininfo
{
	/* Window information */
	WINDOW win;			  /* Number of this window */
	u_char active;			  /* Flag if window is active */
	PIX defpix;			  /* Default pixel format */
	PIX pix;			  /* Current pixel format */
	char name[12];			  /* "win0_0" or "win0" */
	const pixinfo_t *ppi;		  /* Pointer to pixel format info */
	XYPOS hres;			  /* Size of visible window */
	XYPOS vres;
	XYPOS hpos;			  /* Position of window on display */
	XYPOS vpos;

	/* Framebuffer information */
	u_long *pfbuf;			  /* Pointers to buffers */
	u_long fbsize;			  /* Size of one buffer (bytes) */
	u_long linelen;			  /* Bytes per fbhres line */
	u_char fbcount;			  /* Number of active buffers */
	u_char fbmaxcount;		  /* Maximum active buffer count */
	u_char fbdraw;			  /* Index of buffer to draw to */
	u_char fbshow;			  /* Index of buffer to show */
	XYPOS fbhres;			  /* Virtual size of framebuffer */
	XYPOS fbvres;
	XYPOS hoffs;			  /* Offset within framebuffer (>=0) */
	XYPOS voffs;

	/* Drawing information, only accessed by draw commands */
	colinfo_t fg;			  /* Foreground color info */
	colinfo_t bg;			  /* Foreground color info */
	u_int text_attr;		  /* Current text attribute */
	u_int attr;			  /* Attribute only for next drawing */
	XYPOS clip_left;		  /* Current clipping region */
	XYPOS clip_top;
	XYPOS clip_right;
	XYPOS clip_bottom;
	XYPOS horigin;			  /* Current drawing origin */
	XYPOS vorigin;
	pbinfo_t pbi;			  /* Progress bar info */

	/* Alpha and color keying information */
	alphainfo_t ai[2];		  /* Alpha info for A=0 and A=1 */
	u_char alphamode;		  /* 0: alpha0, 1: alpha1, 2: pixel */
	u_char ckmode;			  /* Color keying mode */
	RGBA ckvalue;			  /* Color keying RGBA value */
	RGBA ckmask;			  /* Color keying mask */
	RGBA replace;			  /* Replacement color for window */
	RGBA *cmap;			  /* If CLUT: Pointer to color map */

#ifdef CONFIG_CMD_CMAP
	/* Function to set the color map from index to end; this also updates
	   pwi->cmap. Some hardware has restrictions of how and when setting
	   new palette entries is possible (e.g. only between frames), so
	   please always update the color map in as large quantities as
	   possible. */
	void (*set_cmap)(const wininfo_t *pwi, u_int index, u_int end,
			 RGBA *prgba);
#endif

#ifdef CONFIG_XLCD_CONSOLE_MULTI
	coninfo_t ci;			  /* Console info for this window */
#endif

	/* Display information */
	vidinfo_t *pvi;			  /* Pointer to corresponding display */
};


/* Info about minimal and maximal argument count, sub-command index and
   keyword string for commands using sub-commands (e.g. lcd, window, draw) */
typedef struct kwinfo
{
	u_char	argc_min;
	u_char	argc_max;
	u_char	info1;
	u_char	info2;
	char	*keyword;
} kwinfo_t;


/************************************************************************/
/* PROTOTYPES OF EXPORTED FUNCTIONS					*/
/************************************************************************/

/* Console functions */
#ifndef CONFIG_MULTIPLE_CONSOLES
extern void console_init(wininfo_t *pwi, RGBA fg, RGBA bg);
extern void console_update(wininfo_t *pwi, RGBA fg, RGBA bg);
#endif

extern void console_cls(const wininfo_t *pwi, COLOR32 col);
extern void lcd_putc(const struct stdio_dev *pdev, const char c);
extern void lcd_puts(const struct stdio_dev *pdev, const char *s);

/* Set colinfo structure */
extern void lcd_set_col(wininfo_t *pwi, RGBA rgba, colinfo_t *pci);

/* Move offset if window would not fit within framebuffer */
extern void fix_offset(wininfo_t *wi);

/* Set new framebuffer resolution, pixel format, and/or framebuffer count */
extern int setfbuf(wininfo_t *pwi, XYPOS hres, XYPOS vres,
		   XYPOS fbhres, XYPOS fbvres, PIX pix, u_char fbcount);

/* If not locked, update window hardware and set environment variable */
extern void set_wininfo(const wininfo_t *pwi);

/* Get a pointer to the wininfo structure */
extern wininfo_t *lcd_get_wininfo_p(const vidinfo_t *pvi, WINDOW win);

/* Get a pointer to currently selected lcd panel information */
extern vidinfo_t *lcd_get_sel_vidinfo_p(void);

/* Repeat color value so that it fills the whole 32 bits */
extern COLOR32 col2col32(const wininfo_t *pwi, COLOR32 color);

/* Lookup nearest possible color in given color map */
extern COLOR32 lcd_rgbalookup(RGBA rgba, RGBA *cmap, unsigned count);

/* Parse #rrggbb or #rrggbbaa value; return 1 on error, 0 on success */
extern int parse_rgb(char *s, u_int *prgba);

/* Parse sub-command and return sub-command index */
extern u_short parse_sc(int argc, char *s, u_short sc,
			const kwinfo_t *pki, u_short count);

/* Find next delay entry in power-on/power-off sequence */
extern int find_delay_index(const u_short *delays, int index, u_short value);

/* Initialize panel and window information */
extern void drv_lcd_init(void);

#endif /*!_CMD_LCD_H_*/
