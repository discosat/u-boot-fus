/*
 * Hardware independent lowlevel drawing routines (applying alpha)
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
#include <xlcd_adraw_ll.h>		 /* Own interface */
#include <cmd_lcd.h>			 /* wininfo_t */
#include <video_font.h>			 /* Get font data, width and height */


/************************************************************************/
/* DRAWING GRAPHICS PRIMITIVES						*/
/************************************************************************/

#if CONFIG_XLCD_DRAW & (XLCD_DRAW_PIXEL | XLCD_DRAW_LINE | XLCD_DRAW_CIRC \
			| XLCD_DRAW_TURTLE)
/* Draw pixel by applying alpha of new pixel; pixel is definitely valid */
void adraw_ll_pixel(const wininfo_t *pwi, XYPOS x, XYPOS y,
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
#endif


#if CONFIG_XLCD_DRAW \
	& (XLCD_DRAW_RECT | XLCD_DRAW_CIRC | XLCD_DRAW_PROG | XLCD_DRAW_FILL)
/* Draw filled rectangle, applying alpha; given region is definitely valid and
   x and y are sorted (x1 <= x2, y1 <= y2) */
void adraw_ll_rect(const wininfo_t *pwi, XYPOS x1, XYPOS y1,
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
#endif


#if CONFIG_XLCD_DRAW & (XLCD_DRAW_TEXT | XLCD_DRAW_PROG)
/* Draw a character, applyig alpha; character area is definitely valid */
void adraw_ll_char(const wininfo_t *pwi, XYPOS x, XYPOS y, char c,
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
#endif


/************************************************************************/
/* DRAWING BITMAP ROWS							*/
/************************************************************************/

#if defined(CONFIG_XLCD_PNG) || defined(CONFIG_XLCD_BMP)
/* Normal version for 1bpp, 2bpp, 4bpp */
void adraw_ll_row_PAL(imginfo_t *pii, COLOR32 *p)
{
	int xpix = pii->xpix;
	u_int shift = pii->shift;
	COLOR32 val;
	u_char *prow = pii->prow;

	u_int rowshift = pii->rowshift;
	const wininfo_t *pwi = pii->pwi;

	val = *p;
	for (;;) {
		RGBA rgba;
		RGBA alpha1;
		colinfo_t ci;

		rowshift -= pii->rowbitdepth;
		rgba = pii->palette[(*prow >> rowshift) & pii->rowmask];
		if (!rowshift) {
			prow++;
			rowshift = 8;
		}
		alpha1 = rgba & 0xFF;
		ci.A256 = 256 - alpha1;
		alpha1++;
		ci.RA1 = (rgba >> 24) * alpha1;
		ci.GA1 = ((rgba >> 16) & 0xFF) * alpha1;
		ci.BA1 = ((rgba >> 8) & 0xFF) * alpha1;
		do {
			shift -= pii->bpp;
			if (alpha1 != 1) {
				COLOR32 col;

				col = pwi->ppi->apply_alpha(pwi, &ci,
							    val >> shift);
				val &= ~(pii->mask << shift);
				val |= col << shift;
			}
			if (++xpix >= pii->xend)
				goto DONE;
			if (!shift) {
				*p++ = val;
				val = *p;
				shift = 32;
			}
		} while (xpix % pii->multiwidth);
	}
DONE:
	*p = val;			  /* Store final value */
}


/* Optimized version for 8bpp */
void adraw_ll_row_PAL8(imginfo_t *pii, COLOR32 *p)
{
	int xpix = pii->xpix;
	u_int shift = pii->shift;
	COLOR32 val;
	u_char *prow = pii->prow;

	u_int rowshift = pii->rowshift;
	const wininfo_t *pwi = pii->pwi;

	val = *p;
	for (;;) {
		RGBA rgba;
		RGBA alpha1;
		colinfo_t ci;

		rowshift -= pii->rowbitdepth;
		rgba = pii->palette[*prow++];
		alpha1 = rgba & 0xFF;
		ci.A256 = 256 - alpha1;
		alpha1++;
		ci.RA1 = (rgba >> 24) * alpha1;
		ci.GA1 = ((rgba >> 16) & 0xFF) * alpha1;
		ci.BA1 = ((rgba >> 8) & 0xFF) * alpha1;
		do {
			shift -= pii->bpp;
			if (alpha1 != 1) {
				COLOR32 col;

				col = pwi->ppi->apply_alpha(pwi, &ci,
							    val >> shift);
				val &= ~(pii->mask << shift);
				val |= col << shift;
			}
			if (++xpix >= pii->xend)
				goto DONE;
			if (!shift) {
				*p++ = val;
				val = *p;
				shift = 32;
			}
		} while (xpix % pii->multiwidth);
	}
DONE:
	*p = val;			  /* Store final value */
}
#endif /* CONFIG_XLCD_PNG || CONFIG_XLCD_BMP */


#ifdef CONFIG_XLCD_PNG
void adraw_ll_row_GA(imginfo_t *pii, COLOR32 *p)
{
	int xpix = pii->xpix;
	u_int shift = pii->shift;
	COLOR32 val;
	const wininfo_t *pwi = pii->pwi;
	u_char *prow = pii->prow;

	val = *p;
	for (;;) {
		RGBA alpha1;
		colinfo_t ci;

		alpha1 = prow[1];
		ci.A256 = 256 - alpha1;
		alpha1++;
		ci.RA1 = prow[0] * alpha1;
		ci.GA1 = ci.RA1;
		ci.BA1 = ci.GA1;
		do {
			shift -= pii->bpp;
			if (alpha1 != 1) {
				COLOR32 col;

				col = pwi->ppi->apply_alpha(pwi, &ci,
							    val >> shift);
				val &= ~(pii->mask << shift);
				val |= col << shift;
			}
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
	*p = val;			  /* Store final value */
}


void adraw_ll_row_RGB(imginfo_t *pii, COLOR32 *p)
{
	int xpix = pii->xpix;
	u_int shift = pii->shift;
	COLOR32 val;
	const wininfo_t *pwi = pii->pwi;
	u_char *prow = pii->prow;

	val = *p;
	for (;;) {
		RGBA rgba;
		COLOR32 col = 0;

		rgba = prow[0] << 24;
		rgba |= prow[1] << 16;
		rgba |= prow[2] << 8;
		if (rgba != pii->trans_rgba) {
			rgba |= 0xFF;
			col = pwi->ppi->rgba2col(pwi, rgba);
		}
		do {
			shift -= pii->bpp;
			if (rgba & 0xFF) {
				val &= ~(pii->mask << shift);
				val |= col << shift;
			}
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
	*p = val;			  /* Store final value */
}


void adraw_ll_row_RGBA(imginfo_t *pii, COLOR32 *p)
{
	int xpix = pii->xpix;
	u_int shift = pii->shift;
	COLOR32 val;
	const wininfo_t *pwi = pii->pwi;
	u_char *prow = pii->prow;

	val = *p;
	for (;;) {
		RGBA alpha1;
		colinfo_t ci;

		alpha1 = prow[3];
		ci.A256 = 256 - alpha1;
		alpha1++;
		ci.RA1 = prow[0] * alpha1;
		ci.GA1 = prow[1] * alpha1;
		ci.BA1 = prow[2] * alpha1;
		do {
			shift -= pii->bpp;
			if (alpha1 != 1) {
				COLOR32 col;

				col = pwi->ppi->apply_alpha(pwi, &ci,
							    val >> shift);
				val &= ~(pii->mask << shift);
				val |= col << shift;
			}
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
void adraw_ll_row_BGR(imginfo_t *pii, COLOR32 *p)
{
	int xpix = pii->xpix;
	u_int shift = pii->shift;
	COLOR32 val;
	const wininfo_t *pwi = pii->pwi;
	u_char *prow = pii->prow;

	val = *p;
	for (;;) {
		RGBA rgba;
		COLOR32 col = 0;

		rgba = prow[0] << 8;
		rgba |= prow[1] << 16;
		rgba |= prow[2] << 24;
		if (rgba != pii->trans_rgba) {
			rgba |= 0xFF;
			col = pwi->ppi->rgba2col(pwi, rgba);
		}
		do {
			shift -= pii->bpp;
			if (rgba & 0xFF) {
				val &= ~(pii->mask << shift);
				val |= col << shift;
			}
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
	*p = val;			  /* Store final value */
}


void adraw_ll_row_BGRA(imginfo_t *pii, COLOR32 *p)
{
	int xpix = pii->xpix;
	u_int shift = pii->shift;
	COLOR32 val;
	const wininfo_t *pwi = pii->pwi;
	u_char *prow = pii->prow;

	val = *p;
	for (;;) {
		RGBA alpha1;
		colinfo_t ci;

		alpha1 = prow[3];
		ci.A256 = 256 - alpha1;
		alpha1++;
		ci.RA1 = prow[2] * alpha1;
		ci.GA1 = prow[1] * alpha1;
		ci.BA1 = prow[0] * alpha1;
		do {
			shift -= pii->bpp;
			if (alpha1 != 1) {
				COLOR32 col;

				col = pwi->ppi->apply_alpha(pwi, &ci,
							    val >> shift);
				val &= ~(pii->mask << shift);
				val |= col << shift;
			}
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
	*p = val;			  /* Store final value */
}
#endif /* CONFIG_XLCD_BMP */
