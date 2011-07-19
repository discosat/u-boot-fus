/*
 * Generic LCD commands
 *
 * (C) Copyright 2011
 * Hartmut Keller, F&S Elektronik Systeme GmbH, keller@fs-net.de
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
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.	 See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston,
 * MA 02111-1307 USA
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
typedef unsigned int HVRES;

#include <lcd_panels.h>			  /* vidinfo_t */

/************************************************************************/
/* DEFINITIONS								*/
/************************************************************************/

/* Pixel info flags */
#define PIF_CMAP     0x01		  /* Pixel format uses a color map */
#define PIF_ALPHA    0x02		  /* Pixel have alpha value */

/* Draw attributes */
#define ATTR_HLEFT   0x0000		  /* Available for text + bitmap */
#define ATTR_HRIGHT  0x0001		  /* Available for text + bitmap */
#define ATTR_HCENTER 0x0002		  /* Available for text + bitmap */
#define ATTR_HSCREEN 0x0003		  /* Available for text + bitmap */
#define ATTR_HMASK   0x0003		  /* Available for text + bitmap */
#define ATTR_VTOP    0x0000		  /* Available for text + bitmap */
#define ATTR_VBOTTOM 0x0004		  /* Available for text + bitmap */
#define ATTR_VCENTER 0x0008		  /* Available for text + bitmap */
#define ATTR_VSCREEN 0x000C		  /* Available for text + bitmap */
#define ATTR_VMASK   0x000C		  /* Available for text + bitmap */
#define ATTR_DWIDTH  0x0010		  /* Available for text + bitmap */
#define ATTR_DHEIGHT 0x0020		  /* Available for text + bitmap */
#define ATTR_TRANSP  0x0040		  /* Available for text + bitmap */
#define ATTR_ALPHA   0x0080		  /* Available for all functions */
#define ATTR_BOLD    0x0100		  /* Available for text only */
#define ATTR_INVERSE 0x0200		  /* Available for text only */
#define ATTR_UNDERL  0x0400		  /* Available for text only */
#define ATTR_STRIKE  0x0800		  /* Available for text only */

/* PWM value for maximum voltage */
#define MAX_PWM 4096


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

/* Pixel format information */
typedef struct PIXEL_INFO {
	u_char depth;			/* Actually used bits for the color */
	u_char bpp_shift;		/* Bits per pixel as power of 2;
					   0: 1 bpp, 1: 2 bpp, .. 5: 32bpp */
	u_char flags;			/* Bit 0: 0: true color, 1: palettized
					   Bit 1: 0: no alpha, 1: alpha */

	/* Function to convert RGBA to COLOR32 */
	COLOR32 (*rgba2col)(const wininfo_t *pwi, RGBA rgba);

	/* Function to convert COLOR32 to RGBA */
	RGBA (*col2rgba)(const wininfo_t *pwi, COLOR32 color);

	/* Function to apply Alpha and pre-multiplied pixel to COLOR32 value */
	COLOR32 (*apply_alpha)(const wininfo_t *pwi, const colinfo_t *pci,
			       COLOR32 oldcol);

	char *name;			/* Format description */

} pixinfo_t;

/* Console information */
typedef struct coninfo {
	u_short x;			  /* Current writing position */
	u_short y;			  /* (aligned to characters) */
	COLOR32 fg;			  /* Foreground and background color */
	COLOR32 bg;
} coninfo_t;


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
	u_char  drive;	        /* Drive strength: 0=2mA, 1=4mA, 2=7mA, 3=9mA */
	u_char  reserved;	/* (needed for alignment anyway) */

	/* Driver independent LCD panel info */
	lcdinfo_t lcd;			  /* Info about lcd panel */

	/* Function to get a pointer to the info for pixel format pix */
	const pixinfo_t *(*get_pixinfo_p)(WINDOW win, PIX pix);

	/* Return maximum horizontal framebuffer resolution for this window */
	HVRES (*get_fbmaxhres)(WINDOW win, PIX pix);

	/* Return maximum vertical framebuffer resolution for this window */
	HVRES (*get_fbmaxvres)(WINDOW win, PIX pix);

	/* Align horizontal resolution to next word boundary (round up) */
	HVRES (*align_hres)(WINDOW win, PIX pix, HVRES hres);

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
	HVRES hres;			  /* Size of visible window */
	HVRES vres;
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
	HVRES fbhres;			  /* Virtual size of framebuffer */
	HVRES fbvres;
	XYPOS hoffs;			  /* Offset within framebuffer (>=0) */
	XYPOS voffs;

	/* Color information */
	u_int attr;			  /* Current attribute */
	colinfo_t fg;			  /* Foreground color info */
	colinfo_t bg;			  /* Foreground color info */
	RGBA alpha0;			  /* Alpha value for A=0 */
	RGBA alpha1;			  /* Alpha value for A=1 */
	u_char alphamode;		  /* 0: alpha0, 1: alpha1, 2: pixel */
	u_char ckmode;			  /* Color keying mode */
	RGBA ckvalue;			  /* Color keying RGBA value */
	RGBA ckmask;			  /* Color keying mask */
	RGBA replace;			  /* Replacement color for window */
	RGBA *cmap;			  /* If CLUT: Pointer to color map */

	/* Function to set the color map from index to end; this also updates
	   pwi->cmap. Some hardware has restrictions of how and when setting
	   new palette entries is possible (e.g. only between frames), so
	   please always update the color map in as large quantities as
	   possible. */
	void (*set_cmap)(const wininfo_t *pwi, u_int index, u_int end,
			 RGBA *prgba);

#ifdef CONFIG_MULTIPLE_CONSOLES
	coninfo_t ci;			  /* Console info for this window */
#endif

	/* Display information */
	vidinfo_t *pvi;			  /* Pointer to corresponding display */
};


/* Info about minimal and maximal argument count, sub-command index and
   keyword string for commands using sub-commands (e.g. lcd, window, draw) */
typedef struct kwinfo
{
	u_char  argc_min;
	u_char  argc_max;
	u_char  info1;
	u_char  info2;
	char    *keyword;
} kwinfo_t;


/* Framebuffer pool information */
typedef struct FBPOOL_INFO {
	u_long base;			  /* Base address of framebuffer pool */
	u_long size;			  /* Size of framebuffer pool */
	u_long used;			  /* Current usage */
} fbpoolinfo_t;

#ifdef CONFIG_WINDOW_EXT
/* Table with additional keywords for window */
extern const struct kwinfo winextkeywords[CONFIG_WINDOW_EXT];
#endif /*CONFIG_WININFO_EXT*/


/************************************************************************/
/* PROTOTYPES OF EXPORTED FUNCTIONS					*/
/************************************************************************/

/* Parse #rrggbb or #rrggbbaa value; return 1 on error, 0 on success */
extern int parse_rgb(char *s, u_int *prgba);

/* Parse sub-command and return sub-command index */
extern u_short parse_sc(int argc, char *s, u_short sc,
			const kwinfo_t *pki, u_short count);

/* Find next delay entry in power-on/power-off sequence */
extern int find_delay_index(const u_short *delays, int index, u_short value);

/* Get pointer to the framebuffer pool info */
extern const fbpoolinfo_t *lcd_get_fbpoolinfo_p(void);

/* Initialize panel and window information */
extern void drv_lcd_init(void);

#endif /*!_CMD_LCD_H_*/
