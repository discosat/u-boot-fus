/*
 * Hardware independent LCD controller part, graphics primitives
 *
 * (C) Copyright 2011
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

/* #define DEBUG */

#include <config.h>
#include <common.h>
#include <lcd.h>			  /* Own interface */
#include <cmd_lcd.h>			  /* cmd_lcd_init() */
#include <video_font.h>			  /* Get font data, width and height */
#include <devices.h>			  /* device_t */
#include <serial.h>			  /* serial_putc(), serial_puts() */
#include <watchdog.h>


/************************************************************************/
/* TYPES AND STRUCTURES							*/
/************************************************************************/

DECLARE_GLOBAL_DATA_PTR;

#ifndef CONFIG_MULTIPLE_CONSOLES
coninfo_t coninfo;			  /* Console information */
wininfo_t *console_pwi;			  /* Pointer to window with console */
#endif

/* Color structure used in test pattern generation */
struct iRGB {
	int R;
	int G;
	int B;
};

/************************************************************************/
/* LOCAL VARIABLES							*/
/************************************************************************/



/************************************************************************/
/* PROTOTYPES OF LOCAL FUNCTIONS					*/
/************************************************************************/

static void console_putc(wininfo_t *pwi, coninfo_t *pci, char c);

/* Draw pixel by replacing with new color; pixel is definitely valid */
static void draw_ll_pixel(const wininfo_t *pwi, XYPOS x, XYPOS y, COLOR32 col);

/* Draw pixel by applying alpha of new pixel; pixel is definitely valid */
static void adraw_ll_pixel(const wininfo_t *pwi, XYPOS x, XYPOS y);

/* Draw filled rectangle, replacing pixels with new color; given region is
   definitely valid and x and y are sorted (x1 <= x2, y1 <= y2) */
static void draw_ll_rect(const wininfo_t *pwi, XYPOS x1, XYPOS y1,
			 XYPOS x2, XYPOS y2, COLOR32 col);

/* Draw filled rectangle, applying alpha; given region is definitely valid and
   x and y are sorted (x1 <= x2, y1 <= y2) */
static void adraw_ll_rect(const wininfo_t *pwi, XYPOS x1, XYPOS y1,
			  XYPOS x2, XYPOS y2);

/* Draw a character, replacing pixels with new color; character area is
   definitely valid */
static void draw_ll_char(const wininfo_t *pwi, XYPOS x, XYPOS y, char c,
			 COLOR32 fg, COLOR32 bg);

/* Draw a character, applyig alpha; character area is definitely valid */
static void adraw_ll_char(const wininfo_t *pwi, XYPOS x, XYPOS y, char c);


/************************************************************************/
/* ** Low-Level Graphics Routines					*/
/************************************************************************/

/* Repeat color value so that it fills the whole 32 bits */
static COLOR32 col2col32(const wininfo_t *pwi, COLOR32 color)
{
	switch (pwi->ppi->bpp_shift) {
	case 0:
		color |= color << 1;
		/* Fall through to case 1 */
	case 1:
		color |= color << 2;
		/* Fall through to case 2 */
	case 2:
		color |= color << 4;
		/* Fall through to case 3 */
	case 3:
		color |= color << 8;
		/* Fall through to case 4 */
	case 4:
		color |= color << 16;
		/* Fall through to default */
	default:
		break;
	}

	return color;
}


/* Draw pixel by replacing with new color; pixel is definitely valid */
static void draw_ll_pixel(const wininfo_t *pwi, XYPOS x, XYPOS y, COLOR32 col)
{
	u_int bpp_shift = pwi->ppi->bpp_shift;
	int xpos = x << bpp_shift;
	u_int bpp = 1 << bpp_shift;
	u_long fbuf;
	COLOR32 mask = (1 << bpp) - 1;
	COLOR32 *p;
	u_int shift = 32 - (xpos & 31) - bpp;

	/* Compute framebuffer address of the pixel */
	fbuf = pwi->linelen * y + pwi->pfbuf[pwi->fbdraw];

	/* Remove old pixel and fill in new pixel */
	p = (COLOR32 *)(fbuf + ((xpos >> 5) << 2));
	*p = (*p & ~(mask << shift)) | (col << shift);
}


/* Draw pixel by applying alpha of new pixel; pixel is definitely valid */
static void adraw_ll_pixel(const wininfo_t *pwi, XYPOS x, XYPOS y)
{
	u_int bpp_shift = pwi->ppi->bpp_shift;
	int xpos = x << bpp_shift;
	u_int bpp = 1 << bpp_shift;
	u_long fbuf;
	COLOR32 mask = (1 << bpp) - 1;
	COLOR32 *p;
	COLOR32 val;
	u_int shift = 32 - (xpos & 31) - bpp;
	COLOR32 col;

	/* Compute framebuffer address of the pixel */
	fbuf = pwi->linelen * y + pwi->pfbuf[pwi->fbdraw];

	/* Remove old pixel and fill in new pixel */
	p = (COLOR32 *)(fbuf + ((xpos >> 5) << 2));
	val = *p;
	col = pwi->ppi->apply_alpha(pwi, &pwi->fg, val >> shift);
	val &= ~(mask << shift);
	val |= col << shift;
	*p = val;
}


/* Draw filled rectangle, replacing pixels with new color; given region is
   definitely valid and x and y are sorted (x1 <= x2, y1 <= y2) */
static void draw_ll_rect(const wininfo_t *pwi, XYPOS x1, XYPOS y1,
			 XYPOS x2, XYPOS y2, COLOR32 color)
{
	u_int bpp_shift = pwi->ppi->bpp_shift;
	u_int bpp = 1 << bpp_shift;
	u_long fbuf;
	u_long linelen = pwi->linelen;
	COLOR32 maskleft, maskright;
	COLOR32 *p;
	int count;
	int xpos;

	/* Repeat color so that it fills the whole 32 bits */
	color = col2col32(pwi, color);

	xpos = x2 << bpp_shift;
	maskright = 0xFFFFFFFF >> (xpos & 31);
	maskright = ~(maskright >> bpp);
	x2 = xpos >> 5;

	xpos = x1 << bpp_shift;
	maskleft = 0xFFFFFFFF >> (xpos & 31);
	x1 = xpos >> 5;

	/* Compute framebuffer address of the beginning of the top row */
	fbuf = linelen * y1 + pwi->pfbuf[pwi->fbdraw];
	p = (COLOR32 *)fbuf + x1;
	linelen >>= 2;

	count = y2 - y1 + 1;
	x2 -= x1;
	if (x2) {
		/* Fill rectangle consisting of several words per row */
		do {
			int i;

			/* Handle leftmost word in row */
			p[0] = (p[0] & ~maskleft) | (color & maskleft);

			/* Fill all middle words without masking */
			for (i = 1; i < x2; i++)
				p[i] = color;

			/* Handle rightmost word in row */
			p[x2] = (p[x2] & ~maskright) | (color & maskright);

			/* Go to next row */
			p += linelen;
		} while (--count);
	} else {
		/* Optimized version for the case where only one word has to be
		   checked in each row; this includes vertical lines */
		maskleft &= maskright;
		color &= maskleft;
		do {
			*p = (*p & ~maskleft) | color;
			p += linelen;
		} while (--count);
	}
}


/* Draw filled rectangle, applying alpha; given region is definitely valid and
   x and y are sorted (x1 <= x2, y1 <= y2) */
static void adraw_ll_rect(const wininfo_t *pwi, XYPOS x1, XYPOS y1,
			  XYPOS x2, XYPOS y2)
{
	u_int bpp_shift = pwi->ppi->bpp_shift;
	u_int bpp = 1 << bpp_shift;
	u_long fbuf;
	COLOR32 mask = (1 << bpp) - 1;
	COLOR32 *p;
	u_int shift;
	int ycount, xcount;
	int xpos;

	xcount = x2 - x1 + 1;
	ycount = y2 - y1 + 1;
	xpos = x1 << bpp_shift;
	shift = 32 - (xpos & 31);

	/* Compute framebuffer address of the beginning of the top row */
	fbuf = pwi->linelen*y1 + pwi->pfbuf[pwi->fbdraw] + ((xpos >> 5) << 2);
	do {
		COLOR32 val;
		u_int s = shift;
		int c = xcount;

		p = (COLOR32 *)fbuf;
		val = *p;
		for (;;) {
			COLOR32 col;
			s -= bpp;
			col = pwi->ppi->apply_alpha(pwi, &pwi->fg, val >> s);
			val &= ~(mask << s);
			val |= col << s;
			if (!--c)
				break;
			if (!s) {
				*p++ = val;
				val = *p;
				s = 32;
			}
		};
		*p = val;

		fbuf += pwi->linelen;
	} while (--ycount);
}


/* Draw a character, replacing pixels with new color; character area is
   definitely valid */
static void draw_ll_char(const wininfo_t *pwi, XYPOS x, XYPOS y, char c,
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

		/* Loop twice if double height */
		line_count = (attr & ATTR_DHEIGHT) ? 2 : 1;
		do {
			COLOR32 *p = (COLOR32 *)fbuf;
			u_int s = shift;
			COLOR32 val;
			VIDEO_FONT_TYPE fm = 1<<(VIDEO_FONT_WIDTH-1);

			/* Load first word */
			val = *p;
			do {
				/* Loop twice if double width */
				unsigned col_count = (attr & ATTR_DWIDTH)?2:1;
				do {
					/* Blend FG or BG pixel (BG only if not
					   transparent) */
					s -= bpp;
					if (fd & fm) {
						val &= ~(mask << s);
						val |= fg << s;
					}
					else if (!(attr & ATTR_TRANSP)) {
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


/* Draw a character, applyig alpha; character area is definitely valid */
static void adraw_ll_char(const wininfo_t *pwi, XYPOS x, XYPOS y, char c)
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

		/* Loop twice if double height */
		line_count = (attr & ATTR_DHEIGHT) ? 2 : 1;
		do {
			COLOR32 *p = (COLOR32 *)fbuf;
			u_int s = shift;
			COLOR32 val;
			VIDEO_FONT_TYPE fm = 1<<(VIDEO_FONT_WIDTH-1);

			/* Load first word */
			val = *p;
			do {
				/* Loop twice if double width */
				unsigned col_count = (attr & ATTR_DWIDTH)?2:1;
				do {
					COLOR32 col;

					/* Blend FG or BG pixel (BG only if not
					   transparent) */
					s -= bpp;
					if (fd & fm)
						col = pwi->ppi->apply_alpha(
							pwi, &pwi->fg,
							val >> s);
					else if (!(attr & ATTR_TRANSP))
						col = pwi->ppi->apply_alpha(
							pwi, &pwi->bg,
							val >> s);
					else
						goto TRANS;
					val &= ~(mask << s);
					val |= col << s;
				TRANS:

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


/************************************************************************/
/* HELPER FUNCTIONS FOR GRAPHIC PRIMITIVES				*/
/************************************************************************/

/* Draw test pattern with grid, basic colors, color gradients and circles */
static void test_pattern0(const wininfo_t *pwi)
{
	const pixinfo_t *ppi = pwi->ppi;
	XYPOS dx, dy;
	XYPOS x, y, i;
	XYPOS hleft, vtop, hright, vbottom;
	XYPOS fbhres = (XYPOS)pwi->fbhres;
	XYPOS fbvres = (XYPOS)pwi->fbvres;
	XYPOS r1, r2, scale;

	static const RGBA coltab[] = {
		0xFF0000FF,		  /* R */
		0x00FF00FF,		  /* G */
		0x0000FFFF,		  /* B */
		0xFFFFFFFF,		  /* W */
		0xFFFF00FF,		  /* Y */
		0xFF00FFFF,		  /* M */
		0x00FFFFFF,		  /* C */
	};

	/* We need at least 24x16 resolution */
	if ((fbhres < 24) || (fbvres < 16)) {
		puts("Window too small\n");
		return;
	}
	/* Clear screen */
	lcd_clear(pwi);

	/* Use hres divided by 12 as grid size */
	dx = fbhres/12;
	dy = fbvres/8;

	/* Compute left and top margin for first line as half of the remaining
	   space (that was not multiple of 12) and half of one d x d field */
	hleft = (dx + fbhres % dx)/2;
	vtop = (dy + fbvres % dy)/2;

	/* Compute right and bottom margin for last line in a similar way */
	hright = ((fbhres - hleft)/dx)*dx + hleft;
	vbottom = ((fbvres - vtop)/dy)*dy + vtop;

	/* Draw vertical lines of grid */
	for (x = hleft; x < fbhres; x += dx)
		lcd_rect(pwi, x, 0, x, fbvres-1);

	/* Draw horizontal lines of grid */
	for (y = vtop; y < fbvres; y += dy)
		lcd_rect(pwi, 0, y, fbhres-1, y);

	/* Draw 7 of the 8 basic colors (without black) as rectangles */
	for (i=0; i<7; i++) {
		x = hleft + (i+2)*dx;
		draw_ll_rect(pwi, x+1, vbottom-2*dy+1, x+dx-1, vbottom-1,
			     ppi->rgba2col(pwi, coltab[6-i]));
		draw_ll_rect(pwi, x+1, vtop+1, x+dx-1, vtop+2*dy-1,
			     ppi->rgba2col(pwi, coltab[i]));
	}

	/* Draw grayscale gradient on left, R, G, B gradient on right side */
	scale = vbottom-vtop-2;
	for (y=0; y<=scale; y++) {
		XYPOS yy = y+vtop+1;
		RGBA rgba;

		rgba = (y*255/scale) << 8;
		rgba |= (rgba << 8) | (rgba << 16) | 0xFF;
		draw_ll_rect(pwi, hleft+1, yy, hleft+dx-1, yy,
			     ppi->rgba2col(pwi, rgba));
		draw_ll_rect(pwi, hright-dx+1, yy, hright-2*dx/3, yy,
			     ppi->rgba2col(pwi, rgba & 0xFF0000FF));
		draw_ll_rect(pwi, hright-2*dx/3+1, yy, hright-dx/3, yy,
			     ppi->rgba2col(pwi, rgba & 0x00FF00FF));
		draw_ll_rect(pwi, hright-dx/3+1, yy, hright-1, yy,
			     ppi->rgba2col(pwi, rgba & 0x0000FFFF));
	}

	/* Draw big and small circle; make sure that circle fits on screen */
	if (fbhres > fbvres) {
		r1 = fbvres/2;
		r2 = dy;
	} else {
		r1 = fbhres/2;
		r2 = dx;
	}
	lcd_circle(pwi, fbhres/2, fbvres/2, r1 - 1);
	lcd_circle(pwi, fbhres/2, fbvres/2, r2);

	/* Draw corners */
	if ((fbhres >= 8) && (fbvres >= 8)) {
		COLOR32 col;

		col = ppi->rgba2col(pwi, 0x00FF00FF);  /* Green */
		draw_ll_rect(pwi, 0, 0, 7, 0, col);
		draw_ll_rect(pwi, 0, 1, 0, 7, col);
		draw_ll_rect(pwi, fbhres-8, 0, fbhres-1, 0, col);
		draw_ll_rect(pwi, fbhres-1, 1, fbhres-1, 7, col);
		draw_ll_rect(pwi, 0, fbvres-8, 0, fbvres-2, col);
		draw_ll_rect(pwi, 0, fbvres-1, 7, fbvres-1, col);
		draw_ll_rect(pwi, fbhres-1, fbvres-8, fbhres-1, fbvres-2, col);
		draw_ll_rect(pwi, fbhres-8, fbvres-1, fbhres-1, fbvres-1, col);
	}
}

/* Draw the eight basic colors in two rows of four */
static void test_pattern1(const wininfo_t *pwi)
{
	const pixinfo_t *ppi = pwi->ppi;
	XYPOS xres = (XYPOS)pwi->fbhres;
	XYPOS yres = (XYPOS)pwi->fbvres;
	XYPOS xres_4 = (xres + 2)/4;
	XYPOS yres_2 = (yres + 1)/2;

	/* We need at least 4 pixels in x and 2 pixels in y direction */
	if ((xres < 4) || (yres < 2)) {
		puts("Window too small\n");
		return;
	}

	/* Clear screen with black */
	lcd_clear(pwi);

	/* Draw red, green, blue, black rectangles in top row */
	draw_ll_rect(pwi, 0, 0, xres_4-1, yres_2-1,
		     ppi->rgba2col(pwi, 0xFF0000FF)); /* Red */
	draw_ll_rect(pwi, xres_4, 0, 2*xres_4-1, yres_2-1,
		     ppi->rgba2col(pwi, 0x00FF00FF)); /* Green */
	draw_ll_rect(pwi, 2*xres_4, 0, 3*xres_4-1, yres_2-1,
		     ppi->rgba2col(pwi, 0x0000FFFF)); /* Blue */
	draw_ll_rect(pwi, 3*xres_4, 0, xres-1, yres_2-1,
		     ppi->rgba2col(pwi, 0x000000FF)); /* Black */

	/* Draw cyan, magenta, yellow, white rectangles in bottom row */
	draw_ll_rect(pwi, 0, yres_2, xres_4-1, yres-1,
		     ppi->rgba2col(pwi, 0x00FFFFFF)); /* Cyan */
	draw_ll_rect(pwi, xres_4, yres_2, 2*xres_4-1, yres-1,
		     ppi->rgba2col(pwi, 0xFF00FFFF)); /* Magenta */
	draw_ll_rect(pwi, 2*xres_4, yres_2, 3*xres_4-1, yres-1,
		     ppi->rgba2col(pwi, 0xFFFF00FF)); /* Yellow */
	draw_ll_rect(pwi, 3*xres_4, yres_2, xres-1, yres-1,
		     ppi->rgba2col(pwi, 0xFFFFFFFF)); /* White */
}

/* Draw color gradient, horizontal: hue, vertical: brightness */
static void test_pattern2(const wininfo_t *pwi)
{
	const pixinfo_t *ppi = pwi->ppi;
	int xres = (int)pwi->fbhres;
	int yres = (int)pwi->fbvres;
	int yres_2 = yres/2;
	int scale = (yres-1)/2;
	int hue;

	static const struct iRGB target[] = {
		{0xFF, 0x00, 0x00},	  /* R */
		{0xFF, 0xFF, 0x00},	  /* Y */
		{0x00, 0xFF, 0x00},	  /* G */
		{0x00, 0xFF, 0xFF},	  /* C */
		{0x00, 0x00, 0xFF},	  /* B */
		{0xFF, 0x00, 0xFF},	  /* M */
		{0xFF, 0x00, 0x00}	  /* R */
	};

	/* We need at least 6 pixels in x and 3 pixels in y direction */
	if ((xres < 6) || (yres < 3)) {
		puts("Window too small\n");
		return;
	}

	/* Clear screen with black */
	lcd_clear(pwi);

	for (hue = 0; hue < 6; hue++) {
		int xfrom = (hue*xres + 3)/6;
		int dx = ((hue + 1)*xres + 3)/6 - xfrom;
		struct iRGB from = target[hue];
		struct iRGB to = target[hue+1];
		struct iRGB temp;
		int x, y;
		RGBA rgba;

		for (x=0; x<dx; x++) {
			temp.R = (to.R - from.R)*x/dx + from.R;
			temp.G = (to.G - from.G)*x/dx + from.G;
			temp.B = (to.B - from.B)*x/dx + from.B;

			for (y=0; y<yres_2; y++) {
				rgba = (temp.R * y/scale) << 24;
				rgba |= (temp.G * y/scale) << 16;
				rgba |= (temp.B * y/scale) << 8;
				rgba |= 0xFF;
				draw_ll_pixel(pwi, x + xfrom, y,
					      ppi->rgba2col(pwi, rgba));
			}

			for (y=0; y<yres-yres_2; y++) {
				rgba = ((0xFF-temp.R)*y/scale + temp.R) << 24;
				rgba |= ((0xFF-temp.G)*y/scale + temp.G) << 16;
				rgba |= ((0xFF-temp.B)*y/scale + temp.B) << 8;
				rgba |= 0xFF;
				draw_ll_pixel(pwi, x + xfrom, y + yres_2,
					      ppi->rgba2col(pwi, rgba));
			}
		}
	}
}

/* Draw color gradient: 8 basic colors along edges, gray in the center */
static void test_pattern3(const wininfo_t *pwi)
{
	const pixinfo_t *ppi = pwi->ppi;
	int xres = (int)pwi->fbhres;
	int yres = (int)pwi->fbvres;
	int xres_2a = xres/2;
	int xres_2b = xres - xres_2a - 1;
	int yres_2a = yres/2;
	int yres_2b = yres - yres_2a - 1;
	int x, y;
	RGBA rgb;
	struct iRGB l, m, r;

	struct iRGB tl = {0x00, 0x00, 0x00};	  /* Top left: Black */
	struct iRGB tm = {0x00, 0x00, 0xFF};	  /* Top middle: Blue */
	struct iRGB tr = {0x00, 0xFF, 0xFF};	  /* Top right: Cyan */
	struct iRGB ml = {0xFF, 0x00, 0x00};	  /* Middle left: Red */
	struct iRGB mm = {0x80, 0x80, 0x80};	  /* Middle middle: Gray */
	struct iRGB mr = {0x00, 0xFF, 0x00};	  /* Middle right: Green */
	struct iRGB bl = {0xFF, 0x00, 0xFF};	  /* Bottom left: Magenta */
	struct iRGB bm = {0xFF, 0xFF, 0xFF};	  /* Bottom middle: White */
	struct iRGB br = {0xFF, 0xFF, 0x00};	  /* Bottom right: Yellow */

	/* We need at least 3 pixels in x and 3 pixels in y direction */
	if ((xres < 3) || (yres < 3)) {
		puts("Window too small\n");
		return;
	}

	/* Clear screen */
	lcd_clear(pwi);

	for (y=0; y<yres; y++) {

		/* Compute left, middle and right colors for next row */
		if (y<yres_2a) {
			l.R = (ml.R - tl.R)*y/yres_2a + tl.R;
			l.G = (ml.G - tl.G)*y/yres_2a + tl.G;
			l.B = (ml.B - tl.B)*y/yres_2a + tl.B;

			m.R = (mm.R - tm.R)*y/yres_2a + tm.R;
			m.G = (mm.G - tm.G)*y/yres_2a + tm.G;
			m.B = (mm.B - tm.B)*y/yres_2a + tm.B;

			r.R = (mr.R - tr.R)*y/yres_2a + tr.R;
			r.G = (mr.G - tr.G)*y/yres_2a + tr.G;
			r.B = (mr.B - tr.B)*y/yres_2a + tr.B;
		} else {
			int y2 = y - yres_2a;

			l.R = (bl.R - ml.R)*y2/yres_2b + ml.R;
			l.G = (bl.G - ml.G)*y2/yres_2b + ml.G;
			l.B = (bl.B - ml.B)*y2/yres_2b + ml.B;

			m.R = (bm.R - mm.R)*y2/yres_2b + mm.R;
			m.G = (bm.G - mm.G)*y2/yres_2b + mm.G;
			m.B = (bm.B - mm.B)*y2/yres_2b + mm.B;

			r.R = (br.R - mr.R)*y2/yres_2b + mr.R;
			r.G = (br.G - mr.G)*y2/yres_2b + mr.G;
			r.B = (br.B - mr.B)*y2/yres_2b + mr.B;
		}

		/* Draw left half of row */
		for (x=0; x<xres_2a; x++) {
			rgb = ((m.R - l.R)*x/xres_2a + l.R) << 24;
			rgb |= ((m.G - l.G)*x/xres_2a + l.G) << 16;
			rgb |= ((m.B - l.B)*x/xres_2a + l.B) << 8;
			rgb |= 0xFF;

			draw_ll_pixel(pwi, (XYPOS)x, (XYPOS)y,
				      ppi->rgba2col(pwi, rgb));
		}

		/* Draw right half of row */
		for (x=xres_2a; x<xres; x++) {
			int x2 = x - xres_2a;

			rgb = ((r.R - m.R)*x2/xres_2b + m.R) << 24;
			rgb |= ((r.G - m.G)*x2/xres_2b + m.G) << 16;
			rgb |= ((r.B - m.B)*x2/xres_2b + m.B) << 8;
			rgb |= 0xFF;

			draw_ll_pixel(pwi, (XYPOS)x, (XYPOS)y,
				      ppi->rgba2col(pwi, rgb));
		}
	}
}




/************************************************************************/
/* GRAPHICS PRIMITIVES							*/
/************************************************************************/

/* Fill display with FG color */
void lcd_fill(const wininfo_t *pwi)
{
	if (pwi->attr & ATTR_ALPHA)
		adraw_ll_rect(pwi, 0, 0, pwi->fbhres, pwi->fbvres);
	else
		memset32((unsigned *)pwi->pfbuf[pwi->fbdraw],
			 col2col32(pwi, pwi->fg.col), pwi->fbsize/4);
}

/* Clear display with BG color */
void lcd_clear(const wininfo_t *pwi)
{
	memset32((unsigned *)pwi->pfbuf[pwi->fbdraw],
		 col2col32(pwi, pwi->bg.col), pwi->fbsize/4);
}

/* Draw pixel at (x, y) with FG color */
void lcd_pixel(const wininfo_t *pwi, XYPOS x, XYPOS y)
{
	if ((x < 0) || (x >= (XYPOS)pwi->fbhres)
	    || (y < 0) || (y >= (XYPOS)pwi->fbvres))
		return;

	if (pwi->attr & ATTR_ALPHA)
		adraw_ll_pixel(pwi, x, y);
	else
		draw_ll_pixel(pwi, x, y, pwi->fg.col);
}


/* Draw line from (x1, y1) to (x2, y2) in color */
void lcd_line(const wininfo_t *pwi, XYPOS x1, XYPOS y1, XYPOS x2, XYPOS y2)
{
	int dx, dy, dd;
	XYPOS temp;
	XYPOS xmax, ymax;
	XYPOS xoffs, yoffs;

	dx = (int)x2 - (int)x1;
	if (dx < 0)
		dx = -dx;
	dy = (int)y2 - (int)y1;
	if (dy < 0)
		dy = -dy;

	xmax = (XYPOS)pwi->fbhres - 1;
	ymax = (XYPOS)pwi->fbvres - 1;

	if (dy > dx) {			  /* High slope */
		/* Sort pixels so that y1 <= y2 */
		if (y1 > y2) {
			temp = x1;
			x1 = x2;
			x2 = temp;
			temp = y1;
			y1 = y2;
			y2 = temp;
		}

		/* Return if line is completely above or below the display */
		if ((y2 < 0) || (y1 > ymax))
			return;

		dd = dy;
		dx <<= 1;
		dy <<= 1;

		if (y1 < 0) {
			/* Clip with upper screen edge */
			yoffs = -y1;
			xoffs = (dd + (int)yoffs * dx)/dy;
			dd += xoffs*dy - yoffs*dx;
			y1 = 0;
			x1 += xoffs;
		}

		/* Return if line fragment is fully left or right of display */
		if (((x1 < 0) && (x2 < 0)) || ((x1 > xmax) && (x2 > xmax)))
			return;

		/* We only need y2 as end coordinate */
		if (y2 > ymax)
			y2 = ymax;

		if (dx == 0) {
			/* Draw vertical line */
			lcd_rect(pwi, x1, y1, x2, y2);
			return;
		}

		xoffs = (x1 > x2) ? -1 : 1;
		for (;;) {
			lcd_pixel(pwi, x1, y1);
			if (y1 == y2)
				break;
			y1++;
			dd += dx;
			if (dd >= dy) {
				dd -= dy;
				x1 += xoffs;
			}
		}
	} else {			  /* Low slope */
		/* Sort pixels so that x1 <= x2 */
		if (x1 > x2) {
			temp = x1;
			x1 = x2;
			x2 = temp;
			temp = y1;
			y1 = y2;
			y2 = temp;
		}

		/* Return if line is completely left or right of the display */
		if ((x2 < 0) || (x1 > xmax))
			return;

		dd = dx;
		dx <<= 1;
		dy <<= 1;

		if (x1 < 0) {
			/* Clip with left screen edge */
			xoffs = -x1;
			yoffs = (dd + (int)xoffs * dy)/dx;
			dd += yoffs*dx - xoffs*dy;
			x1 = 0;
			y1 += yoffs;
		}

		/* Return if line fragment is fully above or below display */
		if (((y1 < 0) && (y2 < 0)) || ((y1 > ymax) && (y2 > ymax)))
			return;

		/* We only need x2 as end coordinate */
		if (x2 > xmax)
			x2 = xmax;

		if (dy == 0) {
			/* Draw horizontal line */
			lcd_rect(pwi, x1, y1, x2, y2);
			return;
		}

		yoffs = (y1 > y2) ? -1 : 1;
		for (;;) {
			lcd_pixel(pwi, x1, y1);
			if (x1 == x2)
				break;
			x1++;
			dd += dy;
			if (dd >= dx) {
				dd -= dx;
				y1 += yoffs;
			}
		}
	}
}

/* Draw rectangular frame from (x1, y1) to (x2, y2) in color */
void lcd_frame(const wininfo_t *pwi, XYPOS x1, XYPOS y1, XYPOS x2, XYPOS y2)
{
	XYPOS xmax, ymax;

	/* Sort x and y values */
	if (x1 > x2) {
		XYPOS xtemp = x1;
		x1 = x2;
		x2 = xtemp;
	}
	if (y1 > y2) {
		XYPOS ytemp = y1;
		y1 = y2;
		y2 = ytemp;
	}

	/* Check if object is fully left, right, above or below screen */
	xmax = (XYPOS)pwi->fbhres - 1;
	ymax = (XYPOS)pwi->fbvres - 1;
	if ((x2 < 0) || (y2 < 0) || (x1 > xmax) || (y1 > ymax))
		return;			  /* Done, object not visible */

	/* If the frame is wider than two pixels, we need to draw
	   horizontal lines at the top and bottom */
	if (x2 - x1 > 1) {
		XYPOS xl, xr;

		/* Clip at left and right screen edges if necessary */
		xl = (x1 < 0) ? 0 : x1;
		xr = (x2 > xmax) ? xmax : x2;

		/* Draw top line */
		if (y1 >= 0) {
			lcd_rect(pwi, xl, y1, xr, y1);

			/* We are done if rectangle is only one pixel high */
			if (y1 == y2)
				return;
		}

		/* Draw bottom line */
		if (y2 <= ymax)
			lcd_rect(pwi, xl, y2, xr, y2);

		/* For the vertical lines we only need to draw the region
		   between the horizontal lines, so increment y1 and decrement
		   y2; if rectangle is exactly two pixels high, we don't
		   need to draw any vertical lines at all. */
		if (++y1 == y2--)
			return;
	}

	/* Clip at upper and lower screen edges if necessary */
	if (y1 < 0)
		y1 = 0;
	if (y2 > ymax)
		y2 = ymax;

	/* Draw left line */
	if (x1 >= 0) {
		lcd_rect(pwi, x1, y1, x1, y2);

		/* Return if rectangle is only one pixel wide */
		if (x1 == x2)
			return;
	}

	/* Draw right line */
	if (x2 <= xmax)
		lcd_rect(pwi, x2, y1, x2, y2);
}


/* Draw filled rectangle from (x1, y1) to (x2, y2) in color */
void lcd_rect(const wininfo_t *pwi, XYPOS x1, XYPOS y1, XYPOS x2, XYPOS y2)
{
	XYPOS xmax, ymax;

	/* Sort x and y values */
	if (x1 > x2) {
		XYPOS xtemp = x1;
		x1 = x2;
		x2 = xtemp;
	}
	if (y1 > y2) {
		XYPOS ytemp = y1;
		y1 = y2;
		y2 = ytemp;
	}

	/* Check if object is fully left, right, above or below screen */
	xmax = (XYPOS)pwi->fbhres - 1;
	ymax = (XYPOS)pwi->fbvres - 1;
	if ((x2 < 0) || (y2 < 0) || (x1 > xmax) || (y1 > ymax))
		return;			  /* Done, object not visible */

	/* Clip rectangle to framebuffer boundaries */
	if (x1 < 0)
		x1 = 0;
	if (y1 < 0)
		y1 = 0;
	if (x2 > xmax)
		x2 = xmax;
	if (y2 >= ymax)
		y2 = ymax;

	/* Finally draw rectangle */
	if (pwi->attr & ATTR_ALPHA)
		adraw_ll_rect(pwi, x1, y1, x2, y2);
	else
		draw_ll_rect(pwi, x1, y1, x2, y2, pwi->fg.col);
}


/* Draw circle outline at (x, y) with radius r and color.
 *
 * Circle algorithm
 * ----------------
 * The circle is computed as if it was at the coordinate system origin. We
 * only compute the pixels (dx, dy) for the first quadrant (top right),
 * starting from the top position. The other pixels of the circle can be
 * obtained by utilizing the circle symmetry by inverting dx or dy. The final
 * circle pixel on the display is obtained by adding the circle center
 * coordinates.
 *
 * The algorithm is a so-called midpoint circle algorithm. In the first part
 * of the circle with slope >-1 (low slope, dy>dx), we check whether we have
 * to go from pixel (dx, dy) east (E) to (dx+1, dy) or southeast (SE) to
 * (dx+1, dy-1). This is done by checking the midpoint (dx+1, dy-1/2). If it
 * mathematically lies within the circle, we go E, otherwise SE.
 *
 * Similar to this, we check in the part with slope <-1 (high slope, dy<=dx)
 * whether we have to go southeast (SE) to (dx+1, dy-1) or south (S) to (dx,
 * dy-1). This is done by checking midpoint (dx+1/2, dy-1). If it lies within
 * the circle, we go SE, else S.
 *
 * A point (dx, dy) lies exactly on the circle line, if dx^2 + dy^2 = r^2
 * (circle equation). Thus a pixel is within the circle area, if function
 * F(dx, dy) = dx^2 + dy^2 - r^2 is less than 0. It is outside if F(dx, dy) is
 * greater than 0.
 *
 * Computing the value for this formula for each midpoint anew would be rather
 * time consuming, considering the fractions and squares. However if we only
 * compute the differences from midpoint to midpoint, it will get much easier.
 * Let's assume we already have the value dd = F(dx+1, dy-1/2) at some
 * midpoint, then we can easily obtain the value of the next midpoint:
 *   dd<0:  E:	deltaE	= F(dx+2, dy-1/2) - dd = 2*dx + 3
 *   dd>=0: SE: deltaSE = F(dx+2, dy-3/2) - dd = 2*dx - 2*dy + 5
 *
 * We have to start with the midpoint of pixel (dx=0, dy=r) which is
 *   dd = F(1, r-1/2) = 5/4 - r
 * By a transition about -1/4, we get dd = 1-r. However now we would have to
 * compare with -1/4 instead of with 0. But as center and radius are always
 * integers, this can be neglected.
 *
 * For the second part of the circle with high slope, the differences from
 * point to point can also be easily computed:
 *   dd<0:  SE: deltaSE = F(dx+3/2, dy-2) - dd = 2*dx - 2*dy + 5
 *   dd>=0: S:	deltaS	= F(dx+1/2, dy-2) - dd = -2*dy + 3
 *
 * We also have to consider the case when switching the slope, i.e. when we go
 * from midpoint (dx+1, dy-1/2) to midpoint (dx+1/2, dy-1). Again we only need
 * the difference:
 *   delta = F(dx+1/2, dy-1) - F(dx+1, dy-1/2) = F(dx+1/2, dy-1) - dd
 *	   = -dx - dy
 *
 * This results in the following basic circle algorithm:
 *     dx=0; dy=r; dd=1-r;
 *     while (dy>dx)				       Slope >-1 (low)
 *	   SetPixel(dx, dy);
 *	   if (dd<0)				       (*)
 *	       dd = dd+2*dx+3; dx=dx+1;		       East E
 *	   else
 *	       dd = dd+2*(dx-dy)+5; dx=dx+1; dy=dy-1;  Southeast SE
 *     dd = dd-dx-dy;
 *     while (dy>=0)				       Slope <-1 (high)
 *	   SetPixel(dx, dy)
 *	   if (dd<0)
 *	       dd = dd+2*(dx-dy)+5; dx=dx+1; dy=dy-1;  Southeast SE
 *	   else
 *	       dd = dd-2*dy+3; dy=dy-1;		       South S
 *
 * A small improvement can be obtained if adding && (dy > dx+1) at position
 * (*). Then there are no corners at slope -1 (45 degrees).
 *
 * To avoid drawing pixels twice when using the symmetry, we further handle
 * the first pixel (dx=0, dy=r) and the last pixel (dx=r, dy=0) separately.
 *
 * Remark: this algorithm computes an optimal approximation to a circle, i.e.
 * the result is also symmetric to the angle bisector. */
void lcd_circle(const wininfo_t *pwi, XYPOS x, XYPOS y, XYPOS r)
{
	XYPOS dx = 0;
	XYPOS dy = r;
	XYPOS dd = 1-r;

	if (r < 0)
		return;

	if (r == 0) {
		lcd_pixel(pwi, x, y);
		return;
	}

	/* Draw first two pixels with dx == 0 */
	lcd_pixel(pwi, x, y - dy);
	lcd_pixel(pwi, x, y + dy);
	if (dd < 0)
		dd += 3;		  /* 2*dx + 3, but dx is 0 */
	else				  /* Only possible for r==1 */
		dy--;			  /* dd does not matter, dy is 0 */
	dx++;

	/* Draw part with low slope (every step changes dx, sometimes dy) */
	while (dy > dx) {
		lcd_pixel(pwi, x + dx, y - dy);
		lcd_pixel(pwi, x + dx, y + dy);
		if (dx) {
			lcd_pixel(pwi, x - dx, y - dy);
			lcd_pixel(pwi, x - dx, y + dy);
		}
		if ((dd < 0) && (dy > dx + 1))
			dd += 2*dx + 3;	       /* E */
		else {
			dd += (dx - dy)*2 + 5; /* SE */
			dy--;
		}
		dx++;
	}

	/* Switch to high slope */
	dd = dd - dx - dy;

	/* Draw part with high slope (every step changes dym sometimes dx) */
	while (dy) {
		lcd_pixel(pwi, x + dx, y - dy);
		lcd_pixel(pwi, x - dx, y - dy);
		lcd_pixel(pwi, x + dx, y + dy);
		lcd_pixel(pwi, x - dx, y + dy);

		if (dd < 0) {
			dd += (dx - dy)*2 + 5; /* SE */
			dx++;
		} else
			dd += 3 - dy*2;	       /* S */
		dy--;
	}

	/* Draw final pixels with dy == 0 */
	lcd_pixel(pwi, x + dx, y);
	lcd_pixel(pwi, x - dx, y);
}


/* Draw filled circle at (x, y) with radius r and color. The algorithm is the
   same as explained above at lcd_circle(), however we can skip some tests as
   we always draw a full line from the left to the right of the circle. */
void lcd_disc(const wininfo_t *pwi, XYPOS x, XYPOS y, XYPOS r)
{
	XYPOS dx = 0;
	XYPOS dy = r;
	XYPOS dd = 1-r;

	if (r < 0)
		return;

	/* Draw part with low slope (every step changes dx, sometimes dy) */
	while (dy > dx) {
		lcd_rect(pwi, x - dx, y - dy, x + dx, y - dy);
		lcd_rect(pwi, x - dx, y + dy, x + dx, y + dy);
		if ((dd < 0) && (dy > dx + 1))
			dd += 2*dx + 3;	       /* E */
		else {
			dd += (dx - dy)*2 + 5; /* SE */
			dy--;
		}
		dx++;
	}

	/* Switch to high slope */
	dd = dd - dx - dy;

	/* Draw part with high slope (every step changes dym sometimes dx) */
	while (dy > 0) {
		lcd_rect(pwi, x - dx, y - dy, x + dx, y - dy);
		lcd_rect(pwi, x - dx, y + dy, x + dx, y + dy);
		if (dd < 0) {
			dd += (dx - dy)*2 + 5; /* SE */
			dx++;
		} else
			dd += 3 - dy*2;	       /* S */
		dy--;
	}

	/* Draw final line with dy == 0 */
	lcd_rect(pwi, x - dx, y, x + dx, y);
}


/* Draw text string s at (x, y) with alignment/attribute a and colors fg/bg
   the attributes are as follows:
     Bit 1..0: horizontal alignment:  00: left, 01: right,
	       10: hcenter, 11: screen hcenter (x ignored)
     Bit 3..2: vertical alignment: 00: top, 01: bottom,
	       10: vcenter, 11: screen vcenter (y ignored)
     Bit 4: 0: normal, 1: double width
     Bit 5: 0: normal, 1: double height
     Bit 6: 0: FG+BG, 1: no BG (transparent, bg ignored)
     Bit 7: reserved (blinking?)
     Bit 8: 0: normal, 1: bold
     Bit 9: 0: normal, 1: inverse
     Bit 10: 0: normal, 1: underline
     Bit 11: 0: normal, 1: strike-through

   We only draw fully visible characters. If a character would be fully or
   partly outside of the framebuffer, it is not drawn at all. If you need
   partly visible characters, use a larger framebuffer and show only the part
   with the partly visible characters in a window. */
void lcd_text(const wininfo_t *pwi, XYPOS x, XYPOS y, char *s)
{
	XYPOS len = (XYPOS)strlen(s);
	XYPOS width = VIDEO_FONT_WIDTH;
	XYPOS height = VIDEO_FONT_HEIGHT;
	XYPOS fbhres = (XYPOS)pwi->fbhres;
	XYPOS fbvres = (XYPOS)pwi->fbvres;
	u_int attr = pwi->attr;

	/* Return if string is empty */
	if (s == 0)
		return;

	if (attr & ATTR_DWIDTH)
		width *= 2;		  /* Double width */
	if (attr & ATTR_DHEIGHT)
		height *= 2;		  /* Double height */

	/* Compute y from vertical alignment */
	switch (attr & ATTR_VMASK) {
	case ATTR_VTOP:
		break;

	case ATTR_VBOTTOM:
		y -= height-1;
		break;

	case ATTR_VSCREEN:
		y = fbvres/2;
		/* Fall through to case ATTR_VCENTER */

	case ATTR_VCENTER:
		y -= height/2;
		break;
	}

	/* Return if text is completely or partly above or below framebuffer */
	if ((y < 0) || (y + height > fbvres))
		return;

	/* Compute x from horizontal alignment */
	switch (attr & ATTR_HMASK) {
	case ATTR_HLEFT:
		break;

	case ATTR_HRIGHT:
		x -= len*width - 1;
		break;

	case ATTR_HSCREEN:
		x = (XYPOS)pwi->fbhres/2;
		/* Fall through to case ATTR_HCENTER */

	case ATTR_HCENTER:
		x -= len*width/2;
		break;

	}

	/* Return if text is completely right of framebuffer or if only the
	   first character would be partly inside of the framebuffer */
	if (x + width > fbhres)
		return;

	if (x < 0) {
		/* Compute number of characters left of framebuffer */
		unsigned offs = (-x - 1)/width + 1;

		/* Return if string would be completeley left of framebuffer */
		if (offs >= len)
			return;

		/* Increase x and string position */
		s += offs;
		x += offs*width;
	}

	/* At least one character is within the framebuffer */
	for (;;) {
		char c = *s++;

		/* Stop on end of string or if character would not fit into
		   framebuffer anymore */
		if (!c || (x + width > fbhres))
			break;

		/* Output character and move position */
		if (attr & ATTR_ALPHA)
			adraw_ll_char(pwi, x, y, c);
		else
			draw_ll_char(pwi, x, y, c, pwi->fg.col, pwi->bg.col);
		x += width;
	}
}


/* Draw test pattern */
void lcd_test(const wininfo_t *pwi, u_int pattern)
{
	switch (pattern) {
	default:			  /* Test pattern */
		/* grid, circle, basic colors */
		test_pattern0(pwi);
		break;

	case 1:			/* Color gradient 1 */
		/* Eight colors in two rows a four */
		test_pattern1(pwi);
		break;

	case 2:			/* Color gradient 2 */
		/* Horizontal: colors, vertical: brightness */
		test_pattern2(pwi);
		break;

	case 3:		       /* Color gradient 3 */
		/* Colors along screen edges, gray in center */
		test_pattern3(pwi);
		break;
	}
}


/************************************************************************/
/* OTHER EXPORTED GRAPHICS FUNCTIONS					*/
/************************************************************************/

/* Set colinfo structure */
static void lcd_set_col(wininfo_t *pwi, RGBA rgba, colinfo_t *pci)
{
	RGBA alpha1;

	/* Store RGBA value */
	pci->rgba = rgba;

	/* Premultiply alpha for apply_alpha functions */
	alpha1 = rgba & 0x000000FF;
	pci->A256 = 256 - alpha1;
	alpha1++;
	pci->RA1 = (rgba >> 24) * alpha1;
	pci->GA1 = ((rgba >> 16) & 0xFF) * alpha1;
	pci->BA1 = ((rgba >> 8) & 0xFF) * alpha1;

	/* Store COLOR32 value */
	pci->col = pwi->ppi->rgba2col(pwi, rgba);
}


/* Set the FG color */
void lcd_set_fg(wininfo_t *pwi, RGBA rgba)
{
	lcd_set_col(pwi, rgba, &pwi->fg);
}


/* Set the BG color */
void lcd_set_bg(wininfo_t *pwi, RGBA rgba)
{
	lcd_set_col(pwi, rgba, &pwi->bg);
}


/* Search for nearest color in the color map */
COLOR32 lcd_rgbalookup(RGBA rgba, RGBA *cmap, unsigned count)
{
	unsigned nearest = 0;
	u_char r, g, b, a;
	unsigned i;

	signed mindist = 256*256*4;

	r = (u_char)(rgba >> 24);
	g = (u_char)(rgba >> 16);
	b = (u_char)(rgba >> 8);
	a = (u_char)rgba;

	i = count;
	do {
		short d;
		signed dist;

		rgba = cmap[--i];
		d = (u_char)(rgba >> 24) - r;
		dist = d*d;
		d = (u_char)(rgba >> 16) - g;
		dist += d*d;
		d = (u_char)(rgba >> 8) - b;
		dist += d*d;
		d = (u_char)rgba - a;
		dist += d*d;
		if (dist == 0)
			return (COLOR32)i;	  /* Exact match */
		if (dist < mindist) {
			mindist = dist;
			nearest = i;
		}
	} while (i);

	return (COLOR32)nearest;
}


/************************************************************************/
/* CONSOLE SUPPORT							*/
/************************************************************************/

#ifndef CONFIG_MULTIPLE_CONSOLES
/* Initialize the console with the given window */
void console_init(wininfo_t *pwi, RGBA fg, RGBA bg)
{
	/* Initialize the console */
	console_pwi = pwi;
	coninfo.x = 0;
	coninfo.y = 0;
	coninfo.fg = pwi->ppi->rgba2col(pwi, fg);
	coninfo.bg = pwi->ppi->rgba2col(pwi, bg);
}


/* If the window is the console window, re-initialize console */
void console_update(wininfo_t *pwi, RGBA fg, RGBA bg)
{
	if (console_pwi->win == pwi->win)
		console_init(pwi, fg, bg);
}
#endif /* CONFIG_MULTIPLE_CONSOLES */


#define TABWIDTH (8 * VIDEO_FONT_WIDTH)	  /* 8 chars for tab */
static void console_putc(wininfo_t *pwi, coninfo_t *pci, char c)
{
	int x = pci->x;
	int y = pci->y;
	int xtab;
	COLOR32 fg = pci->fg;
	COLOR32 bg = pci->bg;
	int fbhres = pwi->fbhres;
	int fbvres = pwi->fbvres;

	pwi->attr = 0;
	switch (c) {
	case '\t':			  /* Tab */
		xtab = ((x / TABWIDTH) + 1) * TABWIDTH;
		while ((x + VIDEO_FONT_WIDTH <= fbhres) && (x < xtab)) {
			draw_ll_char(pwi, x, y, ' ', fg, bg);
			x += VIDEO_FONT_WIDTH;
		};
		goto CHECKNEWLINE;

	case '\b':			  /* Backspace */
		if (x >= VIDEO_FONT_WIDTH)
			x -= VIDEO_FONT_WIDTH;
		else if (y >= VIDEO_FONT_HEIGHT) {
			y -= VIDEO_FONT_HEIGHT;
			x = (fbhres/VIDEO_FONT_WIDTH-1) * VIDEO_FONT_WIDTH;
		}
		draw_ll_char(pwi, x, y, ' ', fg, bg);
		break;

	default:			  /* Character */
		draw_ll_char(pwi, x, y, c, fg, bg);
		x += VIDEO_FONT_WIDTH;
	CHECKNEWLINE:
		/* Check if there is room on the row for another character */
		if (x + VIDEO_FONT_WIDTH <= fbhres)
			break;
		/* No: fall through to case '\n' */

	case '\n':			  /* Newline */
		if (y + 2*VIDEO_FONT_HEIGHT <= fbvres)
			y += VIDEO_FONT_HEIGHT;
		else {
			u_long fbuf = pwi->pfbuf[pwi->fbdraw];
			u_long linelen = pwi->linelen;

			/* Scroll everything up */
			memcpy((void *)fbuf,
			       (void *)fbuf + linelen * VIDEO_FONT_HEIGHT,
			       (fbvres - VIDEO_FONT_HEIGHT) * linelen);

			/* Clear bottom line to end of screen with console
			   background color */
			memset32((unsigned *)(fbuf + y*linelen), bg,
				 (fbvres - y)*linelen/4);
		}
		/* Fall through to case '\r' */

	case '\r':			  /* Carriage return */
		x = 0;
		break;
	}

	pci->x = (u_short)x;
	pci->y = (u_short)y;
}


/*----------------------------------------------------------------------*/

#ifdef CONFIG_MULTIPLE_CONSOLES
void lcd_putc(const device_t *pdev, const char c)
{
	wininfo_t *pwi = (wininfo_t *)pdev->priv;
	vidinfo_t *pvi = pwi->pvi;

	if (pvi->is_enabled && pwi->active)
		console_putc(pwi, &pwi->ci, c);
	else
		serial_putc(NULL, c);
}
#else
void lcd_putc(const device_t *pdev, const char c)
{
	wininfo_t *pwi = console_pwi;
	vidinfo_t *pvi = pwi->pvi;

	if (pvi->is_enabled && pwi->active)
		console_putc(pwi, &coninfo, c);
	else
		serial_putc(NULL, c);
}
#endif /*CONFIG_MULTIPLE_CONSOLES*/

/*----------------------------------------------------------------------*/

#ifdef CONFIG_MULTIPLE_CONSOLES
void lcd_puts(const device_t *pdev, const char *s)
{
	wininfo_t *pwi = (wininfo_t *)pdev->priv;
	vidinfo_t *pvi = pwi->pvi;

	if (pvi->is_enabled && pwi->active) {
		coninfo_t *pci = &pwi->ci;
		for (;;)
		{
			char c = *s++;

			if (!c)
				break;
			console_putc(pwi, pci, c);
		}
	} else
		serial_puts(NULL, s);
}
#else
void lcd_puts(const device_t *pdev, const char *s)
{
	wininfo_t *pwi = console_pwi;
	vidinfo_t *pvi = pwi->pvi;

	if (pvi->is_enabled && pwi->active) {
		coninfo_t *pci = &coninfo;
		for (;;)
		{
			char c = *s++;

			if (!c)
				break;
			console_putc(pwi, pci, c);
		}
	} else
		serial_puts(NULL, s);
}
#endif /*CONFIG_MULTIPLE_CONSOLES*/
