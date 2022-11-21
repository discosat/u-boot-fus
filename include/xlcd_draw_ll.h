/*
 * Hardware independent lowlevel drawing routines (storing aplha)
 *
 * (C) Copyright 2012
 * Hartmut Keller, F&S Elektronik Systeme GmbH, keller@fs-net.de
 *
 * SPDX-License-Identifier:	GPL-2.0+
 */

#ifndef _XLCD_DRAW_LL_H_
#define _XLCD_DRAW_LL_H_


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
extern void draw_ll_pixel(const wininfo_t *pwi, XYPOS x, XYPOS y, COLOR32 col);

/* Draw filled rectangle, applying alpha; given region is definitely valid and
   x and y are sorted (x1 <= x2, y1 <= y2) */
extern void draw_ll_rect(const wininfo_t *pwi, XYPOS x1, XYPOS y1,
			 XYPOS x2, XYPOS y2, COLOR32 col);

/* Draw a character, applyig alpha; character area is definitely valid */
extern void draw_ll_char(const wininfo_t *pwi, XYPOS x, XYPOS y, char c,
			 COLOR32 fg, COLOR32 bg);

/* Draw bitmap row for 1bpp, 2bpp, 4bpp and unoptimized 8bpp palette values */
extern void draw_ll_row_PAL(imginfo_t *pii, COLOR32 *p);

/* Optimized version for 8bpp palette values */
extern void draw_ll_row_PAL8(imginfo_t *pii, COLOR32 *p);

/* Draw bitmap row for 8bpp gray scale with 8bpp alpha value */
extern void draw_ll_row_GA(imginfo_t *pii, COLOR32 *p);

/* Draw bitmap row for 24bpp RGB value */
extern void draw_ll_row_RGB(imginfo_t *pii, COLOR32 *p);

/* Draw bitmap row for 24bpp RGB value with 8bpp alpha value */
extern void draw_ll_row_RGBA(imginfo_t *pii, COLOR32 *p);

/* Draw bitmap row for 24bpp BGR value */
extern void draw_ll_row_BGR(imginfo_t *pii, COLOR32 *p);

/* Draw bitmap row for 24bpp BGR value with 8bpp alpha value */
extern void draw_ll_row_BGRA(imginfo_t *pii, COLOR32 *p);

#endif	/* !_XLCD_DRAW_LL_H_ */
