/*
 * Graphic primitives for 16 bit truecolor modes
 *
 * (C) Copyright 2010
 * F&S Elektronik Systeme GmbH
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

#include <config.h>
#include <common.h>
#include <lcd.h>			  /* LCD_COLOR*, COLORVAL, XYPOS, ... */
#include <lcd-16bpp.h>			  /* Own interface */

#if (LCD_BPP == LCD_COLOR16) || (LCD_BPP == LCD_COLOR_MANY)

#include <video_font.h>			  /* Font data, width and height */

DECLARE_GLOBAL_DATA_PTR;

void lcd_pixel16(WIN_INFO *wininfo, XYPOS x, XYPOS y, COLORVAL col)
{
	void *fbbuf = wininfo->fbbase[wininfo->fbmodify];
	COLORVAL mask = 0x0000FFFF;
	COLORVAL *p;

	if (x & 1)
		mask = ~mask;
	fbbuf += y * wininfo->linelen + x/2;
	p = (COLORVAL *)fbbuf;
	*p = (*p & ~mask) | (col & mask);
}

void lcd_rect16(WIN_INFO *wininfo, XYPOS x, XYPOS y,
		ushort width, ushort height, COLORVAL col)
{
	void *fbbuf = wininfo->fbbase[wininfo->fbmodify];
	COLORVAL maskleft, maskright;
	ushort x2 = x + width - 1;
	int i;
	COLORVAL *p;

	if (!width || !height)
		return;

	maskleft = (x & 1) ? 0xFFFF0000 : 0xFFFFFFFF;
	maskright = (x2 & 1) ? 0xFFFFFFFF : 0x0000FFFF;

	x >>= 1;
	x2 >>= 1;
	fbbuf += y * wininfo->linelen + x;
	if (x < x2) {
		/* Fill rectangle */
		do {
			p = (COLORVAL *)fbbuf + x;
			*p = (*p & ~maskleft) | (col & maskleft);
			p++;
			for (i=x+1; i<x2; i++)
				*p++ = col;
			*p = (*p & ~maskright) | (col & maskright);
			fbbuf += wininfo->linelen;
		} while (--height);
	} else {
		/* Optimized version for vertical lines */
		maskleft &= maskright;
		p = (COLORVAL *)fbbuf + x;
		do {
			*p = (*p & ~maskleft) | (col & maskleft);
			p += wininfo->linelen>>1;
		} while (--height);
	}
}

void lcd_chars16(WIN_INFO *wininfo, XYPOS x, XYPOS y, uchar c)
{
	void *fbbuf = wininfo->fbbase[wininfo->fbmodify];
        XYPOS row, col;
	COLORVAL fgcol = wininfo->fg_col;
	COLORVAL bgcol = wininfo->bg_col;
	VIDEO_FONT_TYPE *fontdata;

	fbbuf += y * wininfo->linelen + x/2;
	fontdata = &video_fontdata[c*VIDEO_FONT_HEIGHT];
	for (row = 0; row < VIDEO_FONT_HEIGHT; row++) {
		COLORVAL *p = (COLORVAL *)fbbuf;
		COLORVAL mask = (x & 1) ? 0xFFFF0000 : 0x0000FFFF;
		VIDEO_FONT_TYPE bits;

		bits = *fontdata++;
		for (col = 0; col < VIDEO_FONT_WIDTH; col++) {
			COLORVAL new;

			if (bits & (1<<(sizeof(VIDEO_FONT_TYPE)-1)))
				new = fgcol;
			else
				new = bgcol;
			bits <<= 1;
			*p = (*p & ~mask) | (new & mask);
			mask <<= 16;
			if (!mask) {
				mask = 0x0000FFFF;
				p++;
			}
		}
		fbbuf += wininfo->linelen;
	}
}

#endif /* LCD_BPP==LCD_COLOR16 || LCD_BPP=LCD_COLOR_MANY */
