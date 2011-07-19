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
typedef unsigned short WINDOW;
typedef short XYPOS;

#include <lcd_panels.h>			  /* vidinfo_t */

#if defined(CONFIG_CMD_BMP) || defined(CONFIG_SPLASH_SCREEN)
# include <bmp_layout.h>
# include <asm/byteorder.h>
#endif

/* The following LCD controller hardware specific includes provide extensions
   like DEFAULT_PIXEL_FORMAT, CONFIG_MAX_WINDOWS, CONFIG_MAX_BUFFERS_PER_WIN,
   CONFIG_LCDWIN_EXT, wininfo_ext_t etc. */
#if defined CONFIG_PXA250 || defined CONFIG_PXA27X || defined CONFIG_CPU_MONAHANS
#include <asm/arch-pxa/pxafb.h>
#endif

#if defined(CONFIG_MPC823)
#include <lcdvideo.h>
#endif

#if defined(CONFIG_ATMEL_LCD)
#include <atmel_lcdc.h>
#include <nand.h>
#endif

#if defined(CONFIG_S3C64XX)
#include <lcd_s3c64xx.h>
#endif


/************************************************************************/
/* ENUMERATIONS								*/
/************************************************************************/

enum RGB_TYPE {
	RT_NONE,
	RT_RGB,
	RT_RGBA
};

/* Settings that correspond with lcdwin */
typedef enum WIN_INDEX {
	WI_INFO,
	WI_SELECT,
	WI_IMAGE,
	WI_FBUF,
	WI_RES,
	WI_OFFSET,
	WI_POS,
	WI_COLOR,
	
	/* Unknown keyword (must be the last entry!) */
	WI_UNKNOWN,
} winindex_t;


/************************************************************************/
/* TYPES AND STRUCTURES							*/
/************************************************************************/

/* Pixel format information */
typedef struct PIXEL_INFO {
	u_char depth;			/* Actually used bits for the color */
	u_char bpp_shift;		/* Bits per pixel as power of 2;
					   0: 1 bpp, 1: 2 bpp, .. 5: 32bpp */
	u_short clutsize;		/* Number of CLUT entries
					   (0=non-palettized) */
	char *name;			/* Format description */
} pixinfo_t;



/* Common window information */
typedef struct wininfo
{
	/* Window information */
	WINDOW win;			  /* Number of this window */
	u_char active;			  /* Flag if window is active */
	u_char pix;			  /* Current pixel format */
	const pixinfo_t *pi;		  /* Pointer to pixel format info */
	u_short hres;			  /* Size of visible window */
	u_short vres;
	XYPOS hpos;			  /* Position of window on display */
	XYPOS vpos;

	/* Framebuffer information */
	u_long fbuf[CONFIG_MAX_BUFFERS_PER_WIN]; /* Pointers to buffers */
	u_long fbsize;			  /* Size of one buffer (bytes) */
	u_long linelen;			  /* Bytes per fbhres line */
	u_char fbcount;			  /* Number of active buffers */
	u_char fbmaxcount;		  /* Maximum active buffer count */
	u_char fbdraw;			  /* Index of buffer to draw to */
	u_char fbshow;			  /* Index of buffer to show */
	u_short fbhres;			  /* Virtual size of framebuffer */
	u_short fbvres;
	XYPOS hoffs;			  /* Offset within framebuffer (>=0) */
	XYPOS voffs;

	/* Color information */
	COLOR32 fg;			  /* Current foreground color */
	COLOR32 bg;			  /* Current background color */
	RGBA *cmap;			  /* If CLUT: Pointer to color map */

#ifdef CONFIG_LCDWIN_EXT
	wininfo_ext_t ext;		  /* Hardware specific data */
#endif
} wininfo_t;


/* Info about max. argument count, command index and keyword string of
   commands lcdset and lcdwin */
typedef struct kwinfo
{
	u_char  argc_min;
	u_char  argc_max;
	u_short si;
	char    *keyword;
} kwinfo_t;


/* Framebuffer pool information */
typedef struct FBPOOL_INFO {
	u_long base;			  /* Base address of framebuffer pool */
	u_long size;			  /* Size of framebuffer pool */
	u_long used;			  /* Current usage */
} fbpoolinfo_t;

#ifdef CONFIG_LCDWIN_EXT
/* Table with additional keywords for lcdwin */
extern const struct kwinfo winextkeywords[CONFIG_LCDWIN_EXT];
#endif /*CONFIG_WININFO_EXT*/


/************************************************************************/
/* PROTOTYPES OF EXPORTED FUNCTIONS IMPLEMENTED BY GENERIC cmd_lcd.c	*/
/************************************************************************/

/* Return RT_RGB for #rrggbb, RT_RGBA for #rrggbbaa and RT_NONE otherwise */
extern enum RGB_TYPE parse_rgb_type(char *s);

/* Parse #rrggbb or #rrggbbaa value; return value and type */
extern enum RGB_TYPE parse_rgb(char *s, u_int *prgba);

extern const fbpoolinfo_t *lcd_getfbpoolinfo(void);

/* Initialize panel and window information */
extern void cmd_lcd_init(void);


/************************************************************************/
/* EXPORTED FUNCTIONS IMPLEMENTED BY CONTROLLER SPECIFIC PART		*/
/************************************************************************/

#ifdef CONFIG_LCDWIN_EXT
/* Parse and execute additional variants of command lcdwin; return WI_UNKNOWN
   on error or index > WI_UNKNOWN. */
extern int lcdwin_ext_exec(wininfo_t *pwi, int argc, char *argv[], u_int si);

/* Print info for those additional variants, index is the result from above */
extern void lcdwin_ext_print(wininfo_t *pwi, u_int si);
#endif /*CONFIG_WININFO_EXT*/

/* Get a COLOR32 value from the given RGBA value */
extern COLOR32 lcd_rgba2col(const wininfo_t *pwi, RGBA rgba);

/* Get an RGBA value from a COLOR32 value */
extern RGBA lcd_col2rgba(const wininfo_t *pwi, COLOR32 color);

/* Return pointer to pixel info (NULL if pix not valid for this window) */
extern const pixinfo_t *lcd_getpixinfo(WINDOW win, u_char pix);

/* Returns the next valid pixel format >= pix for this window */
extern u_char lcd_getnextpix(WINDOW win, u_char pix);

/* Return number of image buffers for this window */
extern u_char lcd_getfbmaxcount(WINDOW win);

/* Return maximum horizontal framebuffer resolution for this window */
extern u_short lcd_getfbmaxhres(WINDOW win, u_char pix);

/* Return maximum vertical framebuffer resolution for this window */
extern u_short lcd_getfbmaxvres(WINDOW win, u_char pix);

/* Align horizontal offset to current word boundary (round down) */
extern XYPOS lcd_align_hoffs(const wininfo_t *pwi, XYPOS hpos);

/* Align horizontal resolution to next word boundary (round up) */
extern u_short lcd_align_hres(WINDOW win, u_char pix, u_short hres);

/* Enable the display, return 0 on success, 1 on failure */
extern int lcd_hw_enable(void);

/* Disable the display */
extern void lcd_hw_disable(void);

/* Set new vidinfo to hardware; update info to finally used values */
extern void lcd_hw_vidinfo(vidinfo_t *pvi);

/* Set new wininfo to hardware; info is not changed */
extern int lcd_hw_wininfo(const wininfo_t *pwi, const vidinfo_t *pvi);

/* Initialize LCD controller (GPIOs, clock, etc.) */
extern void lcd_hw_init(void);

#endif /*!_CMD_LCD_H_*/
