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

#define PIXEL_FORMAT_COUNT 15


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
	u_int  clut;			  /* Color look-up table address */
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

const pixinfo_t pixel_info[PIXEL_FORMAT_COUNT] = {
	{ 1, 0,   2, "2-entry color look-up table"},   /* 0 */
	{ 2, 1,   4, "4-entry color look-up table"},   /* 1 */
	{ 4, 2,  16, "16-entry color look-up table"},  /* 2 */
	{ 8, 3, 256, "256-entry color look-up table"}, /* 3 */
	{ 8, 3,   0, "RGBA-2321"},			   /* 4 */
	{16, 4,   0, "RGBA-5650 (default)"},	   /* 5 */
	{16, 4,   0, "RGBA-5551"},			   /* 6 */
	{16, 4,   0, "RGBI-5551 (I=intensity)"},	   /* 7 */
	{18, 5,   0, "RGBA-6660"},			   /* 8 */
	{18, 5,   0, "RGBA-6651"},			   /* 9 */
	{19, 5,   0, "RGBA-6661"},			   /* 10 */
	{24, 5,   0, "RGBA-8880"},			   /* 11 */
	{24, 5,   0, "RGBA-8871"},			   /* 12 */
	{25, 5,   0, "RGBA-8881"},			   /* 13 */
	{28, 5,   0, "RGBA-8884"},			   /* 14 */
};

/* Valid pixel formats for each window */
u_int valid_pixels[5] = {
	0x09AF,			/* Win 0: no alpha formats */
	0x7FFF,			/* Win 1: no restrictions  */
	0x7FF7,			/* Win 2: no CLUT-8 */
	0x7FE7,			/* Win 3: no CLUT-8, no RGBA-2321 */
	0x7FE3,			/* Win 4: no CLUT-4, no ClUT-8, no RGBA-2321 */
};

/* Additional commands recognized as extension of lcdwin; values must start at
   WI_UNKOWN+1. */
enum WIN_INDEX_EXT {
	WI_ALPHA = WI_UNKNOWN+1,
	WI_COLKEY,
};

const kwinfo_t winextkeywords[] = {
	{2, 4, WI_ALPHA,  "alpha"},
	{1, 5, WI_COLKEY, "colkey"},
};


#if 0 //#####
ulong calc_fbsize (void)
{
	//#### TODO
	return 0;
}
#endif

#if 0 //######
/* Compute framebuffer base address from size (immediately before U-Boot) */
ulong lcd_fbsize(ulong fbsize)
{
	/* Round size up to pages */
	fbsize = (fbsize + (PAGE_SIZE-1)) & ~(PAGE_SIZE-1);

	return CFG_UBOOT_BASE - fbsize;
}
#endif


void lcd_ctrl_init(void)
{
	printf("#### lcd_ctrl_init\n");
	//#### TODO
}

void lcd_setcolreg (ushort regno, ushort red, ushort green, ushort blue)
{
	//#### TODO
}

int lcd_enable(void)
{
	//#### TODO
	return 1;
}

/* LCD controller */
void lcd_disable(void)
{
	//#### TODO
}

/* Backlight & Co */
void lcd_panel_disable(void)
{
	//#### TODO
}

/* Handle extension commands for lcdwin; on error, return 1 and keep *pwi
   unchanged */
int lcdwin_ext_exec(wininfo_t *pwi, int argc, char *argv[], u_int si)
{
	wininfo_ext_t *ext = &pwi->ext;

	/* OK, parse arguments and set data */
	switch (si) {
	case WI_ALPHA:
	{
		u_char bld_pix = 1;
		u_char alpha_sel = 0;
		u_int alpha0_rgba;
		u_int alpha1_rgba;
		u_int alpha;
		
		/* Arguments 1+2: alpha0 and alpha1 */
		if ((parse_rgb(argv[2], &alpha0_rgba) == RT_NONE)
		    || (parse_rgb(argv[3], &alpha1_rgba) == RT_NONE))
			return 1;

		/* Argument 3: pix setting */
		if (argc > 4)
			bld_pix = (simple_strtoul(argv[4], NULL, 0) != 0);

		/* Argument 4: sel setting */
		if (argc > 5)
			alpha_sel = (simple_strtoul(argv[5], NULL, 0) != 0);

		/* Prepare alpha value from alpha0 and alpha1 */
		alpha = (alpha0_rgba & 0x0000F000);	   /* B */
		alpha |= (alpha0_rgba & 0x00F00000) >> 4;  /* G */
		alpha |= (alpha0_rgba & 0xF0000000) >> 8;  /* R */
		alpha |= (alpha1_rgba & 0xF0000000) >> 20; /* R */
		alpha |= (alpha1_rgba & 0x00F00000) >> 16; /* G */
		alpha |= (alpha1_rgba & 0x0000F000) >> 12; /* B */

		ext->alpha = alpha;
		ext->bld_pix = bld_pix;
		ext->alpha_sel = alpha_sel;
		break;
	}

	case WI_COLKEY:
	{
		u_char ckdir = 0;
		u_char ckblend = 0;
		u_int ckvalue_rgba;
		u_int ckmask_rgba;

		/* Arguments 4 + 5: color key value and mask as RGB values */
		if (argc > 5) {
			if (argc == 6) {
				printf("Missing argument\n");
				return 1;
			}
			if ((parse_rgb(argv[5], &ckvalue_rgba) == RT_NONE)
			    || (parse_rgb(argv[6], &ckmask_rgba) == RT_NONE))
				return 1;
			ext->ckvalue = ckvalue_rgba;
			ext->ckmask = ckmask_rgba;
		}

		/* Argument 1: enable */
		ext->ckenable = (simple_strtoul(argv[2], NULL, 0) != 0);

		/* Argument 2: dir */
		if (argc > 3)
			ckdir = (simple_strtoul(argv[3], NULL, 0) != 0);
		ext->ckdir = ckdir;

		/* Argument 3: blend */
		if (argc > 4)
			ckblend = (simple_strtoul(argv[4], NULL, 0) != 0);
		ext->ckblend = ckblend;
		break;
	}
		
	default:
		break;
	}

	return 0;
}

/* Print info for all commands in winextkeywords[]; we are also called if si
   is WI_INFO, as this should print *all* window data */
void lcdwin_ext_print(wininfo_t *pwi, u_int si)
{
	wininfo_ext_t *ext = &pwi->ext;

	switch (si) {
	case WI_INFO:
	case WI_ALPHA: {
		u_int alpha0, alpha1;

		alpha0 = ext->alpha & 0x00F00000;	  /* R */
		alpha0 |= (ext->alpha & 0x000F0000) >> 4; /* G */
		alpha0 |= (ext->alpha & 0x0000F000) >> 8; /* B */
		alpha0 |= alpha0 >> 4;

		alpha1 = ext->alpha & 0x0000000F; /* B */
		alpha1 |= (ext->alpha & 0x000000F0) << 4; /* G */
		alpha1 |= (ext->alpha & 0x00000F00) << 8; /* R */
		alpha1 |= alpha1 << 4;
		printf("Alpha:\t\t"
		       "alpha0=#%06x, alpha1=#%06x, bld_pix=%d, alpha_sel=%d\n",
		       alpha0, alpha1, ext->bld_pix, ext->alpha_sel);
		if (si != WI_INFO)
			break;
	}
		/* WI_INFO: fall through to case WI_COLKEY */
	case WI_COLKEY:
		printf("Color Key:\tenable=%d, dir=%d, blend=%d,"
		       "colkey=#%06x, colmask=#%06x\n",
		       ext->ckenable, ext->ckdir, ext->ckblend,
		       ext->ckvalue, ext->ckmask);
		break;

	default:
		break;
	}
}

/* Return a pointer to the pixel format info (NULL if pix not valid) */
const pixinfo_t *lcd_getpixinfo(WINDOW win, u_char pix)
{
	const pixinfo_t *ppi = NULL;

	if ((pix < PIXEL_FORMAT_COUNT)
	    && (valid_pixels[win] & (1<<pix)))
		ppi = &pixel_info[pix];
	return ppi;
}

/* Return index of next valid pixel format >= pix; result is >=
   PIXEL_FORMAT_COUNT if no further format is found; usually the result is
   immediately fed into lcd_getpixinfo() which returns NULL then. */
u_char lcd_getnextpix(WINDOW win, u_char pix)
{
	while (pix < PIXEL_FORMAT_COUNT) {
		if (valid_pixels[win] & (1<<pix))
			break;
		pix++;
	}
	return pix;
}


/************************************************************************/
/* Color Conversions RGBA->COLOR32					*/
/************************************************************************/

static COLOR32 rgba2col_clut2(const wininfo_t *pwi, RGBA rgba)
{
	COLOR32 temp;

	temp = lcd_rgbalookup(rgba, pwi->cmap, 2);
	temp = temp << 31;
	temp = (COLOR32)((signed)temp >> 31);

	return temp;
}

static COLOR32 rgba2col_clut4(const wininfo_t *pwi, RGBA rgba)
{
	COLOR32 temp;

	temp = lcd_rgbalookup(rgba, pwi->cmap, 4);
	temp |= temp << 2;
	temp |= temp << 4;
	temp |= temp << 8;

	return temp | (temp << 16);
}

static COLOR32 rgba2col_clut16(const wininfo_t *pwi, RGBA rgba)
{
	COLOR32 temp;

	temp = lcd_rgbalookup(rgba, pwi->cmap, 16);
	temp |= temp << 4;
	temp |= temp << 8;

	return temp | (temp << 16);
}

static COLOR32 rgba2col_clut256(const wininfo_t *pwi, RGBA rgba)
{
	COLOR32 temp;

	temp = lcd_rgbalookup(rgba, pwi->cmap, 256);
	temp |= temp << 8;

	return temp | (temp << 16);
}

static COLOR32 rgba2col_rgba2321(const wininfo_t *pwi, RGBA rgba)
{
	COLOR32 temp;

	temp = (rgba & 0xC0000000) >> 1;
	temp |= (rgba & 0x00E00000) << 5;
	temp |= (rgba & 0x0000C000) << 10;
	temp |= (rgba & 0x00000080) << 24;
	temp |= temp >> 8;
	return temp | (temp >> 16);
}

static COLOR32 rgba2col_rgba5650(const wininfo_t *pwi, RGBA rgba)
{
	COLOR32 temp;

	temp = rgba & 0xF8000000;
	temp |= (rgba & 0x00FC0000) << 3;
	temp |= (rgba & 0x0000F800) << 5;
	return temp | (temp >> 16);
}

static COLOR32 rgba2col_rgba5551(const wininfo_t *pwi, RGBA rgba)
{
	COLOR32 temp;

	temp = (rgba & 0xF8000000) >> 1;
	temp |= (rgba & 0x00F80000) << 2;
	temp |= (rgba & 0x0000F800) << 5;
	temp |= (rgba & 0x00000080) << 24;
	return temp | (temp >> 16);
}

static COLOR32 rgba2col_rgba6660(const wininfo_t *pwi, RGBA rgba)
{
	COLOR32 temp;

	temp = (rgba & 0xFC000000) >> 14;
	temp |= (rgba & 0x00FC0000) >> 12;
	temp |= (rgba & 0x0000FC00) >> 10;
	return temp;
}

static COLOR32 rgba2col_rgba6651(const wininfo_t *pwi, RGBA rgba)
{
	COLOR32 temp;

	temp = (rgba & 0xFC000000) >> 15;
	temp |= (rgba & 0x00FC0000) >> 13;
	temp |= (rgba & 0x0000FC00) >> 11;
	temp |= (rgba & 0x00000080) << 10;
	return temp;
}

static COLOR32 rgba2col_rgba6661(const wininfo_t *pwi, RGBA rgba)
{
	COLOR32 temp;

	temp = (rgba & 0xFC000000) >> 14;
	temp |= (rgba & 0x00FC0000) >> 12;
	temp |= (rgba & 0x0000FC00) >> 10;
	temp |= (rgba & 0x00000080) << 11;
	return temp;
}

static COLOR32 rgba2col_rgba8880(const wininfo_t *pwi, RGBA rgba)
{
	return rgba >> 8;
}

static COLOR32 rgba2col_rgba8871(const wininfo_t *pwi, RGBA rgba)
{
	COLOR32 temp;

	temp = (rgba & 0x00000080) << 16;
	temp |= rgba >> 9;
	return temp;
}

static COLOR32 rgba2col_rgba8881(const wininfo_t *pwi, RGBA rgba)
{
	COLOR32 temp;

	temp = (rgba & 0x00000080) << 17;
	temp |= rgba >> 8;

	return temp;
}

static COLOR32 rgba2col_rgba8884(const wininfo_t *pwi, RGBA rgba)
{
	COLOR32 temp;

	temp = (rgba & 0x000000F0) << 20;
	temp |= rgba >> 8;

	return temp;
}

typedef COLOR32 (*rgba2col_func)(const wininfo_t *pwi, RGBA rgba);
COLOR32 lcd_rgba2col(const wininfo_t *pwi, RGBA rgba)
{
	static const rgba2col_func rgba2coltab[PIXEL_FORMAT_COUNT] = {
		rgba2col_clut2,
		rgba2col_clut4,
		rgba2col_clut16,
		rgba2col_clut256,
		rgba2col_rgba2321,
		rgba2col_rgba5650,
		rgba2col_rgba5551,
		rgba2col_rgba5551,	  /* RGBI-5551 same as RGBA-5551 */
		rgba2col_rgba6660,
		rgba2col_rgba6651,
		rgba2col_rgba6661,
		rgba2col_rgba8880,
		rgba2col_rgba8871,
		rgba2col_rgba8881,
		rgba2col_rgba8884
	};

	return rgba2coltab[pwi->pix](pwi, rgba);
}


/************************************************************************/
/* Color Conversions COLOR32->RGBA					*/
/************************************************************************/

static RGBA col2rgba_clut2(const wininfo_t *pwi, COLOR32 color)
{
	return pwi->cmap[color & 0x00000001];
}

static RGBA col2rgba_clut4(const wininfo_t *pwi, COLOR32 color)
{
	return pwi->cmap[color & 0x00000003];
}

static RGBA col2rgba_clut16(const wininfo_t *pwi, COLOR32 color)
{
	return pwi->cmap[color & 0x0000000F];
}

static RGBA col2rgba_clut256(const wininfo_t *pwi, COLOR32 color)
{
	return pwi->cmap[color & 0x000000FF];
}

static RGBA col2rgba_rgba2321(const wininfo_t *pwi, COLOR32 color)
{
	COLOR32 temp;
	RGBA rgba;

	rgba = (color & 0x60000000) << 1;   /* R[7:6] */
	rgba |= (color & 0x03000000) >> 10; /* B[7:6] */
	rgba |= rgba >> 2;		    /* R[5:4], B[5:4] */
	rgba |= rgba >> 4;		    /* R[3:0], B[3:0] */

	temp = color & 0x0000001C;	    /* G3[4:2] */
	temp |= (temp << 3) | (temp >> 3);  /* G[7:5], G[1:0] */
	rgba |= temp << 16;

	/* Use arithmetic shift right to duplicate the alpha bit 7 times */ 
	rgba |= (RGBA)((signed)color >> 7) >> 24; /* A[7:0] */

	return rgba;
}

static RGBA col2rgba_rgba5650(const wininfo_t *pwi, COLOR32 color)
{
	RGBA rgba;

	rgba = color & 0xF8000000;	   /* R[7:3] */
	rgba |= (color & 0x001F0000) >> 5; /* B[7:3] */
	rgba |= (rgba >> 5);		   /* R[2:0], B[2:0] */
	rgba &= 0xFF00FFFF;
	rgba |= (color & 0x07E00000) >> 3; /* G[7:2] */
	rgba &= 0xFFFFFF00;		   /* A[7:0] = 0 */
	rgba |= (color & 0x06000000) >> 9; /* G[1:0] */

	return rgba;
}

static RGBA col2rgba_rgba5551(const wininfo_t *pwi, COLOR32 color)
{
	RGBA rgba;

	rgba = (color & 0x7C000000) << 1; /* R[7:3] */
	rgba |= (color & 0x03E00000) >> 2; /* G[7:3] */
	rgba |= (color & 0x001F0000) >> 5; /* G[7:3] */
	rgba |= (rgba & 0xE0E0E000) >> 5;  /* R[2:0], G[2:0], B[2:0] */

	/* Use arithmetic shift right to duplicate the alpha bit 7 times */ 
	rgba |= (RGBA)((signed)color >> 7) >> 24; /* A[7:0] */

	return rgba;
}

static RGBA col2rgba_rgba6660(const wininfo_t *pwi, COLOR32 color)
{
	RGBA rgba;

	rgba = (color & 0x0003F000) << 14;  /* R[7:2] */
	rgba |= (color & 0x00000FC0) << 12; /* G[7:2] */
	rgba |= (color & 0x0000003F) << 10; /* B[7:2] */
	rgba |= (rgba & 0xC0C0C000) >> 6;   /* R[1:0], G[1:0], B[1:0] */

	return rgba;
}

static RGBA col2rgba_rgba6651(const wininfo_t *pwi, COLOR32 color)
{
	RGBA rgba;

	rgba = (color & 0x0001F800) << 15;  /* R[7..2] */
	rgba |= (color & 0x000007E0) << 13; /* G[7..2] */
	rgba |= (color & 0x0000001F) << 11; /* B[7..3] */
	rgba |= (rgba & 0xC0C00000) >> 6;   /* R[1:0], G[1:0] */
	rgba |= (rgba & 0x0000E000) >> 5;   /* B[2:0] */

	rgba |= (RGBA)((signed)(color << 14) >> 7) >> 24; /* A[7..0] */

	return rgba;
}

static RGBA col2rgba_rgba6661(const wininfo_t *pwi, COLOR32 color)
{
	RGBA rgba;

	rgba = (color & 0x0003F000) << 14; /* R[7:2] */
	rgba |= (color & 0x00000FC0) << 12; /* G[7:2] */
	rgba |= (color & 0x0000003F) << 10; /* B[7:2] */
	rgba |= (rgba & 0xC0C0C000) >> 6;   /* R[1:0], G[1:0], B[1:0] */

	rgba |= (RGBA)((signed)(color << 14) >> 7) >> 24; /* A[7..0] */

	return rgba;
}

static RGBA col2rgba_rgba8880(const wininfo_t *pwi, COLOR32 color)
{
	return color << 8;		  /* R[7:0], G[7:0], B[7:0], A=0 */
}

static RGBA col2rgba_rgba8871(const wininfo_t *pwi, COLOR32 color)
{
	RGBA rgba;

	rgba = color << 9;		  /* R[7:0], G[7:0], B[7:1] */
	rgba |= (color & 0x40) << 2;	  /* B[0] */

	rgba |= (RGBA)((signed)(color << 8) >> 7) >> 24; /* A[7:0] */

	return rgba;
}

static RGBA col2rgba_rgba8881(const wininfo_t *pwi, COLOR32 color)
{
	RGBA rgba;

	rgba = color << 8;		  /* R[7:0], G[7:0], B[7:0] */
	rgba |= (RGBA)((signed)(color << 7) >> 7) >> 24; /* A[7:0] */

	return rgba;
}

static RGBA col2rgba_rgba8884(const wininfo_t *pwi, COLOR32 color)
{
	RGBA rgba;

	rgba = color << 8;		  /* R[7:0], G[7:0], B[7:0] */

	color &= 0x0f000000;		  /* A[7:4], A[3:0] */
	rgba |= (color >> 24) | (color >> 20);

	return rgba;
}

typedef RGBA (*col2rgba_func)(const wininfo_t *pwi, COLOR32 color);
RGBA lcd_col2rgba(const wininfo_t *pwi, COLOR32 color)
{
	static const col2rgba_func col2rgbatab[PIXEL_FORMAT_COUNT] = {
		col2rgba_clut2,
		col2rgba_clut4,
		col2rgba_clut16,
		col2rgba_clut256,
		col2rgba_rgba2321,
		col2rgba_rgba5650,
		col2rgba_rgba5551,
		col2rgba_rgba5551,	  /* RGBI-5551 same as RGBA-5551 */
		col2rgba_rgba6660,
		col2rgba_rgba6651,
		col2rgba_rgba6661,
		col2rgba_rgba8880,
		col2rgba_rgba8871,
		col2rgba_rgba8881,
		col2rgba_rgba8884
	};

	return col2rgbatab[pwi->pix](pwi, color);
}


/************************************************************************/
/* Setting Video Infos							*/
/************************************************************************/

void lcd_hw_vidinfo(vidinfo_t *pvi)
{
	ulong hclk;
	unsigned int div;
	unsigned hline;
	unsigned vframe;
	unsigned ticks;

	hclk = get_HCLK();

	/* Do sanity check on all values */
	if ((pvi->clk == 0) && (pvi->fps == 0))
		pvi->fps = 60;		  /* Neither fps nor clk are given */
	if (pvi->hres > 2047)
		pvi->hres = 2047;
	if (pvi->vres > 2047)
		pvi->vres = 2047;
	if (pvi->hfp > 255)
		pvi->hfp = 255;
	if (pvi->hsw > 255)
		pvi->hsw = 255;
	else if (pvi->hsw < 1)
		pvi->hsw = 1;
	if (pvi->hbp > 255)
		pvi->hbp = 255;
	if (pvi->vfp > 255)
		pvi->vfp = 255;
	if (pvi->vsw > 255)
		pvi->vsw = 255;
	else if (pvi->vsw < 1)
		pvi->vsw = 1;
	if (pvi->vbp > 255)
		pvi->vbp = 255;

	/* Number of clocks for one horizontal line */
	hline = pvi->hres + pvi->hfp + pvi->hsw + pvi->hbp;

	/* Number of hlines for one vertical frame */
	vframe = pvi->vres + pvi->vfp + pvi->vsw + pvi->vbp;

	/* Number of clock ticks per frame */
	ticks = hline*vframe;
	
	if (pvi->clk) {
		/* Compute divisor from given pixel clock */
		div = (hclk + pvi->clk/2)/pvi->clk;
	} else {
		/* Ticks per second */
		unsigned ticks_per_second = ticks * pvi->fps;

		/* Compute divisor from given frame rate */
		div = (hclk + ticks_per_second/2)/ticks_per_second;
	}
	if (div < 2)
		div = 2;
	else if (div > 256)
		div = 256;
	pvi->clk = (hclk + div/2)/div;
	pvi->fps = (pvi->clk + ticks/2)/ticks;

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
		(pvi->clkpol) ? 0 : S3C_VIDCON1_IVCLK_RISE_EDGE
		| (pvi->hspol) ? S3C_VIDCON1_IHSYNC_INVERT : 0
		| (pvi->vspol) ? S3C_VIDCON1_IVSYNC_INVERT : 0
		| (pvi->denpol)? S3C_VIDCON1_IVDEN_INVERT : 0;

	/* TV settings are not required when using LCD */
	__REG(S3C_VIDCON2) = 0;

	/* Vertical timing is in VIDTCON0 */
	__REG(S3C_VIDTCON0) =
		S3C_VIDTCON0_VBPD(pvi->vbp)
		| S3C_VIDTCON0_VFPD(pvi->vfp)
		| S3C_VIDTCON0_VSPW(pvi->vsw);

	/* Horizontal timing is in VIDTCON1 */
	__REG(S3C_VIDTCON1) =
		S3C_VIDTCON1_HBPD(pvi->hbp)
		| S3C_VIDTCON1_HFPD(pvi->hfp)
		| S3C_VIDTCON1_HSPW(pvi->hsw);

	/* Display resolution is in VIDTCON2 */
	__REG(S3C_VIDTCON2) =
		S3C_VIDTCON2_LINEVAL(pvi->vres)
		| S3C_VIDTCON2_HOZVAL(pvi->hres);

	/* No video interrupts */
	__REG(S3C_VIDINTCON0) = 0;

	/* Dithering mode */
	__REG(S3C_DITHMODE) =
		S3C_DITHMODE_RDITHPOS_5BIT
		| S3C_DITHMODE_GDITHPOS_6BIT
		| S3C_DITHMODE_BDITHPOS_5BIT
		| pvi->dither ? S3C_DITHMODE_DITHERING_ENABLE : 0;
}

/************************************************************************/
/* Setting Window Infos							*/
/************************************************************************/

int lcd_hw_wininfo(const wininfo_t *pwi, const vidinfo_t *pvi)
{
	const struct LCD_WIN_REGS *winregs = &winregs_table[pwi->win];
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

	hres = lcd_align_hres(pwi->win, pwi->pix, pwi->hres);
	vres = pwi->vres;
	hpos = pwi->hpos;
	vpos = pwi->vpos;
	hoffs = pwi->hoffs;
	voffs = pwi->voffs;
	panel_hres = (XYPOS)pvi->hres;
	panel_vres = (XYPOS)pvi->vres;

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
			hoffs = lcd_align_hoffs(pwi, hoffs + (-hpos));
			hpos = 0;
		} else if (hpos + hres >= panel_hres)
			hres = panel_hres - hpos;
		if (vpos < 0) {
			vres += vpos;
			voffs -= vpos;
			vpos = 0;
		} else if (vpos + vres >= panel_vres)
			vres = panel_vres - hpos;
	}

	/* If nothing visible, disable window */
	if (!hres || !vres || !pwi->fbcount) {
		__REG(winregs->wincon) &= ~S3C_WINCONx_ENWIN_F_ENABLE;
		return 0;		  /* not enabled */
	}

	pagewidth = lcd_align_hres(pwi->win, pwi->pix, hres);
	pagewidth = (pagewidth << pwi->pi->bpp_shift) >> 3;
	if (pagewidth > 16*4)
		burstlen = S3C_WINCONx_BURSTLEN_16WORD;
	else if (pagewidth > 8*4)
		burstlen = S3C_WINCONx_BURSTLEN_8WORD;
	else
		burstlen = S3C_WINCONx_BURSTLEN_4WORD;
	if (pwi->pix == 14) {
		bld_pix = 1;
		alpha_sel = 1;
	} else {
		bld_pix = pwi->ext.bld_pix;
		alpha_sel = pwi->ext.alpha_sel;
	}
	bppmode_f = pwi->pix;
	if (bppmode_f > 13)
		bppmode_f = 13;

	/* General window configuration */
	__REG(winregs->wincon) = 
		S3C_WINCONx_WIDE_NARROW(0)
		| S3C_WINCONx_ENLOCAL_DMA
		| pwi->fbshow ? S3C_WINCONx_BUFSEL_1 : S3C_WINCONx_BUFSEL_0
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

	if (winregs->vidosdalpha)
		__REG(winregs->vidosdalpha) = pwi->ext.alpha;

	/* Set buffer 0 */
	addr = pwi->fbuf[0] + voffs*pwi->linelen;
	addr += (hoffs << pwi->pi->bpp_shift) >> 8;
	__REG(winregs->vidadd[0].start) = addr;
	__REG(winregs->vidadd[0].end) = (addr + vres*pwi->linelen) & 0x00FFFFFF;

	/* Set buffer 1 (if applicable) */
	if (pwi->fbcount > 1) {
		addr = pwi->fbuf[0] + voffs*pwi->linelen;
		addr += (hoffs << pwi->pi->bpp_shift) >> 8;
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

	__REG(winregs->winmap) =
		S3C_WINxMAP_MAPCOLEN_F_DISABLE
		| S3C_WINxMAP_MAPCOLOR(0xFFFFFF);

	if (winregs->keycon)
		__REG(winregs->keycon) =
			(pwi->ext.ckblend ? S3C_WxKEYCON0_KEYBLEN_ENABLE : 0)
			| (pwi->ext.ckenable ? S3C_WxKEYCON0_KEYEN_F_ENABLE : 0)
			| (pwi->ext.ckdir ? S3C_WxKEYCON0_DIRCON_MATCH_BG_IMAGE : 0)
			| pwi->ext.ckmask;

	if (winregs->keyval)
		__REG(winregs->keyval) = pwi->ext.ckvalue;

	return 1;			  /* enabled */
}

u_char lcd_getfbmaxcount(WINDOW win)
{
	return (win <= 1) ? 2 : 1;
}

/* Return maximum horizontal framebuffer resolution for this window */
u_short lcd_getfbmaxhres(WINDOW win, u_char pix)
{
	/* Maximum linelen is 13 bits, that's at most 8191 bytes; however to
	   be word aligned even for 1bpp, we can at most take 8188 */
	return (u_short)((8188 << 3) >> pixel_info[pix].bpp_shift);
}

/* Return maximum vertical framebuffer resolution for this window */
u_short lcd_getfbmaxvres(WINDOW win, u_char pix)
{
	/* No limit on vertical size */
	return 65535;
}

/* Align horizontal offset to current word boundary (round down) */
XYPOS lcd_align_hoffs(const wininfo_t *pwi, XYPOS hoffs)
{
	unsigned shift = 5 - pwi->pi->bpp_shift;

	hoffs >>= shift;
	hoffs <<= shift;

	return hoffs;
}

/* Align horizontal buffer resolution to next word boundary (round up) */
u_short lcd_align_hres(WINDOW win, u_char pix, u_short hres)
{
	unsigned shift = 5 - pixel_info[pix].bpp_shift;
	XYPOS mask = (1 << shift) - 1;

	hres = (hres + mask) & ~mask;
	return hres;
}

/* In addition to the standard video signals, we have five more signals:
   GPF15: PWM output (backlight intensity)
   GPK3:  Buffer Enable (active low): enable display driver chips
   GPK2:  Display Enable (active low): Set signal for display
   GPK1:  VCFL (active high): Activate backlight voltage
   GPK0:  VLCD (active high): Activate LCD voltage */
void lcd_hw_init(void)
{
	/* Call board specific GPIO configuration */
//###	lcd_s3c64xx_board_init(void);

	__REG(S3C_HOSTIFB_MIFPCON) = 0;	  /* 0: normal-path (no by-pass) */
	__REG(HCLK_GATE) |= (1<<3);	  /* Enable clock to LCD */
	__REG(SPCON) = (__REG(SPCON) & ~0x3) | 0x1; /* Select RGB I/F */

	/* Setup GPI: LCD_VD[15:0], no pull-up/down */
	__REG(GPICON) = 0xAAAAAAAA;
	__REG(GPIPUD) = 0x00000000;

	/* Setup GPJ: LCD_VD[23:16], HSYNC, VSYNC, DEN, CLK, no pull-up/down */
	__REG(GPJCON) = 0xAAAAAAAA;
	__REG(GPJPUD) = 0x00000000;

	/* Setup GPF15 to output 0 (backlight intensity 0) */
	__REG(GPFDAT) &= ~(0x1<<15);
	__REG(GPFCON) = (__REG(GPFCON) & ~(0x3<<30)) | (0x1<<30);
	__REG(GPFPUD) &= ~(0x3<<30);

	/* Setup GPK[3] to output 1 (Buffer enable), GPK[2] to output 1
	   (Display enable), GPK[1] to output 0 (VCFL), GPK[0] to output 0
	   (VLCD), no pull-up/down */
	__REG(GPKDAT) = (__REG(GPKDAT) & ~(0xf<<0)) | (0xc<<0);
	__REG(GPKCON0) = (__REG(GPKCON0) & (0xFFF<<0)) | (0x111<0);
	__REG(GPKPUD) &= ~(0x3F<<0);
}

int lcd_hw_enable(void)
{
	/* Activate VLCD */
	__REG(GPKDAT) |= (1<<0);

	/* Activate Buffer Enable */
	__REG(GPKDAT) &= ~(1<<3);

	/* Activate LCD controller */
	__REG(S3C_VIDCON0) |= S3C_VIDCON0_ENVID_F_ENABLE;

	/* Activate Display Enable */
	__REG(GPKDAT) &= ~(1<<2);

	/* Activate VCFL */
	__REG(GPKDAT) |= (1<<1);

	/* Activate VEEK (backlight intensity full) */
	__REG(GPFDAT) |= (0x1<<15);

	return 0;
}

/* Deactivate display in reverse order */
void lcd_hw_disable(void)
{
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
}
