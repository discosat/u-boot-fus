/*
 * Hardware independent lowlevel drawing routines (storing aplha)
 *
 * (C) Copyright 2012
 * Hartmut Keller, F&S Elektronik Systeme GmbH, keller@fs-net.de
 *
 * SPDX-License-Identifier:	GPL-2.0+
 */

/************************************************************************/
/* HEADER FILES								*/
/************************************************************************/

#include <config.h>
#include <common.h>
#include <xlcd_draw_ll.h>		 /* Own interface */
#include <cmd_lcd.h>			 /* wininfo_t */

/************************************************************************/
/* DRAWING GRAPHICS PRIMITIVES						*/
/************************************************************************/

/* draw_ll_pixel() is also called by some test patterns */
#if CONFIG_XLCD_DRAW & (XLCD_DRAW_PIXEL | XLCD_DRAW_LINE | XLCD_DRAW_CIRC \
			| XLCD_DRAW_TURTLE | XLCD_DRAW_TEST)
/* Draw pixel by replacing with new color; pixel is definitely valid */
void draw_ll_pixel(const wininfo_t *pwi, XYPOS x, XYPOS y, COLOR32 col)
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
#endif


#if CONFIG_XLCD_DRAW & (XLCD_DRAW_RECT | XLCD_DRAW_CIRC | XLCD_DRAW_PROG \
			| XLCD_DRAW_FILL | XLCD_DRAW_TEST)
/* Draw filled rectangle, replacing pixels with new color; given region is
   definitely valid and x and y are sorted (x1 <= x2, y1 <= y2) */
void draw_ll_rect(const wininfo_t *pwi, XYPOS x1, XYPOS y1,
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
#endif

/************************************************************************/
/* DRAWING BITMAP ROWS							*/
/************************************************************************/

#if defined(CONFIG_XLCD_PNG) || defined(CONFIG_XLCD_BMP)
/* Version for 1bpp, 2bpp and 4bpp */
void draw_ll_row_PAL(imginfo_t *pii, COLOR32 *p)
{
	int xpix = pii->xpix;
	int xend = pii->xend;
	u_int shift = pii->shift;
	COLOR32 val;
	u_int bpp = pii->bpp;

	u_char *prow = pii->prow;
	u_int rowshift = pii->rowshift;
	//u_int rowbitdepth = pii->rowbitdepth;
	//u_int rowmask = pii->rowmask;
	COLOR32 mask = pii->mask;
	//u_int multiwidth = pii->multiwidth;

	val = *p;
	for (;;) {
		COLOR32 col;

		rowshift -= pii->rowbitdepth;
		col = pii->palette[(*prow >> rowshift) & pii->rowmask];
		if (!rowshift) {
			prow++;
			rowshift = 8;
		}
		do {
			shift -= bpp;
			val &= ~(mask << shift);
			val |= col << shift;
			if (++xpix >= /*pii->*/xend)
				goto DONE;
			if (!shift) {
				*p++ = val;
				val = *p;
				shift = 32;
			}
		} while (xpix % pii->multiwidth);
	}
DONE:
	*p = val; /* Store final value */
}


/* Optimized version for 8bpp, leaving all shifts out */
void draw_ll_row_PAL8(imginfo_t *pii, COLOR32 *p)
{
	int xpix = pii->xpix;
	int xend = pii->xend;
	u_int shift = pii->shift;
	u_int bpp = pii->bpp;
	u_char *prow = pii->prow;
	COLOR32 val = *p;
	COLOR32 mask = pii->mask;
	u_int multiwidth = pii->multiwidth;

	for (;;) {
		COLOR32 col;

		col = (COLOR32)pii->palette[prow[0]];
		do {
			shift -= bpp;
			val &= ~(mask << shift);
			val |= col << shift;

			if (++xpix >= xend)
				goto DONE;
			if (!shift) {
				*p++ = val;
				val = *p;
				shift = 32;
			}
		} while (xpix % multiwidth);
		prow++;
	}
DONE:
	*p = val; /* Store final value */
}
#endif /* CONFIG_XLCD_PNG || CONFIG_XLCD_BMP */


#ifdef CONFIG_XLCD_PNG
void draw_ll_row_GA(imginfo_t *pii, COLOR32 *p)

{
	int xpix = pii->xpix;
	u_int shift = pii->shift;
	COLOR32 val;
	const wininfo_t *pwi = pii->pwi;
	u_char *prow = pii->prow;

	val = *p;
	for (;;) {
		RGBA rgba;
		COLOR32 col;

		rgba = prow[0] << 8;	            /* B[7:0] */
		rgba |= (rgba << 8) | (rgba << 16); /* G[7:0], R[7:0] */
		rgba |= prow[1];		    /* A[7:0] */
		col = pwi->ppi->rgba2col(pwi, rgba);
		do {
			shift -= pii->bpp;
			val &= ~(pii->mask << shift);
			val |= col << shift;
			if (++xpix >= pii->xend)
				goto DONE;
			if (!shift) {
				*p++ = val;
				val = *p;
				shift = 32;
			}
		} while (xpix % pii->multiwidth);
		prow += 2;
	}
DONE:
	*p = val; /* Store final value */

}


void draw_ll_row_RGB(imginfo_t *pii, COLOR32 *p)
{
	int xpix = pii->xpix;
	u_int shift = pii->shift;
	COLOR32 val;
	const wininfo_t *pwi = pii->pwi;
	u_char *prow = pii->prow;

	val = *p;
	for (;;) {
		RGBA rgba;
		COLOR32 col;

		rgba = prow[0] << 24;
		rgba |= prow[1] << 16;
		rgba |= prow[2] << 8;
		if (rgba != pii->trans_rgba)
			rgba |= 0xFF;
		col = pwi->ppi->rgba2col(pwi, rgba);
		do {
			shift -= pii->bpp;
			val &= ~(pii->mask << shift);
			val |= col << shift;
			if (++xpix >= pii->xend)
				goto DONE;
			if (!shift) {
				*p++ = val;
				val = *p;
				shift = 32;
			}
		} while (xpix % pii->multiwidth);
		prow += 3;
	}
DONE:
	*p = val; /* Store final value */
}


void draw_ll_row_RGBA(imginfo_t *pii, COLOR32 *p)
{
	int xpix = pii->xpix;
	u_int shift = pii->shift;
	COLOR32 val;
	const wininfo_t *pwi = pii->pwi;
	u_char *prow = pii->prow;

	val = *p;
	for (;;) {
		RGBA rgba;
		COLOR32 col;

		rgba = prow[0] << 24;
		rgba |= prow[1] << 16;
		rgba |= prow[2] << 8;
		rgba |= prow[3];
		col = pwi->ppi->rgba2col(pwi, rgba);
		do {
			shift -= pii->bpp;
			val &= ~(pii->mask << shift);
			val |= col << shift;
			if (++xpix >= pii->xend)
				goto DONE;
			if (!shift) {
				*p++ = val;
				val = *p;
				shift = 32;
			}
		} while (xpix % pii->multiwidth);
		prow += 4;
	}
DONE:
	*p = val; /* Store final value */
}
#endif /* CONFIG_XLCD_PNG */


#ifdef CONFIG_XLCD_BMP
void draw_ll_row_BGR(imginfo_t *pii, COLOR32 *p)
{
	int xpix = pii->xpix;
	u_int shift = pii->shift;
	COLOR32 val;
	const wininfo_t *pwi = pii->pwi;
	u_char *prow = pii->prow;

	val = *p;
	for (;;) {
		RGBA rgba;
		COLOR32 col;

		rgba = prow[0] << 8;
		rgba |= prow[1] << 16;
		rgba |= prow[2] << 24;
		if (rgba != pii->trans_rgba)
			rgba |= 0xFF;
		col = pwi->ppi->rgba2col(pwi, rgba);
		do {
			shift -= pii->bpp;
			val &= ~(pii->mask << shift);
			val |= col << shift;
			if (++xpix >= pii->xend)
				goto DONE;
			if (!shift) {
				*p++ = val;
				val = *p;
				shift = 32;
			}
		} while (xpix % pii->multiwidth);
		prow += 3;
	}
DONE:
	*p = val; /* Store final value */
}


void draw_ll_row_BGRA(imginfo_t *pii, COLOR32 *p)
{
	int xpix = pii->xpix;
	u_int shift = pii->shift;
	COLOR32 val;
	const wininfo_t *pwi = pii->pwi;
	u_char *prow = pii->prow;

	val = *p;
	for (;;) {
		RGBA rgba;
		COLOR32 col;

		rgba = prow[0] << 8;
		rgba |= prow[1] << 16;
		rgba |= prow[2] << 24;
		rgba |= prow[3];
		col = pwi->ppi->rgba2col(pwi, rgba);
		do {
			shift -= pii->bpp;
			val &= ~(pii->mask << shift);
			val |= col << shift;
			if (++xpix >= pii->xend)
				goto DONE;
			if (!shift) {
				*p++ = val;
				val = *p;
				shift = 32;
			}
		} while (xpix % pii->multiwidth);
		prow += 4;
	}
DONE:
	*p = val; /* Store final value */
}
#endif /* CONFIG_XLCD_BMP */
