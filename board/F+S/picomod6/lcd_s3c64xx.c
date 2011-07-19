#include <config.h>
#include <common.h>
//#include <version.h>
//#include <stdarg.h>
//#include <linux/types.h>
//#include <devices.h>
#include <lcd_s3c64xx.h>		  /* Own interface */
#include <cmd_lcd.h>			  /* wininfo_t, kwinfo_t, ... */
#include <lcd.h>			  /* lcd_rgbalookup() */
#include <s3c-regs-lcd.h>		  /* S3C64XX LCD registers */
#include <regs.h>			  /* GPxDAT, GPxCON, ... */

/************************************************************************/
/* DEFINITIONS								*/
/************************************************************************/

#define DRIVER_NAME "lcd_s3c64xx"	  /* Name of this driver */
#define MAX_WINDOWS 5			  /* S3C64XX has 5 hardware windows */
#define PIXEL_FORMAT_COUNT 15		  /* Number of pixel formats */
#define DEFAULT_PIXEL_FORMAT 5		  /* Default format: RGBA-5650 */
#define IDENT_BLINK_COUNT 5		  /* Blink window 5 times on identify */


/************************************************************************/
/* ENUMERATIONS								*/
/************************************************************************/

/************************************************************************/
/* TYPES AND STRUCTURES							*/
/************************************************************************/

typedef unsigned long LCDREG;

struct LCD_WIN_REGS {
	LCDREG wincon;
	LCDREG vidosdtl;		  /* Top left OSD position */
	LCDREG vidosdbr;		  /* Bottom right OSD position */
	LCDREG vidosdalpha;		  /* Alpha values */
	LCDREG vidosdsize;		  /* OSD size */
	struct vbuf {
		LCDREG start;		  /* Video buffer start address */
		LCDREG end;		  /* Video buffer end address */
	} vidadd[2];
	LCDREG vidsize;			  /* Video buffer size */
	LCDREG keycon;			  /* Color key control */
	LCDREG keyval;			  /* Color key value */
	LCDREG winmap;			  /* Default color mapping */
	u_int  hwcmap;			  /* Color look-up table address */
};


/************************************************************************/
/* LOCAL VARIABLES							*/
/************************************************************************/

/* One wininfo structure per window */
static wininfo_t s3c64xx_wininfo[MAX_WINDOWS];

/* Addresses of the possible image buffers of all windows */
static u_long s3c64xx_fbuf[2+2+1+1+1];

/* Color maps; these entries hold the actually used RGBA value as is possible
   in the hardware. Holding these values in an extra table avoids having to
   read (and convert) the hardware register values everytime again when we
   need this value.
   Window   Offset    Entries   Remarks
   ----------------------------------------------------------------------
   0        0         256       Window 0 supports 1, 2, 4, 8 bpp palettes
   1        256       256       Window 1 supports 1, 2, 4, 8 bpp palettes
   2        512       16        Window 2 supports 1, 2, 4 bpp palettes
   3        528       16        Window 3 supports 1, 2, 4 bpp palettes
   4        544       4         Window 4 supports 1, 2 bpp palettes */
static RGBA cmap[256+256+16+16+4];

/* The offsets for each window into the cmap array */
static int cmap_offset[MAX_WINDOWS] = {
	0, 256, 256+256, 256+256+16, 256+256+16+16
};

/* Valid pixel formats for each window */
static u_int valid_pixels[MAX_WINDOWS] = {
	0x09AF,			/* Win 0: no alpha formats */
	0x7FFF,			/* Win 1: no restrictions  */
	0x7FF7,			/* Win 2: no CMAP-8 */
	0x7FE7,			/* Win 3: no CMAP-8, no RGBA-2321 */
	0x7FE3,			/* Win 4: no CMAP-4, no CMAP-8, no RGBA-2321 */
};

static const struct LCD_WIN_REGS winregs_table[5] = {
	{				  /* Window 0 */
		S3C_WINCON0,
		S3C_VIDOSD0A,
		S3C_VIDOSD0B,
		0,			  /* No alpha in win 0 */
		S3C_VIDOSD0C,
		{
			{S3C_VIDW00ADD0B0, S3C_VIDW00ADD1B0},
			{S3C_VIDW00ADD0B1, S3C_VIDW00ADD1B1}
		},
		S3C_VIDW00ADD2,
		0,			  /* No color keying in win 0 */
		0,
		S3C_WIN0MAP,
		S3C_PA_LCD+0x400
	},
	{				  /* Window 1 */
		S3C_WINCON1,
		S3C_VIDOSD1A,
		S3C_VIDOSD1B,
		S3C_VIDOSD1C,
		S3C_VIDOSD1D,
		{
			{S3C_VIDW01ADD0B0, S3C_VIDW01ADD1B0},
			{S3C_VIDW01ADD0B1, S3C_VIDW01ADD1B1}
		},
		S3C_VIDW01ADD2,
		S3C_W1KEYCON0,
		S3C_W1KEYCON1,
		S3C_WIN1MAP,
		S3C_PA_LCD+0x800
	},
	{				  /* Window 2 */
		S3C_WINCON2,
		S3C_VIDOSD2A,
		S3C_VIDOSD2B,
		S3C_VIDOSD2C,
		S3C_VIDOSD2D,
		{
			{S3C_VIDW02ADD0, S3C_VIDW02ADD1},
			{0, 0}		  /* Only one buffer in win 2 */
		},
		S3C_VIDW02ADD2,
		S3C_W2KEYCON0,
		S3C_W2KEYCON1,
		S3C_WIN2MAP,
		S3C_PA_LCD+0x300
	},
	{				  /* Window 3 */
		S3C_WINCON3,
		S3C_VIDOSD3A,
		S3C_VIDOSD3B,
		S3C_VIDOSD3C,
		0,
		{
			{S3C_VIDW03ADD0, S3C_VIDW03ADD1},
			{0, 0}		  /* Only one buffer in win 4 */
		},
		S3C_VIDW03ADD2,
		S3C_W3KEYCON0,
		S3C_W3KEYCON1,
		S3C_WIN3MAP,
		S3C_PA_LCD+0x320
	},
	{				  /* Window 4 */
		S3C_WINCON4,
		S3C_VIDOSD4A,
		S3C_VIDOSD4B,
		S3C_VIDOSD4C,
		0,
		{
			{S3C_VIDW04ADD0, S3C_VIDW04ADD1},
			{0, 0}		  /* Only one buffer in win 4 */
		},
		S3C_VIDW04ADD2,
		S3C_W4KEYCON0,
		S3C_W4KEYCON1,
		S3C_WIN4MAP,
		S3C_PA_LCD+0x340
	},
};


/************************************************************************/
/* PROTOTYPES OF IMPORTED FUNCTIONS					*/
/************************************************************************/

/* Configure board specific LCD signals, e.g. GPIOs */
extern void s3c64xx_lcd_board_init(void);

/* Switch board specfic LCD signal on */
extern void s3c64xx_lcd_board_enable(int index);

/* Switch board specfic LCD signal on */
extern void s3c64xx_lcd_board_disable(int index);



/************************************************************************/
/* PROTOTYPES OF LOCAL FUNCTIONS					*/
/************************************************************************/


/************************************************************************/
/* Color Conversions RGBA->COLOR32					*/
/************************************************************************/

static COLOR32 r2c_cmap2(const wininfo_t *pwi, RGBA rgba)
{
	return lcd_rgbalookup(rgba, pwi->cmap, 2);
}

static COLOR32 r2c_cmap4(const wininfo_t *pwi, RGBA rgba)
{
	return lcd_rgbalookup(rgba, pwi->cmap, 4);
}

static COLOR32 r2c_cmap16(const wininfo_t *pwi, RGBA rgba)
{
	return lcd_rgbalookup(rgba, pwi->cmap, 16);
}

static COLOR32 r2c_cmap256(const wininfo_t *pwi, RGBA rgba)
{
	return lcd_rgbalookup(rgba, pwi->cmap, 256);
}

static COLOR32 r2c_rgba2321(const wininfo_t *pwi, RGBA rgba)
{
	COLOR32 temp;

	temp = rgba & 0x00000080;	   /* C[7] = A[7] */
	temp |= (rgba & 0xC0000000) >> 25; /* C[6:5] = R[7:6] */
	temp |= (rgba & 0x00E00000) >> 19; /* C[4:2] = G[7:5] */
	temp |= (rgba & 0x0000C000) >> 14; /* C[1:0] = B[7:6] */
	return temp;
}

static COLOR32 r2c_rgba5650(const wininfo_t *pwi, RGBA rgba)
{
	COLOR32 temp;

	temp = (rgba & 0xF8000000) >> 16; /* C[15:11] = R[7:3] */
	temp |= (rgba & 0x00FC0000) >> 13; /* C[10:5] = G[7:2] */
	temp |= (rgba & 0x0000F800) >> 11; /* C[4:0] = B[7:3] */
	return temp;
}

static COLOR32 r2c_rgba5551(const wininfo_t *pwi, RGBA rgba)
{
	COLOR32 temp;

	temp = (rgba & 0x00000080) << 8;  /* C[15] = A[7] */
	temp |= (rgba & 0xF8000000) >> 17; /* C[14:10] = R[7:3] */
	temp |= (rgba & 0x00F80000) >> 14; /* C[9:5] = G[7:3] */
	temp |= (rgba & 0x0000F800) >> 11; /* C[4:0] = B[7:3] */
	return temp;
}

static COLOR32 r2c_rgba6660(const wininfo_t *pwi, RGBA rgba)
{
	COLOR32 temp;

	temp = (rgba & 0xFC000000) >> 14; /* C[17:12] = R[7:2] */
	temp |= (rgba & 0x00FC0000) >> 12; /* C[11:6] = G[7:2] */
	temp |= (rgba & 0x0000FC00) >> 10; /* C[5:0] = B[7:2] */
	return temp;
}

static COLOR32 r2c_rgba6651(const wininfo_t *pwi, RGBA rgba)
{
	COLOR32 temp;

	temp = (rgba & 0x00000080) << 10;  /* C[17] = A[7] */
	temp |= (rgba & 0xFC000000) >> 15; /* C[16:11] = R[7:2] */
	temp |= (rgba & 0x00FC0000) >> 13; /* C[10:5] = G[7:2] */
	temp |= (rgba & 0x0000FC00) >> 11; /* C[4:0] = B[7:3] */
	return temp;
}

static COLOR32 r2c_rgba6661(const wininfo_t *pwi, RGBA rgba)
{
	COLOR32 temp;

	temp = (rgba & 0x00000080) << 11;  /* C[18] = A[7] */
	temp |= (rgba & 0xFC000000) >> 14; /* C[17:12] = R[7:2] */
	temp |= (rgba & 0x00FC0000) >> 12; /* C[11:6] = G[7:2] */
	temp |= (rgba & 0x0000FC00) >> 10; /* C[5:0] = B[7:2] */
	return temp;
}

static COLOR32 r2c_rgba8880(const wininfo_t *pwi, RGBA rgba)
{
	return rgba >> 8;
}

static COLOR32 r2c_rgba8871(const wininfo_t *pwi, RGBA rgba)
{
	COLOR32 temp;

	temp = (rgba & 0x00000080) << 16;
	temp |= rgba >> 9;
	return temp;
}

static COLOR32 r2c_rgba8881(const wininfo_t *pwi, RGBA rgba)
{
	COLOR32 temp;

	temp = (rgba & 0x00000080) << 17;
	temp |= rgba >> 8;

	return temp;
}

static COLOR32 r2c_rgba8884(const wininfo_t *pwi, RGBA rgba)
{
	COLOR32 temp;

	temp = (rgba & 0x000000F0) << 20;
	temp |= rgba >> 8;

	return temp;
}

/************************************************************************/
/* Color Conversions COLOR32->RGBA; functions must use LSBs to convert	*/
/************************************************************************/

static RGBA c2r_cmap2(const wininfo_t *pwi, COLOR32 color)
{
	return pwi->cmap[color & 0x00000001];
}

static RGBA c2r_cmap4(const wininfo_t *pwi, COLOR32 color)
{
	return pwi->cmap[color & 0x00000003];
}

static RGBA c2r_cmap16(const wininfo_t *pwi, COLOR32 color)
{
	return pwi->cmap[color & 0x0000000F];
}

static RGBA c2r_cmap256(const wininfo_t *pwi, COLOR32 color)
{
	return pwi->cmap[color & 0x000000FF];
}

static RGBA c2r_rgba2321(const wininfo_t *pwi, COLOR32 color)
{
	COLOR32 temp;
	RGBA rgba;

	rgba = (color & 0x00000060) << 25;  /* R[7:6] */
	rgba |= ((color & 0x00000003) << 30) >> 16; /* B[7:6] */
	rgba |= rgba >> 2;		    /* R[5:4], B[5:4] */
	rgba |= rgba >> 4;		    /* R[3:0], B[3:0] */

	temp = color & 0x0000001C;	    /* G3[4:2] */
	temp |= (temp << 3) | (temp >> 3);  /* G[7:5], G[1:0] */
	rgba |= temp << 16;

	if (color & 0x80)
		rgba |= 0xFF;

	return rgba;
}

static RGBA c2r_rgba5650(const wininfo_t *pwi, COLOR32 color)
{
	RGBA rgba;

	rgba = (color >> 9) << 25;	   /* R[7:3] */
	rgba |= (color << 27) >> 16;	   /* B[7:3] */
	rgba |= (rgba >> 5);		   /* R[2:0], B[2:0] */
	rgba &= 0xFF00FFFF;
	rgba |= (color & 0x000007E0) << 13;/* G[7:2] */
	rgba |= 0x000000FF;		   /* A[7:0] = 0xFF */
	rgba |= (color & 0x00000600) << 7; /* G[1:0] */

	return rgba;
}

static RGBA c2r_rgba5551(const wininfo_t *pwi, COLOR32 color)
{
	RGBA rgba;

	rgba = (color >> 10) << 27;	   /* R[7:3] */
	rgba |= (color & 0x000003E0) << 14;/* G[7:3] */
	rgba |= (color << 27) >> 16;       /* B[7:3] */
	rgba |= (rgba & 0xE0E0E000) >> 5;  /* R[2:0], G[2:0], B[2:0] */

	if (color & 0x8000)
		rgba |= 0xFF;		   /* A[7:0] = 0x00 or 0xFF */

	return rgba;
}

static RGBA c2r_rgba6660(const wininfo_t *pwi, COLOR32 color)
{
	RGBA rgba;

	rgba = (color >> 12) << 26;	    /* R[7:2] */
	rgba |= (color & 0x00000FC0) << 12; /* G[7:2] */
	rgba |= (color << 26) >> 16;        /* B[7:2] */
	rgba |= (rgba & 0xC0C0C000) >> 6;   /* R[1:0], G[1:0], B[1:0] */
	rgba |= 0xFF;			    /* A[7:0] = 0xFF */

	return rgba;
}

static RGBA c2r_rgba6651(const wininfo_t *pwi, COLOR32 color)
{
	RGBA rgba;

	rgba = (color >> 11) << 26;	    /* R[7..2] */
	rgba |= (color & 0x000007E0) << 13; /* G[7..2] */
	rgba |= (color << 27) >> 16;	    /* B[7..3] */
	rgba |= (rgba & 0xC0C00000) >> 6;   /* R[1:0], G[1:0] */
	rgba |= (rgba & 0x0000E000) >> 5;   /* B[2:0] */

	if (color & 0x00020000)
		rgba |= 0xFF;		    /* A[7:0] = 0x00 or 0xFF */

	return rgba;
}

static RGBA c2r_rgba6661(const wininfo_t *pwi, COLOR32 color)
{
	RGBA rgba;

	rgba = (color >> 12) << 26;	    /* R[7:2] */
	rgba |= (color & 0x00000FC0) << 12; /* G[7:2] */
	rgba |= (color << 26) >> 16;        /* B[7:2] */
	rgba |= (rgba & 0xC0C0C000) >> 6;   /* R[1:0], G[1:0], B[1:0] */
	if (color & 0x00040000)
		rgba |= 0xFF;		    /* A[7:0] = 0x00 or 0xFF */

	return rgba;
}

static RGBA c2r_rgba8880(const wininfo_t *pwi, COLOR32 color)
{
	return (color << 8) | 0xFF;	  /* R[7:0], G[7:0], B[7:0], A=0xFF */
}

static RGBA c2r_rgba8871(const wininfo_t *pwi, COLOR32 color)
{
	RGBA rgba;

	rgba = color << 9;		  /* R[7:0], G[7:0], B[7:1] */
	rgba |= (color & 0x40) << 2;	  /* B[0] */

	if (color & 0x00800000)
		rgba |= 0xFF;		  /* A[7:0] = 0x00 or 0xFF */

	return rgba;
}

static RGBA c2r_rgba8881(const wininfo_t *pwi, COLOR32 color)
{
	RGBA rgba;

	rgba = color << 8;		  /* R[7:0], G[7:0], B[7:0] */
	if (color & 0x01000000)
		rgba |= 0xFF;		  /* A[7:0] = 0x00 or 0xFF */

	return rgba;
}

static RGBA c2r_rgba8884(const wininfo_t *pwi, COLOR32 color)
{
	RGBA rgba;

	rgba = color << 8;		  /* R[7:0], G[7:0], B[7:0] */

	color <<= 4;
	color >>= 28;
	rgba |= color | (color << 4);     /* A[7:0] */

	return rgba;
}


/************************************************************************/
/* Apply Alpha COLOR32 * (256-alpha) + pre-multiplied pixel -> COLOR32	*/
/************************************************************************/

/* The following functions blend an existing pixel with a new pixel by
   applying an alpha value. The mathematically correct value would be:

     C = (C_old * (255 - A_pix) + C_pix * A_pix) / 255

   But dividing by 255 is rather time consuming. Therefore we use a slightly
   different blending function that divides by 256 which can be done by a
   right shift.

     C = (C_old * (256 - A_pix) + C_pix * (A_pix + 1)) / 256

   This formula generates only a slightly different result which is barely
   visible. This approach is also commonly used by LCD controller hardware.

     R_new = (R_old * (256 - A_pix) + R_pix * (A_pix + 1)) / 256
     G_new = (G_old * (256 - A_pix) + G_pix * (A_pix + 1)) / 256
     B_new = (B_old * (256 - A_pix) + B_pix * (A_pix + 1)) / 256
     A_new = A_old;

   Here are some considerations about performance optimization.

   - Usually, for example when drawing a line, rectangle or circle, several
     pixels with the same color/alpha are drawn. Therefore we can compute the
     part for the new pixel beforehand and only need to do the part for the
     existing pixel here. This we call pre-multiplied alpha and is done in
     lcd_set_col() (or the bitmap line drawing functions). The pre-multiplied
     data is part of the colinfo_t structure.

   - We could provide a global function apply_alpha() that gets two RGBA
     values (or one RGBA value and pre-multiplied data) and does the color
     computation for that pixel. However this would mean calling col2rgba()
     for the existing pixel to get the RGBA value, applying alpha and then
     calling rgba2col() to convert the pixel back to framebuffer format. All
     these steps involve quite a lot bit shifting which results in a rather
     slow processing. For example col2rgba() separates R, G, B of the
     framebuffer format and stuffs them in an RGBA value while apply_alpha()
     would immediately separate this again to R, G, B. We can avoid some of
     this overhead by having a special function for each pixel format. Here we
     don't need to assemble a temporary RGBA value, but we can immediately use
     R, G, B for the alpha-applying computation. And we can immediately
     convert R, G, and B back to the required framebuffer format.

   - We ary multiplying 8 bits by 8 bits with a 16 bit result were the most
     significant 8 bits are of interest. This allows processing of two color
     values in one go in a 32 bit register. By doing this, we can avoid one of
     the multiplications and one of the additions.

   - The right shift for /256 can be combined with the shift to the final bit
     position in the RGBA word reducing the number of needed shifts. */

#if 0
/* To understand the algorithm better this is how the apply_alpha() function
   would look if no pre-multiplication was used */
static RGBA apply_alpha(RGBA old, RGBA pix)
{
	RGBA RB, G;
	RGBA alpha1, alpha256;

	/* Get the alpha values from the new pixel */
	alpha1 = pix & 0x000000FF;
	alpha256 = 256 - alpha1;
	alpha1++;

	/* Remove A, shift values to a position so that after the
	   multiplication the result is already at the correct bit position */
	old >>= 8;
	pix >>= 8;

	/* Handle R and B together in one register, removing G */
	RB = (old & ~0xFF00) * alpha256 + (pix & ~0xFF00) * alpha1;

	/* Handle G, removing R and B */
	G = (old & 0xFF00) * alpha256 + (pix & 0xFF00) * alpha1;

	/* Combine result */
	return (RB & 0xFF00FF00) | (G & 0x00FF0000) | (old & 0x000000FF);
}
#endif

static COLOR32 aa_cmap(const wininfo_t *pwi, const colinfo_t *pci,
		       COLOR32 color, u_int count)
{
	RGBA RB, G;
	RGBA rgba, new;

	rgba = pwi->cmap[color & (count-1)];
	new = rgba & 0xFF;

	/* Remove A, shift values to a position so that after the
	   multiplication the result is already at the correct bit position */
	rgba >>= 8;

	/* Handle R and B together in one register, removing G */
	RB = (pci->RA1 << 16) | pci->BA1;
	RB = RB + (rgba & ~0xFF00) * pci->A256;

	/* Handle G, removing R and B */
	G = (rgba & 0xFF00) * pci->A256 + (pci->GA1 << 8);

	new |= RB & 0xFF00FF00;
	new |= G & 0x00FF0000;

	/* Now search matching color in color table */
	return lcd_rgbalookup(new, pwi->cmap, count);
}

static COLOR32 aa_cmap2(const wininfo_t *pwi, const colinfo_t *pci,
			COLOR32 color)
{
	return aa_cmap(pwi, pci, color, 2);
}

static COLOR32 aa_cmap4(const wininfo_t *pwi, const colinfo_t *pci,
			COLOR32 color)
{
	return aa_cmap(pwi, pci, color, 4);
}

static COLOR32 aa_cmap16(const wininfo_t *pwi, const colinfo_t *pci,
			 COLOR32 color)
{
	return aa_cmap(pwi, pci, color, 16);
}

static COLOR32 aa_cmap256(const wininfo_t *pwi, const colinfo_t *pci,
			  COLOR32 color)
{
	return aa_cmap(pwi, pci, color, 256);
}

static COLOR32 aa_rgba2321(const wininfo_t *pwi, const colinfo_t *pci,
			   COLOR32 color)
{
	RGBA BR, G;
	COLOR32 new;

	/* Handle R and B in one go */
	BR = (color & 0x60) << 1; /* BR[7:6] = R[7:6] */
	BR |= (color & 0x03);	  /* BR[23:22] = B[7:6] */
	BR |= BR >> 2;		  /* BR[23:20] = B[7:4], BR[7:4] = R[7:4] */
	BR |= BR >> 4;		  /* BR[23:16] = B[7:0], BR[7:0] = R[7:0] */

	/* Multiply B and R with (256-alpha) and add the pre-multiplied B and
	   R values; result is in BR[31:24] = B[7:0] and BR[15:8] = R[7:0] */
	BR = ((pci->RA1 | (pci->BA1 << 16)) + BR*pci->A256);

	G = (color & 0x1C);		  /* G[7:5] */
	G |= (G << 3) | (G >> 3);	  /* G[7:0] */

	/* Multiply G with (256-alpha) and add the pre-multiplied G; result is
	   in G[15:7], we only need G[15:9] */
	G = (pci->GA1 + G*pci->A256) & 0xE000;

	/* Combine R, G and B in the final value */
	new = color & 0x80;		  /* new[7] = A[7] */
	new |= (BR & 0xC000) >> 9;	  /* new[6:5] = R[7:6] */
	new |= G >> 11;			  /* new[4:2] = G[7:5] */
	new |= BR >> 30;		  /* new[1:0] = B[7:6] */

	return new;
}

static COLOR32 aa_rgba5650(const wininfo_t *pwi, const colinfo_t *pci,
			   COLOR32 color)
{
	RGBA BR, G;
	COLOR32 new;

	/* Handle R and B in one go */
	BR = color & 0xF800;	  /* BR[15:11] = R[7:3] */
	BR |= color << 27;	  /* BR[31:27] = B[7:3] */
	BR >>= 8;		  /* BR[23:19] = B[7:3], BR[7:3] = R[7:3] */
	BR |= (BR & ~0x1F0000) >> 5; /* BR[23:16] = B[7:0], BR[7:0] = R[7:0] */

	/* Multiply B and R with (256-alpha) and add the pre-multiplied B and
	   R values; result is in BR[31:24] = B[7:0] and BR[15:8] = R[7:0] */
	BR = (pci->RA1 | (pci->BA1 << 16)) + BR*pci->A256;

	G = (color & 0x07E0) >> 3;	  /* G[7:2] */
	G |= G >> 6;			  /* G[7:0] */

	/* Multiply G with (256-alpha) and add the pre-multiplied G; result is
	   in G[15:7], we only need G[15:9] */
	G = (pci->GA1 + G*pci->A256) & 0xFC00;

	/* Combine R, G and B in the final value */
	new = BR & 0xF800;		  /* new[15:11] = R[7:3] */
	new |= G >> 5;			  /* new[10:5] = G[7:2] */
	new |= BR >> 27;		  /* new[4:0] = B[7:3] */

	return new;
}

static COLOR32 aa_rgba5551(const wininfo_t *pwi, const colinfo_t *pci,
			   COLOR32 color)
{
	RGBA BR, G;
	COLOR32 new;

	/* Handle R and B in one go */
	BR = (color & 0x7C00) << 1; /* BR[15:11] = R[7:3] */
	BR |= color << 27;	  /* BR[31:27] = B[7:3] */
	BR >>= 8;		  /* BR[23:19] = B[7:3], BR[7:3] = R[7:3] */
	BR |= (BR & ~0x1F0000) >> 5; /* BR[23:16] = B[7:0], BR[7:0] = R[7:0] */

	/* Multiply B and R with (256-alpha) and add the pre-multiplied B and
	   R values; result is in BR[31:24] = B[7:0] and BR[15:8] = R[7:0] */
	BR = (pci->RA1 | (pci->BA1 << 16)) + BR*pci->A256;

	G = (color & 0x03E0) >> 3;	  /* G[7:3] */
	G |= G >> 5;			  /* G[7:0] */

	/* Multiply G with (256-alpha) and add the pre-multiplied G; result is
	   in G[15:7], we only need G[15:9] */
	G = (pci->GA1 + G*pci->A256) & 0xFC00;

	/* Combine R, G and B in the final value */
	new = color & 0x8000;		  /* new[15] = A[7] */
	new |= (BR & 0xF800) >> 1;	  /* new[14:10] = R[7:3] */
	new |= G >> 6;			  /* new[9:5] = G[7:3] */
	new |= BR >> 27;		  /* new[4:0] = B[7:3] */

	return new;
}

static COLOR32 aa_rgba6660(const wininfo_t *pwi, const colinfo_t *pci,
			   COLOR32 color)
{
	RGBA BR, G;
	COLOR32 new;

	/* Handle R and B in one go */
	BR = (color & 0x3F000) >> 2; /* BR[15:10] = R[7:2] */
	BR |= color << 26;	  /* BR[31:26] = B[7:2] */
	BR >>= 8;		  /* BR[23:18] = B[7:2], BR[7:2] = R[7:2] */
	BR |= (BR & ~0x3F0000) >> 6; /* BR[23:16] = B[7:0], BR[7:0] = R[7:0] */

	/* Multiply B and R with (256-alpha) and add the pre-multiplied B and
	   R values; result is in BR[31:24] = B[7:0] and BR[15:8] = R[7:0] */
	BR = (pci->RA1 | (pci->BA1 << 16)) + BR*pci->A256;

	G = (color & 0x00F30) >> 4;	  /* G[7:2] */
	G |= G >> 6;			  /* G[7:0] */

	/* Multiply G with (256-alpha) and add the pre-multiplied G; result is
	   in G[15:7], we only need G[15:9] */
	G = (pci->GA1 + G*pci->A256) & 0xFC00;

	/* Combine R, G and B in the final value */
	new = (BR & 0xFC00) << 2;	  /* new[17:12] = R[7:2] */
	new |= G >> 4;			  /* new[11:6] = G[7:2] */
	new |= BR >> 26;		  /* new[5:0] = B[7:2] */

	return new;
}

static COLOR32 aa_rgba6651(const wininfo_t *pwi, const colinfo_t *pci,
			   COLOR32 color)
{
	RGBA RG, B;
	COLOR32 new;

	/* Handle R and G in one go */
	RG = (color & 0x1F800) << 7;  /* RG[23:18] = R[7:2] */
	RG |= (color & 0x007E0) >> 3; /* RG[7:2] = G[7:2] */
	RG |= (RG & ~0x3F0000) >> 6;  /* RG[23:16] = R[7:0], RG[7:0] = G[7:0] */

	/* Multiply B and R with (256-alpha) and add the pre-multiplied B and
	   R values; result is in BR[31:24] = B[7:0] and BR[15:8] = R[7:0] */
	RG = ((pci->RA1 << 16) | (pci->GA1 << 16)) + RG*pci->A256;

	B = (color & 0x0001F) << 3;	  /* B[7:3] */
	B |= B >> 5;			  /* B[7:0] */

	/* Multiply G with (256-alpha) and add the pre-multiplied G; result is
	   in G[15:7], we only need G[15:9] */
	B = (pci->BA1 + B*pci->A256) & 0xF800;

	/* Combine R, G and B in the final value */
	new = color & 0x20000;		  /* new[17] = A[7] */
	new |= (RG & 0xFC000000) >> 15;	  /* new[16:11] = R[7:2] */
	new |= (RG & 0x0000FC00) >> 5;	  /* new[10:5] = G[7:2] */
	new |= B >> 11;			  /* new[4:0] = B[7:3] */

	return new;
}

static COLOR32 aa_rgba6661(const wininfo_t *pwi, const colinfo_t *pci,
			   COLOR32 color)
{
	RGBA BR, G;
	COLOR32 new;

	/* Handle R and B in one go */
	BR = (color & 0x3F000) >> 2; /* BR[15:10] = R[7:2] */
	BR |= color << 26;	  /* BR[31:26] = B[7:2] */
	BR >>= 8;		  /* BR[23:18] = B[7:2], BR[7:2] = R[7:2] */
	BR |= (BR & ~0x3F0000) >> 6; /* BR[23:16] = B[7:0], BR[7:0] = R[7:0] */

	/* Multiply B and R with (256-alpha) and add the pre-multiplied B and
	   R values; result is in BR[31:24] = B[7:0] and BR[15:8] = R[7:0] */
	BR = (pci->RA1 | (pci->BA1 << 16)) + BR*pci->A256;

	G = (color & 0x00F30) >> 4;	  /* G[7:2] */
	G |= G >> 6;			  /* G[7:0] */

	/* Multiply G with (256-alpha) and add the pre-multiplied G; result is
	   in G[15:7], we only need G[15:9] */
	G = (pci->GA1 + G*pci->A256) & 0xFC00;

	/* Combine R, G and B in the final value */
	new = color & 0x40000;		  /* new[18] = A[7] */
	new |= (BR & 0xFC00) << 2;	  /* new[17:12] = R[7:2] */
	new |= G >> 4;			  /* new[11:6] = G[7:2] */
	new |= BR >> 26;		  /* new[5:0] = B[7:2] */

	return new;
}

static COLOR32 aa_rgba8880(const wininfo_t *pwi, const colinfo_t *pci,
			   COLOR32 color)
{
	RGBA RB, G;
	COLOR32 new;

	/* Handle R and B in one go */
	RB = (color & 0x00FF00FF); /* RB[23:16] = R[7:0], RB[7:0] = B[7:0] */

	/* Multiply B and R with (256-alpha) and add the pre-multiplied B and
	   R values; result is in BR[31:24] = B[7:0] and BR[15:8] = R[7:0] */
	RB = ((pci->RA1 << 16) | pci->BA1) + RB*pci->A256;

	G = (color >> 8) & 0xFF;     /* G[7:0] */

	/* Multiply G with (256-alpha) and add the pre-multiplied G; result is
	   in G[15:7], we only need G[15:9] */
	G = (pci->GA1 + G*pci->A256) & 0xFC00;

	/* Combine R, G and B in the final value */
	new = (RB & 0xFF00FFFF) >> 8; /* new[23:16] = R[7:0], new[7:0]=B[7:0] */
	new |= G & 0xFF00;	      /* new[15:8] = G[7:0] */

	return new;
}

static COLOR32 aa_rgba8871(const wininfo_t *pwi, const colinfo_t *pci,
			   COLOR32 color)
{
	RGBA RB, G;
	COLOR32 new;

	/* Handle R and B in one go */
	RB = (color << 1) & 0xFF00FF; /* RB[23:16] = R[7:0], RB[7:1] = B[7:1] */
	RB |= (RB & 0x80) >> 7;	      /* RB[7:0] = B[7:0] */

	/* Multiply B and R with (256-alpha) and add the pre-multiplied B and
	   R values; result is in BR[31:24] = B[7:0] and BR[15:8] = R[7:0] */
	RB = ((pci->RA1 << 16) | pci->BA1) + RB*pci->A256;

	G = (color >> 7) & 0xFF;     /* G[7:0] */

	/* Multiply G with (256-alpha) and add the pre-multiplied G; result is
	   in G[15:7], we only need G[15:9] */
	G = (pci->GA1 + G*pci->A256) & 0xFC00;

	/* Combine R, G and B in the final value */
	new = (RB & 0xFF00FFFF) >> 9; /* new[22:15] = R[7:0], new[6:0]=B[7:1] */
	new |= (G & 0xFF00) >> 1;     /* new[14:7] = G[7:0] */
	new |= color & 0x20000;	      /* new[23] = A[7] */

	return new;
}

static COLOR32 aa_rgba8881(const wininfo_t *pwi, const colinfo_t *pci,
			   COLOR32 color)
{
	RGBA RB, G;
	COLOR32 new;

	/* Handle R and B in one go */
	RB = (color & 0x00FF00FF); /* RB[23:16] = R[7:0], RB[7:0] = B[7:0] */

	/* Multiply B and R with (256-alpha) and add the pre-multiplied B and
	   R values; result is in BR[31:24] = B[7:0] and BR[15:8] = R[7:0] */
	RB = ((pci->RA1 << 16) | pci->BA1) + RB*pci->A256;

	G = (color >> 8) & 0xFF;     /* G[7:0] */

	/* Multiply G with (256-alpha) and add the pre-multiplied G; result is
	   in G[15:7], we only need G[15:9] */
	G = (pci->GA1 + G*pci->A256) & 0xFC00;

	/* Combine R, G and B in the final value */
	new = (RB & 0xFF00FFFF) >> 8; /* new[23:16] = R[7:0], new[7:0]=B[7:0] */
	new |= G & 0xFF00;	      /* new[15:8] = G[7:0] */
	new |= color & 0x01000000;    /* new[24] = A[7] */

	return new;
}

static COLOR32 aa_rgba8884(const wininfo_t *pwi, const colinfo_t *pci,
			   COLOR32 color)
{
	RGBA RB, G;
	COLOR32 new;

	/* Handle R and B in one go */
	RB = (color & 0x00FF00FF); /* RB[23:16] = R[7:0], RB[7:0] = B[7:0] */

	/* Multiply B and R with (256-alpha) and add the pre-multiplied B and
	   R values; result is in BR[31:24] = B[7:0] and BR[15:8] = R[7:0] */
	RB = ((pci->RA1 << 16) | pci->BA1) + RB*pci->A256;

	G = (color >> 8) & 0xFF;     /* G[7:0] */

	/* Multiply G with (256-alpha) and add the pre-multiplied G; result is
	   in G[15:7], we only need G[15:9] */
	G = (pci->GA1 + G*pci->A256) & 0xFC00;

	/* Combine R, G and B in the final value */
	new = (RB & 0xFF00FFFF) >> 8; /* new[23:16] = R[7:0], new[7:0]=B[7:0] */
	new |= G & 0xFF00;	      /* new[15:8] = G[7:0] */
	new |= color & 0x0F000000;    /* new[27:24] = A[7:4] */

	return new;
}


/************************************************************************/
/* Pixel Information Table						*/
/************************************************************************/

const pixinfo_t pixel_info[PIXEL_FORMAT_COUNT] = {
	{ 1, 0,  2, r2c_cmap2,    c2r_cmap2,    aa_cmap2,
	  "2-entry color map"},		                      /* 0 */
	{ 2, 1,  4, r2c_cmap4,    c2r_cmap4,    aa_cmap4,
	  "4-entry color map"},		                      /* 1 */
	{ 4, 2, 16, r2c_cmap16,   c2r_cmap16,   aa_cmap16,
	  "16-entry color map"},	                      /* 2 */
	{ 8, 3,256, r2c_cmap256,  c2r_cmap256,  aa_cmap256,
	  "256-entry color map"},	                      /* 3 */
	{ 8, 3,  0, r2c_rgba2321, c2r_rgba2321, aa_rgba2321,
	  "RGBA-2321"},			                      /* 4 */
	{16, 4,  0, r2c_rgba5650, c2r_rgba5650, aa_rgba5650,
	 "RGBA-5650 (default)"},	                      /* 5 */
	{16, 4,  0, r2c_rgba5551, c2r_rgba5551, aa_rgba5551,
	 "RGBA-5551"},			                      /* 6 */
	{16, 4,  0, r2c_rgba5551, c2r_rgba5551, aa_rgba5551,
	 "RGBI-5551 (I=intensity)"},	                      /* 7 */
	{18, 5,  0, r2c_rgba6660, c2r_rgba6660, aa_rgba6660,
	 "RGBA-6660"},			                      /* 8 */
	{18, 5,  0, r2c_rgba6651, c2r_rgba6651, aa_rgba6651,
	 "RGBA-6651"},					      /* 9 */
	{19, 5,  0, r2c_rgba6661, c2r_rgba6661, aa_rgba6661,
	 "RGBA-6661"},					      /* 10 */
	{24, 5,  0, r2c_rgba8880, c2r_rgba8880, aa_rgba8880,
	 "RGBA-8880"},					      /* 11 */
	{24, 5,  0, r2c_rgba8871, c2r_rgba8871, aa_rgba8871,
	 "RGBA-8871"},					      /* 12 */
	{25, 5,  0, r2c_rgba8881, c2r_rgba8881, aa_rgba8881,
	 "RGBA-8881"},					      /* 13 */
	{28, 5,  0, r2c_rgba8884, c2r_rgba8884, aa_rgba8884,
	 "RGBA-8884"}			                      /* 14 */
};

/* Return a pointer to the pixel format info (NULL if pix not valid) */
static const pixinfo_t *s3c64xx_get_pixinfo_p(WINDOW win, u_char pix)
{
	const pixinfo_t *ppi = NULL;

	if ((pix < PIXEL_FORMAT_COUNT)
	    && (valid_pixels[win] & (1<<pix)))
		ppi = &pixel_info[pix];
	return ppi;
}

/* Set color map entry for windows 0 and 1; we always use format RGBA8881 */
static void set_cmap32(const wininfo_t *pwi, u_int index, u_int end,
		       RGBA *prgba)
{
	RGBA *cmap = pwi->cmap;
	u_int *hwcmap = (u_int *)winregs_table[pwi->win].hwcmap;

	__REG(S3C_WPALCON) |= S3C_WPALCON_PALUPDATEEN;
	do {
		RGBA rgba = *prgba++;
		RGBA newcmap;
		u_int newhw;

		newcmap = rgba & 0xFFFFFF00;
		newhw = rgba >> 8;
		if (rgba & 0x80) {
			newcmap |= 0xFF;
			newhw |= 0x1000000;
		}
		cmap[index] = newcmap;
		hwcmap[index] = newhw;
	} while (++index <= end);
	__REG(S3C_WPALCON) &= ~S3C_WPALCON_PALUPDATEEN;
}

/* Set color map entry for windows 2 to 4; we always use format RGBA5551 */
static void set_cmap16(const wininfo_t *pwi, u_int index, u_int end,
		       RGBA *prgba)
{
	RGBA *cmap = pwi->cmap;
	u_short *hwcmap = (u_short *)winregs_table[pwi->win].hwcmap;

	__REG(S3C_WPALCON) |= S3C_WPALCON_PALUPDATEEN;
	do {
		RGBA rgba = *prgba++;
		RGBA newcmap;
		u_int newhw;

		newcmap = rgba & 0xF8F8F800;
		newcmap |= (rgba & 0xE0E0E000) >> 5;
		newhw = (rgba & 0xF8000000) >> 17; /* C[14:10] = R[7:3] */
		newhw |= (rgba & 0x00F80000) >> 14; /* C[9:5] = G[7:3] */
		newhw |= (rgba & 0x0000F800) >> 11; /* C[4:0] = B[7:3] */
		newhw = rgba >> 8;
		if (rgba & 0x80) {
			newcmap |= 0xFF;
			newhw |= 0x8000;
		}
		cmap[index] = newcmap;
		hwcmap[index] = (u_short)newhw;
	} while (++index <= end);
	__REG(S3C_WPALCON) &= ~S3C_WPALCON_PALUPDATEEN;
}

/* Return maximum horizontal framebuffer resolution for this window */
static HVRES s3c64xx_get_fbmaxhres(WINDOW win, u_char pix)
{
	/* Maximum linelen is 13 bits, that's at most 8191 bytes; however to
	   be word aligned even for 1bpp, we can at most take 8188 */
	return (u_short)((8188 << 3) >> pixel_info[pix].bpp_shift);
}

/* Return maximum vertical framebuffer resolution for this window */
static HVRES s3c64xx_get_fbmaxvres(WINDOW win, u_char pix)
{
	/* No limit on vertical size */
	return 65535;
}

/* Align horizontal buffer resolution to next word boundary (round up) */
static HVRES s3c64xx_align_hres(WINDOW win, u_char pix, HVRES hres)
{
	unsigned shift = 5 - pixel_info[pix].bpp_shift;
	XYPOS mask = (1 << shift) - 1;

	hres = (hres + mask) & ~mask;
	return hres;
}

/* Align horizontal offset to current word boundary (round down) */
static XYPOS s3c64xx_align_hoffs(const wininfo_t *pwi, XYPOS hoffs)
{
	unsigned shift = 5 - pwi->ppi->bpp_shift;

	hoffs >>= shift;
	hoffs <<= shift;

	return hoffs;
}

/************************************************************************/
/* Setting Video Infos							*/
/************************************************************************/

static void s3c64xx_set_vidinfo(vidinfo_t *pvi)
{
	ulong hclk;
	unsigned int div;
	unsigned hline;
	unsigned vframe;
	unsigned ticks;
	u_int drive;

	hclk = get_HCLK();

	/* Do sanity check on all values */
	if ((pvi->lcd.clk == 0) && (pvi->lcd.fps == 0))
		pvi->lcd.fps = 60;	  /* Neither fps nor clk are given */
	if (pvi->lcd.hres > 2047)
		pvi->lcd.hres = 2047;
	if (pvi->lcd.vres > 2047)
		pvi->lcd.vres = 2047;
	if (pvi->lcd.hfp > 255)
		pvi->lcd.hfp = 255;
	if (pvi->lcd.hsw > 255)
		pvi->lcd.hsw = 255;
	else if (pvi->lcd.hsw < 1)
		pvi->lcd.hsw = 1;
	if (pvi->lcd.hbp > 255)
		pvi->lcd.hbp = 255;
	if (pvi->lcd.vfp > 255)
		pvi->lcd.vfp = 255;
	if (pvi->lcd.vsw > 255)
		pvi->lcd.vsw = 255;
	else if (pvi->lcd.vsw < 1)
		pvi->lcd.vsw = 1;
	if (pvi->lcd.vbp > 255)
		pvi->lcd.vbp = 255;
	if (pvi->drive < 3) {
		pvi->drive = 2;
		drive = 0;
	} else if (pvi->drive < 5) {
		pvi->drive = 4;
		drive = 1;
	} else if (pvi->drive < 8) {
		pvi->drive = 7;
		drive = 2;
	} else {
		pvi->drive = 9;
		drive = 3;
	}

	/* Number of clocks for one horizontal line */
	hline = pvi->lcd.hres + pvi->lcd.hfp + pvi->lcd.hsw + pvi->lcd.hbp;

	/* Number of hlines for one vertical frame */
	vframe = pvi->lcd.vres + pvi->lcd.vfp + pvi->lcd.vsw + pvi->lcd.vbp;

	/* Number of clock ticks per frame */
	ticks = hline*vframe;
	
	if (pvi->lcd.clk) {
		/* Compute divisor from given pixel clock */
		div = (hclk + pvi->lcd.clk/2)/pvi->lcd.clk;
	} else {
		/* Ticks per second */
		unsigned ticks_per_second = ticks * pvi->lcd.fps;

		/* Compute divisor from given frame rate */
		div = (hclk + ticks_per_second/2)/ticks_per_second;
	}
	if (div < 2)
		div = 2;
	else if (div > 256)
		div = 256;
	pvi->lcd.clk = (hclk + div/2)/div;
	pvi->lcd.fps = (pvi->lcd.clk + ticks/2)/ticks;

	/* Output selection, clock setting and enable is in VIDCON0; keep
	   enabled status */
	__REG(S3C_VIDCON0) = (__REG(S3C_VIDCON0) & 0x3)
		| S3C_VIDCON0_INTERLACE_F_PROGRESSIVE
		| S3C_VIDCON0_VIDOUT_RGB_IF
		| S3C_VIDCON0_PNRMODE_RGB_P
		| S3C_VIDCON0_CLKVALUP_ST_FRM
		| S3C_VIDCON0_CLKVAL_F(div-1)
		| S3C_VIDCON0_CLKDIR_DIVIDED
		| S3C_VIDCON0_CLKSEL_F_HCLK;

	/* Signal polarity is in VIDCON1 */
	__REG(S3C_VIDCON1) =
		((pvi->lcd.clkpol) ? 0 : S3C_VIDCON1_IVCLK_RISE_EDGE)
		| ((pvi->lcd.hspol) ? S3C_VIDCON1_IHSYNC_INVERT : 0)
		| ((pvi->lcd.vspol) ? S3C_VIDCON1_IVSYNC_INVERT : 0)
		| ((pvi->lcd.denpol)? S3C_VIDCON1_IVDEN_INVERT : 0);

	/* TV settings are not required when using LCD */
	__REG(S3C_VIDCON2) = 0;

	/* Vertical timing is in VIDTCON0 */
	__REG(S3C_VIDTCON0) =
		S3C_VIDTCON0_VBPD(pvi->lcd.vbp)
		| S3C_VIDTCON0_VFPD(pvi->lcd.vfp)
		| S3C_VIDTCON0_VSPW(pvi->lcd.vsw);

	/* Horizontal timing is in VIDTCON1 */
	__REG(S3C_VIDTCON1) =
		S3C_VIDTCON1_HBPD(pvi->lcd.hbp)
		| S3C_VIDTCON1_HFPD(pvi->lcd.hfp)
		| S3C_VIDTCON1_HSPW(pvi->lcd.hsw);

	/* Display resolution is in VIDTCON2 */
	__REG(S3C_VIDTCON2) =
		S3C_VIDTCON2_LINEVAL(pvi->lcd.vres)
		| S3C_VIDTCON2_HOZVAL(pvi->lcd.hres);

	/* No video interrupts */
	__REG(S3C_VIDINTCON0) = 0;

	/* Dithering mode */
	__REG(S3C_DITHMODE) =
		S3C_DITHMODE_RDITHPOS_5BIT
		| S3C_DITHMODE_GDITHPOS_6BIT
		| S3C_DITHMODE_BDITHPOS_5BIT
		| (pvi->frc ? S3C_DITHMODE_DITHERING_ENABLE : 0);

	/* Drive strength */
	__REG(SPCON) = (__REG(SPCON) & ~(0x3 << 24)) | (drive << 24);

//###	printf("###VIDCON0=0x%lx, VIDCON1=0x%lx, VIDTCON0=0x%lx, VIDTCON1=0x%lx, VIDTCON2=0x%lx, DITHMODE=0x%lx, hclk=%lu\n", __REG(S3C_VIDCON0), __REG(S3C_VIDCON1), __REG(S3C_VIDTCON0), __REG(S3C_VIDTCON1), __REG(S3C_VIDTCON2), __REG(S3C_DITHMODE), hclk);
}

/************************************************************************/
/* Setting Window Infos							*/
/************************************************************************/

static void s3c64xx_set_wininfo(const wininfo_t *pwi)
{
	const struct LCD_WIN_REGS *winregs = &winregs_table[pwi->win];
	const vidinfo_t *pvi = pwi->pvi;
	unsigned pagewidth;
	unsigned burstlen;
	unsigned bld_pix;
	unsigned alpha_sel;
	unsigned bppmode_f;
	signed hpos, vpos;
	unsigned hres, vres;
	unsigned hoffs, voffs;
	XYPOS panel_hres;
	XYPOS panel_vres;
	u_long addr;
	unsigned alpha;

	hres = pvi->align_hres(pwi->win, pwi->pix, pwi->hres);
	vres = pwi->vres;
	hpos = pwi->hpos;
	vpos = pwi->vpos;
	hoffs = pwi->hoffs;
	voffs = pwi->voffs;
	panel_hres = (XYPOS)pvi->lcd.hres;
	panel_vres = (XYPOS)pvi->lcd.vres;

	/* Compute size of actual window and window viewport in framebuffer;
	   we trust that fbhres and fbvres are already checked against
	   lcd_getfbmaxhres() and lcd_getfbmaxvres() respectively in
	   cmd_lcd.c. Also hres/vres should not exceed fbhres/fbvres. */
	if ((hpos + hres < 0) || (hpos >= panel_hres)
	    || (vpos + vres < 0) || (vpos >= panel_vres)) {
		/* Window is completeley outside of panel */
		hres = 0;
		vres = 0;
	} else {
		/* If part of window reaches outside of panel, reduce size */
		if (hpos < 0) {
			hres += hpos;
			hoffs -= hpos;
			printf("###vorher=%d,", hoffs);
			hoffs = pvi->align_hoffs(pwi, hoffs);
			printf("###nachher=%d\n", hoffs);
			hpos = 0;
		} else if (hpos + hres >= panel_hres)
			hres = panel_hres - hpos;
		if (vpos < 0) {
			vres += vpos;
			voffs -= vpos;
			vpos = 0;
		} else if (vpos + vres >= panel_vres)
			vres = panel_vres - vpos;
	}

	/* If nothing visible, disable window */
	if (!hres || !vres || !pwi->fbcount) {
		__REG(winregs->wincon) &= ~S3C_WINCONx_ENWIN_F_ENABLE;
//###		printf("###Win %u disable: WINCON=0x%lx\n", pwi->win, __REG(winregs->wincon));
		return;			  /* not enabled */
	}

	pagewidth = pvi->align_hres(pwi->win, pwi->pix, hres);
	pagewidth = (pagewidth << pwi->ppi->bpp_shift) >> 3;
	if (pagewidth > 16*4)
		burstlen = S3C_WINCONx_BURSTLEN_16WORD;
	else if (pagewidth > 8*4)
		burstlen = S3C_WINCONx_BURSTLEN_8WORD;
	else
		burstlen = S3C_WINCONx_BURSTLEN_4WORD;

	if (pwi->pix == 14) {
		bld_pix = S3C_WINCONx_BLD_PIX_PIXEL;
		alpha_sel = S3C_WINCONx_ALPHA_SEL_1;
	} else {
		bld_pix =
			(pwi->alphamode & 0x2) ? S3C_WINCONx_BLD_PIX_PIXEL : 0;
		alpha_sel =
			(pwi->alphamode & 0x2) ? S3C_WINCONx_ALPHA_SEL_1 : 0;
	}
	bppmode_f = pwi->pix;
	if (bppmode_f > 13)
		bppmode_f = 13;

	/* General window configuration */
	__REG(winregs->wincon) = 
		S3C_WINCONx_WIDE_NARROW(0)
		| S3C_WINCONx_ENLOCAL_DMA
		| (pwi->fbshow ? S3C_WINCONx_BUFSEL_1 : S3C_WINCONx_BUFSEL_0)
		| S3C_WINCONx_BUFAUTOEN_DISABLE
		| S3C_WINCONx_BITSWP_DISABLE
		| S3C_WINCONx_BYTSWP_DISABLE
		| S3C_WINCONx_HAWSWP_DISABLE
		| S3C_WINCONx_INRGB_RGB
		| burstlen
		| bld_pix
		| (bppmode_f << 2)
		| alpha_sel
		| S3C_WINCONx_ENWIN_F_ENABLE;

	__REG(winregs->vidosdtl) =
		S3C_VIDOSDxA_OSD_LTX_F(hpos)
		| S3C_VIDOSDxA_OSD_LTY_F(vpos);

	__REG(winregs->vidosdbr) =
		S3C_VIDOSDxB_OSD_RBX_F(hpos + hres - 1)
		| S3C_VIDOSDxB_OSD_RBY_F(vpos + vres - 1);

	if (winregs->vidosdsize)
		__REG(winregs->vidosdsize) = 0; /* Only for TV-Encoder */

	alpha = (pwi->alpha0 & 0x0000F000);	   /* B */
	alpha |= (pwi->alpha0 & 0x00F00000) >> 4;  /* G */
	alpha |= (pwi->alpha0 & 0xF0000000) >> 8;  /* R */
	alpha |= (pwi->alpha1 & 0xF0000000) >> 20; /* R */
	alpha |= (pwi->alpha1 & 0x00F00000) >> 16; /* G */
	alpha |= (pwi->alpha1 & 0x0000F000) >> 12; /* B */
	if (winregs->vidosdalpha)
		__REG(winregs->vidosdalpha) = alpha;

	/* Set buffer 0 */
	addr = pwi->pfbuf[0] + voffs*pwi->linelen;
	addr += (hoffs << pwi->ppi->bpp_shift) >> 3;
	__REG(winregs->vidadd[0].start) = addr;
	__REG(winregs->vidadd[0].end) = (addr + vres*pwi->linelen) & 0x00FFFFFF;

	/* Set buffer 1 (if applicable) */
	if (pwi->fbcount > 1) {
		addr = pwi->pfbuf[1] + voffs*pwi->linelen;
		addr += (hoffs << pwi->ppi->bpp_shift) >> 8;
		if (winregs->vidadd[1].start)
			__REG(winregs->vidadd[1].start) = addr;
		if (winregs->vidadd[1].end)
			__REG(winregs->vidadd[1].end) =
				       (addr + vres*pwi->linelen) & 0x00FFFFFF;
	}

	__REG(winregs->vidsize) =
		S3C_VIDWxxADD2_OFFSIZE_F(pwi->linelen - pagewidth)
		| S3C_VIDWxxADD2_PAGEWIDTH_F(pagewidth);

	__REG(S3C_WPALCON) =
		S3C_WPALCON_W4PAL_16BIT_A
		| S3C_WPALCON_W3PAL_16BIT_A
		| S3C_WPALCON_W2PAL_16BIT_A
		| S3C_WPALCON_W1PAL_25BIT_A
		| S3C_WPALCON_W0PAL_25BIT_A;

	/* Set a fix color for window (instead of normal content) if alpha of
	   the replacement color is >127 */
	__REG(winregs->winmap) =
		(pwi->replace >> 8)
		| ((pwi->replace & 0x80) ? S3C_WINxMAP_MAPCOLEN_F_ENABLE : 0);

	if (winregs->keycon)
		__REG(winregs->keycon) =
		 ((pwi->ckmode & 2) ? S3C_WxKEYCON0_KEYBLEN_ENABLE : 0)
		 | ((pwi->ckvalue & 0x80) ? S3C_WxKEYCON0_KEYEN_F_ENABLE : 0)
		 | ((pwi->ckmode & 1) ? S3C_WxKEYCON0_DIRCON_MATCH_BG_IMAGE : 0)
		 | (pwi->ckmask >> 8);

	if (winregs->keyval)
		__REG(winregs->keyval) = pwi->ckvalue >> 8;

//###	printf("###Window %u enabled: wincon=0x%lx, vidosdtl=0x%lx, vidosdbr=0x%lx, vidosdsize=0x%lx\n", pwi->win, __REG(winregs->wincon), __REG(winregs->vidosdtl), __REG(winregs->vidosdbr), __REG(winregs->vidosdsize));
//###	printf("### vidosdalpha=0x%lx, vidadd[0].start=0x%lx, vidadd[0].end=0x%lx, vidsize=0x%lx\n", __REG(winregs->vidosdalpha), __REG(winregs->vidadd[0].start), __REG(winregs->vidadd[0].end), __REG(winregs->vidsize));
//###	printf("### WPALCON=0x%lx, winmap=0x%lx, keycon=0x%lx, keyval=0x%lx\n", __REG(S3C_WPALCON), __REG(winregs->winmap), __REG(winregs->keycon), __REG(winregs->keyval));
	return;				  /* enabled */
}

/* Activate display in power-on sequence order */
static void s3c64xx_enable(const vidinfo_t *pvi)
{
	u_short delay = 0;
	int index = 0;
	const u_short *ponseq = pvi->lcd.ponseq;

	/* Find next delay entry */
	while ((index = find_delay_index(ponseq, index, delay)) >= 0) {
		u_short newdelay = ponseq[index];

		/* Wait for the delay difference, if delay is higher */
		if (newdelay > delay) {
			udelay((newdelay - delay)*1000);
			delay = newdelay;
		}

		if (index == PON_CONTR) {
			/* Activate LCD controller */
			__REG(S3C_VIDCON0) |=
				S3C_VIDCON0_ENVID_ENABLE
				| S3C_VIDCON0_ENVID_F_ENABLE;
		}

		/* Switch appropriate signal on; this is board specific */
		s3c64xx_lcd_board_enable(index);
	}

#if 0 //####
	/* Activate VLCD */
	__REG(GPKDAT) |= (1<<0);

	/* Activate Buffer Enable */
	__REG(GPKDAT) &= ~(1<<3);

	/* Activate LCD controller */
	__REG(S3C_VIDCON0) |=
		S3C_VIDCON0_ENVID_ENABLE | S3C_VIDCON0_ENVID_F_ENABLE;

	/* Activate Display Enable */
	__REG(GPKDAT) &= ~(1<<2);

	/* Activate VCFL */
	__REG(GPKDAT) |= (1<<1);

	/* Activate VEEK (backlight intensity full) */
	__REG(GPFDAT) |= (0x1<<15);
#endif	// ###
}

/* Deactivate display in power-off sequence order */
static void s3c64xx_disable(const vidinfo_t *pvi)
{
	u_short delay = 0;
	int index = 0;
	const u_short *poffseq = pvi->lcd.poffseq;

	/* Find next delay entry */
	while ((index = find_delay_index(poffseq, index, delay)) >= 0) {
		u_short newdelay = poffseq[index];

		/* Wait for the delay difference, if delay is higher */
		if (newdelay > delay) {
			udelay((newdelay - delay)*1000);
			delay = newdelay;
		}

		/* Switch appropriate signal off; this is board specific */
		s3c64xx_lcd_board_enable(index);

		if (index == PON_CONTR) {
			/* Deactivate LCD controller */
			__REG(S3C_VIDCON0) &= ~S3C_VIDCON0_ENVID_F_ENABLE;
		}
	}

#if 0 //#####
	/* Deactivate VEEK (backlight intensity off) */
	__REG(GPFDAT) &= ~(0x1<<15);

	/* Deactivate VCFL */
	__REG(GPKDAT) &= ~(1<<1);

	/* Deactivate Display Enable */
	__REG(GPKDAT) |= (1<<2);

	/* Deactivate LCD controller */
	__REG(S3C_VIDCON0) &= ~S3C_VIDCON0_ENVID_F_ENABLE;

	/* Deactivate Buffer Enable */
	__REG(GPKDAT) |= (1<<3);

	/* Deactivate VLCD */
	__REG(GPKDAT) &= ~(1<<0);
#endif //####
}




/* One vidinfo structure (=support for one display) */
static const vidinfo_t s3c64xx_vidinfo = {
	driver_name: DRIVER_NAME,
	wincount: MAX_WINDOWS,
	pixcount: PIXEL_FORMAT_COUNT,
	pwi: s3c64xx_wininfo,
	get_pixinfo_p: s3c64xx_get_pixinfo_p,
	get_fbmaxhres: s3c64xx_get_fbmaxhres,
	get_fbmaxvres: s3c64xx_get_fbmaxvres,
	align_hres: s3c64xx_align_hres,
	align_hoffs: s3c64xx_align_hoffs,
	set_vidinfo: s3c64xx_set_vidinfo,
	set_wininfo: s3c64xx_set_wininfo,
	enable: s3c64xx_enable,
	disable: s3c64xx_disable,
};

/************************************************************************/
/* EXPORTED FUNCTIONS							*/
/************************************************************************/

/* In addition to the standard video signals, we have five more signals:
   GPF15: PWM output (backlight intensity)
   GPK3:  Buffer Enable (active low): enable display driver chips
   GPK2:  Display Enable (active low): Set signal for display
   GPK1:  VCFL (active high): Activate backlight voltage
   GPK0:  VLCD (active high): Activate LCD voltage */
void s3c64xx_lcd_init(vidinfo_t *pvi)
{
	u_long *pfbuf;
	wininfo_t *pwi;
	WINDOW win;

	/* Configure board specific LCD signals, e.g. GPIOs used for LCD power
	   (VLCD), Display Enable, signal buffers, PWM, backlight (VCFL) */
	s3c64xx_lcd_board_init();

	__REG(S3C_HOSTIFB_MIFPCON) = 0;	  /* 0: normal-path (no by-pass) */
	__REG(HCLK_GATE) |= (1<<3);	  /* Enable clock to LCD */
	__REG(SPCON) = (__REG(SPCON) & ~0x3) | 0x1; /* Select RGB I/F */

	/* Setup GPI: LCD_VD[15:0], no pull-up/down */
	__REG(GPICON) = 0xAAAAAAAA;
	__REG(GPIPUD) = 0x00000000;

	/* Setup GPJ: LCD_VD[23:16], HSYNC, VSYNC, DEN, CLK, no pull-up/down */
	__REG(GPJCON) = 0xAAAAAAAA;
	__REG(GPJPUD) = 0x00000000;

	/* Copy our vidinfo to the global array pointer */
	*pvi = s3c64xx_vidinfo;

	/* Initialize hardware-specific part of the windows information:
	   Pointer to framebuffers, default pixel format, maximum buffer
	   count, color map info and color map access. */
	pfbuf = s3c64xx_fbuf;
	for (win=0, pwi=s3c64xx_wininfo; win < MAX_WINDOWS; win++, pwi++) {
		u_int fbmaxcount = (win < 2) ? 2 : 1;
		pwi->cmap = cmap + cmap_offset[win];
		pwi->set_cmap = (win <= 1) ? set_cmap32 : set_cmap16;
		pwi->defpix = DEFAULT_PIXEL_FORMAT;
		pwi->pfbuf = pfbuf;
		pwi->fbmaxcount = (u_char)fbmaxcount;
		pfbuf += fbmaxcount;
	}
}

