/*
 * Hardware independent LCD support for BMP files
 *
 * (C) Copyright 2012
 * Hartmut Keller, F&S Elektronik Systeme GmbH, keller@fs-net.de
 *
 * SPDX-License-Identifier:	GPL-2.0+
 */

#include <config.h>
#include <common.h>
#include <cmd_lcd.h>			  /* wininfo_t, pixinfo_t */
#include <xlcd_bitmap.h>		  /* bminfo_t, CT_*, ... */
#include <watchdog.h>			  /* WATCHDOG_RESET() */

#ifdef CONFIG_CMD_DRAW
#include <xlcd_draw_ll.h>		  /* draw_ll_row_*() */
#endif

#ifdef CONFIG_CMD_ADRAW
#include <xlcd_adraw_ll.h>		  /* adraw_ll_row_*() */
#endif

#if CONFIG_XLCD_DRAW & XLCD_DRAW_BITMAP

/************************************************************************/
/* DEFINITIONS								*/
/************************************************************************/

/************************************************************************/
/* LOCAL VARIABLES							*/
/************************************************************************/
static RGBA palette[256];

/************************************************************************/
/* Local Helper Functions						*/
/************************************************************************/

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


/************************************************************************/
/* Exported functions							*/
/************************************************************************/

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
const char *draw_bmp(imginfo_t *pii, u_long addr)
{
	u_char colortype;
	u_char bitdepth;
	u_int compression;
	u_int pixelsize;		  /* Size of one full pixel (bits) */
	u_int rowlen;
	int rowpos;
	u_char *p;
	draw_row_func_t draw_row;	  /* Draw bitmap row */

	static const draw_row_func_t draw_row_tab[][6] = {
#ifdef CONFIG_CMD_DRAW
		{
			/* ATTR_ALPHA = 0 */
			NULL,
			draw_ll_row_PAL,
			draw_ll_row_PAL8, /* Optimized for 8bpp */
			NULL,		  /* No BMPs with grayscale+alpha */
			draw_ll_row_BGR,
			draw_ll_row_BGRA
		},
#endif
#ifdef CONFIG_CMD_ADRAW
		{
			/* ATTR_ALPHA = 1 */
			NULL,
			adraw_ll_row_PAL,
			adraw_ll_row_PAL8, /* Optimized for 8bpp */
			NULL,		   /* No BMPs with grayscale+alpha */
			adraw_ll_row_BGR,
			adraw_ll_row_BGRA
		}
#endif
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
	rowpos = (pii->xpix / pii->multiwidth) * bitdepth;
	pii->rowbitdepth = bitdepth;
	pii->palette = palette;

	/* Determine the correct draw_row function */
	{
		u_char temp = colortype;
		if ((temp == CT_PALETTE) && (bitdepth == 8))
			temp = CT_GRAY;	  /* Optimized version for 8bpp */
		else if ((temp == CT_TRUECOL) && (bitdepth == 5))
			temp = CT_GRAY_ALPHA; /* Version for 16bpp */
#if defined(CONFIG_CMD_DRAW) && defined(CONFIG_CMD_ADRAW)
		draw_row = draw_row_tab[pii->applyalpha][temp];
#else
		draw_row = draw_row_tab[0][temp];
#endif
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
		pii->ypix = (pii->bi.vres * pii->multiheight);
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
			} while (pii->ypix % pii->multiheight);
			p += rowlen;
			WATCHDOG_RESET();
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
			} while (pii->ypix % pii->multiheight);
			p += rowlen;
			WATCHDOG_RESET();
		}
	}
DONE:
	/* We're done */
	return NULL;
}


/* Get a bminfo structure with BMP bitmap information */
int get_bminfo_bmp(bminfo_t *pbi, u_long addr)
{
	int vres;
	u_int c;
	u_char *p = (u_char *)addr;

	/* Check for BMP image. A BMP image starts with a 14-byte file header:
	   Offset 0: signature (2 bytes)
	   Offset 2: file size (unreliable, 4 bytes)
	   Offset 6: reserved, should be 0 (4 bytes)
	   Offset 10: image data offset (4 bytes)
	   We check for the signature and the reserved zero-value. */
	if ((p[0] != 'B') && (p[1] != 'M') && (get_le32(p+6) != 0))
		return 0;

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

	return 1;
}


/* Scan integrity of a BMP bitmap and return end address */
u_long scan_bmp(u_long addr)
{
	u_long filesize;
	u_char *p = (u_char *)addr;

	/* Check for BMP image */
	if ((p[0] != 'B') || (p[1] != 'M') || (get_le32(p+6) != 0))
		return 0;

	/* The filesize is in the file header at offset 2; however there are
	   some BMP files where this value is not set. Then use the start of
	   the image data in the file header at offset 10 and add to it the
	   image data size from the bitmap info header at offset 20. */
	filesize = get_le32(p+2);
	if (filesize == 0)
		filesize = get_le32(p+10) + get_le32(p+14+20);
	addr += filesize;

	return addr;
}

#endif /* CONFIG_XLCD_DRAW & XLCD_DRAW_BITMAP */
