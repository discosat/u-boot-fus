/*
 * Hardware independent lowlevel text drawing routine (storing aplha)
 *
 * (C) Copyright 2012
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

/************************************************************************/
/* HEADER FILES								*/
/************************************************************************/

#include <config.h>
#include <common.h>
#include <xlcd_draw_ll.h>		 /* Own interface */
#include <cmd_lcd.h>			 /* wininfo_t */
#include <video_font.h>			 /* Get font data, width and height */
#include <video_font_data.h>		 /* video_fontdata */

#if defined(CONFIG_XLCD_CONSOLE) \
	|| (CONFIG_XLCD_DRAW & (XLCD_DRAW_TEXT | XLCD_DRAW_PROG))
/* Draw a character, replacing pixels with new color; character area is
   definitely valid */
void draw_ll_char(const wininfo_t *pwi, XYPOS x, XYPOS y, char c,
		  COLOR32 fg, COLOR32 bg)
{
	u_int bpp_shift = pwi->ppi->bpp_shift;
	u_int bpp = 1 << bpp_shift;
	int xpos = x << bpp_shift;
	u_long fbuf;
	u_long linelen = pwi->linelen;
	u_int attr = pwi->attr;
	XYPOS underl = (attr & ATTR_UNDERL) ? VIDEO_FONT_UNDERL : -1;
	XYPOS strike = (attr & ATTR_STRIKE) ? VIDEO_FONT_STRIKE : -1;
	const VIDEO_FONT_TYPE *pfont;
	COLOR32 mask = (1 << bpp) - 1;	  /* This also works for bpp==32! */
	u_int shift = 32 - (xpos & 31);

	/* Compute framebuffer address of the pixel */
	fbuf = linelen * y + ((xpos >> 5) << 2) + pwi->pfbuf[pwi->fbdraw];

	/* Compute start of character within font data */
	pfont = video_fontdata;
	pfont += sizeof(VIDEO_FONT_TYPE) * VIDEO_FONT_HEIGHT * c;

	for (y = 0; y < VIDEO_FONT_HEIGHT; y++) {
		VIDEO_FONT_TYPE fd;	  /* Font data (one character row) */
		unsigned line_count;	  /* Loop twice if double height */

		/* If underline or strike-through line is reached, use fully
		   set pixel, otherwise get character pixel data and apply
		   bold and inverse attributes */
		if ((y == underl) || (y == strike))
			fd = (VIDEO_FONT_TYPE)0xFFFFFFFF;
		else {
			fd = *pfont;
			if (attr & ATTR_BOLD)
				fd |= fd>>1;
		}
		if (attr & ATTR_INVERSE)
			fd = ~fd;
		pfont++;		  /* Next character row */

		/* Loop up to four times if multiple height */
		line_count = ((attr & ATTR_VS_MASK) >> 6) + 1;
		do {
			COLOR32 *p = (COLOR32 *)fbuf;
			u_int s = shift;
			COLOR32 val;
			VIDEO_FONT_TYPE fm = 1<<(VIDEO_FONT_WIDTH-1);

			/* Load first word */
			val = *p;
			do {
				/* Loop up to four times if multiple width */
				unsigned col_count;

				col_count = ((attr & ATTR_HS_MASK) >> 4) + 1;
				do {
					/* Blend FG or BG pixel (BG only if not
					   transparent) */
					s -= bpp;
					if (fd & fm) {
						val &= ~(mask << s);
						val |= fg << s;
					}
					else if (!(attr & ATTR_NO_BG)) {
						val &= ~(mask << s);
						val |= bg << s;
					}

					/* Shift mask to next pixel */
					if (!s) {
						/* Store old word and load
						   next word; reset mask to
						   first pixel in word */
						*p++ = val;
						s = 32;
						val = *p;
					}
				} while (--col_count);
				fm >>= 1;
			} while (fm);

			/* Store back last word */
			*p = val;

			/* Go to next line */
			fbuf += linelen;
		} while (--line_count);
	}
}
#endif
