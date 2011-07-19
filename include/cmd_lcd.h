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
/* ENUMERATIONS								*/
/************************************************************************/

enum RGB_TYPE {
	RT_NONE,
	RT_RGB,
	RT_RGBA
};

/************************************************************************/
/* TYPES AND STRUCTURES							*/
/************************************************************************/

/* Forward declaration */
typedef struct wininfo wininfo_t;
typedef struct vidinfo vidinfo_t;

/* Pixel format information */
typedef struct PIXEL_INFO {
	u_char depth;			/* Actually used bits for the color */
	u_char bpp_shift;		/* Bits per pixel as power of 2;
					   0: 1 bpp, 1: 2 bpp, .. 5: 32bpp */
	u_short clutsize;		/* Number of CLUT entries
					   (0=non-palettized) */

	/* Function to convert RGBA to COLOR32 */
	COLOR32 (*rgba2col)(const wininfo_t *pwi, RGBA rgba);

	/* Function to convert COLOR32 to RGBA */
	RGBA (*col2rgba)(const wininfo_t *pwi, COLOR32 color);

	char *name;			/* Format description */

} pixinfo_t;

/* Console information */
typedef struct CON_INFO {
	u_short x;			  /* Current writing position */
	u_short y;			  /* (aligned to characters) */
	COLOR32 fg;			  /* Foreground and background color */
	COLOR32 bg;
} coninfo_t;


struct vidinfo 
{
	/* Driver specific display info */
	char *driver_name;		  /* Name of display driver */
	VID vid;			  /* Current display number */
	u_char is_enabled;		  /* Flag if LCD is switched on */
	WINDOW wincount;		  /* Number of available windows */
	WINDOW win_sel;			  /* Currently selected window */
	PIX pixcount;			  /* Number of av. pixel formats */
	wininfo_t *pwi;			  /* Pointer to info about windows */

	/* Driver independent LCD panel info */
	lcdinfo_t lcd;			  /* Info about lcd panel */

	/* Function to parse additional window sub-commands (optional) */
	u_short (*winext_parse)(int argc, char *s);

	/* Function to execute additional window sub-commands (optional) */
	int (*winext_exec)(wininfo_t *pwi, int argc, char *argv[], u_short sc);

	/* Function to print info for additional window sub-commands (opt.) */
	void (*winext_show)(const wininfo_t *pwi);

	/* Function to print help for additional window sub-commands (opt.) */
	void (*winext_help)(void);

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
	int (*enable)(void);

	/* Function to switch LCD off */
	void (*disable)(void);
};


/* Common window information */
struct wininfo
{
	/* Window information */
	WINDOW win;			  /* Number of this window */
	u_char active;			  /* Flag if window is active */
	PIX defpix;			  /* Default pixel format */
	PIX pix;			  /* Current pixel format */
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
	COLOR32 fg;			  /* Current foreground color */
	COLOR32 bg;			  /* Current background color */
	RGBA *cmap;			  /* If CLUT: Pointer to color map */

#ifdef CONFIG_MULTIPLE_CONSOLES
	coninfo_t ci;			  /* Console info for this window */
#endif

	/* Display information */
	vidinfo_t *pvi;			  /* Pointer to corresponding display */
	
	/* Additional hardware specific data */
	void *ext;			  /* Pointer to extended info */
};


/* Info about minimal and maximal argument count, sub-command index and
   keyword string for commands using sub-commands (e.g. lcd, window, draw) */
typedef struct kwinfo
{
	u_char  argc_min;
	u_char  argc_max;
	u_short sc;
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

/* Return RT_RGB for #rrggbb, RT_RGBA for #rrggbbaa and RT_NONE otherwise */
extern enum RGB_TYPE parse_rgb_type(char *s);

/* Parse #rrggbb or #rrggbbaa value; return value and type */
extern enum RGB_TYPE parse_rgb(char *s, u_int *prgba);

/* Parse sub-command and return sub-command index */
extern u_short parse_sc(int argc, char *s, u_short sc,
			const kwinfo_t *pki, u_short count);

/* Get pointer to the framebuffer pool info */
extern const fbpoolinfo_t *lcd_get_fbpoolinfo_p(void);

/* Initialize panel and window information */
extern void drv_lcd_init(void);

#endif /*!_CMD_LCD_H_*/
