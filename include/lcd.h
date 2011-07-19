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


/************************************************************************/
/* HEADER FILES								*/
/************************************************************************/

#include "cmd_lcd.h"			  /* wininfo_t, XYPOS, WINDOW, ... */


/************************************************************************/
/* DEFINITIONS								*/
/************************************************************************/

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

/************************************************************************/
/* EXPORTED VARIABLES							*/
/************************************************************************/

extern char lcd_is_enabled;


/************************************************************************/
/* PROTOTYPES OF EXPORTED FUNCTIONS 					*/
/************************************************************************/

/* Video functions */

#if defined(CONFIG_RBC823)
void	lcd_disable	(void);
#endif

/* int	lcd_init(void *lcdbase); */
extern void console_init(wininfo_t *pwi);
extern void console_update(wininfo_t *pwi);
extern void lcd_putc(const char c);
extern void lcd_puts(const char *s);
extern void lcd_printf(const char *fmt, ...);

/* Relocate all windows to newaddr, starting at window win */
extern void lcd_relocbuffers(u_long newaddr, WINDOW win);

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


#endif	/* _LCD_H_ */
