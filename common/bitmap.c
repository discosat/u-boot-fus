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
   3. Support for JPG bitmaps is prepared, but needs to be fully implemented.
   4. Probably split file and move PNG part to png.c, BMP part to bmp.c and
      JPG part to jpg.c. Only keep generic bitmap functions here.
 */

/************************************************************************/
/* ** HEADER FILES							*/
/************************************************************************/

/* #define DEBUG */

#include <config.h>
#include <common.h>
#include <command.h>			  /* U_BOOT_CMD */
#include <cmd_lcd.h>			  /* wininfo_t, pixinfo_t */
#include <bitmap.h>			  /* Own interface */
#include <malloc.h>			  /* malloc(), free() */
#include <linux/ctype.h>		  /* isalpha() */
#include <zlib.h>			  /* z_stream, inflateInit(), ... */
#include <watchdog.h>


/************************************************************************/
/* DEFINITIONS								*/
/************************************************************************/

/* Bitmap color types */
#define CT_UNKNOWN 0
#define CT_PALETTE 1			  /* Pixels refer to a color map */
#define CT_GRAY 2			  /* Pixels are gray values */
#define CT_GRAY_ALPHA 3			  /* Pixels are gray + alpha values */
#define CT_TRUECOL 4			  /* Pixels are RGB values */
#define CT_TRUECOL_ALPHA 5		  /* Pixels are RGBA values */

/* Bitmap flags; more flags may follow */
#define BF_INTERLACED 0x01		  /* Bitmap is stored interlaced */
#define BF_COMPRESSED 0x02		  /* Bitmap is compressed */
#define BF_BOTTOMUP   0x04		  /* Bitmap is stored bottom up */


#ifdef CONFIG_CMD_PNG
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
#endif /* CONFIG_CMD_PNG */


/************************************************************************/
/* ENUMERATIONS								*/
/************************************************************************/

/* Bitmap types */
enum bitmap_types {
	BT_UNKNOWN,
#ifdef CONFIG_CMD_PNG
	BT_PNG,
#endif
#ifdef CONFIG_CMD_BMP
	BT_BMP,
#endif
#ifdef CONFIG_CMD_JPG
	BT_JPG,				  /* ### Not supported yet ### */
#endif
};


/************************************************************************/
/* TYPES AND STRUCTURES							*/
/************************************************************************/

/* Bitmap information */
typedef struct bminfo {
	u_char type;			  /* Bitmap type (BT_*) */
	u_char colortype;		  /* Color type (CT_*) */
	u_char bitdepth;		  /* Per color channel */
	u_char flags;			  /* Flags (BF_*) */
	XYPOS hres;			  /* Width of bitmap in pixels */
	XYPOS vres;			  /* Height of bitmap in pixels */
	const char *bm_name;		  /* Bitmap type "BMP", "PNG", ... */
	const char *ct_name;		  /* Color type "palette", ... */
} bminfo_t;

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


/************************************************************************/
/* LOCAL VARIABLES							*/
/************************************************************************/

/* Bitmap color types */
static const char * const ctype_tab[] = {
	"(unknown)",			  /* CT_UNKNOWN */
	"palette",			  /* CT_PALETTE */
	"grayscale",			  /* CT_GRAY */
	"grayscale+alpha",		  /* CT_GRAY_ALPHA */
	"truecolor",			  /* CT_TRUECOL */
	"truecolor+alpha",		  /* CT_TRUECOL_ALPHA */
};

#ifdef CONFIG_CMD_PNG
/* PNG signature and IHDR header; this must be the start of each PNG bitmap */
static const u_char png_signature[16] = {
	137, 80, 78, 71, 13, 10, 26, 10,  /* PNG signature */
	0, 0, 0, 13, 'I', 'H', 'D', 'R'	  /* IHDR chunk header */
};
#endif /* CONFIG_CMD_PNG */

/* Buffer for palette */
static RGBA palette[256];

#if defined(CONFIG_CMD_BMP) || defined(CONFIG_CMD_PNG)
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
	}
DONE:
	*p = val;			  /* Store final value */
}
#endif /* CONFIG_CMD_BMP || CONFIG_CMD_PNG */


#ifdef CONFIG_CMD_PNG
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
#endif /* CONFIG_CMD_PNG */


#ifdef CONFIG_CMD_BMP
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
#endif /* CONFIG_CMD_BMP */


/************************************************************************/
/* DRAW PNG IMAGES							*/
/************************************************************************/

#ifdef CONFIG_CMD_PNG
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
#endif /* CONFIG_CMD_PNG */


/************************************************************************/
/* DRAW BMP IMAGES							*/
/************************************************************************/

#ifdef CONFIG_CMD_BMP
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
#endif /* CONFIG_CMD_BMP */


/************************************************************************/
/* DRAW JPG IMAGES							*/
/************************************************************************/

#ifdef CONFIG_CMD_JPG
/* Draw JPG image */
static const char *draw_jpg(imginfo_t *pii, u_long addr)
{
	return "JPG images not yet supported";
}
#endif /* CONFIG_CMD_JPG */


/************************************************************************/
/* GENERIC BITMAP FUNCTIONS						*/
/************************************************************************/

const struct bmtype {
	const char *(*draw_bm)(imginfo_t *pii, u_long addr);
	const char *name;
} bmtype_tab[] = {
	[BT_UNKNOWN] = {NULL,     "???"},
#ifdef CONFIG_CMD_PNG
	[BT_PNG] =     {draw_png, "PNG"},
#endif
#ifdef CONFIG_CMD_BMP
	[BT_BMP] =     {draw_bmp, "BMP"},
#endif
#ifdef CONFIG_CMD_JPG
	[BT_JPG] =     {draw_jpg, "JPG"},
#endif
};


/* Get bitmap information; should only be called if bitmap integrity is OK,
   i.e. after lcd_scan_bitmap() was successful */
static void lcd_get_bminfo(bminfo_t *pbi, u_long addr)
{
	u_char *p = (u_char *)addr;

#ifdef CONFIG_CMD_PNG
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
		pbi->hres = (XYPOS)get_be32(p);
		pbi->vres = (XYPOS)get_be32(p+4);
	} else
#endif /* CONFIG_CMD_PNG */

#ifdef CONFIG_CMD_BMP
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
		pbi->hres = (XYPOS)get_le32(p+4);
		vres = (int)get_le32(p+8);
		if (vres < 0)
			vres = -vres;
		else
			pbi->flags |= BF_BOTTOMUP;
		pbi->vres = (XYPOS)vres;
	} else
#endif /* CONFIG_CMD_BMP */

#ifdef CONFIG_CMD_JPG
	if (0) {
		/* TODO: add JPG support */
	} else
#endif /* CONFIG_CMD_JPG */

	{
		/* Unknown format */
		pbi->type = BT_UNKNOWN;
		pbi->colortype = CT_UNKNOWN;
		pbi->bitdepth = 0;
		pbi->flags = 0;
		pbi->hres = 0;
		pbi->vres = 0;
	}

	/* Load bitmap type and color type string */
	pbi->bm_name = bmtype_tab[pbi->type].name;
	pbi->ct_name = ctype_tab[pbi->colortype];
}


/* Draw bitmap from address addr at (x, y) with alignment/attribute a */
const char *lcd_bitmap(const wininfo_t *pwi, XYPOS x, XYPOS y, u_long addr)
{
	imginfo_t ii;
	XYPOS hres, vres;
	XYPOS fbhres, fbvres;
	int xpos;
	u_int attr;

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
	return bmtype_tab[ii.bi.type].draw_bm(&ii, addr);
}


/* Scan bitmap (check integrity) at addr and return end address */
u_long lcd_scan_bitmap(u_long addr)
{
	u_char *p = (u_char *)addr;

#ifdef CONFIG_CMD_PNG
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
		return addr;
	}
#endif /* CONFIG_CMD_PNG */

#ifdef CONFIG_CMD_BMP
	/* Check for BMP image */
	if ((p[0] == 'B') && (p[1] == 'M') && (get_le32(p+6) == 0)) {
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

		return addr;
	}
#endif /* CONFIG_CMD_BMP */

#ifdef CONFIG_CMD_JPG
	/* Check for JPG image */
	if (0) {
		/* TODO: Add support for scanning a JPG bitmap */
		return addr;
	}
#endif /* CONFIG_CMD_JPG */

	return 0;			  /* Unknown bitmap type */
}


/************************************************************************/
/* Command bminfo							*/
/************************************************************************/

static int do_bminfo(cmd_tbl_t *cmdtp, int flag, int argc, char *argv[])
{
	u_int base, addr;
	u_int i;
	u_int start;
	u_int count;

	if (argc < 2) {
		printf("Missing argument\n");
		return 1;
	}

	/* Get base address */
	base = simple_strtoul(argv[1], NULL, 16);

	start = 0;
	if (argc > 2)
		start = simple_strtoul(argv[2], NULL, 0);

	count = 0xFFFFFFFF;
	if (argc > 3)
		count = simple_strtoul(argv[3], NULL, 0);

	/* Print header line for bitmap info list */
	printf("#\tOffset\t\thres x vres\tbpp\tType\tCIB\tInfo\n");
	printf("--------------------------------------------"
	       "-----------------------------------\n");
	addr = base;
	for (i=0; ; i++)
	{
		bminfo_t bi;
		u_int bmaddr = addr;

		/* Scan bitmap structure and get end of current bitmap; stop
		   on error, this is usually the end of the (multi-)image */
		addr = lcd_scan_bitmap(addr);
		if (!addr)
			break;

		/* Get bitmap info and show it */
		lcd_get_bminfo(&bi, bmaddr);
		if (i >= start) {
			printf("%d\t0x%08x\t%4d x %d\t%d\t%s\t%c%c%c\t%s\n",
			       i, bmaddr - base, bi.hres, bi.vres, bi.bitdepth,
			       bi.bm_name,
			       (bi.flags & BF_COMPRESSED) ? 'C' : '-',
			       (bi.flags & BF_INTERLACED) ? 'I' : '-',
			       (bi.flags & BF_BOTTOMUP) ? 'B' : '-',
			       bi.ct_name);
			if (--count == 0)
				break;
		}
	}
	if (!i)
		puts("(no bitmap found)\n");

	return 0;
}

U_BOOT_CMD(
	bminfo, 4,	1,	do_bminfo,
	"bminfo\t- show (multi-)bitmap information in a list\n",
	"addr [start [count]]\n"
	"    - show information about bitmap(s) stored at addr\n"
);


