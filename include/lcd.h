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
#include "devices.h"			  /* device_t */

/************************************************************************/
/* DEFINITIONS								*/
/************************************************************************/


/************************************************************************/
/* PROTOTYPES OF EXPORTED FUNCTIONS 					*/
/************************************************************************/

/* Console functions */
#ifndef CONFIG_MULTIPLE_CONSOLES
extern void console_init(wininfo_t *pwi, RGBA fg, RGBA bg);
extern void console_update(wininfo_t *pwi, RGBA fg, RGBA bg);
#endif

extern void lcd_putc(const device_t *pdev, const char c);
extern void lcd_puts(const device_t *pdev, const char *s);

/* Set the FG color */
extern void lcd_set_fg(wininfo_t *pwi, RGBA rgba);

/* Set the BG color */
extern void lcd_set_bg(wininfo_t *pwi, RGBA rgba);

/* Fill display with FG color */
extern void lcd_fill(const wininfo_t *pwi);

/* Fill display with BG color */
extern void lcd_clear(const wininfo_t *pwi);

/* Draw pixel at (x, y) with FG color */
extern void lcd_pixel(const wininfo_t *pwi, XYPOS x, XYPOS y);

/* Draw line from (x1, y1) to (x2, y2) in FG color */
extern void lcd_line(const wininfo_t *pwi, XYPOS x1, XYPOS y1,
		     XYPOS x2, XYPOS y2);

/* Draw rectangular frame from (x1, y1) to (x2, y2) in FG color */
extern void lcd_frame(const wininfo_t *pwi, XYPOS x1, XYPOS y1,
		      XYPOS x2, XYPOS y2);

/* Draw filled rectangle from (x1, y1) to (x2, y2) in FG color */
extern void lcd_rect(const wininfo_t *pwi, XYPOS x1, XYPOS y1,
		     XYPOS x2, XYPOS y2);

/* Draw circle outline at (x, y) with radius r and FG color */
extern void lcd_circle(const wininfo_t *pwi, XYPOS x, XYPOS y, XYPOS r);

/* Draw filled circle at (x, y) with radius r and FG color */
extern void lcd_disc(const wininfo_t *pwi, XYPOS x, XYPOS y, XYPOS r);

/* Draw text string s at (x, y) with FG/BG color and alignment/attribute */
extern void lcd_text(const wininfo_t *pwi, XYPOS x, XYPOS y, char *s);

/* Draw test pattern */
extern void lcd_test(const wininfo_t *pwi, u_int pattern);

/* Lookup nearest possible color in given color map */
extern COLOR32 lcd_rgbalookup(RGBA rgba, RGBA *cmap, unsigned count);

#endif	/* _LCD_H_ */
