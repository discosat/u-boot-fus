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
/* PROTOTYPES OF EXPORTED FUNCTIONS					*/
/************************************************************************/

/* Console functions */
#ifndef CONFIG_MULTIPLE_CONSOLES
extern void console_init(wininfo_t *pwi, RGBA fg, RGBA bg);
extern void console_update(wininfo_t *pwi, RGBA fg, RGBA bg);
#endif

extern void console_cls(const wininfo_t *pwi, COLOR32 col);

extern void lcd_putc(const device_t *pdev, const char c);
extern void lcd_puts(const device_t *pdev, const char *s);

/* Set colinfo structure */
extern void lcd_set_col(wininfo_t *pwi, RGBA rgba, colinfo_t *pci);

/* Fill display with given color */
extern void lcd_fill(const wininfo_t *pwi, const colinfo_t *pci);

/* Draw pixel at (x, y) with given color */
extern void lcd_pixel(const wininfo_t *pwi, XYPOS x, XYPOS y,
		      const colinfo_t *pci);

/* Draw line from (x1, y1) to (x2, y2) in given color */
extern void lcd_line(const wininfo_t *pwi, XYPOS x1, XYPOS y1,
		     XYPOS x2, XYPOS y2, const colinfo_t *pci);

/* Draw rectangular frame from (x1, y1) to (x2, y2) in given color */
extern void lcd_frame(const wininfo_t *pwi, XYPOS x1, XYPOS y1,
		      XYPOS x2, XYPOS y2, const colinfo_t *pci);

/* Draw filled rectangle from (x1, y1) to (x2, y2) in given color */
extern void lcd_rect(const wininfo_t *pwi, XYPOS x1, XYPOS y1,
		     XYPOS x2, XYPOS y2, const colinfo_t *pci);

/* Draw rounded frame from (x1, y1) to (x2, y2) with radius r and given color */
extern void lcd_rframe(const wininfo_t *pwi, XYPOS x1, XYPOS y1,
		       XYPOS x2, XYPOS y2, XYPOS r, const colinfo_t *pci);

/* Draw filled rounded rectangle from (x1, y1) to (x2, y2) with radius r */
extern void lcd_rrect(const wininfo_t *pwi, XYPOS x1, XYPOS y1,
		      XYPOS x2, XYPOS y2, XYPOS r, const colinfo_t *pci);

/* Draw circle outline at (x, y) with radius r and given color */
extern void lcd_circle(const wininfo_t *pwi, XYPOS x, XYPOS y, XYPOS r,
		       const colinfo_t *pci);

/* Draw filled circle at (x, y) with radius r and given color */
extern void lcd_disc(const wininfo_t *pwi, XYPOS x, XYPOS y, XYPOS r,
		     const colinfo_t *pci);

/* Draw text string s at (x, y) with given colors and alignment/attribute */
extern void lcd_text(const wininfo_t *pwi, XYPOS x, XYPOS y, char *s,
		     const colinfo_t *pci_fg, const colinfo_t *pci_bg);

/* Draw test pattern */
extern int lcd_test(const wininfo_t *pwi, u_int pattern);

/* Lookup nearest possible color in given color map */
extern COLOR32 lcd_rgbalookup(RGBA rgba, RGBA *cmap, unsigned count);

#endif	/* _LCD_H_ */
