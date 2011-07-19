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

/* ###### Noch einzubauen:
   CONFIG_LCD_LOGO
   CONFIG_LCD_INFO_BELOW_LOGO
   ######
*/

/* Get a big endian 32 or 16 bit number from address p (may be unaligned) */
#define get_be32(p) (((((((p)[0]<<8)|(p)[1])<<8)|(p)[2])<<8)|(p)[3])
#define get_be16(p) (((p)[0]<<8)|(p)[1])

/* Get a little endian 32 or 16 bit number from address p (may be unaligned) */
#define get_le32(p) (((((((p)[3]<<8)|(p)[2])<<8)|(p)[1])<<8)|(p)[0])
#define get_le16(p) (((p)[1]<<8)|(p)[0])

/* We build the basic mask by using arithmetic right shift */ 
#define BASEMASK(bpp) (COLOR32)((signed)0x80000000 >> (bpp-1))

/* Maximum length of a PNG row while decoding */
#define MAX_PNGROWLEN 4096

/* PNG chunk ids */
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



#define RMASK 0xFF000000
#define GMASK 0x00FF0000
#define BMASK 0x0000FF00
#define AMASK 0x000000FF


/************************************************************************/
/* ** LOGO DATA								*/
/************************************************************************/
#ifdef CONFIG_LCD_LOGO
# include <bmp_logo.h>		/* Get logo data, width and height	*/
# if (CONSOLE_COLOR_WHITE >= BMP_LOGO_OFFSET) && (LCD_BPP != LCD_COLOR16)
#  error Default Color Map overlaps with Logo Color Map
# endif
#endif

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

/* Possible modes when scanning a PNG bitmap */
typedef enum SCANMODE
{
	SM_SKIP,			  /* Just skip file */
	SM_INFO,			  /* Just load image info (for list) */
	SM_LOAD				  /* Load image */
} scanmode_t;

/* Type for bitmap information */
typedef struct imginfo imginfo_t;	  /* Forward declaration */

/* Type for lowlevel-drawing a bitmap row */
typedef void (*draw_row_func_t)(const imginfo_t *, const u_char *, COLOR32 *);

/* Image information */
struct imginfo
{
	/* Framebuffer info */
	const wininfo_t *pwi;		  /* Pointer to windo info */
	XYPOS xpix;			  /* Current bitmap column */
	XYPOS xend;			  /* Last bitmap column to draw */
	XYPOS y;			  /* y-coordinate to draw to */
	COLOR32 mask;			  /* Mask used in framebuffer */
	u_int bpp;			  /* Framebuffer bitdepth */
	u_int shift;			  /* Shift value for current x */
	u_long fbuf;			  /* Frame buffer addres at current x */

	/* Bitmap source data info */
	u_int rowmask;			  /* Mask used in bitmap data */
	u_int rowbitdepth;		  /* Bitmap bitdepth */
	u_int rowshift;			  /* Shift value for current pos */

	u_int applyalpha;		  /* ATTR_ALPHA 0: not set, 1: set */
	u_int dwidthshift;		  /* ATTR_DWIDTH 0: not set, 1: set */
	u_int dheightshift;		  /* ATTR_DHEIGHT 0: not set, 1: set */
	RGBA trans_rgba;		  /* Transparent color if truecolor */
	bminfo_t bi;			  /* Generic bitmap information */
};


/************************************************************************/
/* LOCAL VARIABLES							*/
/************************************************************************/

/* PNG signature and IHDR header; this must be the start of each PNG bitmap */
static const u_char png_signature[16] = {
	137, 80, 78, 71, 13, 10, 26, 10,  /* PNG signature */
	0, 0, 0, 13, 'I', 'H', 'D', 'R'	  /* IHDR chunk header */
};

/* Buffer for two PNG rows; one to fill, one for the line before for reference
   (required in some filters) */
static u_char row[2][MAX_PNGROWLEN+1];	  /* +1 for filter type */

/* Buffer for palette */
static RGBA palrgba[256];
static COLOR32 palcol32[256];


/************************************************************************/
/* PROTOTYPES OF LOCAL FUNCTIONS					*/
/************************************************************************/

static int lcd_init(void *lcdbase);

static void *lcd_logo (void);


#if LCD_BPP == LCD_COLOR8
extern void lcd_setcolreg (ushort regno,
				ushort red, ushort green, ushort blue);
#endif
#if LCD_BPP == LCD_MONOCHROME
extern void lcd_initcolregs (void);
#endif

static int lcd_getbgcolor (void);
static void lcd_setfgcolor (int color);
static void lcd_setbgcolor (int color);

char lcd_is_enabled = 0;

#ifdef	NOT_USED_SO_FAR
static void lcd_getcolreg (ushort regno,
				ushort *red, ushort *green, ushort *blue);
static int lcd_getfgcolor (void);
#endif	/* NOT_USED_SO_FAR */

static void console_putc(wininfo_t *pwi, coninfo_t *pci, char c);

/* Draw pixel; pixel is definitely visible */
static void lcd_ll_pixel(const wininfo_t *pwi, XYPOS x, XYPOS y, COLOR32 color);

/* Draw filled rectangle; given region is definitely visible */
static void lcd_ll_rect(const wininfo_t *pwi, XYPOS x1, XYPOS y1,
			XYPOS x2, XYPOS y2, COLOR32 color);

/* Draw a character; character area is definitely visible */
static void lcd_ll_char(const wininfo_t *pwi, XYPOS x, XYPOS y, char c, u_int a,
			COLOR32 fg, COLOR32 bg);


/************************************************************************/
/* ** Low-Level Graphics Routines					*/
/************************************************************************/

static void lcd_ll_pixel(const wininfo_t *pwi, XYPOS x, XYPOS y, COLOR32 color)
{
	u_int shift = pwi->ppi->bpp_shift;
	int xpos = x << shift;
	u_int bpp = 1 << shift;
	u_long fbuf;
	COLOR32 mask = (1 << bpp) - 1;
	COLOR32 *p;

	/* Compute framebuffer address of the pixel */
	fbuf = pwi->linelen * y + pwi->pfbuf[pwi->fbdraw];

	/* Shift the mask to the appropriate pixel */
	mask <<= 32 - (xpos & 31) - bpp;

	/* Remove old pixel and fill in new pixel */
	p = (COLOR32 *)(fbuf + ((xpos >> 5) << 2));
	*p = (*p & ~mask) | (color & mask);
}


/* Draw filled rectangle; given region is definitely visible */
static void lcd_ll_rect(const wininfo_t *pwi, XYPOS x1, XYPOS y1,
			XYPOS x2, XYPOS y2, COLOR32 color)
{
	u_int shift = pwi->ppi->bpp_shift;
	u_int bpp = 1 << shift;
	u_int x_shift = 5-shift;
	u_int x_mask = (1 << x_shift) - 1;
	u_long fbuf;
	u_long linelen = pwi->linelen;
	COLOR32 maskleft, maskright;
	COLOR32 *p;
	int count;

	/* The left mask consists of 1s starting at pixel x1 */
	maskleft = 0xFFFFFFFF >> ((x1 & x_mask)*bpp);

	/* The right mask consists of 1s up to pixel x2 */
	maskright = 0xFFFFFFFF << ((x_mask - (x2 & x_mask))*bpp);

	/* Compute framebuffer address of the beginning of the top row */
	fbuf = linelen * y1 + pwi->pfbuf[pwi->fbdraw];

	count = y2 - y1 + 1;
	x1 >>= x_shift;
	x2 >>= x_shift;
	if (x1 < x2) {
		/* Fill rectangle consisting of several words per row */
		do {
			int i;

			/* Handle leftmost word in row */
			p = (COLOR32 *)fbuf + x1;
			*p = (*p & ~maskleft) | (color & maskleft);
			p++;

			/* Fill all middle words without masking */
			for (i = x1 + 1; i < x2; i++)
				*p++ = color;

			/* Handle rightmost word in row */
			*p = (*p & ~maskright) | (color & maskright);

			/* Go to next row */
			fbuf += linelen;
		} while (--count);
	} else {
		/* Optimized version for the case where only one word has to be
		   checked in each row; this includes vertical lines */
		maskleft &= maskright;
		p = (COLOR32 *)fbuf + x1;
		do {
			*p = (*p & ~maskleft) | (color & maskleft);
			p += linelen >> 2;
		} while (--count);
	}
}


/* Draw a character; character area is definitely visible */
static void lcd_ll_char(const wininfo_t *pwi, XYPOS x, XYPOS y, char c, u_int a,
			COLOR32 fg, COLOR32 bg)
{
	u_int shift = pwi->ppi->bpp_shift;
	u_int bpp = 1 << shift;
	u_int x_shift = 5-shift;
	u_int x_mask = (1 << x_shift) - 1;
	u_long fbuf;
	u_long linelen = pwi->linelen;
	XYPOS underl = (a & ATTR_UNDERL) ? VIDEO_FONT_UNDERL : -1;
	XYPOS strike = (a & ATTR_STRIKE) ? VIDEO_FONT_STRIKE : -1;
	VIDEO_FONT_TYPE *pfont;
	COLOR32 mask;

	/* Compute framebuffer address of the pixel */
	fbuf = linelen * y + pwi->pfbuf[pwi->fbdraw];

	/* We build the basic mask by using arithmetic right shift */ 
	mask = BASEMASK(bpp);

	/* Shift the mask to the appropriate pixel */
	mask >>= (x & x_mask)*bpp;
	x >>= x_shift;

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
			if (a & ATTR_BOLD)
				fd |= fd>>1;
		}
		if (a & ATTR_INVERSE)
			fd = ~fd;
		pfont++;		  /* Next character row */

		/* Loop twice if double height */
		line_count = (a & ATTR_DHEIGHT) ? 2 : 1;
		do {
			COLOR32 *p = (COLOR32 *)fbuf + x;
			COLOR32 m = mask;
			COLOR32 d;
			VIDEO_FONT_TYPE fm = 1<<(VIDEO_FONT_WIDTH-1);

			/* Load first word */
			d = *p;
			do {
				/* Loop twice if double width */
				unsigned col_count = (a & ATTR_DWIDTH) ? 2 : 1;
				do {
					/* Blend FG or BG pixel (BG only if not
					   transparent) */
					if (fd & fm)
						d = (d & ~m) | (fg & m);
					else if (!(a & ATTR_TRANSP))
						d = (d & ~m) | (bg & m);

					/* Shift mask to next pixel */
					m >>= bpp;
					if (!m) {
						/* Store old word and load
						   next word; reset mask to
						   first pixel in word */
						*p++ = d;
						m = BASEMASK(bpp);
						d = *p;
					}
				} while (--col_count);
				fm >>= 1;
			} while (fm);

			/* Store back last word */
			*p = d;

			/* Go to next line */
			fbuf += linelen;
		} while (--line_count);
	}
}

/************************************************************************/
/* HELPER FUNCTIONS FOR GRAPHIC PRIMITIVES				*/
/************************************************************************/

static void test_pattern0(const wininfo_t *pwi)
{
	COLOR32 col;
	const pixinfo_t *ppi = pwi->ppi;
	XYPOS dx, dy;
	XYPOS x, y, i;
	XYPOS hleft, vtop, hright, vbottom;
	XYPOS fbhres = (XYPOS)pwi->fbhres;
	XYPOS fbvres = (XYPOS)pwi->fbvres;
	XYPOS r1, r2, scale;
	RGBA rgb;

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

	/* Fill screen with black */
	lcd_fill(pwi, ppi->rgba2col(pwi, 0x000000FF));

	/* Use hres divided by 12 as grid size */
	col = ppi->rgba2col(pwi, 0xFFFFFFFF);  /* White */
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
		lcd_ll_rect(pwi, x, 0, x, fbvres-1, col);

	/* Draw horizontal lines of grid */
	for (y = vtop; y < fbvres; y += dy)
		lcd_ll_rect(pwi, 0, y, fbhres-1, y, col);

	/* Draw 7 of the 8 basic colors (without black) as rectangles */
	for (i=0; i<7; i++) {
		x = hleft + (i+2)*dx;
		lcd_ll_rect(pwi, x+1, vbottom-2*dy+1, x+dx-1, vbottom-1, 
			    ppi->rgba2col(pwi, coltab[6-i]));
		lcd_ll_rect(pwi, x+1, vtop+1, x+dx-1, vtop+2*dy-1, 
			    ppi->rgba2col(pwi, coltab[i]));
	}

	scale = vbottom-vtop-2;
	for (y=0; y<=scale; y++) {
		XYPOS yy = y+vtop+1;

		rgb = y*255/scale;
		lcd_ll_rect(pwi, hleft+1, yy, hleft+dx-1, yy,
			ppi->rgba2col(pwi, (rgb<<24)|(rgb<<16)|(rgb<<8)|0xff));
		lcd_ll_rect(pwi, hright-dx+1, yy, hright-2*dx/3, yy,
			    ppi->rgba2col(pwi, (rgb<<24) | 0xff));
		lcd_ll_rect(pwi, hright-2*dx/3+1, yy, hright-dx/3, yy,
			    ppi->rgba2col(pwi, (rgb<<16) | 0xff));
		lcd_ll_rect(pwi, hright-dx/3+1, yy, hright-1, yy,
			    ppi->rgba2col(pwi, (rgb<<8) | 0xff));
	}

	/* Draw big and small circle; make sure that circle fits on screen */
	if (fbhres > fbvres) {
		r1 = fbvres/2;
		r2 = dy;
	} else {
		r1 = fbhres/2;
		r2 = dx;
	}
	lcd_circle(pwi, fbhres/2, fbvres/2, r1 - 1, col);
	lcd_circle(pwi, fbhres/2, fbvres/2, r2, col);

	/* Draw corners */
	if ((fbhres >= 8) && (fbvres >= 8)) {
		col = ppi->rgba2col(pwi, 0x00FF00FF);  /* Green */
		lcd_ll_rect(pwi, 0, 0, 7, 0, col);
		lcd_ll_rect(pwi, 0, 1, 0, 7, col);
		lcd_ll_rect(pwi, fbhres-8, 0, fbhres-1, 0, col);
		lcd_ll_rect(pwi, fbhres-1, 1, fbhres-1, 7, col);
		lcd_ll_rect(pwi, 0, fbvres-8, 0, fbvres-2, col);
		lcd_ll_rect(pwi, 0, fbvres-1, 7, fbvres-1, col);
		lcd_ll_rect(pwi, fbhres-1, fbvres-8, fbhres-1, fbvres-2, col);
		lcd_ll_rect(pwi, fbhres-8, fbvres-1, fbhres-1, fbvres-1, col);
	}

}

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

	/* Fill screen with black */
	lcd_fill(pwi, ppi->rgba2col(pwi, 0x000000FF));

	/* Draw red, green, blue, black rectangles in top row */
	lcd_ll_rect(pwi, 0, 0, xres_4-1, yres_2-1,
		    ppi->rgba2col(pwi, 0xFF0000FF)); /* Red */
	lcd_ll_rect(pwi, xres_4, 0, 2*xres_4-1, yres_2-1,
		    ppi->rgba2col(pwi, 0x00FF00FF)); /* Green */
	lcd_ll_rect(pwi, 2*xres_4, 0, 3*xres_4-1, yres_2-1,
		    ppi->rgba2col(pwi, 0x0000FFFF)); /* Blue */
	lcd_ll_rect(pwi, 3*xres_4, 0, xres-1, yres_2-1,
		    ppi->rgba2col(pwi, 0x000000FF)); /* Black */

	/* Draw cyan, magenta, yellow, white rectangles in bottom row */
	lcd_ll_rect(pwi, 0, yres_2, xres_4-1, yres-1,
		    ppi->rgba2col(pwi, 0x00FFFFFF)); /* Cyan */
	lcd_ll_rect(pwi, xres_4, yres_2, 2*xres_4-1, yres-1,
		    ppi->rgba2col(pwi, 0xFF00FFFF)); /* Magenta */
	lcd_ll_rect(pwi, 2*xres_4, yres_2, 3*xres_4-1, yres-1,
		    ppi->rgba2col(pwi, 0xFFFF00FF)); /* Yellow */
	lcd_ll_rect(pwi, 3*xres_4, yres_2, xres-1, yres-1,
		    ppi->rgba2col(pwi, 0xFFFFFFFF)); /* White */
}

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

	/* Fill screen with black */
	lcd_fill(pwi, ppi->rgba2col(pwi, 0x000000FF));

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
				lcd_ll_pixel(pwi, x + xfrom, y,
					     ppi->rgba2col(pwi, rgba));
			}

			for (y=0; y<yres-yres_2; y++) {
				rgba = ((0xFF-temp.R)*y/scale + temp.R) << 24;
				rgba |= ((0xFF-temp.G)*y/scale + temp.G) << 16;
				rgba |= ((0xFF-temp.B)*y/scale + temp.B) << 8;
				rgba |= 0xFF;
				lcd_ll_pixel(pwi, x + xfrom, y + yres_2,
					     ppi->rgba2col(pwi, rgba));
			}
		}
	}
}

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

	/* Fill screen with black */
	lcd_fill(pwi, ppi->rgba2col(pwi, 0x000000FF));

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

			lcd_ll_pixel(pwi, (XYPOS)x, (XYPOS)y,
				     ppi->rgba2col(pwi, rgb));
		}

		/* Draw right half of row */
		for (x=xres_2a; x<xres; x++) {
			int x2 = x - xres_2a;

			rgb = ((r.R - m.R)*x2/xres_2b + m.R) << 24;
			rgb |= ((r.G - m.G)*x2/xres_2b + m.G) << 16;
			rgb |= ((r.B - m.B)*x2/xres_2b + m.B) << 8;
			rgb |= 0xFF;

			lcd_ll_pixel(pwi, (XYPOS)x, (XYPOS)y,
				     ppi->rgba2col(pwi, rgb));
		}
	}
}


/* Multiply the old pixel with the new pixel value and its alpha. The correct
   value would be:

     C = (C_old * (255 - A_pix) + C_pix * A_pix) / 255

   But dividing by 255 is rather time consuming. Therefore we use a slightly
   different blending function that divides by 256 which can be done by a
   right shift.

     C = (C_old * (256 - A_pix) + C_pix * (A_pix + 1)) / 256

   This formula generates a slightly different result which is barely visible.
   This approach is also commonly used by LCD controller hardware.

     R = (R_old * (256 - A_pix) + R_pix * (A_pix + 1)) / 256
     G = (G_old * (256 - A_pix) + G_pix * (A_pix + 1)) / 256
     B = (B_old * (256 - A_pix) + B_pix * (A_pix + 1)) / 256
     A = A_old;

   In fact the right shift of /256 can be combined with the shift to the final
   position in the RGBA word. Also multiplying can be done in another bit
   position which again removes the need for some shifts. */
static RGBA apply_alpha(RGBA old, RGBA pix)
{
	RGBA R, G, B;
	RGBA alpha1, alpha256;

	alpha1 = pix & 0x000000FF;
	alpha256 = 256 - alpha1;
	alpha1++;
	R = ((old >> 24) * alpha256 + (pix >> 24) * alpha1) & 0x0000FF00;
	G = ((old & 0xFF0000) >> 8) * alpha256;
	G += ((pix & 0xFF0000) >> 8) * alpha1;
	B = ((old & 0xFF00) * alpha256 + (pix & 0xFF00) * alpha1) & 0x00FF0000;
	return (R << 16) | (G & 0x00FF0000) | (B >> 8) | (old & 0x000000FF);
}


static void draw_ll_row_gray_pal(const imginfo_t *pii, const u_char *prow,
				 COLOR32 *p)
{
	int xpix = pii->xpix;
	int xend = pii->xend;
	u_int shift = pii->shift;
	COLOR32 val;

	u_int rowshift = pii->rowshift;
	u_char rowval;

	rowval = *prow++;
	val = *p;
	for (;;) {
		COLOR32 col;

		rowshift -= pii->rowbitdepth;
		col = palcol32[(rowval >> rowshift) & pii->rowmask];
		do {
			COLOR32 pixmask;

			shift -= pii->bpp;
			pixmask = pii->mask << shift;
			val = (val & ~pixmask) | (col & pixmask);
			if (++xpix >= xend) {
				*p = val; /* Store final value */
				return;
			}
			if (!shift) {
				*p++ = val;
				val = *p;
				shift = 32;
			}
		} while (xpix & pii->dwidthshift);

		if (!rowshift) {
			rowval = *prow++;
			rowshift = 8;
		}
	}
}

static void draw_ll_row_gray_alpha(const imginfo_t *pii, const u_char *prow,
				   COLOR32 *p)
				   
{
	int xpix = pii->xpix;
	u_int shift = pii->shift;
	COLOR32 val;
	const wininfo_t *pwi = pii->pwi;

	val = *p;
	for (;;) {
		RGBA rgba;
		COLOR32 col;

		rgba = *prow++ << 8;
		rgba |= (rgba << 8) | (rgba << 16);
		rgba |= *prow++;
		col = pwi->ppi->rgba2col(pwi, rgba);
		do {
			COLOR32 pixmask;

			shift -= pii->bpp;
			pixmask = pii->mask << shift;
			val = (val & ~pixmask) | (col & pixmask);
			if (++xpix >= pii->xend) {
				*p = val; /* Store final value */
				return;
			}
			if (!shift) {
				*p++ = val;
				val = *p;
				shift = 32;
			}
		} while (xpix & pii->dwidthshift);
	}
}

static void draw_ll_row_truecol(const imginfo_t *pii, const u_char *prow,
				COLOR32 *p)
{
	int xpix = pii->xpix;
	u_int shift = pii->shift;
	COLOR32 val;
	const wininfo_t *pwi = pii->pwi;

	val = *p;
	for (;;) {
		RGBA rgba;
		COLOR32 col;

		rgba = *prow++ << 24;
		rgba |= *prow++ << 16;
		rgba |= *prow++ << 8;
		if (rgba != pii->trans_rgba)
			rgba |= 0xFF;
		col = pwi->ppi->rgba2col(pwi, rgba);
		do {
			COLOR32 pixmask;

			shift -= pii->bpp;
			pixmask = pii->mask << shift;
			val = (val & ~pixmask) | (col & pixmask);
			if (++xpix >= pii->xend) {
				*p = val; /* Store final value */
				return;
			}
			if (!shift) {
				*p++ = val;
				val = *p;
				shift = 32;
			}
		} while (xpix & pii->dwidthshift);
	}
}

static void draw_ll_row_truecol_alpha(const imginfo_t *pii, const u_char *prow,
				      COLOR32 *p)
{
	int xpix = pii->xpix;
	u_int shift = pii->shift;
	COLOR32 val;
	const wininfo_t *pwi = pii->pwi;

	val = *p;
	for (;;) {
		RGBA rgba;
		COLOR32 col;

		rgba = *prow++ << 24;
		rgba |= *prow++ << 16;
		rgba |= *prow++ << 8;
		rgba |= *prow++;
		col = pwi->ppi->rgba2col(pwi, rgba);
		do {
			COLOR32 pixmask;

			shift -= pii->bpp;
			pixmask = pii->mask << shift;
			val = (val & ~pixmask) | (col & pixmask);
			if (++xpix >= pii->xend) {
				*p = val; /* Store final value */
				return;
			}
			if (!shift) {
				*p++ = val;
				val = *p;
				shift = 32;
			}
		} while (xpix & pii->dwidthshift);
	}
}

static void adraw_ll_row_gray_pal(const imginfo_t *pii, const u_char *prow,
				  COLOR32 *p)
{
	int xpix = pii->xpix;
	int xend = pii->xend;
	u_int shift = pii->shift;
	COLOR32 val;

	u_int rowshift = pii->rowshift;
	u_char rowval;
	const wininfo_t *pwi = pii->pwi;

	rowval = *prow++;
	val = *p;
	for (;;) {
		RGBA rgba;

		rowshift -= pii->rowbitdepth;
		rgba = palrgba[(rowval >> rowshift) & pii->rowmask];
		do {
			COLOR32 pixmask;
			COLOR32 col;
			RGBA rgba_bg;

			shift -= pii->bpp;
			pixmask = pii->mask << shift;
			rgba_bg = pwi->ppi->col2rgba(pwi, val >> shift);
			rgba_bg = apply_alpha(rgba_bg, rgba);
			col = pwi->ppi->rgba2col(pwi, rgba_bg);
			val = (val & ~pixmask) | (col & pixmask);
			if (++xpix >= xend) {
				*p = val; /* Store final value */
				return;
			}
			if (!shift) {
				*p++ = val;
				val = *p;
				shift = 32;
			}
		} while (xpix & pii->dwidthshift);

		if (!rowshift) {
			rowval = *prow++;
			rowshift = 8;
		}
	}
}

static void adraw_ll_row_gray_alpha(const imginfo_t *pii, const u_char *prow,
				    COLOR32 *p)
{
	int xpix = pii->xpix;
	u_int shift = pii->shift;
	COLOR32 val;
	const wininfo_t *pwi = pii->pwi;

	val = *p;
	for (;;) {
		RGBA rgba;

		rgba = *prow++ << 8;
		rgba |= (rgba << 8) | (rgba << 16);
		rgba |= *prow++;
		do {
			COLOR32 pixmask;
			COLOR32 col;
			RGBA rgba_bg;

			shift -= pii->bpp;
			pixmask = pii->mask << shift;
			rgba_bg = pwi->ppi->col2rgba(pwi, val >> shift);
			rgba_bg = apply_alpha(rgba_bg, rgba);
			col = pwi->ppi->rgba2col(pwi, rgba_bg);
			val = (val & ~pixmask) | (col & pixmask);
			if (++xpix >= pii->xend) {
				*p = val; /* Store final value */
				return;
			}
			if (!shift) {
				*p++ = val;
				val = *p;
				shift = 32;
			}
		} while (xpix & pii->dwidthshift);
	}
}

static void adraw_ll_row_truecol(const imginfo_t *pii, const u_char *prow,
				 COLOR32 *p)
{
	int xpix = pii->xpix;
	u_int shift = pii->shift;
	COLOR32 val;
	const wininfo_t *pwi = pii->pwi;

	val = *p;
	for (;;) {
		RGBA rgba;

		rgba = *prow++ << 24;
		rgba |= *prow++ << 16;
		rgba |= *prow++ << 8;
		if (rgba != pii->trans_rgba)
			rgba |= 0xFF;
		do {
			COLOR32 pixmask;
			COLOR32 col;
			RGBA rgba_bg;

			shift -= pii->bpp;
			pixmask = pii->mask << shift;
			rgba_bg = pwi->ppi->col2rgba(pwi, val >> shift);
			rgba_bg = apply_alpha(rgba_bg, rgba);
			col = pwi->ppi->rgba2col(pwi, rgba_bg);
			val = (val & ~pixmask) | (col & pixmask);
			if (++xpix >= pii->xend) {
				*p = val; /* Store final value */
				return;
			}
			if (!shift) {
				*p++ = val;
				val = *p;
				shift = 32;
			}
		} while (xpix & pii->dwidthshift);
	}
}

static void adraw_ll_row_truecol_alpha(const imginfo_t *pii, const u_char *prow,
				       COLOR32 *p)
{
	int xpix = pii->xpix;
	u_int shift = pii->shift;
	COLOR32 val;
	const wininfo_t *pwi = pii->pwi;

	val = *p;
	for (;;) {
		RGBA rgba;

		rgba = *prow++ << 24;
		rgba |= *prow++ << 16;
		rgba |= *prow++ << 8;
		rgba |= *prow++;
		do {
			COLOR32 pixmask;
			COLOR32 col;
			RGBA rgba_bg;

			shift -= pii->bpp;
			pixmask = pii->mask << shift;
			rgba_bg = pwi->ppi->col2rgba(pwi, val >> shift);
			rgba_bg = apply_alpha(rgba_bg, rgba);
			col = pwi->ppi->rgba2col(pwi, rgba_bg);
			val = (val & ~pixmask) | (col & pixmask);
			if (++xpix >= pii->xend) {
				*p = val; /* Store final value */
				return;
			}
			if (!shift) {
				*p++ = val;
				val = *p;
				shift = 32;
			}
		} while (xpix & pii->dwidthshift);
	}
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


static const char *draw_png(imginfo_t *pii, u_long addr)
{
	u_char colortype;
	u_char bitdepth;
	int png_done;  //####
	int current;
	u_int pixelsize;		  /* Size of one full pixel (bits) */
	u_int fsize;			  /* Size of one filter unit */
	u_int rowlen;
	int rowpos;
	char *errmsg = NULL;
	z_stream zs;
	draw_row_func_t draw_row;
	u_char *p;

	static const draw_row_func_t draw_row_tab[2][6] = {
		{	
			/* ATTR_ALPHA = 0 */
			NULL,
			draw_ll_row_gray_pal,
			draw_ll_row_gray_pal,
			draw_ll_row_gray_alpha,
			draw_ll_row_truecol,
			draw_ll_row_truecol_alpha
		},
		{
			/* ATTR_ALPHA = 1 */
			NULL,
			adraw_ll_row_gray_pal,
			adraw_ll_row_gray_pal,
			adraw_ll_row_gray_alpha,
			adraw_ll_row_truecol,
			adraw_ll_row_truecol_alpha
		}
	};
	colortype = pii->bi.colortype;
	bitdepth = pii->bi.bitdepth;
		
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
	if (rowlen > MAX_PNGROWLEN)
		return "PNG width exceeds internal decoding buffer size";
	pii->rowmask = (1 << bitdepth)-1;
	rowpos = (pii->xpix >> pii->dwidthshift) * bitdepth;
	pii->rowbitdepth = bitdepth;
	pii->rowshift = 8 - (rowpos & 7);
	rowpos >>= 3;

	/* Fill reference row (for UP and PAETH filter) with 0 */
	memset(row[0], 0, rowlen+1);

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
			palrgba[--n] = gray | 0xFF; /* A=0xFF (fully opaque) */
			gray -= delta;
		} while (n);
	}

	/* Init zlib decompression */
	zs.zalloc = NULL;
	zs.zfree = NULL;
	zs.next_in = NULL;
	zs.avail_in = 0;
	zs.next_out = row[1];
	zs.avail_out = rowlen+1;	  /* +1 for filter type */
	if (inflateInit(&zs) != Z_OK)
		return "Can't initialize zlib\n";

	/* Go to first chunk after IHDR */
	addr += 8 + 8+13+4; 		  /* Signature size + IHDR size */

	/* Read chunks until IEND chunk is encountered; because we have called
	   lcd_scan_bitmap() above, we know that the PNG structure is OK and
	   we don't need to worry too much about bad chunks. */
	png_done = 0; //####
	current = 1;
	fsize = pixelsize/8;		  /* in bytes, at least 1 */
	if (fsize < 1)
		fsize = 1;
	draw_row = draw_row_tab[pii->applyalpha][colortype];
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
		if (chunk_id == CHUNK_IEND) {
			//####
			if (!png_done)
				errmsg = "Not all compressed data used\n";
			//####
			break;
		}

		/* Only accept PLTE chunk in palette bitmaps */
		if ((chunk_id == CHUNK_PLTE) && (colortype == 3)) {
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
				palrgba[i] = rgba;
			}
			continue;
		}

		/* Handle tRNS chunk */
		if (chunk_id == CHUNK_tRNS) {
			if ((colortype == 0) && (chunk_size >= 2)) {
				u_int index = get_be16(p);

				/* Make palette entry transparent */
				if (index < 1 << bitdepth)
					palrgba[index] &= 0xFFFFFF00;
			} else if ((colortype == 2) && (chunk_size >= 6)) {
				RGBA rgba;

				rgba = get_be16(p) << 24;
				rgba |= get_be16(p+2) << 16;
				rgba |= get_be16(p+4) << 8;
				rgba |= 0x00; /* Transparent, is now valid */
				pii->trans_rgba = rgba;
			} else if (colortype == 3) {
				u_int i;
				u_int entries;

				entries = 1 << bitdepth;
				if (entries > chunk_size)
					entries = chunk_size;
				for (i=0; i<entries; i++)
					palrgba[i] = (palrgba[i]&~0xFF) | *p++;
			}
			continue;
		}

		/* Accept all other chunks, but ignore them */
		if (chunk_id != CHUNK_IDAT)
			continue;

		/* Handle IDAT chunk */
		//##### Hier nun beim ersten Mal ggf. die palrgba[] nach
		//##### palcol32[] konvertieren.

		zs.next_in = p;		  /* Provide next data */
		zs.avail_in = chunk_size;

		//####
		/* Compressed data was all handled, but there's more */
		if (png_done) {
			errmsg = "Extra data after Z_STREAM_END\n";
			break;
		}
		//####

		/* Process image rows until we need more input data */
		do {
			int zret;

			/* Decompress data for next PNG row */
			zret = inflate(&zs, Z_SYNC_FLUSH);
			if (zret == Z_STREAM_END) {
				zret = Z_OK;
				png_done = 1;
			}
			if (zret != Z_OK) {
				errmsg = "zlib decompression failed\n";
				break;
			}
			if (zs.avail_out == 0) {
				int i;
				u_char *pc = row[current];     /* current */
				u_char *pp = row[1-current]+1; /* previous */
				u_char filtertype = *pc++;
				XYPOS y;

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
						pc[i] += paeth(0, pp[i], 0);
					for (; i<rowlen; i++)
						pc[i] += paeth(pc[i-fsize],
							       pp[i],
							       pp[i-fsize]);
					break;
				}

				/* If row is in framebuffer range, draw it */
				y = pii->y;
				if ((y >= 0) && (y < pii->pwi->fbvres)) {
					u_long fbuf = y*pii->pwi->linelen;
					fbuf += pii->fbuf;
					draw_row(pii, row[current]+1+rowpos,
						 (COLOR32 *)fbuf);//###
				}
				current = 1 - current;
				zs.next_out = row[current];
				zs.avail_out = rowlen + 1;
			}
		} while (zs.avail_in > 0); /* Repeat as long as we have data */
	} while (!errmsg);

	/* Release resources of zlib decompression */
	inflateEnd(&zs);

	/* We're done */
	return errmsg;
}


static const char *draw_bmp(imginfo_t *pii, u_long addr)
{
	return "###BMP bitmaps not yet implemented\n###";
}

static const char *draw_jpg(imginfo_t *pii, u_long addr)
{
	return "JPG bitmaps not yet supported\n";
}

/************************************************************************/
/* GRAPHICS PRIMITIVES							*/
/************************************************************************/

/* Fill display with color */
void lcd_fill(const wininfo_t *pwi, COLOR32 color)
{
	memset32((unsigned *)pwi->pfbuf[pwi->fbdraw], color, pwi->fbsize/4);
}

/* Draw pixel at (x, y) with color */
void lcd_pixel(const wininfo_t *pwi, XYPOS x, XYPOS y, COLOR32 color)
{
	if ((x >= 0) && (x < (XYPOS)pwi->fbhres)
	    && (y >= 0) && (y < (XYPOS)pwi->fbvres))
		lcd_ll_pixel(pwi, x, y, color);
}


/* Draw line from (x1, y1) to (x2, y2) in color */
void lcd_line(const wininfo_t *pwi, XYPOS x1, XYPOS y1, XYPOS x2, XYPOS y2,
	      COLOR32 color)
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
			lcd_ll_rect(pwi, x1, y1, x2, y2, color);
			return;
		}

		xoffs = (x1 > x2) ? -1 : 1;
		for (;;) {
			if ((x1 >= 0) && (x1 <= xmax))
				lcd_ll_pixel(pwi, x1, y1, color);
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
			lcd_ll_rect(pwi, x1, y1, x2, y2, color);
			return;
		}

		yoffs = (y1 > y2) ? -1 : 1;
		for (;;) {
			if ((y1 >= 0) && (y1 <= ymax))
				lcd_ll_pixel(pwi, x1, y1, color);
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
void lcd_frame(const wininfo_t *pwi, XYPOS x1, XYPOS y1, XYPOS x2, XYPOS y2,
	       COLOR32 color)
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
			lcd_ll_rect(pwi, xl, y1, xr, y1, color);

			/* We are done if rectangle is only one pixel high */
			if (y1 == y2)
				return;
		}

		/* Draw bottom line */
		if (y2 <= ymax)
			lcd_ll_rect(pwi, xl, y2, xr, y2, color);

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
		lcd_ll_rect(pwi, x1, y1, x1, y2, color);

		/* Return if rectangle is only one pixel wide */
		if (x1 == x2)
			return;
	}

	/* Draw right line */
	if (x2 <= xmax)
		lcd_ll_rect(pwi, x2, y1, x2, y2, color);
}


/* Draw filled rectangle from (x1, y1) to (x2, y2) in color */
void lcd_rect(const wininfo_t *pwi, XYPOS x1, XYPOS y1, XYPOS x2, XYPOS y2,
	      COLOR32 color)
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
	lcd_ll_rect(pwi, x1, y1, x2, y2, color);
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
void lcd_circle(const wininfo_t *pwi, XYPOS x, XYPOS y, XYPOS r, COLOR32 color)
{
	XYPOS dx = 0;
	XYPOS dy = r;
	XYPOS dd = 1-r;

	if (r < 0)
		return;

	if (r == 0) {
		lcd_pixel(pwi, x, y, color);
		return;
	}

	/* Draw first two pixels with dx == 0 */
	lcd_pixel(pwi, x, y - dy, color);
	lcd_pixel(pwi, x, y + dy, color);
	if (dd < 0)
		dd += 3;		  /* 2*dx + 3, but dx is 0 */
	else				  /* Only possible for r==1 */
		dy--;			  /* dd does not matter, dy is 0 */
	dx++;

	/* Draw part with low slope (every step changes dx, sometimes dy) */
	while (dy > dx) {
		lcd_pixel(pwi, x + dx, y - dy, color);
		lcd_pixel(pwi, x + dx, y + dy, color);
		if (dx) {
			lcd_pixel(pwi, x - dx, y - dy, color);
			lcd_pixel(pwi, x - dx, y + dy, color);
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
		lcd_pixel(pwi, x + dx, y - dy, color);
		lcd_pixel(pwi, x - dx, y - dy, color);
		lcd_pixel(pwi, x + dx, y + dy, color);
		lcd_pixel(pwi, x - dx, y + dy, color);

		if (dd < 0) {
			dd += (dx - dy)*2 + 5; /* SE */
			dx++;
		} else
			dd += 3 - dy*2;	       /* S */
		dy--;
	}

	/* Draw final pixels with dy == 0 */
	lcd_pixel(pwi, x + dx, y, color);
	lcd_pixel(pwi, x - dx, y, color);
}


/* Draw filled circle at (x, y) with radius r and color. The algorithm is the
   same as explained above at lcd_circle(), however we can skip some tests as
   we always draw a full line from the left to the right of the circle. */
void lcd_disc(const wininfo_t *pwi, XYPOS x, XYPOS y, XYPOS r, COLOR32 color)
{
	XYPOS dx = 0;
	XYPOS dy = r;
	XYPOS dd = 1-r;

	if (r < 0)
		return;

	/* Draw part with low slope (every step changes dx, sometimes dy) */
	while (dy > dx) {
		lcd_rect(pwi, x - dx, y - dy, x + dx, y - dy, color);
		lcd_rect(pwi, x - dx, y + dy, x + dx, y + dy, color);
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
		lcd_rect(pwi, x - dx, y - dy, x + dx, y - dy, color);
		lcd_rect(pwi, x - dx, y + dy, x + dx, y + dy, color);
		if (dd < 0) {
			dd += (dx - dy)*2 + 5; /* SE */
			dx++;
		} else
			dd += 3 - dy*2;	       /* S */
		dy--;
	}

	/* Draw final line with dy == 0 */
	lcd_rect(pwi, x - dx, y, x + dx, y, color);
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
void lcd_text(const wininfo_t *pwi, XYPOS x, XYPOS y, char *s, u_int a,
	      COLOR32 fg, COLOR32 bg)
{
	XYPOS len = (XYPOS)strlen(s);
	XYPOS width = VIDEO_FONT_WIDTH;
	XYPOS height = VIDEO_FONT_HEIGHT;
	XYPOS fbhres = (XYPOS)pwi->fbhres;
	XYPOS fbvres = (XYPOS)pwi->fbvres;

	/* Return if string is empty */
	if (s == 0)
		return;

	if (a & ATTR_DWIDTH)
		width *= 2;		  /* Double width */
	if (a & ATTR_DHEIGHT)
		height *= 2;		  /* Double height */

	/* Compute y from vertical alignment */
	switch (a & ATTR_VMASK) {
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
	switch (a & ATTR_HMASK) {
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
		lcd_ll_char(pwi, x, y, c, a, fg, bg);
		x += width;
	}
}

/* Draw bitmap from address addr at (x, y) with alignment/attribute a */
const char *lcd_bitmap(const wininfo_t *pwi, XYPOS x, XYPOS y, u_long addr,
		       u_int attr)
{
	imginfo_t ii;
	XYPOS hres, vres;
	XYPOS fbhres, fbvres;
	int xpos;

	const char *(*draw_bm_tab[])(imginfo_t *pii, u_long addr) = {
		NULL,
		draw_png,
		draw_bmp,
		draw_jpg
	};

	/* Store away some values to get the registers free */
	ii.y = y;
	ii.applyalpha = ((attr & ATTR_ALPHA) != 0);
	ii.dwidthshift = ((attr & ATTR_DWIDTH) != 0);
	ii.dheightshift = ((attr & ATTR_DHEIGHT) != 0);

	/* Do a quick scan if bitmap integrity is OK */
	if (!lcd_scan_bitmap(addr))
		return "Unknown bitmap type\n";

	/* Get bitmap info */
	lcd_get_bminfo(&ii.bi, addr);
	if (ii.bi.colortype == CT_UNKNOWN)
		return "Invalid bitmap color type\n";

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
	ii.xend = x + hres;
	if (ii.xend > pwi->fbhres)
		ii.xend = pwi->fbhres;

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
		pbi->colortype = (pbi->bitdepth <= 8) ? CT_PALETTE : CT_TRUECOL;
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
	coninfo.fg = pwi->fg;
	coninfo.bg = pwi->bg;
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
	COLOR32 fg = coninfo.fg;
	COLOR32 bg = coninfo.bg;
	int fbhres = pwi->fbhres;
	int fbvres = pwi->fbvres;

	switch (c) {
	case '\t':			  /* Tab */
		xtab = ((x / TABWIDTH) + 1) * TABWIDTH;
		while ((x + VIDEO_FONT_WIDTH <= fbhres) && (x < xtab)) {
			lcd_ll_char(pwi, x, y, ' ', 0, fg, bg);
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
		lcd_ll_char(pwi, x, y, ' ', 0, fg, bg);
		break;

	default:			  /* Character */
		lcd_ll_char(pwi, x, y, c, 0, fg, bg);
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

/*----------------------------------------------------------------------*/
#if 0
void lcd_printf(const char *fmt, ...)
{
	va_list args;
	char buf[CFG_PBSIZE];

	va_start(args, fmt);
	vsprintf(buf, fmt, args);
	va_end(args);

	lcd_puts(buf);
}
#endif



/************************************************************************/
/**  Small utility to check that you got the colours right		*/
/************************************************************************/
#ifdef LCD_TEST_PATTERN

#define	N_BLK_VERT	2
#define	N_BLK_HOR	3

static int test_colors[N_BLK_HOR*N_BLK_VERT] = {
	CONSOLE_COLOR_RED,	CONSOLE_COLOR_GREEN,	CONSOLE_COLOR_YELLOW,
	CONSOLE_COLOR_BLUE,	CONSOLE_COLOR_MAGENTA,	CONSOLE_COLOR_CYAN,
};

static void test_pattern (void)
{
	ushort v_max  = panel_info.vl_row;
	ushort h_max  = panel_info.vl_col;
	ushort v_step = (v_max + N_BLK_VERT - 1) / N_BLK_VERT;
	ushort h_step = (h_max + N_BLK_HOR  - 1) / N_BLK_HOR;
	ushort v, h;
	uchar *pix = (uchar *)lcd_base;

	/* WARNING: Code silently assumes 8bit/pixel */
	for (v=0; v<v_max; ++v) {
		uchar iy = v / v_step;
		for (h=0; h<h_max; ++h) {
			uchar ix = N_BLK_HOR * iy + (h/h_step);
			*pix++ = test_colors[ix];
		}
	}
}
#endif /* LCD_TEST_PATTERN */



/*----------------------------------------------------------------------*/


/************************************************************************/
/* ** ROM capable initialization part - needed to reserve FB memory	*/
/************************************************************************/
#ifndef CONFIG_S3C64XX
/*
 * This is called early in the system initialization to grab memory
 * for the LCD controller.
 * Returns new address for monitor, after reserving LCD buffer memory
 *
 * Note that this is running from ROM, so no write access to global data.
 */
ulong lcd_setmem (ulong addr)
{
	ulong size;
	int line_length = (panel_info.vl_col * NBITS (panel_info.vl_bpix)) / 8;

	debug ("LCD panel info: %d x %d, %d bit/pix\n",
		panel_info.vl_col, panel_info.vl_row, NBITS (panel_info.vl_bpix) );

	size = line_length * panel_info.vl_row;

	/* Round up to nearest full page */
	size = (size + (PAGE_SIZE - 1)) & ~(PAGE_SIZE - 1);

	/* Allocate pages for the frame buffer. */
	addr -= size;

	debug ("Reserving %ldk for LCD Framebuffer at: %08lx\n", size>>10, addr);

	return (addr);
}
#endif /*!CONFIG_S3C64XX*/

/*----------------------------------------------------------------------*/
#if 0 //####

static void lcd_setfgcolor(int color)
{
#ifdef CONFIG_ATMEL_LCD
	lcd_color_fg = color;
#else
	lcd_color_fg = color & 0x0F;
#endif
}

/*----------------------------------------------------------------------*/

static void lcd_setbgcolor (int color)
{
#ifdef CONFIG_ATMEL_LCD
	lcd_color_bg = color;
#else
	lcd_color_bg = color & 0x0F;
#endif
}

/*----------------------------------------------------------------------*/

#ifdef	NOT_USED_SO_FAR
static int lcd_getfgcolor (void)
{
	return lcd_color_fg;
}
#endif	/* NOT_USED_SO_FAR */

/*----------------------------------------------------------------------*/

static int lcd_getbgcolor (void)
{
	return lcd_color_bg;
}
#endif //0####

/*----------------------------------------------------------------------*/

/************************************************************************/
/* ** Chipset depending Bitmap / Logo stuff...                          */
/************************************************************************/
#ifdef CONFIG_LCD_LOGO
void bitmap_plot (int x, int y)
{
#ifdef CONFIG_ATMEL_LCD
	uint *cmap;
#else
	ushort *cmap;
#endif
	ushort i, j;
	uchar *bmap;
	uchar *fb;
	ushort *fb16;
#if defined CONFIG_PXA250 || defined CONFIG_PXA27X || defined CONFIG_CPU_MONAHANS
	struct pxafb_info *fbi = &panel_info.pxa;
#elif defined(CONFIG_MPC823)
	volatile immap_t *immr = (immap_t *) CFG_IMMR;
	volatile cpm8xx_t *cp = &(immr->im_cpm);
#endif

	debug ("Logo: width %d  height %d  colors %d  cmap %d\n",
		BMP_LOGO_WIDTH, BMP_LOGO_HEIGHT, BMP_LOGO_COLORS,
		(int)(sizeof(bmp_logo_palette)/(sizeof(ushort))));

	bmap = &bmp_logo_bitmap[0];
	fb   = (uchar *)(lcd_base + y * lcd_line_length + x);

	if (NBITS(panel_info.vl_bpix) < 12) {
		/* Leave room for default color map */
#if defined CONFIG_PXA250 || defined CONFIG_PXA27X || defined CONFIG_CPU_MONAHANS
		cmap = (ushort *)fbi->palette;
#elif defined(CONFIG_MPC823)
		cmap = (ushort *)&(cp->lcd_cmap[BMP_LOGO_OFFSET*sizeof(ushort)]);
#elif defined(CONFIG_ATMEL_LCD)
		cmap = (uint *) (panel_info.mmio + ATMEL_LCDC_LUT(0));
#else
		/*
		 * default case: generic system with no cmap (most likely 16bpp)
		 * We set cmap to the source palette, so no change is done.
		 * This avoids even more ifdef in the next stanza
		 */
		cmap = bmp_logo_palette;
#endif

		WATCHDOG_RESET();

		/* Set color map */
		for (i=0; i<(sizeof(bmp_logo_palette)/(sizeof(ushort))); ++i) {
			ushort colreg = bmp_logo_palette[i];
#ifdef CONFIG_ATMEL_LCD
			uint lut_entry;
#ifdef CONFIG_ATMEL_LCD_BGR555
			lut_entry = ((colreg & 0x000F) << 11) |
				    ((colreg & 0x00F0) <<  2) |
				    ((colreg & 0x0F00) >>  7);
#else /* CONFIG_ATMEL_LCD_RGB565 */
			lut_entry = ((colreg & 0x000F) << 1) |
				    ((colreg & 0x00F0) << 3) |
				    ((colreg & 0x0F00) << 4);
#endif
			*(cmap + BMP_LOGO_OFFSET) = lut_entry;
			cmap++;
#else /* !CONFIG_ATMEL_LCD */
#ifdef  CFG_INVERT_COLORS
			*cmap++ = 0xffff - colreg;
#else
			*cmap++ = colreg;
#endif
#endif /* CONFIG_ATMEL_LCD */
		}

		WATCHDOG_RESET();

		for (i=0; i<BMP_LOGO_HEIGHT; ++i) {
			memcpy (fb, bmap, BMP_LOGO_WIDTH);
			bmap += BMP_LOGO_WIDTH;
			fb   += panel_info.vl_col;
		}
	}
	else { /* true color mode */
		u16 col16;
		fb16 = (ushort *)(lcd_base + y * lcd_line_length + x);
		for (i=0; i<BMP_LOGO_HEIGHT; ++i) {
			for (j=0; j<BMP_LOGO_WIDTH; j++) {
				col16 = bmp_logo_palette[(bmap[j]-16)];
				fb16[j] =
					((col16 & 0x000F) << 1) |
					((col16 & 0x00F0) << 3) |
					((col16 & 0x0F00) << 4);
				}
			bmap += BMP_LOGO_WIDTH;
			fb16 += panel_info.vl_col;
		}
	}

	WATCHDOG_RESET();
}
#endif /* CONFIG_LCD_LOGO */

/*----------------------------------------------------------------------*/
#if defined(CONFIG_CMD_BMP) || defined(CONFIG_SPLASH_SCREEN)
/*
 * Display the BMP file located at address bmp_image.
 * Only uncompressed.
 */
int lcd_display_bitmap(ulong bmp_image, int x, int y)
{
#ifdef CONFIG_ATMEL_LCD
	uint *cmap;
#elif !defined(CONFIG_MCC200)
	ushort *cmap = NULL;
#endif
	ushort *cmap_base = NULL;
	ushort i, j;
	uchar *fb;
	bmp_image_t *bmp=(bmp_image_t *)bmp_image;
	uchar *bmap;
	ushort padded_line;
	unsigned long width, height, byte_width;
	unsigned long pwidth = panel_info.vl_col;
	unsigned colors, bpix, bmp_bpix;
	unsigned long compression;
#if defined CONFIG_PXA250 || defined CONFIG_PXA27X || defined CONFIG_CPU_MONAHANS
	struct pxafb_info *fbi = &panel_info.pxa;
#elif defined(CONFIG_MPC823)
	volatile immap_t *immr = (immap_t *) CFG_IMMR;
	volatile cpm8xx_t *cp = &(immr->im_cpm);
#endif

	if (!((bmp->header.signature[0]=='B') &&
		(bmp->header.signature[1]=='M'))) {
		printf ("Error: no valid bmp image at %lx\n", bmp_image);
		return 1;
	}

	width = le32_to_cpu (bmp->header.width);
	height = le32_to_cpu (bmp->header.height);
	bmp_bpix = le16_to_cpu(bmp->header.bit_count);
	colors = 1 << bmp_bpix;
	compression = le32_to_cpu (bmp->header.compression);

	bpix = NBITS(panel_info.vl_bpix);

	if ((bpix != 1) && (bpix != 8) && (bpix != 16)) {
		printf ("Error: %d bit/pixel mode, but BMP has %d bit/pixel\n",
			bpix, bmp_bpix);
		return 1;
	}

	/* We support displaying 8bpp BMPs on 16bpp LCDs */
	if (bpix != bmp_bpix && (bmp_bpix != 8 || bpix != 16)) {
		printf ("Error: %d bit/pixel mode, but BMP has %d bit/pixel\n",
			bpix,
			le16_to_cpu(bmp->header.bit_count));
		return 1;
	}

	debug ("Display-bmp: %d x %d  with %d colors\n",
		(int)width, (int)height, (int)colors);

#if !defined(CONFIG_MCC200)
	/* MCC200 LCD doesn't need CMAP, supports 1bpp b&w only */
	if (bmp_bpix == 8) {
#if defined CONFIG_PXA250 || defined CONFIG_PXA27X || defined CONFIG_CPU_MONAHANS
		cmap = (ushort *)fbi->palette;
#elif defined(CONFIG_MPC823)
		cmap = (ushort *)&(cp->lcd_cmap[255*sizeof(ushort)]);
#elif defined(CONFIG_ATMEL_LCD)
		cmap = (uint *) (panel_info.mmio + ATMEL_LCDC_LUT(0));
#else
		cmap = panel_info.cmap;
#endif

		cmap_base = cmap;

		/* Set color map */
		for (i=0; i<colors; ++i) {
			bmp_color_table_entry_t cte = bmp->color_table[i];
#if !defined(CONFIG_ATMEL_LCD)
			ushort colreg =
				( ((cte.red)   << 8) & 0xf800) |
				( ((cte.green) << 3) & 0x07e0) |
				( ((cte.blue)  >> 3) & 0x001f) ;
#ifdef CFG_INVERT_COLORS
			*cmap = 0xffff - colreg;
#else
			*cmap = colreg;
#endif
#if defined(CONFIG_PXA250)
			cmap++;
#elif defined(CONFIG_MPC823)
			cmap--;
#endif
#else /* CONFIG_ATMEL_LCD */
			lcd_setcolreg(i, cte.red, cte.green, cte.blue);
#endif
		}
	}
#endif

	/*
	 *  BMP format for Monochrome assumes that the state of a
	 * pixel is described on a per Bit basis, not per Byte.
	 *  So, in case of Monochrome BMP we should align widths
	 * on a byte boundary and convert them from Bit to Byte
	 * units.
	 *  Probably, PXA250 and MPC823 process 1bpp BMP images in
	 * their own ways, so make the converting to be MCC200
	 * specific.
	 */
#if defined(CONFIG_MCC200)
	if (bpix==1)
	{
		width = ((width + 7) & ~7) >> 3;
		x     = ((x + 7) & ~7) >> 3;
		pwidth= ((pwidth + 7) & ~7) >> 3;
	}
#endif

	padded_line = (width&0x3) ? ((width&~0x3)+4) : (width);

#ifdef CONFIG_SPLASH_SCREEN_ALIGN
	if (x == BMP_ALIGN_CENTER)
		x = max(0, (pwidth - width) / 2);
	else if (x < 0)
		x = max(0, pwidth - width + x + 1);

	if (y == BMP_ALIGN_CENTER)
		y = max(0, (panel_info.vl_row - height) / 2);
	else if (y < 0)
		y = max(0, panel_info.vl_row - height + y + 1);
#endif /* CONFIG_SPLASH_SCREEN_ALIGN */

	if ((x + width)>pwidth)
		width = pwidth - x;
	if ((y + height)>panel_info.vl_row)
		height = panel_info.vl_row - y;

	bmap = (uchar *)bmp + le32_to_cpu (bmp->header.data_offset);
	fb   = (uchar *) (lcd_base +
		(y + height - 1) * lcd_line_length + x);

	switch (bmp_bpix) {
	case 1: /* pass through */
	case 8:
		if (bpix != 16)
			byte_width = width;
		else
			byte_width = width * 2;

		for (i = 0; i < height; ++i) {
			WATCHDOG_RESET();
			for (j = 0; j < width; j++) {
				if (bpix != 16) {
#if defined CONFIG_PXA250 || defined CONFIG_PXA27X || defined CONFIG_CPU_MONAHANS || defined(CONFIG_ATMEL_LCD)
					*(fb++) = *(bmap++);
#elif defined(CONFIG_MPC823) || defined(CONFIG_MCC200)
					*(fb++) = 255 - *(bmap++);
#endif
				} else {
					*(uint16_t *)fb = cmap_base[*(bmap++)];
					fb += sizeof(uint16_t) / sizeof(*fb);
				}
			}
			bmap += (width - padded_line);
			fb   -= (byte_width + lcd_line_length);
		}
		break;

#if defined(CONFIG_BMP_16BPP)
	case 16:
		for (i = 0; i < height; ++i) {
			WATCHDOG_RESET();
			for (j = 0; j < width; j++) {
#if defined(CONFIG_ATMEL_LCD_BGR555)
				*(fb++) = ((bmap[0] & 0x1f) << 2) |
					(bmap[1] & 0x03);
				*(fb++) = (bmap[0] & 0xe0) |
					((bmap[1] & 0x7c) >> 2);
				bmap += 2;
#else
				*(fb++) = *(bmap++);
				*(fb++) = *(bmap++);
#endif
			}
			bmap += (padded_line - width) * 2;
			fb   -= (width * 2 + lcd_line_length);
		}
		break;
#endif /* CONFIG_BMP_16BPP */

	default:
		break;
	};

	return (0);
}
#endif

#ifdef CONFIG_VIDEO_BMP_GZIP
extern bmp_image_t *gunzip_bmp(unsigned long addr, unsigned long *lenp);
#endif


#if 0 //######
static void *lcd_logo (void)
{
#ifdef CONFIG_SPLASH_SCREEN
	char *s;
	ulong addr;
	static int do_splash = 1;

	if (do_splash && (s = getenv("splashimage")) != NULL) {
		int x = 0, y = 0;
		do_splash = 0;

		addr = simple_strtoul (s, NULL, 16);
#ifdef CONFIG_SPLASH_SCREEN_ALIGN
		if ((s = getenv ("splashpos")) != NULL) {
			if (s[0] == 'm')
				x = BMP_ALIGN_CENTER;
			else
				x = simple_strtol (s, NULL, 0);

			if ((s = strchr (s + 1, ',')) != NULL) {
				if (s[1] == 'm')
					y = BMP_ALIGN_CENTER;
				else
					y = simple_strtol (s + 1, NULL, 0);
			}
		}
#endif /* CONFIG_SPLASH_SCREEN_ALIGN */

#ifdef CONFIG_VIDEO_BMP_GZIP
		bmp_image_t *bmp = (bmp_image_t *)addr;
		unsigned long len;

		if (!((bmp->header.signature[0]=='B') &&
		      (bmp->header.signature[1]=='M'))) {
			addr = (ulong)gunzip_bmp(addr, &len);
		}
#endif

		if (lcd_display_bitmap (addr, x, y) == 0) {
			return ((void *)lcd_base);
		}
	}
#endif /* CONFIG_SPLASH_SCREEN */

#ifdef CONFIG_LCD_LOGO
	bitmap_plot (0, 0);
#endif /* CONFIG_LCD_LOGO */

#ifdef CONFIG_LCD_INFO
	console_col = LCD_INFO_X / VIDEO_FONT_WIDTH;
	console_row = LCD_INFO_Y / VIDEO_FONT_HEIGHT;
	lcd_show_board_info();
#endif /* CONFIG_LCD_INFO */

#if defined(CONFIG_LCD_LOGO) && !defined(CONFIG_LCD_INFO_BELOW_LOGO)
	return ((void *)((ulong)lcd_base + BMP_LOGO_HEIGHT * lcd_line_length));
#else
	return ((void *)lcd_base);
#endif /* CONFIG_LCD_LOGO && !CONFIG_LCD_INFO_BELOW_LOGO */
}

#endif //0#####

/************************************************************************/
/************************************************************************/



COLOR32 lcd_rgbalookup(RGBA rgba, RGBA *cmap, unsigned count)
{
#if 0
	unsigned nearest = 0;
	short r, g, b, a;
	unsigned i;

	signed mindist = 256*256*4;

	r = (short)(rgba >> 24);
	g = (short)((rgba >> 16) & 0xFF);
	b = (short)((rgba >> 8) & 0xFF);
	a = (short)(rgba & 0xFF);

	for (i=0; i<count; i++) {
		short dr, dg, db, da;
		signed dist;

		rgba = cmap[i];
		dr = (short)(rgba >> 24) - r;
		dg = (short)((rgba >> 16) & 0xFF) - g;
		db = (short)((rgba >> 8) & 0xFF) - b;
		da = (short)(rgba & 0xFF) - a;
		dist = dr*dr + dg*dg + db*db + da*da;
		if (dist == 0)
			return (COLOR32)i;	  /* Exact match */
		if (dist < mindist) {
			mindist = dist;
			nearest = i;
		}
	}

	return (COLOR32)nearest;
#else
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
#endif
}
