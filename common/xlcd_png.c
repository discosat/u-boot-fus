/*
 * Hardware independent LCD support for PNG files
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
#include <malloc.h>			  /* malloc(), free() */
#include <linux/ctype.h>		  /* isalpha() */
#include <u-boot/zlib.h>		  /* z_stream, inflateInit(), ... */
#include <watchdog.h>			  /* WATCHDOG_RESET() */

#ifdef CONFIG_CMD_DRAW
#include <xlcd_draw_ll.h>		  /* draw_ll_row_*() */
#endif

#ifdef CONFIG_CMD_ADRAW
#include <xlcd_adraw_ll.h>		  /* adraw_ll_row_*() */
#endif

#if CONFIG_XLCD_DRAW & XLCD_DRAW_BITMAP

extern void *zalloc(void *, unsigned, unsigned);
extern void zfree(void *, void *, unsigned);


/************************************************************************/
/* DEFINITIONS								*/
/************************************************************************/

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


/************************************************************************/
/* LOCAL VARIABLES							*/
/************************************************************************/

/* PNG signature and IHDR header; this must be the start of each PNG bitmap */
static const u_char png_signature[16] = {
	137, 80, 78, 71, 13, 10, 26, 10,  /* PNG signature */
	0, 0, 0, 13, 'I', 'H', 'D', 'R'	  /* IHDR chunk header */
};

static RGBA palette[256];


/************************************************************************/
/* Local Helper Functions						*/
/************************************************************************/

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


/************************************************************************/
/* Exported functions							*/
/************************************************************************/

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
const char *draw_png(imginfo_t *pii, u_long addr)
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

	static const draw_row_func_t draw_row_tab[][6] = {
#ifdef CONFIG_CMD_DRAW
		{
			/* ATTR_ALPHA = 0 */
			NULL,
			draw_ll_row_PAL,
			draw_ll_row_PAL8,
			draw_ll_row_GA,
			draw_ll_row_RGB,
			draw_ll_row_RGBA
		},
#endif
#ifdef CONFIG_CMD_ADRAW
		{
			/* ATTR_ALPHA = 1 */
			NULL,
			adraw_ll_row_PAL,
			adraw_ll_row_PAL8,
			adraw_ll_row_GA,
			adraw_ll_row_RGB,
			adraw_ll_row_RGBA
		}
#endif
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
	rowpos = (pii->xpix / pii->multiwidth) * pixelsize;
	pii->palette = palette;

	/* Determine the correct draw_row function */
	{
		u_char temp = colortype;
		if (temp == CT_GRAY)
			temp = CT_PALETTE; /* Gray also uses the palette */
		if ((temp == CT_PALETTE) && (bitdepth == 8))
			temp = CT_GRAY;	  /* Optimized version for 8bpp */
#if defined(CONFIG_CMD_DRAW) && defined(CONFIG_CMD_ADRAW)
		draw_row = draw_row_tab[pii->applyalpha][temp];
#else
		draw_row = draw_row_tab[0][temp];
#endif
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

	WATCHDOG_RESET();

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
			WATCHDOG_RESET();
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
				} while (pii->ypix % pii->multiheight);

				/* Toggle current between 0 and rowlen+1 */
				current = rowlen + 1 - current;
				zs.next_out = prow + current;
				zs.avail_out = rowlen + 1;
			}
			WATCHDOG_RESET();
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


/* Get a bminfo structure with PNG bitmap information */
int get_bminfo_png(bminfo_t *pbi, u_long addr)
{
	u_char *p = (u_char *)addr;
	static const u_char png_colortypes[7] = {
		CT_GRAY,	  /* 0 */
		CT_UNKNOWN,	  /* 1 */
		CT_TRUECOL,	  /* 2 */
		CT_PALETTE,	  /* 3 */
		CT_GRAY_ALPHA,	  /* 4 */
		CT_UNKNOWN,	  /* 5 */
		CT_TRUECOL_ALPHA  /* 6 */
	};

	/* Check for PNG image; a PNG image begins with an 8-byte signature,
	   followed by a IHDR chunk; the IHDR chunk consists of an 8-byte
	   header, 13 bytes data and a 4-byte checksum. As the IHDR chunk
	   header is also constant, we can directly compare 16 bytes. */
	if (memcmp(p, png_signature, 16) != 0)
		return 0;

	/* The PNG IHDR structure consists of 13 bytes
	     Offset 0: width (4 bytes)
	     Offset 4: height (4 bytes)
	     Offset 8: bitdepth (1 byte)
	     Offset 9: color type (1 byte)
	     Offset 10: compression method (1 byte)
	     Offset 11: filter method (1 byte)
	     Offset 12: interlace method (1 byte)
	   We don't need the compression method and the filter method here. */
	p += 16;			  /* go to IHDR data */
	pbi->type = BT_PNG;
	pbi->colortype = (p[9]>6) ? CT_UNKNOWN : png_colortypes[p[9]];
	pbi->bitdepth = p[8];
	pbi->flags = BF_COMPRESSED | (p[12] ? BF_INTERLACED : 0);
	pbi->hres = (XYPOS)get_be32(p);
	pbi->vres = (XYPOS)get_be32(p+4);

	return 1;
}


/* Scan integrity of a PNG bitmap and return end address */
u_long scan_png(u_long addr)
{
	u_char *p = (u_char *)addr;

	/* Check for PNG image */
	if (memcmp(p, png_signature, 16) != 0)
		return 0;

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

#endif /* CONFIG_XLCD_DRAW & XLCD_DRAW_BITMAP */
