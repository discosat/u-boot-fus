/*
 * Hardware independent LCD controller part
 *
 * (C) Copyright 2011
 * Hartmut Keller, F&S Elektronik Systeme GmbH, keller@fs-net.de
 *
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

#ifndef _LCD_H_
#define _LCD_H_

/* Define some types to be used with displays */
typedef unsigned int COLOR32;
typedef unsigned int RGBA;
typedef unsigned int WINDOW;
typedef short XYPOS;

/************************************************************************/
/* HEADER FILES								*/
/************************************************************************/
#include "cmd_lcd.h"			  /* struct kwinfo */

#if defined(CONFIG_CMD_BMP) || defined(CONFIG_SPLASH_SCREEN)
# include <bmp_layout.h>
# include <asm/byteorder.h>
#endif

/* Architecture specific includes */
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
/* DEFINITIONS								*/
/************************************************************************/

#ifndef PAGE_SIZE
# define PAGE_SIZE	4096
#endif

#define ARRAYSIZE(array) (sizeof(array)/sizeof(array[0]))

/* Maximum length of a panel name */
#define MAX_NAME 32

/* Values for type entry of vidinfo_t; if TFT is set, all other bits must be
   zero. Therefore the range of this value is 0..8 */
#define VI_TYPE_SINGLESCAN 0x00		  /* Bit 0: single or dual scan */
#define VI_TYPE_DUALSCAN   0x01
#define VI_TYPE_4BITBUS    0x00		  /* Bit 1: 4-bit or 8-bit buswidth */
#define VI_TYPE_8BITBUS    0x02
#define VI_TYPE_STN        0x00		  /* Bit 2: STN or CSTN */
#define VI_TYPE_CSTN       0x04
#define VI_TYPE_STN_CSTN   0x00		  /* Bit 3: (C)STN or TFT */
#define VI_TYPE_TFT        0x08

/*
 *  Information about displays we are using. This is for configuring
 *  the LCD controller and memory allocation. Someone has to know what
 *  is connected, as we can't autodetect anything.
 */
#define CFG_HIGH	0	/* Pins are active high			*/
#define CFG_LOW		1	/* Pins are active low			*/

#define LCD_MONOCHROME	0
#define LCD_COLOR2	1
#define LCD_COLOR4	2
#define LCD_COLOR8	3
#define LCD_COLOR16	4	/* e.g. R5G5B5A1 or R5G6B5 */
#define LCD_COLOR32	5	/* unpacked formats with >16 bpp, e.g.
				   R6G6B6A1, R8G8B8, R8G8B8A4, etc. */
#define LCD_COLOR_MANY  -1      /* Support different pixel formats */

/*----------------------------------------------------------------------*/
#if defined(CONFIG_LCD_INFO_BELOW_LOGO)
# define LCD_INFO_X		0
# define LCD_INFO_Y		(BMP_LOGO_HEIGHT + VIDEO_FONT_HEIGHT)
#elif defined(CONFIG_LCD_LOGO)
# define LCD_INFO_X		(BMP_LOGO_WIDTH + 4 * VIDEO_FONT_WIDTH)
# define LCD_INFO_Y		(VIDEO_FONT_HEIGHT)
#else
# define LCD_INFO_X		(VIDEO_FONT_WIDTH)
# define LCD_INFO_Y		(VIDEO_FONT_HEIGHT)
#endif

/* Default to 8bpp if bit depth not specified */
#ifndef LCD_BPP
# define LCD_BPP			LCD_COLOR8
#endif
#ifndef LCD_DF
# define LCD_DF			1
#endif

/* Calculate nr. of bits per pixel  and nr. of colors */
#define NBITS(bit_code)		(1 << (bit_code))
#define NCOLORS(bit_code)	(1 << NBITS(bit_code))

/************************************************************************/
/* CONSOLE CONSTANTS							*/
/************************************************************************/
#if LCD_BPP == LCD_MONOCHROME

/*
 * Simple black/white definitions
 */
# define CONSOLE_COLOR_BLACK	0
# define CONSOLE_COLOR_WHITE	1	/* Must remain last / highest	*/

#elif LCD_BPP == LCD_COLOR8

/*
 * 8bpp color definitions
 */
# define CONSOLE_COLOR_BLACK	0
# define CONSOLE_COLOR_RED	1
# define CONSOLE_COLOR_GREEN	2
# define CONSOLE_COLOR_YELLOW	3
# define CONSOLE_COLOR_BLUE	4
# define CONSOLE_COLOR_MAGENTA	5
# define CONSOLE_COLOR_CYAN	6
# define CONSOLE_COLOR_GREY	14
# define CONSOLE_COLOR_WHITE	15	/* Must remain last / highest	*/

#else

/*
 * 16bpp color definitions
 */
# define CONSOLE_COLOR_BLACK	0x0000
# define CONSOLE_COLOR_WHITE	0xffff	/* Must remain last / highest	*/

#endif /* color definitions */

/************************************************************************/
/* CONSOLE DEFINITIONS & FUNCTIONS					*/
/************************************************************************/
#if 0 //#####

#if defined(CONFIG_LCD_LOGO) && !defined(CONFIG_LCD_INFO_BELOW_LOGO)
# define CONSOLE_ROWS		((panel_info.vl_row-BMP_LOGO_HEIGHT) \
					/ VIDEO_FONT_HEIGHT)
#else
# define CONSOLE_ROWS		(panel_info.vl_row / VIDEO_FONT_HEIGHT)
#endif

#define CONSOLE_COLS		(panel_info.vl_col / VIDEO_FONT_WIDTH)
#define CONSOLE_ROW_SIZE	(VIDEO_FONT_HEIGHT * lcd_line_length)
#define CONSOLE_ROW_FIRST	(lcd_console_address)
#define CONSOLE_ROW_SECOND	(lcd_console_address + CONSOLE_ROW_SIZE)
#define CONSOLE_ROW_LAST	(lcd_console_address + CONSOLE_SIZE \
					- CONSOLE_ROW_SIZE)
#define CONSOLE_SIZE		(CONSOLE_ROW_SIZE * CONSOLE_ROWS)
#define CONSOLE_SCROLL_SIZE	(CONSOLE_SIZE - CONSOLE_ROW_SIZE)

#if LCD_BPP == LCD_MONOCHROME
# define COLOR_MASK(c)		((c)	  | (c) << 1 | (c) << 2 | (c) << 3 | \
				 (c) << 4 | (c) << 5 | (c) << 6 | (c) << 7))
#else
# define COLOR_MASK(c)		(c)
#endif

#endif //0####

/************************************************************************/
/* ENUMERATIONS								*/
/************************************************************************/

/************************************************************************/
/* TYPES AND STRUCTURES							*/
/************************************************************************/

typedef struct CON_INFO {
	WINDOW  win;			  /* Window where console is on */
	u_short x;			  /* Current writing position */
	u_short y;			  /* (aligned to characters) */
	COLOR32 fg;			  /* Foreground and background color */
	COLOR32 bg;
} coninfo_t;


/* Pixel format information */
typedef struct PIXEL_INFO {
	u_int  hwmode;			/* Mode required for hardware */
	u_char depth;			/* Actually used bits for the color */
	u_char bpp_shift;		/* Bits per pixel as power of 2;
					   0: 1 bpp, 1: 2 bpp, .. 5: 32bpp */
	u_short clutsize;		/* Number of CLUT entries
					   (0=non-palettized) */
	char *name;			/* Format description */
} pixinfo_t;


/* Framebuffer pool information */
typedef struct FBPOOL_INFO {
	u_long base;			  /* Base address of framebuffer pool */
	u_long size;			  /* Size of framebuffer pool */
	u_long used;			  /* Current usage */
} fbpoolinfo_t;

/*
 * Common LCD panel information
 */
typedef struct vidinfo {
	/* Horizontal control */
	u_short	hfp;		/* Front porch (between data and HSYNC) */
	u_short hsw;		/* Horizontal sync pulse (HSYNC) width */
	u_short hbp;		/* Back porch (between HSYNC and data) */
	u_short	hres;		/* Horizontal pixel resolution (i.e. 640) */

	/* Vertical control */
	u_short vfp;		/* Front porch (between data and VSYNC) */
	u_short	vsw;		/* Vertical sync pulse (VSYNC) width */
	u_short vbp;		/* Back porch (between VSYNC and data) */
	u_short	vres;		/* Vertical pixel resolution (i.e. 480) */

	/* Signal polarity */
	u_char  hspol;	        /* HSYNC polarity (0=normal, 1=inverted) */
	u_char  vspol;		/* VSYNC polarity (0=normal, 1=inverted) */
	u_char  denpol;		/* DEN polarity (0=normal, 1=inverted) */
	u_char  clkpol;		/* Clock polarity (0=normal, 1=inverted) */

	/* Timings */
	u_int   fps;		/* Frame rate (in frames per second) */
	u_int   clk;		/* Pixel clock (in Hz) */

	/* Backlight settings */
	u_int   pwmvalue;	/* PWM value (voltage) */
	u_int   pwmfreq;	/* PWM frequency */
	u_char  pwmenable;	/* 0: disabled, 1: enabled */

	/* Display type */
	u_char  type;		/* Bit 0: 0: 4-bit bus, 1: 8-bit bus
				   Bit 1: 0: single-scan, 1: dual-scan
				   Bit 2: 0: STN, 1: CSTN
				   Bit 3: 0: (C)STN, 1: TFT */

	/* Additional settings */
	u_char  strength;	/* Drive strength: 0=2mA, 1=4mA, 2=7mA, 3=9mA */
	u_char	dither;		/* Dither mode (FRC) #### */

	/* General info */
	u_short	hdim;		/* Width of display area in millimeters */
	u_short	vdim;		/* Height of display area in millimeters */
	char    name[MAX_NAME];	/* Manufacturer, display and resolution */
} vidinfo_t;


typedef struct wininfo
{
	u_long fbuf[MAX_BUFFERS_PER_WIN]; /* Pointers to buffers */
	u_long fbsize;			  /* Size of one buffer (bytes) */
	u_long linelen;			  /* Bytes per fbhres line */
	u_char fbcount;			  /* Number of active buffers */
	u_char fbmaxcount;		  /* Maximum active buffer count */
	u_char fbdraw;			  /* Index of buffer to draw to */
	u_char fbshow;			  /* Index of buffer to show */
	u_char pix;			  /* Current pixel format */
	const pixinfo_t *pi;		  /* Pointer to pixel format info */
	u_short fbhres;			  /* Virtual size of framebuffer */
	u_short fbvres;
	u_short hoffs;			  /* Offset within framebuffer */
	u_short voffs;
	u_short hres;			  /* Size of visible window */
	u_short vres;
	XYPOS hpos;			  /* Position of window on display */
	XYPOS vpos;
	COLOR32 fg;			  /* Foreground color */
	COLOR32 bg;			  /* Background color */
	RGBA *cmap;			  /* If CLUT: Pointer to color map */

#ifdef CONFIG_LCDWIN_EXT
	struct wininfo_ext ext;		  /* Hardware specific data */
#endif
} wininfo_t;


/************************************************************************/
/* EXPORTED VARIABLES							*/
/************************************************************************/

extern vidinfo_t panel_info;
extern char lcd_is_enabled;


/************************************************************************/
/* PROTOTYPES OF EXPORTED FUNCTIONS IMPLEMENTED BY GENERIC PART lcd.c   */
/************************************************************************/

/* Video functions */

#if defined(CONFIG_RBC823)
void	lcd_disable	(void);
#endif

/* int	lcd_init(void *lcdbase); */
extern void lcd_putc(const char c);
extern void lcd_puts(const char *s);
extern void lcd_printf(const char *fmt, ...);

/* Enable the display, return 0 on success */
extern int lcd_enable(void);

/* Disable the display */
extern void lcd_disable(void);

/* Find predefined panel by index, returns 0 on success and 1 on bad index */
extern int lcd_getpanel(vidinfo_t *pvi, u_int index);

/* Search panel by string, start at index; return index (or 0 if no match) */ 
extern u_int lcd_searchpanel(char *s, u_int index);

/* Get a copy of the panel info */
extern void lcd_getvidinfo(vidinfo_t *vi);

/* Set/update panel info */
extern void lcd_setvidinfo(vidinfo_t *vi);

/* Get window information */
extern void lcd_getwininfo(wininfo_t *wi, WINDOW win);

/* Set new/updated window information */
extern void lcd_setwininfo(wininfo_t *wi, WINDOW win);

/* Get pointer to current window information */
extern const wininfo_t *lcd_getwininfop(WINDOW win);

/* Relocate all windows to newaddr, starting at window win */
extern void lcd_relocbuffers(u_long newaddr, WINDOW win);

/* Resize framebuffer for current window, relocate all subsequent windows */
extern int lcd_setfbuf(wininfo_t *pwi, WINDOW win, u_short fbhres,
		       u_short fbvres, u_char pix, u_char fbcount);

/* Get current framebuffer pool information */
extern void lcd_getfbpool(fbpoolinfo_t *pfp);

/* Set new framebuffer pool information */
extern void lcd_setfbpool(fbpoolinfo_t *pfp);


/* Draw pixel at (x, y) with color */
extern void lcd_pixel(const wininfo_t *pwi, XYPOS x, XYPOS y, COLOR32 color);

/* Draw line from (x1, y1) to (x2, y2) in color */
extern void lcd_line(const wininfo_t *pwi, XYPOS x1, XYPOS y1,
		     XYPOS x2, XYPOS y2, COLOR32 color);

/* Draw rectangular frame from (x1, y1) to (x2, y2) in color */
extern void lcd_frame(const wininfo_t *pwi, XYPOS x1, XYPOS y1,
		      XYPOS x2, XYPOS y2, COLOR32 color);

/* Draw filled rectangle from (x1, y1) to (x2, y2) in color */
extern void lcd_rect(const wininfo_t *pwi, XYPOS x1, XYPOS y1,
		     XYPOS x2, XYPOS y2, COLOR32 color);

/* Draw circle outline at (x, y) with radius r and color */
extern void lcd_circle(const wininfo_t *pwi, XYPOS x, XYPOS y, XYPOS r,
		       COLOR32 color);

/* Draw filled circle at (x, y) with radius r and color */
extern void lcd_disc(const wininfo_t *pwi, XYPOS x, XYPOS y, XYPOS r,
		     COLOR32 color);

/* Draw text string s at (x, y) with alignment/attribute a and colors fg/bg */
extern void lcd_text(const wininfo_t *pwi, XYPOS x, XYPOS y, char *s, u_int a,
		     COLOR32 fg, COLOR32 bg);

/* Draw bitmap from address addr at (x, y) with alignment/attribute a */
extern void lcd_bitmap(const wininfo_t *pwi, XYPOS x, XYPOS y, u_long addr,
		       u_int a);

/* Lookup nearest possible color in given color map */
extern COLOR32 lcd_rgbalookup(RGBA rgba, RGBA *cmap, unsigned count);


#ifdef CONFIG_LCDWIN_EXT
/* Parse and execute additional variants of command lcdwin; return WI_UNKNOWN
   on error or index > WI_UNKNOWN. */
extern int lcdwin_ext_exec(wininfo_t *pwi, int argc, char *argv[], u_int si);

/* Print info for those additional variants, index is the result from above */
extern void lcdwin_ext_print(wininfo_t *pwi, u_int si);

/* Table with additional keywords for lcdwin */
extern const struct kwinfo winextkeywords[CONFIG_LCDWIN_EXT];

#endif /*CONFIG_WININFO_EXT*/


/************************************************************************/
/* EXPORTED FUNCTIONS IMPLEMENTED BY CONTROLLER SPECIFIC PART		*/
/************************************************************************/

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

/* Set new vidinfo to hardware */
extern void lcd_hw_vidinfo(vidinfo_t *pvi_new, vidinfo_t *pvi_old);

/* Set new wininfo to hardware */
extern void lcd_hw_wininfo(wininfo_t *pwi_new, WINDOW win, wininfo_t *pwi_old);


#endif	/* _LCD_H_ */
