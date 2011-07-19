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



void lcd_chars16(wininfo_t *pwi, XYPOS x, XYPOS y, char c)
{
	u_long fbuf = wininfo->fbbase[wininfo->fbmodify];
	u_long linelen;
        XYPOS row, col;
	COLORVAL fgcol = wininfo->fg_col;
	COLORVAL bgcol = wininfo->bg_col;
	VIDEO_FONT_TYPE *fontdata;

	linelen = pwi->linelen;
	fbuf = y * linelen + x/2 + pwi->fbuf[pwi->fbdraw];
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
		fbuf += linelen;
	}
}

#endif /* LCD_BPP==LCD_COLOR16 || LCD_BPP=LCD_COLOR_MANY */
