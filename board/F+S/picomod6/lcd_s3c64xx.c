#include <config.h>
#include <common.h>
//#include <version.h>
//#include <stdarg.h>
//#include <linux/types.h>
//#include <devices.h>
#include <lcd_s3c64xx.h>		  /* Own interface */
#include <lcd.h>			  /* Common lcd interface */
#include <cmd_lcd.h>			  /* parse_rgb(), struct kwinfo */
#include <s3c-regs-lcd.h>		  /* S3C64XX LCD registers */

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
	} vidadd[MAX_BUFFERS_PER_WIN];
	LCDREG vidsize;			  /* Video buffer size */
	LCDREG keycon;			  /* Color key control */
	LCDREG keyval;			  /* Color key value */
	LCDREG winmap;			  /* Default color mapping */
	u_int  clut;			  /* Color look-up table address */
};

static const struct LCD_WIN_REGS winregs[MAX_WINDOWS] = {
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
	{ 0,  1, 0,   2, "2-entry color look-up table"},   /* 0 */
	{ 1,  2, 1,   4, "4-entry color look-up table"},   /* 1 */
	{ 2,  4, 2,  16, "16-entry color look-up table"},  /* 2 */
	{ 3,  8, 3, 256, "256-entry color look-up table"}, /* 3 */
	{ 4,  8, 3,   0, "RGBA-2321"},			   /* 4 */
	{ 5, 16, 4,   0, "RGBA-5650 (default)"},	   /* 5 */
	{ 6, 16, 4,   0, "RGBA-5551"},			   /* 6 */
	{ 7, 16, 4,   0, "RGBI-5551 (I=intensity)"},	   /* 7 */
	{ 8, 18, 5,   0, "RGBA-6660"},			   /* 8 */
	{ 9, 18, 5,   0, "RGBA-6651"},			   /* 9 */
	{10, 19, 5,   0, "RGBA-6661"},			   /* 10 */
	{11, 24, 5,   0, "RGBA-8880"},			   /* 11 */
	{12, 24, 5,   0, "RGBA-8871"},			   /* 12 */
	{13, 25, 5,   0, "RGBA-8881"},			   /* 13 */
	{13, 28, 5,   0, "RGBA-8884"},			   /* 14 */
};

/* Valid pixel formats for each window */
u_int valid_pixels[MAX_WINDOWS] = {
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

const struct kwinfo winextkeywords[] = {
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
		u_char pix = 1;
		u_char sel = 0;
		u_int alpha0_rgba;
		u_int alpha1_rgba;
		
		/* Arguments 1+2: alpha0 and alpha1 */
		if ((parse_rgb(argv[2], &alpha0_rgba) == RT_NONE)
		    || (parse_rgb(argv[3], &alpha1_rgba) == RT_NONE))
			return 1;

		/* Argument 3: pix setting */
		if (argc > 4)
			pix = (simple_strtoul(argv[4], NULL, 0) != 0);

		/* Argument 4: sel setting */
		if (argc > 5)
			sel = (simple_strtoul(argv[5], NULL, 0) != 0);

		ext->alpha0 = alpha0_rgba;
		ext->alpha1 = alpha1_rgba;
		ext->pix = pix;
		ext->sel = sel;
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
	case WI_ALPHA:
		printf("Alpha:\t\talpha0=#%06x, alpha1=#%06x, pix=%d, sel=%d\n",
		       ext->alpha0, ext->alpha1, ext->pix, ext->sel);
		if (si != WI_INFO)
			break;
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

void lcd_hw_vidinfo(vidinfo_t *pvi_new, vidinfo_t *pvi_old)
{
	//#### TODO ####
}

/************************************************************************/
/* Setting Window Infos							*/
/************************************************************************/

void lcd_hw_wininfo(wininfo_t *pwi_new, WINDOW win, wininfo_t *pwi_old)
{
	if (pwi_new->pix != pwi_old->pix)
		pwi_new->pi = &pixel_info[pwi_new->pix];

	//#### TODO ####
}

u_char lcd_getfbmaxcount(WINDOW win)
{
	return (win <= 1) ? 2 : 1;
}
