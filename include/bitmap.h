/*
 * Support for bitmaps (PNG, BMP, JPG)
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

#ifndef _BITMAP_H_
#define _BITMAP_H_

#include "cmd_lcd.h"			  /* wininfo_t, XYPOS, WINDOW, ... */


/************************************************************************/
/* PROTOTYPES OF EXPORTED FUNCTIONS 					*/
/************************************************************************/

/* Draw bitmap from address addr at (x, y) with alignment/attribute */
extern const char *lcd_bitmap(const wininfo_t *pwi, XYPOS x, XYPOS y,
			      u_long addr);

/* Scan the bitmap at addr and return end address (0 on error) */
extern u_long lcd_scan_bitmap(u_long addr);

#endif	/* _BITMAP_H_ */
