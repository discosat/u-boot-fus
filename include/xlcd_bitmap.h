/*
 * Support for bitmaps (PNG, BMP, JPG)
 *
 * (C) Copyright 2012
 * Hartmut Keller, F&S Elektronik Systeme GmbH, keller@fs-net.de
 *
 * SPDX-License-Identifier:	GPL-2.0+
 */

#ifndef _XLCD_BITMAP_H_
#define _XLCD_BITMAP_H_

#include "cmd_lcd.h"			  /* wininfo_t, XYPOS */


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


/************************************************************************/
/* ENUMERATIONS								*/
/************************************************************************/

/* Bitmap types */
enum bitmap_types {
	BT_UNKNOWN,
#ifdef CONFIG_XLCD_PNG
	BT_PNG,
#endif
#ifdef CONFIG_XLCD_BMP
	BT_BMP,
#endif
#ifdef CONFIG_XLCD_JPG
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
	u_int multiwidth;		  /* Width factor (1..4) */
	u_int multiheight;		  /* Height factor (1..4) */
	RGBA trans_rgba;		  /* Transparent color if truecolor */
	RGBA *palette;			  /* Pointer to bitmap palette */
	bminfo_t bi;			  /* Generic bitmap information */
};


/************************************************************************/
/* PROTOTYPES OF EXPORTED FUNCTIONS 					*/
/************************************************************************/

/* Each module supporting a bitmap type must provide three functions:
   draw_xxx() to draw the bitmap
   get_bminfo_xxx() to fill a bminfo structure
   scan_xxx() to scan and skip a bitmap of that type */

/* PNG support */
extern const char *draw_png(imginfo_t *pii, u_long addr);
extern int get_bminfo_png(bminfo_t *pbi, u_long addr);
extern u_long scan_png(u_long addr);

/* BMP support */
extern const char *draw_bmp(imginfo_t *pii, u_long addr);
extern int get_bminfo_bmp(bminfo_t *pbi, u_long addr);
extern u_long scan_bmp(u_long addr);

/* JPG support */
extern const char *draw_jpg(imginfo_t *pii, u_long addr);
extern int get_bminfo_jpg(bminfo_t *pbi, u_long addr);
extern u_long scan_jpg(u_long addr);

#endif	/* _XLCD_BITMAP_H_ */
