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

struct testinfo {
	XYPOS minhres;
	XYPOS minvres;
	void (*draw_ll_pattern)(const wininfo_t *pwi,
				XYPOS x1, XYPOS y1, XYPOS x2, XYPOS y2);
};


/************************************************************************/
/* LOCAL VARIABLES							*/
/************************************************************************/



/************************************************************************/
/* PROTOTYPES OF LOCAL FUNCTIONS					*/
/************************************************************************/

/* Repeat color value so that it fills the whole 32 bits */
static COLOR32 col2col32(const wininfo_t *pwi, COLOR32 color);

/* Draw pixel by replacing with new color; pixel is definitely valid */
static void draw_ll_pixel(const wininfo_t *pwi, XYPOS x, XYPOS y, COLOR32 col);

/* Draw pixel by applying alpha of new pixel; pixel is definitely valid */
static void adraw_ll_pixel(const wininfo_t *pwi, XYPOS x, XYPOS y,
			   const colinfo_t *pci);

/* Draw filled rectangle, replacing pixels with new color; given region is
   definitely valid and x and y are sorted (x1 <= x2, y1 <= y2) */
static void draw_ll_rect(const wininfo_t *pwi, XYPOS x1, XYPOS y1,
			 XYPOS x2, XYPOS y2, COLOR32 col);

/* Draw filled rectangle, applying alpha; given region is definitely valid and
   x and y are sorted (x1 <= x2, y1 <= y2) */
static void adraw_ll_rect(const wininfo_t *pwi, XYPOS x1, XYPOS y1,
			  XYPOS x2, XYPOS y2, const colinfo_t *pci);

/* Draw a character, replacing pixels with new color; character area is
   definitely valid */
static void draw_ll_char(const wininfo_t *pwi, XYPOS x, XYPOS y, char c,
			 COLOR32 fg, COLOR32 bg);

/* Draw a character, applyig alpha; character area is definitely valid */
static void adraw_ll_char(const wininfo_t *pwi, XYPOS x, XYPOS y, char c,
			  const colinfo_t *pci_fg, const colinfo_t *pci_bg);

/* Draw test pattern with grid, basic colors, color gradients and circles */
static void lcd_ll_pattern0(const wininfo_t *pwi,
			    XYPOS x1, XYPOS y1, XYPOS x2, XYPOS y2);

/* Draw the eight basic colors in two rows of four */
static void lcd_ll_pattern1(const wininfo_t *pwi,
			    XYPOS x1, XYPOS y1, XYPOS x2, XYPOS y2);

/* Draw color gradient, horizontal: hue, vertical: brightness */
static void lcd_ll_pattern2(const wininfo_t *pwi,
			    XYPOS x1, XYPOS y1, XYPOS x2, XYPOS y2);

/* Draw color gradient: 8 basic colors along edges, gray in the center */
static void lcd_ll_pattern3(const wininfo_t *pwi,
			    XYPOS x1, XYPOS y1, XYPOS x2, XYPOS y2);

/* Draw character to console */
static void console_putc(wininfo_t *pwi, coninfo_t *pci, char c);


/************************************************************************/
/* HELPER FUNCTIONS FOR GRAPHIC PRIMITIVES				*/
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


/************************************************************************/
/* ** Low-Level Graphics Routines					*/
/************************************************************************/

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
static void adraw_ll_pixel(const wininfo_t *pwi, XYPOS x, XYPOS y,
			   const colinfo_t *pci)
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
	col = pwi->ppi->apply_alpha(pwi, pci, val >> shift);
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
			  XYPOS x2, XYPOS y2, const colinfo_t *pci)
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
			col = pwi->ppi->apply_alpha(pwi, pci, val >> s);
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


/* Draw a character, applyig alpha; character area is definitely valid */
static void adraw_ll_char(const wininfo_t *pwi, XYPOS x, XYPOS y, char c,
			  const colinfo_t *pci_fg, const colinfo_t *pci_bg)
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
					COLOR32 col;

					/* Blend FG or BG pixel (BG only if not
					   transparent) */
					s -= bpp;
					if (fd & fm)
						col = pwi->ppi->apply_alpha(
							pwi, pci_fg, val >> s);
					else if (!(attr & ATTR_NO_BG))
						col = pwi->ppi->apply_alpha(
							pwi, pci_bg, val >> s);
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


/* Draw test pattern with grid, basic colors, color gradients and circles */
static void lcd_ll_pattern0(const wininfo_t *pwi,
			    XYPOS x1, XYPOS y1, XYPOS x2, XYPOS y2)
{
	const pixinfo_t *ppi = pwi->ppi;
	XYPOS dx, dy;
	XYPOS x, y;
	XYPOS i;
	XYPOS hleft, vtop, hright, vbottom;
	XYPOS r1, r2, scale;
	XYPOS hres, vres;
	COLOR32 col;
	colinfo_t ci;

	static const RGBA const coltab[] = {
		0xFF0000FF,		  /* R */
		0x00FF00FF,		  /* G */
		0x0000FFFF,		  /* B */
		0xFFFFFFFF,		  /* W */
		0xFFFF00FF,		  /* Y */
		0xFF00FFFF,		  /* M */
		0x00FFFFFF,		  /* C */
	};

	/* Use hres divided by 12 and vres divided by 8 as grid size */
	hres = x2-x1+1;
	vres = y2-y1+1;
	dx = hres/12;
	dy = vres/8;

	/* Compute left and top margin for first line as half of the remaining
	   space (that was not multiple of 12 or 8 respectively) and half of
	   a grid rectangle size */
	hleft = (dx + hres % 12)/2 + x1;
	vtop = (dy + vres % 8)/2 + y1;

	/* Compute right and bottom margin for last line in a similar way */
	hright = hleft + (12-1)*dx;
	vbottom = vtop + (8-1)*dy;

	/* Draw lines and circles in white; the circle command needs a colinfo
	   structure for the color; however we know that ATTR_ALPHA is cleared
	   so it is enough to set the col entry of this structure. */
	col = ppi->rgba2col(pwi, 0xFFFFFFFF);  /* White */
	ci.col = col;

	/* Draw vertical lines of grid */
	for (x = hleft; x <= hright; x += dx)
		draw_ll_rect(pwi, x, y1, x, y2, col);

	/* Draw horizontal lines of grid */
	for (y = vtop; y <= vbottom; y += dy)
		draw_ll_rect(pwi, x1, y, x2, y, col);

	/* Draw 7 of the 8 basic colors (without black) as rectangles */
	x = 2*dx + hleft + 1;
	for (i=0; i<7; i++) {
		draw_ll_rect(pwi, x, vbottom-2*dy+1, x+dx-2, vbottom-1,
			     ppi->rgba2col(pwi, coltab[6-i]));
		draw_ll_rect(pwi, x, vtop+1, x+dx-2, vtop+2*dy-1,
			     ppi->rgba2col(pwi, coltab[i]));
		x += dx;
	}

	/* Draw grayscale gradient on left, R, G, B gradient on right side */
	scale = vbottom-vtop-2;
	y = vtop+1;
	for (i=0; i<=scale; i++) {
		RGBA rgba;

		rgba = (i*255/scale) << 8;
		rgba |= (rgba << 8) | (rgba << 16) | 0xFF;
		draw_ll_rect(pwi, hleft+1, y, hleft+dx-1, y,
			     ppi->rgba2col(pwi, rgba));
		draw_ll_rect(pwi, hright-dx+1, y, hright-2*dx/3, y,
			     ppi->rgba2col(pwi, rgba & 0xFF0000FF));
		draw_ll_rect(pwi, hright-2*dx/3+1, y, hright-dx/3, y,
			     ppi->rgba2col(pwi, rgba & 0x00FF00FF));
		draw_ll_rect(pwi, hright-dx/3+1, y, hright-1, y,
			     ppi->rgba2col(pwi, rgba & 0x0000FFFF));
		y++;
	}

	/* Draw big and small circle; make sure that circle fits on screen */
	if (hres > vres) {
		r1 = vres-1;
		r2 = dy;
	} else {
		r1 = hres-1;
		r2 = dx;
	}
	r1 = r1/2;
	x = hres/2 + x1;
	y = vres/2 + y1;

	/* Draw two circles */
	lcd_rframe(pwi, x-r1, y-r1, x+r1-1, y+r1-1, r1, &ci);
	lcd_rframe(pwi, x-r2, y-r2, x+r2-1, y+r2-1, r2, &ci);

	/* Draw corners; the window is min. 24x16, so +/-7 will always fit */
	col = ppi->rgba2col(pwi, 0x00FF00FF);  /* Green */
	draw_ll_rect(pwi, x1, y1, x1+7, y1, col); /* top left */
	draw_ll_rect(pwi, x1, y1, x1, y1+7, col);
	draw_ll_rect(pwi, x2-7, y1, x2, y1, col); /* top right */
	draw_ll_rect(pwi, x2, y1, x2, y1+7, col);
	draw_ll_rect(pwi, x1, y2-7, x1, y2, col); /* bottom left */
	draw_ll_rect(pwi, x1, y2, x1+7, y2, col);
	draw_ll_rect(pwi, x2, y2-7, x2, y2, col); /* bottom right */
	draw_ll_rect(pwi, x2-7, y2, x2, y2, col);
}


/* Draw the eight basic colors in two rows of four */
static void lcd_ll_pattern1(const wininfo_t *pwi,
			    XYPOS x1, XYPOS y1, XYPOS x2, XYPOS y2)
{
	const pixinfo_t *ppi = pwi->ppi;
	XYPOS xres_1_4, xres_2_4, xres_3_4;
	XYPOS yres_1_2 = (y1+y2)/2;

	/* Draw red and cyan rectangles in first column */
	xres_1_4 = (3*x1 + x2)/4;	  /* 1/4 hres */
	draw_ll_rect(pwi, x1, y1, xres_1_4, yres_1_2,
		     ppi->rgba2col(pwi, 0xFF0000FF)); /* Red */
	draw_ll_rect(pwi, x1, yres_1_2 + 1, xres_1_4, y2,
		     ppi->rgba2col(pwi, 0x00FFFFFF)); /* Cyan */

	/* Draw green and magenta rectangles in second column */
	xres_1_4++;
	xres_2_4 = (x1 + x2)/2;		  /* 2/4 hres */
	draw_ll_rect(pwi, xres_1_4, y1, xres_2_4, yres_1_2,
		     ppi->rgba2col(pwi, 0x00FF00FF)); /* Green */
	draw_ll_rect(pwi, xres_1_4, yres_1_2 + 1, xres_2_4, y2,
		     ppi->rgba2col(pwi, 0xFF00FFFF)); /* Magenta */

	/* Draw blue and yellow rectangles in third column */
	xres_2_4++;
	xres_3_4 = (x1 + 3*x2)/4;	  /* 3/4 hres */
	draw_ll_rect(pwi, xres_2_4, y1, xres_3_4, yres_1_2,
		     ppi->rgba2col(pwi, 0x0000FFFF)); /* Blue */
	draw_ll_rect(pwi, xres_2_4, yres_1_2 + 1, xres_3_4, y2,
		     ppi->rgba2col(pwi, 0xFFFF00FF)); /* Yellow */

	/* Draw black and white rectangles in fourth column */
	xres_3_4++;
#if 0	/* Drawing black not necessary, window was already cleared black */
	draw_ll_rect(pwi, xres_3_4, y1, x2, yres_1_2,
		     ppi->rgba2col(pwi, 0x000000FF)); /* Black */
#endif
	draw_ll_rect(pwi, xres_3_4, yres_1_2 + 1, x2, y2,
		     ppi->rgba2col(pwi, 0xFFFFFFFF)); /* White */
}


/* Draw color gradient, horizontal: hue, vertical: brightness */
static void lcd_ll_pattern2(const wininfo_t *pwi,
			    XYPOS x1, XYPOS y1, XYPOS x2, XYPOS y2)
{
	const pixinfo_t *ppi = pwi->ppi;
	int xres = x2 - x1;
	int yres_1_2 = (y2 - y1)/2 + 1 + y1;
	int xfrom = x1;
	int hue = 0;
	static const struct iRGB const target[] = {
		{0xFF, 0x00, 0x00},	  /* R */
		{0xFF, 0xFF, 0x00},	  /* Y */
		{0x00, 0xFF, 0x00},	  /* G */
		{0x00, 0xFF, 0xFF},	  /* C */
		{0x00, 0x00, 0xFF},	  /* B */
		{0xFF, 0x00, 0xFF},	  /* M */
		{0xFF, 0x00, 0x00}	  /* R */
	};

	do {
		struct iRGB from = target[hue++];
		struct iRGB to = target[hue];
		int xto = hue * xres / 6 + 1 + x1;
		int dx = xto - xfrom;
		int x;

		for (x = xfrom; x < xto; x++) {
			int sx = x - xfrom;
			int dy, y;
			struct iRGB temp;
			RGBA rgba;

			temp.R = (to.R - from.R)*sx/dx + from.R;
			temp.G = (to.G - from.G)*sx/dx + from.G;
			temp.B = (to.B - from.B)*sx/dx + from.B;

			dy = yres_1_2 - y1;
			for (y = y1; y < yres_1_2; y++) {
				int sy = y - y1;

				rgba = (temp.R * sy/dy) << 24;
				rgba |= (temp.G * sy/dy) << 16;
				rgba |= (temp.B * sy/dy) << 8;
				rgba |= 0xFF;
				draw_ll_pixel(pwi, (XYPOS)x, (XYPOS)y,
					      ppi->rgba2col(pwi, rgba));
			}

			dy = y2 - yres_1_2;
			for (y = yres_1_2; y <= y2; y++) {
				int sy = y - yres_1_2;

				rgba = ((0xFF-temp.R)*sy/dy + temp.R) << 24;
				rgba |= ((0xFF-temp.G)*sy/dy + temp.G) << 16;
				rgba |= ((0xFF-temp.B)*sy/dy + temp.B) << 8;
				rgba |= 0xFF;
				draw_ll_pixel(pwi, (XYPOS)x, (XYPOS)y,
					      ppi->rgba2col(pwi, rgba));
			}
		}
		xfrom = xto;
	} while (hue < 6);
}


/* Draw color gradient: 8 basic colors along edges, gray in the center */
static void lcd_ll_pattern3(const wininfo_t *pwi,
			    XYPOS x1, XYPOS y1, XYPOS x2, XYPOS y2)
{
	const pixinfo_t *ppi = pwi->ppi;
	int xres_1_2 = (x2 - x1)/2 + 1 + x1;
	int yres_1_2 = (y2 - y1)/2 + 1 + y1;
	int y;

	struct iRGB const tl = {0x00, 0x00, 0x00}; /* Top left: Black */
	struct iRGB const tm = {0x00, 0x00, 0xFF}; /* Top middle: Blue */
	struct iRGB const tr = {0x00, 0xFF, 0xFF}; /* Top right: Cyan */
	struct iRGB const ml = {0xFF, 0x00, 0x00}; /* Middle left: Red */
	struct iRGB const mm = {0x80, 0x80, 0x80}; /* Middle middle: Gray */
	struct iRGB const mr = {0x00, 0xFF, 0x00}; /* Middle right: Green */
	struct iRGB const bl = {0xFF, 0x00, 0xFF}; /* Bottom left: Magenta */
	struct iRGB const bm = {0xFF, 0xFF, 0xFF}; /* Bottom middle: White */
	struct iRGB const br = {0xFF, 0xFF, 0x00}; /* Bottom right: Yellow */

	for (y = y1; y <= y2; y++) {
		struct iRGB l, m, r;
		int x, dx;
		int sy, dy;
		RGBA rgb;

		/* Compute left, middle and right colors for next row */
		if (y < yres_1_2) {
			sy = y - y1;
			dy = yres_1_2 - y1;

			l.R = (ml.R - tl.R)*sy/dy + tl.R;
			l.G = (ml.G - tl.G)*sy/dy + tl.G;
			l.B = (ml.B - tl.B)*sy/dy + tl.B;

			m.R = (mm.R - tm.R)*sy/dy + tm.R;
			m.G = (mm.G - tm.G)*sy/dy + tm.G;
			m.B = (mm.B - tm.B)*sy/dy + tm.B;

			r.R = (mr.R - tr.R)*sy/dy + tr.R;
			r.G = (mr.G - tr.G)*sy/dy + tr.G;
			r.B = (mr.B - tr.B)*sy/dy + tr.B;
		} else {
			sy = y - yres_1_2;
			dy = y2 - yres_1_2;

			l.R = (bl.R - ml.R)*sy/dy + ml.R;
			l.G = (bl.G - ml.G)*sy/dy + ml.G;
			l.B = (bl.B - ml.B)*sy/dy + ml.B;

			m.R = (bm.R - mm.R)*sy/dy + mm.R;
			m.G = (bm.G - mm.G)*sy/dy + mm.G;
			m.B = (bm.B - mm.B)*sy/dy + mm.B;

			r.R = (br.R - mr.R)*sy/dy + mr.R;
			r.G = (br.G - mr.G)*sy/dy + mr.G;
			r.B = (br.B - mr.B)*sy/dy + mr.B;
		}

		/* Draw left half of row */
		dx = xres_1_2 - x1;
		for (x = x1; x < xres_1_2; x++) {
			int sx = x - x1;

			rgb = ((m.R - l.R)*sx/dx + l.R) << 24;
			rgb |= ((m.G - l.G)*sx/dx + l.G) << 16;
			rgb |= ((m.B - l.B)*sx/dx + l.B) << 8;
			rgb |= 0xFF;

			draw_ll_pixel(pwi, (XYPOS)x, (XYPOS)y,
				      ppi->rgba2col(pwi, rgb));
		}

		/* Draw right half of row */
		dx = x2 - xres_1_2;
		for (x = xres_1_2; x <= x2; x++) {
			int sx = x - xres_1_2;

			rgb = ((r.R - m.R)*sx/dx + m.R) << 24;
			rgb |= ((r.G - m.G)*sx/dx + m.G) << 16;
			rgb |= ((r.B - m.B)*sx/dx + m.B) << 8;
			rgb |= 0xFF;

			draw_ll_pixel(pwi, (XYPOS)x, (XYPOS)y,
				      ppi->rgba2col(pwi, rgb));
		}
	}
}


/************************************************************************/
/* GRAPHICS PRIMITIVES							*/
/************************************************************************/

/* Fill clipping region with given color */
void lcd_fill(const wininfo_t *pwi, const colinfo_t *pci)
{
	XYPOS x1, y1, x2, y2;

	/* Move from clipping region coordinates to absolute coordinates */
	x1 = pwi->clip_left;
	y1 = pwi->clip_top;
	x2 = pwi->clip_right;
	y2 = pwi->clip_bottom;

	if (pwi->attr & ATTR_ALPHA)
		adraw_ll_rect(pwi, x1, y1, x2, y2, pci);
	else
		draw_ll_rect(pwi, x1, y1, x2, y2, pci->col);
}

/* Draw pixel at (x, y) with given color */
void lcd_pixel(const wininfo_t *pwi, XYPOS x, XYPOS y, const colinfo_t *pci)
{
	if ((x < pwi->clip_left) || (x > pwi->clip_right)
	    || (y < pwi->clip_top) || (y > pwi->clip_bottom))
		return;

	if (pwi->attr & ATTR_ALPHA)
		adraw_ll_pixel(pwi, x, y, pci);
	else
		draw_ll_pixel(pwi, x, y, pci->col);
}


/* Draw line from (x1, y1) to (x2, y2) in color */
void lcd_line(const wininfo_t *pwi, XYPOS x1, XYPOS y1, XYPOS x2, XYPOS y2,
	      const colinfo_t *pci)
{
	int dx, dy, dd;
	XYPOS xmin, ymin, xmax, ymax;
	XYPOS xoffs, yoffs;
	XYPOS xinc, yinc;

	dx = (int)x2 - (int)x1;
	if (dx < 0)
		dx = -dx;
	dy = (int)y2 - (int)y1;
	if (dy < 0)
		dy = -dy;

	xmin = pwi->clip_left;
	ymin = pwi->clip_top;
	xmax = pwi->clip_right;
	ymax = pwi->clip_bottom;

	if (dy > dx) {			  /* High slope */
		/* Sort pixels so that y1 <= y2 */
		if (y1 > y2) {
			XYPOS temp;

			temp = x1;
			x1 = x2;
			x2 = temp;
			temp = y1;
			y1 = y2;
			y2 = temp;
		}

		/* Return if line is completely above or below the display */
		if ((y2 < ymin) || (y1 > ymax))
			return;

		dd = dy;
		dx <<= 1;
		dy <<= 1;

		xinc = (x1 > x2) ? -1 : 1;
		if (y1 < ymin) {
			/* Clip with upper screen edge */
			yoffs = ymin - y1;
			xoffs = (dd + (int)yoffs * dx)/dy;
			dd += xoffs*dy - yoffs*dx;
			y1 = ymin;
			x1 += xinc*xoffs;
		}

		/* Return if line fragment is fully left or right of display */
		if (((x1 < xmin) && (x2 < xmin))
		    || ((x1 > xmax) && (x2 > xmax)))
			return;

		/* We only need y2 as end coordinate */
		if (y2 > ymax)
			y2 = ymax;

		/* If line is vertical, we can use the more efficient
		   rectangle function */
		if (dx == 0) {
			lcd_rect(pwi, x1, y1, x2, y2, pci);
			return;
		}

		/* Draw line from top to bottom, i.e. every loop cycle go one
		   pixel down and sometimes one pixel left or right */
		for (;;) {
			lcd_pixel(pwi, x1, y1, pci);
			if (y1 == y2)
				break;
			y1++;
			dd += dx;
			if (dd >= dy) {
				dd -= dy;
				x1 += xinc;
			}
		}
	} else {			  /* Low slope */
		/* Sort pixels so that x1 <= x2 */
		if (x1 > x2) {
			XYPOS temp;

			temp = x1;
			x1 = x2;
			x2 = temp;
			temp = y1;
			y1 = y2;
			y2 = temp;
		}

		/* Return if line is completely left or right of the display */
		if ((x2 < xmin) || (x1 > xmax))
			return;

		dd = dx;
		dx <<= 1;
		dy <<= 1;

		yinc = (y1 > y2) ? -1 : 1;
		if (x1 < xmin) {
			/* Clip with left screen edge */
			xoffs = xmin - x1;
			yoffs = (dd + (int)xoffs * dy)/dx;
			dd += yoffs*dx - xoffs*dy;
			x1 = xmin;
			y1 += yinc*yoffs;
		}

		/* Return if line fragment is fully above or below display */
		if (((y1 < xmin) && (y2 < xmin))
		    || ((y1 > ymax) && (y2 > ymax)))
			return;

		/* We only need x2 as end coordinate */
		if (x2 > xmax)
			x2 = xmax;

		/* If line is horizontal, we can use the more efficient
		   rectangle function */
		if (dy == 0) {
			/* Draw horizontal line */
			lcd_rect(pwi, x1, y1, x2, y2, pci);
			return;
		}

		/* Draw line from left to right, i.e. every loop cycle go one
		   pixel right and sometimes one pixel up or down */
		for (;;) {
			lcd_pixel(pwi, x1, y1, pci);
			if (x1 == x2)
				break;
			x1++;
			dd += dy;
			if (dd >= dx) {
				dd -= dx;
				y1 += yinc;
			}
		}
	}
}

/* Draw rectangular frame from (x1, y1) to (x2, y2) in given color; x1<=x2 and
   y1<=y2 must be valid! */
void lcd_frame(const wininfo_t *pwi, XYPOS x1, XYPOS y1, XYPOS x2, XYPOS y2,
	       const colinfo_t *pci)
{
	/* If the frame is wider than two pixels, we need to draw
	   horizontal lines at the top and bottom; clipping is done in
	   lcd_rect() so we don't care about clipping here. */
	if (x2 - x1 > 1) {
		/* Draw top line */
		lcd_rect(pwi, x1, y1, x2, y1, pci);

		/* We are done if rectangle is exactly one pixel high */
		if (y1 == y2)
			return;

		/* Draw bottom line */
		lcd_rect(pwi, x1, y2, x2, y2, pci);

		/* For the vertical lines we only need to draw the region
		   between the horizontal lines, so increment y1 and decrement
		   y2; if rectangle is exactly two pixels high, we don't
		   need to draw any vertical lines at all. */
		if (++y1 == y2--)
			return;
	}

	/* Draw left line */
	lcd_rect(pwi, x1, y1, x1, y2, pci);

	/* Return if rectangle is exactly one pixel wide */
	if (x1 == x2)
		return;

	/* Draw right line */
	lcd_rect(pwi, x2, y1, x2, y2, pci);
}


/* Draw filled rectangle from (x1, y1) to (x2, y2) in color; x1<=x2 and
   y1<=y2 must be valid! */
void lcd_rect(const wininfo_t *pwi, XYPOS x1, XYPOS y1, XYPOS x2, XYPOS y2,
	      const colinfo_t *pci)
{
	XYPOS xmin, ymin;
	XYPOS xmax, ymax;

	/* Check if object is fully left, right, above or below screen */
	xmin = pwi->clip_left;
	ymin = pwi->clip_top;
	xmax = pwi->clip_right;
	ymax = pwi->clip_bottom;
	if ((x2 < xmin) || (y2 < ymin) || (x1 > xmax) || (y1 > ymax))
		return;			  /* Done, object not visible */

	/* Clip rectangle to framebuffer boundaries */
	if (x1 < xmin)
		x1 = xmin;
	if (y1 < ymin)
		y1 = ymin;
	if (x2 > xmax)
		x2 = xmax;
	if (y2 >= ymax)
		y2 = ymax;

	/* Finally draw rectangle */
	if (pwi->attr & ATTR_ALPHA)
		adraw_ll_rect(pwi, x1, y1, x2, y2, pci);
	else
		draw_ll_rect(pwi, x1, y1, x2, y2, pci->col);
}


/* Draw unfilled frame from (x1, y1) to (x2, y2) using rounded corners with
 * radius r. Call with x1=x-r, y1=y-r, x2=x+r, y2=y+r to draw a circle with
 * radius r at centerpoint (x,y).
 *
 * Circle algorithm for corners
 * ----------------------------
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
 * Clipping is done in lcd_pixel() so we don't care about clipping here.
 *
 * Remark: this algorithm computes an optimal approximation to a circle, i.e.
 * the result is also symmetric to the angle bisector. */
void lcd_rframe(const wininfo_t *pwi, XYPOS x1, XYPOS y1, XYPOS x2, XYPOS y2,
		XYPOS r, const colinfo_t *pci)
{
	XYPOS dx, dy, dd, maxr;

	if (r < 0)
		return;

	/* Check for the maximum possible radius for these coordinates */
	maxr = x2-x1;
	if (maxr > y2-y1)
		maxr = y2-y1;
	maxr = maxr/2;
	if (r > maxr)
		r = maxr;

	/* If r=0, draw standard frame without rounded corners */
	if (r == 0) {
		lcd_frame(pwi, x1, y1, x2, y2, pci);
		return;
	}

	/* Move coordinates to the centers of the quarter circle centers */
	x1 += r;
	y1 += r;
	x2 -= r;
	y2 -= r;

	/* Initialize midpoint values */
	dx = 0;
	dy = r;
	dd = 1-r;

	/* Draw top and bottom horizontal lines (dx == 0) */
	lcd_rect(pwi, x1, y1 - dy, x2, y1 - dy, pci);
	lcd_rect(pwi, x1, y2 + dy, x2, y2 + dy, pci);
	if (dd < 0)
		dd += 3;		  /* 2*dx + 3, but dx is 0 */
	else				  /* Only possible for r==1 */
		dy--;			  /* dd does not matter, dy is 0 */
	dx++;

	/* Draw part with low slope (every step changes dx, sometimes dy) */
	while (dy > dx) {
		lcd_pixel(pwi, x1 - dx, y1 - dy, pci);
		lcd_pixel(pwi, x2 + dx, y1 - dy, pci);
		lcd_pixel(pwi, x1 - dx, y2 + dy, pci);
		lcd_pixel(pwi, x2 + dx, y2 + dy, pci);
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
		lcd_pixel(pwi, x1 - dx, y1 - dy, pci);
		lcd_pixel(pwi, x2 + dx, y1 - dy, pci);
		lcd_pixel(pwi, x1 - dx, y2 + dy, pci);
		lcd_pixel(pwi, x2 + dx, y2 + dy, pci);

		if (dd < 0) {
			dd += (dx - dy)*2 + 5; /* SE */
			dx++;
		} else
			dd += 3 - dy*2;	       /* S */
		dy--;
	}

	/* Draw left and right vertical lines (dy == 0) */
	lcd_rect(pwi, x1 - dx, y1, x1 - dx, y2, pci);
	lcd_rect(pwi, x2 + dx, y1, x2 + dx, y2, pci);
}


/* Draw filled rectangle from (x1, y1) to (x2, y2) using rounded corners with
   radius r in given color. Call with x1=x-r, y1=y-r, x2=x+r, y2=y+r to draw a
   filled circle at (x, y) with radius r. The algorithm is the same as
   explained above at lcd_rframe(), however we can skip some tests as we
   always draw a full line from the left to the right of the circle. As
   clipping is done in lcd_rect(), we don't care about clipping here. */
void lcd_rrect(const wininfo_t *pwi, XYPOS x1, XYPOS y1, XYPOS x2, XYPOS y2,
	       XYPOS r, const colinfo_t *pci)
{
	XYPOS dx, dy, dd, maxr;

	if (r < 0)
		return;

	/* Check for the maximum possible radius for these coordinates */
	maxr = x2-x1;
	if (maxr > y2-y1)
		maxr = y2-y1;
	maxr = maxr/2;
	if (r > maxr)
		r = maxr;
	/* If r=0, draw standard filled rectangle without rounded corners */
	if (r == 0)
		lcd_rect(pwi, x1, y1, x2, y2, pci);

	/* Move coordinates to the centers of the quarter circle centers */
	x1 += r;
	y1 += r;
	x2 -= r;
	y2 -= r;

	/* Initialize midpoint values */
	dx = 0;
	dy = r;
	dd = 1-r;

	/* Draw part with low slope (every step changes dx, sometimes dy) */
	while (dy > dx) {
		lcd_rect(pwi, x1 - dx, y1 - dy, x2 + dx, y1 - dy, pci);
		lcd_rect(pwi, x1 - dx, y2 + dy, x2 + dx, y2 + dy, pci);
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
		lcd_rect(pwi, x1 - dx, y1 - dy, x2 + dx, y1 - dy, pci);
		lcd_rect(pwi, x1 - dx, y2 + dy, x2 + dx, y2 + dy, pci);
		if (dd < 0) {
			dd += (dx - dy)*2 + 5; /* SE */
			dx++;
		} else
			dd += 3 - dy*2;	       /* S */
		dy--;
	}

	/* Draw final vertical middle part (dy == 0) */
	lcd_rect(pwi, x1 - dx, y1, x2 + dx, y2, pci);
}


/* Draw circle outline at (x, y) with radius r and given color */
void lcd_circle(const wininfo_t *pwi, XYPOS x, XYPOS y, XYPOS r,
		const colinfo_t *pci)
{
	lcd_rframe(pwi, x-r, y-r, x+r, y+r, r, pci);
}


/* Draw filled circle at (x, y) with radius r and given color */
void lcd_disc(const wininfo_t *pwi, XYPOS x, XYPOS y, XYPOS r,
	      const colinfo_t *pci)
{
	lcd_rrect(pwi, x-r, y-r, x+r, y+r, r, pci);
}


/* Draw text string s at (x, y) with alignment/attribute a and colors fg/bg
   the attributes are as follows:
     Bit 1..0: horizontal refpoint:  00: left, 01: hcenter,
		10: right, 11: right+1
     Bit 3..2: vertical refpoint: 00: top, 01: vcenter,
		10: bottom, 11: bottom+1
     Bit 5..4: character width: 00: normal (1x), 01: double (2x),
		10: triple (3x), 11: quadruple (4x)
     Bit 7..6: character height: 00: normal (1x), 01: double (2x),
		10: triple (3x), 11: quadruple (4x)
     Bit 8:    0: normal, 1: bold
     Bit 9:    0: normal, 1: inverse
     Bit 10:   0: normal, 1: underline
     Bit 11:   0: normal, 1: strike-through
     Bit 12:   0: FG+BG, 1: only FG (BG transparent)

   We only draw fully visible characters. If a character would be fully or
   partly outside of the framebuffer, it is not drawn at all. If you need
   partly visible characters, use a larger framebuffer and show only the part
   with the partly visible characters in a window. */
void lcd_text(const wininfo_t *pwi, XYPOS x, XYPOS y, char *s,
	      const colinfo_t *pci_fg, const colinfo_t *pci_bg)
{
	XYPOS len = (XYPOS)strlen(s);
	XYPOS width = VIDEO_FONT_WIDTH;
	XYPOS height = VIDEO_FONT_HEIGHT;
	XYPOS xmin = pwi->clip_left;
	XYPOS ymin = pwi->clip_top;
	XYPOS xmax = pwi->clip_right+1;
	XYPOS ymax = pwi->clip_bottom+1;
	u_int attr = pwi->attr;

	/* Return if string is empty */
	if (s == 0)
		return;

	/* Apply multiple width and multiple height */
	width *= ((attr & ATTR_HS_MASK) >> 4) + 1;
	height *= ((attr & ATTR_VS_MASK) >> 6) + 1;

	/* Compute y from vertical alignment */
	switch (attr & ATTR_VMASK) {
	case ATTR_VTOP:
		break;

	case ATTR_VCENTER:
		y -= height/2;
		break;

	case ATTR_VBOTTOM:
		y++;
		/* Fall through to case ATTR_VBOTTOM1 */

	case ATTR_VBOTTOM1:
		y -= height;
		break;
	}

	/* Return if text is completely or partly above or below framebuffer */
	if ((y < ymin) || (y + height > ymax))
		return;

	/* Compute x from horizontal alignment */
	switch (attr & ATTR_HMASK) {
	case ATTR_HLEFT:
		break;

	case ATTR_HCENTER:
		x -= len*width/2;
		break;

	case ATTR_HRIGHT:
		x++;
		/* Fall through to ATTR_HRIGHT1 */

	case ATTR_HRIGHT1:
		x -= len*width;
		break;
	}

	/* Return if text is completely right of framebuffer or if only the
	   first character would be partly inside of the framebuffer */
	if (x + width > xmax)
		return;

	if (x < xmin) {
		/* Compute number of characters left of framebuffer */
		unsigned offs = (xmin - x - 1)/width + 1;

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
		if (!c || (x + width > xmax))
			break;

		/* Output character and move position */
		if (attr & ATTR_ALPHA)
			adraw_ll_char(pwi, x, y, c, pci_fg, pci_bg);
		else
			draw_ll_char(pwi, x, y, c, pci_fg->col, pci_bg->col);
		x += width;
	}
}


/* Draw test pattern */
int lcd_test(const wininfo_t *pwi, u_int pattern)
{
	XYPOS x1 = pwi->clip_left;
	XYPOS y1 = pwi->clip_top;
	XYPOS x2 = pwi->clip_right;
	XYPOS y2 = pwi->clip_bottom;
	const struct testinfo ti[] = {
		{24, 16, lcd_ll_pattern0}, /* Grid, circle, basic colors */
		{ 4,  2, lcd_ll_pattern1}, /* Eight colors in 4x2 fields */
		{ 6,  3, lcd_ll_pattern2}, /* Hor: colors, vert: brightness */
		{ 6,  3, lcd_ll_pattern3}  /* Colors at screen borders */
	};
	const struct testinfo *pti;

	/* Get info to the given pattern */
	if (pattern > ARRAYSIZE(ti))
		pattern = 0;
	pti = &ti[pattern];

	/* Return with error if window is too small */
	if ((x2-x1+1 < pti->minhres) || (y2-y1+1 < pti->minvres))
		return 1;

	/* Clear window in black */
	draw_ll_rect(pwi, x1, y1, x2, y2, pwi->ppi->rgba2col(pwi, 0x000000FF));

	/* Call lowlevel drawing function for pattern */
	pti->draw_ll_pattern(pwi, x1, y1, x2, y2);

	return 0;
}


/************************************************************************/
/* OTHER EXPORTED GRAPHICS FUNCTIONS					*/
/************************************************************************/

/* Set colinfo structure */
void lcd_set_col(wininfo_t *pwi, RGBA rgba, colinfo_t *pci)
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
	console_cls(pwi, coninfo.bg);
}


/* If the window is the console window, re-initialize console */
void console_update(wininfo_t *pwi, RGBA fg, RGBA bg)
{
	if (console_pwi->win == pwi->win)
		console_init(pwi, fg, bg);
}
#endif /* CONFIG_MULTIPLE_CONSOLES */

/* Clear the console window with given color */
void console_cls(const wininfo_t *pwi, COLOR32 col)
{
	memset32((unsigned *)pwi->pfbuf[pwi->fbdraw], col2col32(pwi, col),
		 pwi->fbsize/4);
}


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
