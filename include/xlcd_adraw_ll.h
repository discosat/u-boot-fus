/*
 * Hardware independent lowlevel drawing routines (applying alpha)
 *
 * (C) Copyright 2012
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

#ifndef _XLCD_ADRAW_LL_H_
#define _XLCD_ADRAW_LL_H_


/************************************************************************/
/* HEADER FILES								*/
/************************************************************************/

#include "cmd_lcd.h"			  /* wininfo_t, XYPOS, WINDOW, ... */
#include "xlcd_bitmap.h"		  /* imginfo_t */

/************************************************************************/
/* DEFINITIONS								*/
/************************************************************************/


/************************************************************************/
/* PROTOTYPES OF EXPORTED FUNCTIONS					*/
/************************************************************************/

/* Draw pixel by applying alpha of new pixel; pixel is definitely valid */
extern void adraw_ll_pixel(const wininfo_t *pwi, XYPOS x, XYPOS y,
			   const colinfo_t *pci);

/* Draw filled rectangle, applying alpha; given region is definitely valid and
   x and y are sorted (x1 <= x2, y1 <= y2) */
extern void adraw_ll_rect(const wininfo_t *pwi, XYPOS x1, XYPOS y1,
			  XYPOS x2, XYPOS y2, const colinfo_t *pci);

/* Draw a character, applyig alpha; character area is definitely valid */
extern void adraw_ll_char(const wininfo_t *pwi, XYPOS x, XYPOS y, char c,
			  const colinfo_t *pci_fg, const colinfo_t *pci_bg);

/* Draw bitmap row for 1bpp, 2bpp, 4bpp and unoptimized 8bpp palette values */
extern void adraw_ll_row_PAL(imginfo_t *pii, COLOR32 *p);

/* Optimized version for 8bpp palette values */
extern void adraw_ll_row_PAL8(imginfo_t *pii, COLOR32 *p);

/* Draw bitmap row for 8bpp gray scale with 8bpp alpha value */
extern void adraw_ll_row_GA(imginfo_t *pii, COLOR32 *p);

/* Draw bitmap row for 24bpp RGB value */
extern void adraw_ll_row_RGB(imginfo_t *pii, COLOR32 *p);

/* Draw bitmap row for 24bpp RGB value with 8bpp alpha value */
extern void adraw_ll_row_RGBA(imginfo_t *pii, COLOR32 *p);

/* Draw bitmap row for 24bpp BGR value */
extern void adraw_ll_row_BGR(imginfo_t *pii, COLOR32 *p);

/* Draw bitmap row for 24bpp BGR value with 8bpp alpha value */
extern void adraw_ll_row_BGRA(imginfo_t *pii, COLOR32 *p);

#endif	/* !_XLCD_ADRAW_LL_H_ */
