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

/* ##### TODO
   1. All *_ll_* functions could be moved to a separate file. This allows
      replacing them with architecure specific optimized code. For example on
      ARMv6 there are special commands that could be used to separate the
      bytes of the RGBA value
   2. Check if draw_ll_*() and adraw_ll_*() can use the 2D graphics
      acceleration. For example on S3C6410, bitblt() can be done in so-called
      host mode, where each pixel can be sent one by one from the host CPU.
      The 2D-graphics then does alpha-blending and then we could convert the
      result to the framebuffer format (some formats like RGBA5650 and
      RGBA5551 are directly supported as target format).
 */

/************************************************************************/
/* ** HEADER FILES							*/
/************************************************************************/

/* #define DEBUG */

#include <config.h>
#include <common.h>
#include <command.h>
//#include <version.h>
#include <lcd.h>			  /* Own interface */
#include <cmd_lcd.h>			  /* cmd_lcd_init() */
#include <video_font.h>			  /* Get font data, width and height */
#include <stdarg.h>
#include <malloc.h>			  /* malloc(), free() */
#include <linux/types.h>
#include <linux/ctype.h>		  /* isalpha() */
#include <devices.h>
#include <zlib.h>			  /* z_stream, inflateInit(), ... */
#if defined(CONFIG_POST)
#include <post.h>
#endif
#include <watchdog.h>


/************************************************************************/
/* DEFINITIONS								*/
/************************************************************************/

#ifdef CONFIG_PNG
/* Chunk IDs for PNG bitmaps */
#define CHUNK_IHDR 0x49484452		  /* "IHDR" Image header */
#define CHUNK_IDAT 0x49444154		  /* "IDAT" Image data */
#define CHUNK_IEND 0x49454E44		  /* "IEND" Image end */
#define CHUNK_PLTE 0x504C5445		  /* "PLTE" Palette */
#define CHUNK_bKGD 0x624B4744		  /* "bKGD" Background */
#define CHUNK_tRNS 0x74524E53		  /* "tRNS" Transparency */
#define CHUNK_tEXt 0x74455874		  /* "tEXt" Text */
#define CHUNK_zTXt 0x7A545874		  /* "zTXt" zLib compressed text */
#define CHUNK_iTXt 0x69545874		  /* "iTXt" International text */
#define CHUNK_tIME 0x74494D45		  /* "tIME" Last modification time */
#define CHUNK_cHRM 0x6348524D		  /* "cHRM" Chromaticities/WhiteP */
#define CHUNK_gAMA 0x67414D41		  /* "gAMA" Image gamma */
#define CHUNK_iCCP 0x69434350		  /* "iCCP" Embedded ICC profile */
#define CHUNK_sBIT 0x73424954		  /* "sBIT" Significant bits */
#define CHUNK_sRGB 0x73524742		  /* "sRGB" Standard RGB color space */
#define CHUNK_hIST 0x68495354		  /* "hIST" Color histogram */
#define CHUNK_pHYs 0x70485973		  /* "pHYs" Physical pixel size */
#define CHUNK_sPLT 0x73504C54		  /* "sPLT" Suggested palette */
#endif /* CONFIG_PNG */


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

#if defined(CONFIG_BMP) || defined(CONFIG_PNG)
/* Type for bitmap information */
typedef struct imginfo imginfo_t;	  /* Forward declaration */

/* Type for lowlevel-drawing a bitmap row */
typedef void (*draw_row_func_t)(imginfo_t *, COLOR32 *);

/* Image information */
struct imginfo
{
	/* Framebuffer info */
	XYPOS xpix;			  /* Current bitmap column */
	XYPOS xend;			  /* Last bitmap column to draw */
	u_int shift;			  /* Shift value for current x */
	u_int bpp;			  /* Framebuffer bitdepth */
	COLOR32 mask;			  /* Mask used in framebuffer */
	XYPOS y;			  /* y-coordinate to draw to */
	XYPOS ypix;			  /* Current bitmap row */
	XYPOS yend;			  /* Last bitmap row to draw */
	u_long fbuf;			  /* Frame buffer addres at current x */
	const wininfo_t *pwi;		  /* Pointer to windo info */

	/* Bitmap source data info */
	u_int rowmask;			  /* Mask used in bitmap data */
	u_int rowbitdepth;		  /* Bitmap bitdepth */
	u_int rowshift;			  /* Shift value for current pos */
	u_int rowshift0;		  /* Shift value at beginning of row */
	u_char *prow;			  /* Pointer to row data */

	RGBA hash_rgba;			  /* Remember last RGBA value */
	COLOR32 hash_col;		  /* and corresponding color */

	u_int applyalpha;		  /* ATTR_ALPHA 0: not set, 1: set */
	u_int dwidthshift;		  /* ATTR_DWIDTH 0: not set, 1: set */
	u_int dheightshift;		  /* ATTR_DHEIGHT 0: not set, 1: set */
	RGBA trans_rgba;		  /* Transparent color if truecolor */
	bminfo_t bi;			  /* Generic bitmap information */
};
#endif /* CONFIG_BMP || CONFIG_PNG */


/************************************************************************/
/* LOCAL VARIABLES							*/
/************************************************************************/

#ifdef CONFIG_PNG
/* PNG signature and IHDR header; this must be the start of each PNG bitmap */
static const u_char png_signature[16] = {
	137, 80, 78, 71, 13, 10, 26, 10,  /* PNG signature */
	0, 0, 0, 13, 'I', 'H', 'D', 'R'	  /* IHDR chunk header */
};
#endif /* CONFIG_PNG */

/* Buffer for palette */
static RGBA palette[256];


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
static void draw_ll_char(const wininfo_t *pwi, XYPOS x, XYPOS y, char c);

/* Draw a character, applyig alpha; character area is definitely valid */
static void adraw_ll_char(const wininfo_t *pwi, XYPOS x, XYPOS y, char c);


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
	switch (bpp_shift) {
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
static void draw_ll_char(const wininfo_t *pwi, XYPOS x, XYPOS y, char c)
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
					val &= ~(mask << s);
					if (fd & fm)
						val |= pwi->fg.col << s;
					else if (!(attr & ATTR_TRANSP))
						val |= pwi->bg.col << s;

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

#if defined(CONFIG_BMP) || defined(CONFIG_PNG)
/* Version for 1bpp, 2bpp and 4bpp */
static void draw_ll_row_PAL(imginfo_t *pii, COLOR32 *p)
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
	//u_int dwidthshift = pii->dwidthshift;

	val = *p;
	for (;;) {
		COLOR32 col;

		rowshift -= pii->rowbitdepth;
		col = palette[(*prow >> rowshift) & pii->rowmask];
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
		} while (xpix & pii->dwidthshift);
	}
DONE:
	*p = val; /* Store final value */
}

/* Normal version for 1bpp, 2bpp, 4bpp */
static void adraw_ll_row_PAL(imginfo_t *pii, COLOR32 *p)
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
		rgba = palette[(*prow >> rowshift) & pii->rowmask];
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
				COLOR32 pixmask;
				COLOR32 col;

				col = pwi->ppi->apply_alpha(pwi, &ci,
							    val >> shift);
				pixmask = pii->mask << shift;
				val = (val & ~pixmask) | (col & pixmask);
			}
			if (++xpix >= pii->xend)
				goto DONE;
			if (!shift) {
				*p++ = val;
				val = *p;
				shift = 32;
			}
		} while (xpix & pii->dwidthshift);
	}
DONE:
	*p = val;			  /* Store final value */
}

/* Optimized version for 8bpp, leaving all shifts out */
static void draw_ll_row_PAL8(imginfo_t *pii, COLOR32 *p)
{
	int xpix = pii->xpix;
	int xend = pii->xend;
	u_int shift = pii->shift;
	u_int bpp = pii->bpp;
	u_char *prow = pii->prow;
	COLOR32 val = *p;
	COLOR32 mask = pii->mask;
	u_int dwidthshift = pii->dwidthshift;

	for (;;) {
		COLOR32 col;

		col = palette[prow[0]];
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
		} while (xpix & dwidthshift);
		prow++;
	}
DONE:
	*p = val; /* Store final value */
}

/* Optimized version for 8bpp */
static void adraw_ll_row_PAL8(imginfo_t *pii, COLOR32 *p)
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
		rgba = palette[*prow++];
		alpha1 = rgba & 0xFF;
		ci.A256 = 256 - alpha1;
		alpha1++;
		ci.RA1 = (rgba >> 24) * alpha1;
		ci.GA1 = ((rgba >> 16) & 0xFF) * alpha1;
		ci.BA1 = ((rgba >> 8) & 0xFF) * alpha1;
		do {
			shift -= pii->bpp;
			if (alpha1 != 1) {
				COLOR32 pixmask;
				COLOR32 col;

				col = pwi->ppi->apply_alpha(pwi, &ci,
							    val >> shift);
				pixmask = pii->mask << shift;
				val = (val & ~pixmask) | (col & pixmask);
			}
			if (++xpix >= pii->xend)
				goto DONE;
			if (!shift) {
				*p++ = val;
				val = *p;
				shift = 32;
			}
		} while (xpix & pii->dwidthshift);
	}
DONE:
	*p = val;			  /* Store final value */
}
#endif /* CONFIG_BMP || CONFIG_PNG */


#ifdef CONFIG_PNG
static void draw_ll_row_GA(imginfo_t *pii, COLOR32 *p)
				   
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
		} while (xpix & pii->dwidthshift);
		prow += 2;
	}
DONE:
	*p = val; /* Store final value */

}

static void adraw_ll_row_GA(imginfo_t *pii, COLOR32 *p)
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
				COLOR32 pixmask;
				COLOR32 col;

				col = pwi->ppi->apply_alpha(pwi, &ci,
							    val >> shift);
				pixmask = pii->mask << shift;
				val = (val & ~pixmask) | (col & pixmask);
			}
			if (++xpix >= pii->xend)
				goto DONE;
			if (!shift) {
				*p++ = val;
				val = *p;
				shift = 32;
			}
		} while (xpix & pii->dwidthshift);
		prow += 2;
	}
DONE:
	*p = val;			  /* Store final value */
}


static void draw_ll_row_RGB(imginfo_t *pii, COLOR32 *p)
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
		} while (xpix & pii->dwidthshift);
		prow += 3;
	}
DONE:
	*p = val; /* Store final value */
}

static void adraw_ll_row_RGB(imginfo_t *pii, COLOR32 *p)
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
		} while (xpix & pii->dwidthshift);
		prow += 3;
	}
DONE:
	*p = val;			  /* Store final value */
}


static void draw_ll_row_RGBA(imginfo_t *pii, COLOR32 *p)
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
		} while (xpix & pii->dwidthshift);
		prow += 4;
	}
DONE:
	*p = val; /* Store final value */
}


static void adraw_ll_row_RGBA(imginfo_t *pii, COLOR32 *p)
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
		} while (xpix & pii->dwidthshift);
		prow += 4;
	}
DONE:
	*p = val; /* Store final value */
}
#endif /* CONFIG_PNG */


#ifdef CONFIG_BMP
static void draw_ll_row_BGR(imginfo_t *pii, COLOR32 *p)
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
		} while (xpix & pii->dwidthshift);
		prow += 3;
	}
DONE:
	*p = val; /* Store final value */
}

static void adraw_ll_row_BGR(imginfo_t *pii, COLOR32 *p)
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
		} while (xpix & pii->dwidthshift);
		prow += 3;
	}
DONE:
	*p = val;			  /* Store final value */
}


static void draw_ll_row_BGRA(imginfo_t *pii, COLOR32 *p)
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
		} while (xpix & pii->dwidthshift);
		prow += 4;
	}
DONE:
	*p = val; /* Store final value */
}


static void adraw_ll_row_BGRA(imginfo_t *pii, COLOR32 *p)
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
		} while (xpix & pii->dwidthshift);
		prow += 4;
	}
DONE:
	*p = val;			  /* Store final value */
}
#endif /* CONFIG_BMP */


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
	XYPOS xres = (XYPOS)pwi->hres;
	XYPOS yres = (XYPOS)pwi->vres;
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
	int xres = (int)pwi->hres;
	int yres = (int)pwi->vres;
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
	int xres = (int)pwi->hres;
	int yres = (int)pwi->vres;
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


#ifdef CONFIG_PNG
/* Get a big endian 32 bit number from address p (may be unaligned) */
static u_int get_be32(u_char *p)
{
	return (p[0]<<24) | (p[1]<<16) | (p[2]<<8) | p[3];
}

/* Get a big endian 16 bit number from address p (may be unaligned) */
static u_short get_be16(u_char *p)
{
	return (p[0] << 8) | p[1];
}

/* Special so-called paeth predictor for PNG line filtering */
static u_char paeth(u_char a, u_char b, u_char c)
{
	int p = a + b - c;
	int pa = p - a;
	int pb = p - b;
	int pc = p - c;
	if (pa < 0)
		pa = -pa;
	if (pb < 0)
		pb = -pb;
	if (pc < 0)
		pc = -pc;

	if ((pa <= pb) && (pa <= pc))
		return a;
	if (pb <= pc)
		return b;
	return c;
}


extern void *zalloc(void *, unsigned, unsigned);
extern void zfree(void *, void *, unsigned);

/* Draw PNG image. A PNG image consists of a signature and then a sequence of
   chunks. Each chunk has the same structure:
       Offset 0: Chunk data size in bytes (4 bytes)
       Offset 4: Chunk ID, 4 characters, case has a meaning (4 bytes)
       Offset 8: Chunk data (size bytes)
       Offset 8+size: CRC32 checksum (4 bytes)
   After the PNG signature, there must follow an IHDR chunk. This chunk tells
   the image size, color type and compression type. Then there may follow all
   the other chunks. The image data itself is contained in one or more IDAT
   chunks. The file must end with an IEND chunk. We are ignoring all other
   chunks but two: PLTE, which tells the color palette in palette images and
   tRNS that tells the transparency in color types without alpha value.

   The image data is compressed with the deflate method of zlib. Therefore we
   need to inflate it here again. Before deflating, each row of the image is
   processed by one of five filters: none, sub, up, average or paeth. The idea
   is to only encode differences to neighbouring pixels (and here chosing the
   minimal differences). These differences (usually small values) compress
   better than the original pixels. The filter type is added as a prefix byte
   to the row data of each image row. They work seperately on each byte of a
   pixel, or (if pixels are smaller than 1 byte) on full bytes. This makes
   applying the reverse filter here during decoding rather simple.

   We do not support images with 16 bits per color channel and we don't
   support interlaced images. These image types make no sense with our
   embedded hardware. If you have such bitmaps, you have to convert them
   first to a supported format with an external image program. */
static const char *draw_png(imginfo_t *pii, u_long addr)
{
	u_char colortype;
	u_char bitdepth;
	int current;
	u_int pixelsize;		  /* Size of one full pixel (bits) */
	u_int fsize;			  /* Size of one filter unit */
	u_int rowlen;
	int rowpos;
	int palconverted = 1;
	char *errmsg = NULL;
	z_stream zs;
	u_char *p;
	u_char *prow;
	draw_row_func_t draw_row;	  /* Draw bitmap row */

	static const draw_row_func_t draw_row_tab[2][6] = {
		{	
			/* ATTR_ALPHA = 0 */
			NULL,
			draw_ll_row_PAL,
			draw_ll_row_PAL8,
			draw_ll_row_GA,
			draw_ll_row_RGB,
			draw_ll_row_RGBA
		},
		{
			/* ATTR_ALPHA = 1 */
			NULL,
			adraw_ll_row_PAL,
			adraw_ll_row_PAL8,
			adraw_ll_row_GA,
			adraw_ll_row_RGB,
			adraw_ll_row_RGBA
		}
	};

	colortype = pii->bi.colortype;
	bitdepth = pii->bi.bitdepth;
		
	/* We can not handle some types of bitmaps */
	if (bitdepth > 8)
		return "Unsupported PNG bit depth\n";
	if ((bitdepth != 1) && (bitdepth != 2) && (bitdepth != 4)
	    && (bitdepth != 8))
		return "Invalid PNG bit depth\n";
	if (pii->bi.flags & BF_INTERLACED)
		return "No support for interlaced PNG images\n";
	p = (u_char *)(addr + 16);	  /* IHDR data */
	if (p[10] != 0)
		return "Unsupported PNG compression method\n";
	if (p[11] != 0)
		return "Unsupported PNG filter method\n";

	/* Compute some often used values */
	if (colortype == CT_GRAY_ALPHA)
		pixelsize = 16;		  /* 1 byte gray + 1 byte alpha */
	else if (colortype == CT_TRUECOL)
		pixelsize = 24;		  /* 3 bytes RGB */
	else if (colortype == CT_TRUECOL_ALPHA)
		pixelsize = 32;		  /* 4 bytes RGBA */
	else
		pixelsize = bitdepth;	  /* index or gray */
	rowlen = (pii->bi.hres * pixelsize + 7) / 8; /* round to bytes */

	/* Allocate row buffer for decoding two rows of the bitmap; we need
	   one additional byte per row for the filter type */
	prow = malloc(2*rowlen+2);
	if (!prow)
		return "Can't allocate decode buffer for PNG data";

	pii->rowmask = (1 << bitdepth)-1;
	pii->rowbitdepth = bitdepth;
	rowpos = (pii->xpix >> pii->dwidthshift) * pixelsize;

	/* Determine the correct draw_row function */
	{
		u_char temp = colortype;
		if (temp == CT_GRAY)
			temp = CT_PALETTE; /* Gray also uses the palette */
		if ((temp == CT_PALETTE) && (bitdepth == 8))
			temp = CT_GRAY;	  /* Optimized version for 8bpp */
		draw_row = draw_row_tab[pii->applyalpha][temp];
	}

	/* If we use the palette, fill it with gray gradient as default */
	if ((colortype == CT_PALETTE) || (colortype == CT_GRAY)) {
		u_int n = 1 << bitdepth;
		RGBA delta;
		RGBA gray;

		/* For each color component, we must multiply the palette
		   index with 255/(colorcount-1). This part can be precomputed
		   without loss of precision for all our possible bitdepths:
		     8bpp: 255/255 = 1 = 0x01, 4bpp: 255/15 = 17 = 0x11,
		     2bpp: 255/3 = 85 = 0x55, 1bpp: 255/1 = 255 = 0xFF
		   And what's more: we can compute it for R, G, B in one go
		   and we don't even need any shifting. And if we accumulate
		   the value in the loop, we don't even need to multiply. */
		gray = 0xFFFFFF00;
		delta = gray/(n - 1);
		do {
			palette[--n] = gray | 0xFF; /* A=0xFF (fully opaque) */
			gray -= delta;
		} while (n);

		if (!pii->applyalpha)
			palconverted = 0;
	}

	/* Fill reference row (for UP and PAETH filter) with 0 */
	current = rowlen + 1;
	memset(prow, 0, rowlen+1);

	/* Init zlib decompression */
	zs.zalloc = zalloc;
	zs.zfree = zfree;
	zs.next_in = Z_NULL;
	zs.avail_in = 0;
	zs.next_out = prow + current;
	zs.avail_out = rowlen+1;	  /* +1 for filter type */
#if defined(CONFIG_HW_WATCHDOG) || defined(CONFIG_WATCHDOG)
	zs.outcb = (cb_func)WATCHDOG_RESET;
#else
	zs.outcb = Z_NULL;
#endif	/* CONFIG_HW_WATCHDOG */
	if (inflateInit(&zs) != Z_OK)
		return "Can't initialize zlib\n";

	/* Go to first chunk after IHDR */
	addr += 8 + 8+13+4; 		  /* Signature size + IHDR size */

	/* Read chunks until IEND chunk is encountered; because we have called
	   lcd_scan_bitmap() above, we know that the PNG structure is OK and
	   we don't need to worry too much about bad chunks. */
	fsize = pixelsize/8;		  /* in bytes, at least 1 */
	if (fsize < 1)
		fsize = 1;
	do {
		u_char *p = (u_char *)addr;
		u_int chunk_size;
		u_int chunk_id;

		/* Get chunk ID and size */
		chunk_size = get_be32(p);
		chunk_id = get_be32(p+4);
		addr += chunk_size + 8+4;  /* chunk header size + CRC size */
		p += 8;			   /* move to chunk data */

		/* End of bitmap */
		if (chunk_id == CHUNK_IEND)
			break;

		/* Only accept PLTE chunk in palette bitmaps */
		if ((chunk_id == CHUNK_PLTE) && (colortype == CT_PALETTE)) {
			unsigned int i;
			unsigned int entries;

			/* Ignore extra data if chunk_size is wrong */
			entries = chunk_size/3;
			if (entries > 1 << bitdepth)
				entries = 1 << bitdepth;

			/* Store values in palette */
			for (i=0; i<entries; i++) {
				RGBA rgba;

				rgba = *p++ << 24;
				rgba |= *p++ << 16;
				rgba |= *p++ << 8;
				rgba |= 0xFF;	  /* Fully opaque */
				palette[i] = rgba;
			}
			continue;
		}

		/* Handle tRNS chunk */
		if (chunk_id == CHUNK_tRNS) {
			if ((colortype == CT_GRAY) && (chunk_size >= 2)) {
				u_int index = get_be16(p);

				/* Make palette entry transparent */
				if (index < 1 << bitdepth)
					palette[index] &= 0xFFFFFF00;
			} else if ((colortype == CT_TRUECOL)
				   && (chunk_size >= 6)) {
				RGBA rgba;

				rgba = get_be16(p) << 24;
				rgba |= get_be16(p+2) << 16;
				rgba |= get_be16(p+4) << 8;
				rgba |= 0x00; /* Transparent, is now valid */
				pii->trans_rgba = rgba;
			} else if (colortype == CT_PALETTE) {
				u_int i;
				u_int entries;

				entries = 1 << bitdepth;
				if (entries > chunk_size)
					entries = chunk_size;
				for (i=0; i<entries; i++)
					palette[i] = (palette[i]&~0xFF) | *p++;
			}
			continue;
		}

		/* Accept all other chunks, but ignore them */
		if (chunk_id != CHUNK_IDAT)
			continue;

		/* Handle IDAT chunk; if we come here for the first time and
		   have a palette based image, convert all colors to COLOR32
		   values; this avoids having to convert the values again and
		   again for each pixel. */
		if (!palconverted) {
			u_int i;
			const wininfo_t *pwi = pii->pwi;

			palconverted = 1;
			i = 1<<bitdepth;
			do {
				RGBA rgba = palette[--i];
				COLOR32 col = pwi->ppi->rgba2col(pwi, rgba);
				palette[i] = (RGBA)col;
			} while (i);
		}

		zs.next_in = p;		  /* Provide next data */
		zs.avail_in = chunk_size;

		/* Process image rows until we need more input data */
		do {
			int zret;

			/* Decompress data for next PNG row */
			zret = inflate(&zs, Z_SYNC_FLUSH);
			if (zret == Z_STREAM_END)
				zret = Z_OK;
			if (zret != Z_OK) {
				errmsg = "zlib decompression failed\n";
				break;
			}
			if (zs.avail_out == 0) {
				int i;

				/* Current row */
				u_char *pc = prow + current;
				/* Previous row */
				u_char *pp = prow + (rowlen+2) - current;
				u_char filtertype = *pc++;

				/* Apply the filter on this row */
				switch (filtertype) {
				case 0:			  /* none */
					break;

				case 1:			  /* sub */
					for (i=fsize; i<rowlen; i++)
						pc[i] += pc[i-fsize];
					break;

				case 2:			  /* up */
					for (i=0; i<rowlen; i++)
						pc[i] += pp[i];
					break;

				case 3:			  /* average */
					for (i=0; i<fsize; i++)
						pc[i] += pp[i]/2;
					for (; i<rowlen; i++)
						pc[i] += (pc[i-fsize]+pp[i])/2;
					break;

				case 4:			  /* paeth */
					for (i=0; i<fsize; i++)
						//pc[i] += paeth(0, pp[i], 0);
						pc[i] += pp[i];
					for (; i<rowlen; i++)
						pc[i] += paeth(pc[i-fsize],
							       pp[i],
							       pp[i-fsize]);
					break;
				}

				/* If row is in framebuffer range, draw it */
				do {
					XYPOS y = pii->y + pii->ypix;
					if (y>=0) {
						u_long fbuf;

						fbuf = y*pii->pwi->linelen;
						fbuf += pii->fbuf;
						pii->rowshift = 8-(rowpos & 7);
						pii->prow = prow + current + 1
							+ (rowpos>>3);
						draw_row(pii, (COLOR32 *)fbuf);
					}
					if (++pii->ypix >= pii->yend)
						goto DONE;
				} while (pii->ypix & pii->dheightshift);
			
				/* Toggle current between 0 and rowlen+1 */
				current = rowlen + 1 - current;
				zs.next_out = prow + current;
				zs.avail_out = rowlen + 1;
			}
		} while (zs.avail_in > 0); /* Repeat as long as we have data */
	} while (!errmsg);

DONE:
	/* Release resources of zlib decompression */
	inflateEnd(&zs);

	/* Free row buffer */
	free(prow);

	/* We're done */
	return errmsg;
}
#endif /* CONFIG_PNG */


#ifdef CONFIG_BMP
/* Get a little endian 32 bit number from address p (may be unaligned) */
static u_int get_le32(u_char *p)
{
	return (p[3]<<24) | (p[2]<<16) | (p[1]<<8) | p[0];
}

/* Get a little endian 16 bit number from address p (may be unaligned) */
static u_short get_le16(u_char *p)
{
	return (p[1] << 8) | p[0];
}


/* Draw BMP image. A BMP image consists of a file header telling file type and
   file size and a bitmap info header telling the image resolution, color type
   and compression method. This is followed by the palette data (in case of a
   palette bitmap) and the image data. The image data of each row is padded so
   that the row is a multiple of 4 bytes long.

   We do not support images with run-length encoded image data. The BMP format
   was added as it is rather simple to decode and displays very fast. If you
   need smaller bitmaps, use the PNG format, as this compresses much better
   than the BMP run-length encoding. We also do not support images with 16 or
   32 bits per pixel. These formats would require color masks and would be
   again rather slow.

   A normal BMP image is encoded bottom-up. But there are also images with
   top-down encoding. We support both types of BMP bitmaps. */
static const char *draw_bmp(imginfo_t *pii, u_long addr)
{
	u_char colortype;
	u_char bitdepth;
	u_int compression;
	u_int pixelsize;		  /* Size of one full pixel (bits) */
	u_int rowlen;
	int rowpos;
	u_char *p;
	draw_row_func_t draw_row;	  /* Draw bitmap row */

	static const draw_row_func_t draw_row_tab[2][6] = {
		{	
			/* ATTR_ALPHA = 0 */
			NULL,
			draw_ll_row_PAL,
			draw_ll_row_PAL8, /* Optimized for 8bpp */
			NULL,		  /* No BMPs with grayscale+alpha */
			draw_ll_row_BGR,
			draw_ll_row_BGRA
		},
		{
			/* ATTR_ALPHA = 1 */
			NULL,
			adraw_ll_row_PAL,
			adraw_ll_row_PAL8, /* Optimized for 8bpp */
			NULL,		   /* No BMPs with grayscale+alpha */
			adraw_ll_row_BGR,
			adraw_ll_row_BGRA
		}
	};

	colortype = pii->bi.colortype;
	bitdepth = pii->bi.bitdepth;

	/* We don't support more than 8 bits per color channel */
	if ((bitdepth > 8) || (bitdepth == 5))
		return "Unsupported BMP bit depth\n";
	if ((bitdepth != 1) && (bitdepth != 2) && (bitdepth != 4)
	    && (bitdepth != 8))
		return "Invalid BMP bit depth\n";
	p = (u_char *)(addr + 14);	  /* Bitmap Info Header */
	compression = get_le32(p+16);
	if (compression > 0)
		return "Unsupported BMP compression method\n";
	pixelsize = get_le16(p+14);

	/* Round to 32 bit value */
	rowlen = ((pii->bi.hres * pixelsize + 31) >> 5) << 2; /* in bytes */
	pii->rowmask = (1 << bitdepth)-1;
	rowpos = (pii->xpix >> pii->dwidthshift) * bitdepth;
	pii->rowbitdepth = bitdepth;

	/* Determine the correct draw_row function */
	{
		u_char temp = colortype;
		if ((temp == CT_PALETTE) && (bitdepth == 8))
			temp = CT_GRAY;	  /* Optimized version for 8bpp */
		else if ((temp == CT_TRUECOL) && (bitdepth == 5))
			temp = CT_GRAY_ALPHA; /* Version for 16bpp */
		draw_row = draw_row_tab[pii->applyalpha][temp];
	}

	/* Read palette from image */
	if (colortype == CT_PALETTE) {
		u_int entries;
		u_int i;
		const wininfo_t *pwi = pii->pwi;

		entries = get_le32(p+32);
		if (entries == 0)
			entries = 1 << bitdepth;
		p = (u_char *)(addr+54);  /* go to color table */
		for (i=0; i<entries; i++, p+=4) {
			RGBA rgba;

			rgba = p[0] << 8;   /* B[7:0] */
			rgba |= p[1] << 16; /* G[7:0] */
			rgba |= p[2] << 24; /* R[7:0] */
			rgba |= 0xFF;	    /* A[7:0] = 0xFF */

			/* If we do not apply alpha directly, i.e. we need the
			   COLOR2 value of these RGBA values later, then
			   already do the conversion now. This avoids having
			   to do this conversion over and over again for each
			   individual pixel of the bitmap. */
			if (!pii->applyalpha)
				rgba = (RGBA)pwi->ppi->rgba2col(pwi, rgba);
			palette[i] = rgba;
		}
	}

	/* Go to image data */
	p = (u_char *)addr;
	p += get_le32(p+10);

	if (pii->bi.flags & BF_BOTTOMUP) {
		/* Draw bottom-up bitmap; we have to recompute yend and ypix */
		pii->ypix = (pii->bi.vres << pii->dheightshift);
		pii->yend = (pii->y < 0) ? -pii->y : 0;

		for (;;) {
			/* If row is in framebuffer range, draw it */
			do {
				XYPOS y = pii->y + --pii->ypix;
				if (y < pii->pwi->fbvres) {
					u_long fbuf;

					fbuf = y*pii->pwi->linelen + pii->fbuf;
					pii->prow = p + (rowpos >> 3);
					pii->rowshift = 8-(rowpos & 7);
					draw_row(pii, (COLOR32 *)fbuf);
				}
				if (pii->ypix <= pii->yend)
					goto DONE;
			} while (pii->ypix & pii->dheightshift);
			p += rowlen;
		}
	} else {		
		/* Draw top-down bitmap */
		for (;;) {
			/* If row is in framebuffer range, draw it */
			do {
				XYPOS y = pii->y + pii->ypix;
				if (y >= 0) {
					u_long fbuf;

					fbuf = y*pii->pwi->linelen + pii->fbuf;
					pii->prow = p + (rowpos >> 3);
					pii->rowshift = 8 - (rowpos & 7);
					draw_row(pii, (COLOR32 *)fbuf);
				}
				if (++pii->ypix >= pii->yend)
					goto DONE;
			} while (pii->ypix & pii->dheightshift);
			p += rowlen;
		}
	}
DONE:
	/* We're done */
	return NULL;
}
#endif /* CONFIG_BMP */
	

/************************************************************************/
/* GRAPHICS PRIMITIVES							*/
/************************************************************************/

/* Fill display with FG color */
void lcd_fill(const wininfo_t *pwi)
{
	if (pwi->attr & ATTR_ALPHA)
		adraw_ll_rect(pwi, 0, 0, pwi->fbhres, pwi->fbvres);
	else
		memset32((unsigned *)pwi->pfbuf[pwi->fbdraw], pwi->fg.col,
			 pwi->fbsize/4);
}

/* Clear display with BG color */
void lcd_clear(const wininfo_t *pwi)
{
	memset32((unsigned *)pwi->pfbuf[pwi->fbdraw], pwi->bg.col,
		 pwi->fbsize/4);
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
		if ((y2 < 0) || (y1 > xmax))
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
 *   dd<0:  E:  deltaE  = F(dx+2, dy-1/2) - dd = 2*dx + 3
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
 *   dd>=0: S:  deltaS  = F(dx+1/2, dy-2) - dd = -2*dy + 3
 *
 * We also have to consider the case when switching the slope, i.e. when we go
 * from midpoint (dx+1, dy-1/2) to midpoint (dx+1/2, dy-1). Again we only need
 * the difference:
 *   delta = F(dx+1/2, dy-1) - F(dx+1, dy-1/2) = F(dx+1/2, dy-1) - dd
 *         = -dx - dy
 *
 * This results in the following basic circle algorithm:
 *     dx=0; dy=r; dd=1-r;
 *     while (dy>dx)                                   Slope >-1 (low)
 *         SetPixel(dx, dy);
 *         if (dd<0)                                   (*)
 *             dd = dd+2*dx+3; dx=dx+1;                East E
 *         else
 *             dd = dd+2*(dx-dy)+5; dx=dx+1; dy=dy-1;  Southeast SE
 *     dd = dd-dx-dy;
 *     while (dy>=0)                                   Slope <-1 (high)
 *         SetPixel(dx, dy)
 *         if (dd<0)
 *             dd = dd+2*(dx-dy)+5; dx=dx+1; dy=dy-1;  Southeast SE
 *         else
 *             dd = dd-2*dy+3; dy=dy-1;                South S
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
     Bit 4: 0: FG+BG, 1: no BG (transparent, bg ignored)
     Bit 5: 0: normal, 1: double width
     Bit 6: 0: normal, 1: double height
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
			draw_ll_char(pwi, x, y, c);
		x += width;
	}
}

/* Draw bitmap from address addr at (x, y) with alignment/attribute a */
const char *lcd_bitmap(const wininfo_t *pwi, XYPOS x, XYPOS y, u_long addr)
{
	imginfo_t ii;
	XYPOS hres, vres;
	XYPOS fbhres, fbvres;
	int xpos;
	u_int attr;

	const char *(*draw_bm_tab[])(imginfo_t *pii, u_long addr) = {
		NULL,
		draw_png,
		draw_bmp,
	};

	/* Do a quick scan if bitmap integrity is OK */
	if (!lcd_scan_bitmap(addr))
		return "Unknown bitmap type\n";

	/* Get bitmap info */
	lcd_get_bminfo(&ii.bi, addr);
	if (ii.bi.colortype == CT_UNKNOWN)
		return "Invalid bitmap color type\n";

	/* Prepare attribute */
	attr = pwi->attr;
	ii.applyalpha = ((attr & ATTR_ALPHA) != 0);
	ii.dwidthshift = ((attr & ATTR_DWIDTH) != 0);
	ii.dheightshift = ((attr & ATTR_DHEIGHT) != 0);

	/* Apply double width and double height */
	hres = (XYPOS)ii.bi.hres << ii.dwidthshift;
	vres = (XYPOS)ii.bi.vres << ii.dheightshift;

	/* Apply horizontal alignment */
	fbhres = (XYPOS)pwi->fbhres;
	fbvres = (XYPOS)pwi->fbvres;
	switch (attr & ATTR_HMASK) {
	case ATTR_HLEFT:
		break;

	case ATTR_HRIGHT:
		x -= hres-1;
		break;

	case ATTR_HCENTER:
		x -= hres/2;
		break;

	case ATTR_HSCREEN:
		x = (fbhres - hres)/2;
		break;
	}

	/* Apply vertical alignment */
	switch (attr & ATTR_VMASK) {
	case ATTR_VTOP:
		break;

	case ATTR_VBOTTOM:
		y -= vres-1;
		break;

	case ATTR_VCENTER:
		y -= vres/2;
		break;

	case ATTR_VSCREEN:
		y = (fbvres - vres)/2;
		break;
	}

	/* Return if image is completely outside of framebuffer */
	if ((x >= fbhres) || (x+hres < 0) || (y >= fbvres) || (y+vres < 0))
		return NULL;

	/* Compute end pixel in this row */
	ii.xend = hres;
	if (ii.xend + x > pwi->fbhres)
		ii.xend = pwi->fbhres - x;

	/* xpix counts the bitmap columns from 0 to xend; however if x < 0, we
	   start at the appropriate offset. */
	ii.xpix = 0;
	if (x < 0) {
		ii.xpix = -x;		  /* We start at an offset */
		x = 0;			  /* Current row position (pixels) */
	}

	ii.pwi = pwi;
	ii.bpp = 1 << pwi->ppi->bpp_shift;
	xpos = x * ii.bpp;
	ii.fbuf = pwi->pfbuf[pwi->fbdraw] + ((xpos >> 5) << 2);
	ii.shift = 32 - (xpos & 31);
	ii.mask = (1 << ii.bpp) - 1;	  /* This also works for bpp==32! */
	ii.trans_rgba = 0x000000FF;	  /* No transparent color set yet */
	ii.hash_rgba = 0x000000FF;	  /* Preload hash color */
	ii.hash_col = pwi->ppi->rgba2col(pwi, 0x000000FF);

	ii.ypix = 0;
	ii.yend = vres;
	ii.y = y;
	if (ii.yend + y > pwi->fbvres)
		ii.yend = pwi->fbvres - y;

	/* Actually draw the bitmap */
	return draw_bm_tab[ii.bi.type](&ii, addr);
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



/* Get bitmap information; should only be called if bitmap integrity is OK,
   i.e. after lcd_scan_bitmap() was successful */
void lcd_get_bminfo(bminfo_t *pbi, u_long addr)
{
	u_char *p = (u_char *)addr;

	/* Check for PNG image; a PNG image begins with an 8-byte signature,
	   followed by a IHDR chunk; the IHDR chunk consists of an 8-byte
	   header, 13 bytes data and a 4-byte checksum. As the IHDR chunk
	   header is also constant, we can directly compare 16 bytes. */
	if (memcmp(p, png_signature, 16) == 0) {
		static const u_char png_colortypes[7] = {
			CT_GRAY,	  /* 0 */
			CT_UNKNOWN,	  /* 1 */
			CT_TRUECOL,	  /* 2 */
			CT_PALETTE,	  /* 3 */
			CT_GRAY_ALPHA,	  /* 4 */
			CT_UNKNOWN,	  /* 5 */
			CT_TRUECOL_ALPHA  /* 6 */
		};

		/* The PNG IHDR structure consists of 13 bytes
		     Offset 0: width (4 bytes)
		     Offset 4: height (4 bytes)
		     Offset 8: bitdepth (1 byte)
		     Offset 9: color type (1 byte)
		     Offset 10: compression method (1 byte)
		     Offset 11: filter method (1 byte)
		     Offset 12: interlace method (1 byte)
		   We don't need the compression method and the filter method
		   here. */
		p += 16;		  /* go to IHDR data */
		pbi->type = BT_PNG;
		pbi->colortype = (p[9]>6) ? CT_UNKNOWN : png_colortypes[p[9]];
		pbi->bitdepth = p[8];
		pbi->flags = BF_COMPRESSED | (p[12] ? BF_INTERLACED : 0);
		pbi->hres = (HVRES)get_be32(p);
		pbi->vres = (HVRES)get_be32(p+4);

		return;
	}

	/* Check for BMP image. A BMP image starts with a 14-byte file header:
	   Offset 0: signature (2 bytes)
	   Offset 2: file size (unreliable, 4 bytes)
	   Offset 6: reserved, should be 0 (4 bytes)
	   Offset 10: image data offset (4 bytes)
	   We check for the signature and the reserved zero-value. */
	if ((p[0] == 'B') && (p[1] == 'M') && (get_le32(p+6) == 0)) {
		int vres;
		u_int c;

		/* After the file header follows a 40-byte bitmap info header:
		     Offset 0: size of bitmap info header: 40 (4 bytes)
		     Offset 4: width (4 bytes)
		     Offset 8: height; >0: bottom-up; <0: top-down (4 bytes)
		     Offset 12: planes: 1 (2 bytes)
		     Offset 14: bitdepth (2 bytes)
		     Offset 16: compression: 0: none, 1: RLE8, 2: RLE4,
		                3: bitfields (4 bytes)
		     Offset 20: image data size (4 bytes)
		     Offset 24: horizontal pixels per meter (4 bytes)
		     Offset 28: vertical pixels per meter (4 bytes)
		     Offset 32: Color table entries (0=max) (4 bytes)
		     Offset 36: Number of used colors (0=all) (4 bytes) */
		p += 14;
		pbi->type = BT_BMP;
		pbi->bitdepth = get_le16(p+14);
		switch (pbi->bitdepth) {
		case 1:
		case 4:
		case 8:
			pbi->colortype = CT_PALETTE;
			break;

		case 16:
			pbi->bitdepth = 5;
			pbi->colortype = CT_TRUECOL;
			break;
		case 24:
			pbi->bitdepth = 8;
			pbi->colortype = CT_TRUECOL;
			break;
		case 32:
			pbi->bitdepth = 8;
			pbi->colortype = CT_TRUECOL_ALPHA;
			break;

		default:
			pbi->colortype = CT_UNKNOWN;
			break;
		}
		c = get_le32(p+16);
		pbi->flags = ((c == 1) || (c == 2)) ? BF_COMPRESSED : 0;
		pbi->hres = (HVRES)get_le32(p+4);
		vres = (int)get_le32(p+8);
		if (vres < 0)
			vres = -vres;
		else
			pbi->flags |= BF_BOTTOMUP;
		pbi->vres = (HVRES)vres;

		return;
	}

	/* Unknown format */
	pbi->type = BT_UNKNOW;
	pbi->colortype = CT_UNKNOWN;
	pbi->bitdepth = 0;
	pbi->flags = 0;
	pbi->hres = 0;
	pbi->vres = 0;
}


/* Scan bitmap (check integrity) at addr and return end address */
u_long lcd_scan_bitmap(u_long addr)
{
	u_char *p = (u_char *)addr;

	/* Check for PNG image */
	if (memcmp(p, png_signature, 16) == 0) {
		addr += 8 + 8+13+4;		  /* Signature + IHDR */

		/* Read chunks until IEND chunk is encountered. */
		do
		{
			p = (u_char *)addr;

			/* Check for valid chunk header (characters) */
			if (!isalpha(p[4]) || !isalpha(p[5])
			    || !isalpha(p[6]) || !isalpha(p[7]))
				return 0;	  /* Invalid chunk header */

			/* Go to next chunk (header + data + CRC size) */
			addr += get_be32(p) + 8 + 4;
		} while (strncmp((char *)p+4, "IEND", 4) != 0);
	}

	/* Check for BMP image */
	else if ((p[0] == 'B') && (p[1] == 'M') && (get_le32(p+6) == 0)) {
		u_long filesize;

		/* The filesize is in the file header at offset 2; however
		   there are some BMP files where this value is not set. Then
		   use the start of the image data in the file header at
		   offset 10 and add to it the image data size from the bitmap
		   info header at offset 20. */
		filesize = get_le32(p+2);
		if (filesize == 0)
			filesize = get_le32(p+10) + get_le32(p+14+20);
		addr += filesize;
	} else
		addr = 0;		  /* Unknown bitmap type */

	return addr;
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

/* Initialize the console with the given window */
void console_init(wininfo_t *pwi)
{
	/* Initialize the console */
	console_pwi = pwi;
	coninfo.x = 0;
	coninfo.y = 0;
	coninfo.fg = pwi->fg.col;
	coninfo.bg = pwi->bg.col;
}

/* If the window is the console window, re-initialize console */
void console_update(wininfo_t *pwi)
{
	if (console_pwi->win == pwi->win)
		console_init(pwi);
}

#define TABWIDTH (8 * VIDEO_FONT_WIDTH)	  /* 8 chars for tab */
static void console_putc(wininfo_t *pwi, coninfo_t *pci, char c)
{
	int x = coninfo.x;
	int y = coninfo.y;
	int xtab;
	COLOR32 fg = coninfo.fg;   //##### nicht verwendet, sondern fg aus pwi
	COLOR32 bg = coninfo.bg;   //##### nicht verwendet, sondern bg aus pwi
	int fbhres = pwi->fbhres;
	int fbvres = pwi->fbvres;

	pwi->attr = 0;
	switch (c) {
	case '\t':			  /* Tab */
		xtab = ((x / TABWIDTH) + 1) * TABWIDTH;
		while ((x + VIDEO_FONT_WIDTH <= fbhres) && (x < xtab)) {
			draw_ll_char(pwi, x, y, ' ');
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
		draw_ll_char(pwi, x, y, ' ');
		break;

	default:			  /* Character */
		draw_ll_char(pwi, x, y, c);
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

	coninfo.x = (u_short)x;
	coninfo.y = (u_short)y;
}


/*----------------------------------------------------------------------*/

#ifdef CONFIG_MULTIPLE_CONSOLES
void lcd_putc(const char c, void *priv)
{
	wininfo_t *pwi = (wininfo_t *)priv;
	vidinfo_t *pvi = pwi->pvi;
	
	if (pvi->is_enabled && pwi->active)
		console_putc(pwi, &pwi->ci, c);
	else
		serial_putc(c);
}
#else
void lcd_putc(const char c)
{
	wininfo_t *pwi = console_pwi;
	vidinfo_t *pvi = pwi->pvi;
	
	if (pvi->is_enabled && pwi->active)
		console_putc(pwi, &coninfo, c);
	else
		serial_putc(c);
}
#endif /*CONFIG_MULTIPLE_CONSOLES*/

/*----------------------------------------------------------------------*/

#ifdef CONFIG_MULTIPLE_CONSOLES
void lcd_puts(const char *s, void *priv)
{
	wininfo_t *pwi = (wininfo_t *)priv;
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
		serial_puts(s);
}
#else
void lcd_puts(const char *s)
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
		serial_puts(s);
}
#endif /*CONFIG_MULTIPLE_CONSOLES*/
