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


/************************************************************************/
/* ENUMERATIONS								*/
/************************************************************************/

/* Additional commands recognized as extension of command window */
enum WIN_INDEX_EXT {
	WI_EXT_UNKNOWN,
	WI_EXT_ALPHA,
	WI_EXT_COLKEY,
};


/************************************************************************/
/* TYPES AND STRUCTURES							*/
/************************************************************************/

/* This is the extension part, available as wininfo_t.ext */
typedef struct wininfo_ext {
	/* Alpha information */
	u_int  alpha;			  /* Alpha R/G/B AEN=0/1 */
	u_char bld_pix;			  /* Blend 0: per plane, 1: per pixel */
	u_char alpha_sel;		  /* Alpha selection */

	/* Color key information */
	u_char ckena;			  /* 0: disabled, 1: enabled */
	u_char ckdir;			  /* compare to 0: fg, 1: bg */
	u_char ckblend;			  /* 0: no blending, 1: blend */
	u_int  ckvalue;			  /* Color key value */
	u_int  ckmask;			  /* Mask which value bits matter */
} wininfo_ext_t;

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


/************************************************************************/
/* LOCAL VARIABLES							*/
/************************************************************************/

/* One wininfo structure per window */
static wininfo_t s3c64xx_wininfo[MAX_WINDOWS];

/* Addresses of the possible image buffers of all windows */
u_long s3c64xx_fbuf[2+2+1+1+1];

/* One wininfo_ext extension structure per window */
wininfo_ext_t s3c64xx_wininfo_ext[MAX_WINDOWS];

/* Valid pixel formats for each window */
u_int valid_pixels[MAX_WINDOWS] = {
	0x09AF,			/* Win 0: no alpha formats */
	0x7FFF,			/* Win 1: no restrictions  */
	0x7FF7,			/* Win 2: no CMAP-8 */
	0x7FE7,			/* Win 3: no CMAP-8, no RGBA-2321 */
	0x7FE3,			/* Win 4: no CMAP-4, no CMAP-8, no RGBA-2321 */
};

/* Keywords for additional window sub-commands */
const kwinfo_t winext_kw[] = {
	{2, 4, WI_EXT_ALPHA,  "alpha"},
	{1, 5, WI_EXT_COLKEY, "colkey"},
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
/* PROTOTYPES OF LOCAL FUNCTIONS					*/
/************************************************************************/


/************************************************************************/
/* EXPORTED FUNCTIONS CALLED VIA vidinfo_t				*/
/************************************************************************/

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


/* Parse additional window sub-commands; on error return 0 */
static u_short s3c64xx_winext_parse(int argc, char *s)
{
	return parse_sc(argc, s, WI_EXT_UNKNOWN, winext_kw,
			ARRAYSIZE(winext_kw));
}

/* Handle additional window sub-commands; on error, return 1 and keep *pwi
   unchanged */
static int s3c64xx_winext_exec(wininfo_t *pwi, int argc, char *argv[],
			       u_short si)
{
	wininfo_ext_t *ext = (wininfo_ext_t *)pwi->ext;

	/* OK, parse arguments and set data */
	switch (si) {
	case WI_EXT_ALPHA: {
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

	case WI_EXT_COLKEY: {
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
			ext->ckvalue = ckvalue_rgba >> 8;
			ext->ckmask = ckmask_rgba >> 8;
		}

		/* Argument 1: enable */
		ext->ckena = (simple_strtoul(argv[2], NULL, 0) != 0);

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

	/* Update hardware with new settings */
	pwi->pvi->set_wininfo(pwi);

	return 0;
}

/* Show info for all extended info pointed to by wininfo_t.ext */
static void s3c64xx_winext_show(const wininfo_t *pwi)
{
	wininfo_ext_t *ext = (wininfo_ext_t *)pwi->ext;
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
	printf("Color Key:\tenable=%d, dir=%d, blend=%d, colkey=#%06x,"
	       " colmask=#%06x\n", ext->ckena, ext->ckdir, ext->ckblend,
	       ext->ckvalue, ext->ckmask);
}


/* Extended help message for lcdwin */
static void s3c64xx_winext_help(void)
{
	puts("window alpha [alpha0 [alpha1 [pix [sel]]]]\n"
	     "    - Set per-window alpha values\n"
	     "window colkey [enable [dir [blend [value [mask]]]]]\n"
	     "    - Set per-window color key values\n");
}


/************************************************************************/
/* Color Conversions RGBA->COLOR32					*/
/************************************************************************/

static COLOR32 r2c_cmap2(const wininfo_t *pwi, RGBA rgba)
{
	COLOR32 temp;

	temp = lcd_rgbalookup(rgba, pwi->cmap, 2);
	temp = temp << 31;
	temp = (COLOR32)((signed)temp >> 31);

	return temp;
}

static COLOR32 r2c_cmap4(const wininfo_t *pwi, RGBA rgba)
{
	COLOR32 temp;

	temp = lcd_rgbalookup(rgba, pwi->cmap, 4);
	temp |= temp << 2;
	temp |= temp << 4;
	temp |= temp << 8;

	return temp | (temp << 16);
}

static COLOR32 r2c_cmap16(const wininfo_t *pwi, RGBA rgba)
{
	COLOR32 temp;

	temp = lcd_rgbalookup(rgba, pwi->cmap, 16);
	temp |= temp << 4;
	temp |= temp << 8;

	return temp | (temp << 16);
}

static COLOR32 r2c_cmap256(const wininfo_t *pwi, RGBA rgba)
{
	COLOR32 temp;

	temp = lcd_rgbalookup(rgba, pwi->cmap, 256);
	temp |= temp << 8;

	return temp | (temp << 16);
}

static COLOR32 r2c_rgba2321(const wininfo_t *pwi, RGBA rgba)
{
	COLOR32 temp;

	temp = rgba & 0x00000080;	   /* C[7] = A[7] */
	temp |= (rgba & 0xC0000000) >> 25; /* C[6:5] = R[7:6] */
	temp |= (rgba & 0x00E00000) >> 19; /* C[4:2] = G[7:5] */
	temp |= (rgba & 0x0000C000) >> 14; /* C[1:0] = B[7:6] */
	temp |= temp << 8;		   /* C[15:8] */
	return temp | (temp << 16);	   /* C[31:16] */
}

static COLOR32 r2c_rgba5650(const wininfo_t *pwi, RGBA rgba)
{
	COLOR32 temp;

	temp = rgba & 0xF8000000;
	temp |= (rgba & 0x00FC0000) << 3;
	temp |= (rgba & 0x0000F800) << 5;
	return temp | (temp >> 16);
}

static COLOR32 r2c_rgba5551(const wininfo_t *pwi, RGBA rgba)
{
	COLOR32 temp;

	temp = (rgba & 0xF8000000) >> 1;
	temp |= (rgba & 0x00F80000) << 2;
	temp |= (rgba & 0x0000F800) << 5;
	temp |= (rgba & 0x00000080) << 24;
	return temp | (temp >> 16);
}

static COLOR32 r2c_rgba6660(const wininfo_t *pwi, RGBA rgba)
{
	COLOR32 temp;

	temp = (rgba & 0xFC000000) >> 14;
	temp |= (rgba & 0x00FC0000) >> 12;
	temp |= (rgba & 0x0000FC00) >> 10;
	return temp;
}

static COLOR32 r2c_rgba6651(const wininfo_t *pwi, RGBA rgba)
{
	COLOR32 temp;

	temp = (rgba & 0xFC000000) >> 15;
	temp |= (rgba & 0x00FC0000) >> 13;
	temp |= (rgba & 0x0000FC00) >> 11;
	temp |= (rgba & 0x00000080) << 10;
	return temp;
}

static COLOR32 r2c_rgba6661(const wininfo_t *pwi, RGBA rgba)
{
	COLOR32 temp;

	temp = (rgba & 0xFC000000) >> 14;
	temp |= (rgba & 0x00FC0000) >> 12;
	temp |= (rgba & 0x0000FC00) >> 10;
	temp |= (rgba & 0x00000080) << 11;
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


const pixinfo_t pixel_info[PIXEL_FORMAT_COUNT] = {
	{ 1, 0,  2, r2c_cmap2,    c2r_cmap2,    "2-entry color map"},   /* 0*/
	{ 2, 1,  4, r2c_cmap4,    c2r_cmap4,    "4-entry color map"},	/* 1*/
	{ 4, 2, 16, r2c_cmap16,   c2r_cmap16,   "16-entry color map"},	/* 2*/
	{ 8, 3,256, r2c_cmap256,  c2r_cmap256,  "256-entry color map"}, /* 3*/
	{ 8, 3,  0, r2c_rgba2321, c2r_rgba2321, "RGBA-2321"},           /* 4*/
	{16, 4,  0, r2c_rgba5650, c2r_rgba5650, "RGBA-5650 (default)"},	/* 5*/
	{16, 4,  0, r2c_rgba5551, c2r_rgba5551, "RGBA-5551"},           /* 6*/
	{16, 4,  0, r2c_rgba5551, c2r_rgba5551, "RGBI-5551 (I=intensity)"},/*7*/
	{18, 5,  0, r2c_rgba6660, c2r_rgba6660, "RGBA-6660"},           /* 8*/
	{18, 5,  0, r2c_rgba6651, c2r_rgba6651, "RGBA-6651"},           /* 9*/
	{19, 5,  0, r2c_rgba6661, c2r_rgba6661, "RGBA-6661"},           /*10*/
	{24, 5,  0, r2c_rgba8880, c2r_rgba8880, "RGBA-8880"},           /*11*/
	{24, 5,  0, r2c_rgba8871, c2r_rgba8871, "RGBA-8871"},           /*12*/
	{25, 5,  0, r2c_rgba8881, c2r_rgba8881, "RGBA-8881"},           /*13*/
	{28, 5,  0, r2c_rgba8884, c2r_rgba8884, "RGBA-8884"}            /*14*/
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
		| (pvi->lcd.dither ? S3C_DITHMODE_DITHERING_ENABLE : 0);

//###	printf("###VIDCON0=0x%lx, VIDCON1=0x%lx, VIDTCON0=0x%lx, VIDTCON1=0x%lx, VIDTCON2=0x%lx, DITHMODE=0x%lx, hclk=%lu\n", __REG(S3C_VIDCON0), __REG(S3C_VIDCON1), __REG(S3C_VIDTCON0), __REG(S3C_VIDTCON1), __REG(S3C_VIDTCON2), __REG(S3C_DITHMODE), hclk);
}

/************************************************************************/
/* Setting Window Infos							*/
/************************************************************************/

static void s3c64xx_set_wininfo(const wininfo_t *pwi)
{
	const struct LCD_WIN_REGS *winregs = &winregs_table[pwi->win];
	const vidinfo_t *pvi = pwi->pvi;
	const wininfo_ext_t *ext = (const wininfo_ext_t *)pwi->ext;
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
		bld_pix = 1;
		alpha_sel = 1;
	} else {
		bld_pix = ext->bld_pix;
		alpha_sel = ext->alpha_sel;
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

	if (winregs->vidosdalpha)
		__REG(winregs->vidosdalpha) = ext->alpha;

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

	__REG(winregs->winmap) =
		S3C_WINxMAP_MAPCOLEN_F_DISABLE
		| S3C_WINxMAP_MAPCOLOR(0xFFFFFF);

	if (winregs->keycon)
		__REG(winregs->keycon) =
			(ext->ckblend ? S3C_WxKEYCON0_KEYBLEN_ENABLE : 0)
			| (ext->ckena ? S3C_WxKEYCON0_KEYEN_F_ENABLE : 0)
			| (ext->ckdir ? S3C_WxKEYCON0_DIRCON_MATCH_BG_IMAGE : 0)
			| ext->ckmask;

	if (winregs->keyval)
		__REG(winregs->keyval) = ext->ckvalue;

//###	printf("###Window %u enabled: wincon=0x%lx, vidosdtl=0x%lx, vidosdbr=0x%lx, vidosdsize=0x%lx\n", pwi->win, __REG(winregs->wincon), __REG(winregs->vidosdtl), __REG(winregs->vidosdbr), __REG(winregs->vidosdsize));
//###	printf("### vidosdalpha=0x%lx, vidadd[0].start=0x%lx, vidadd[0].end=0x%lx, vidsize=0x%lx\n", __REG(winregs->vidosdalpha), __REG(winregs->vidadd[0].start), __REG(winregs->vidadd[0].end), __REG(winregs->vidsize));
//###	printf("### WPALCON=0x%lx, winmap=0x%lx, keycon=0x%lx, keyval=0x%lx\n", __REG(S3C_WPALCON), __REG(winregs->winmap), __REG(winregs->keycon), __REG(winregs->keyval));
	return;				  /* enabled */
}

static int s3c64xx_enable(void)
{
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


//###	printf("###VIDCON0=0x%lx, VIDCON1=0x%lx, VIDTCON0=0x%lx, VIDTCON1=0x%lx, VIDTCON2=0x%lx, DITHMODE=0x%lx\n", __REG(S3C_VIDCON0), __REG(S3C_VIDCON1), __REG(S3C_VIDTCON0), __REG(S3C_VIDTCON1), __REG(S3C_VIDTCON2), __REG(S3C_DITHMODE));
	return 0;
}

/* Deactivate display in reverse order */
static void s3c64xx_disable(void)
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




/* One vidinfo structure (=support for one display) */
static const vidinfo_t s3c64xx_vidinfo = {
	driver_name: DRIVER_NAME,
	wincount: MAX_WINDOWS,
	pixcount: PIXEL_FORMAT_COUNT,
	pwi: s3c64xx_wininfo,
	winext_parse: s3c64xx_winext_parse,
	winext_exec: s3c64xx_winext_exec,
	winext_show: s3c64xx_winext_show,
	winext_help: s3c64xx_winext_help,
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
	wininfo_ext_t *pwinext;
	wininfo_t *pwi;
	WINDOW win;

	/* Call board specific GPIO configuration */
//###	s3c64xx_lcd_board_init(void);

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
	__REG(GPKCON0) = (__REG(GPKCON0) & ~(0xFFF<<0)) | (0x111<<0);
	__REG(GPKPUD) &= ~(0x3F<<0);

	/* Copy our vidinfo to the global array pointer */
	*pvi = s3c64xx_vidinfo;

	/* Initialize hardware-specific part of the windows information:
	   Pointer to framebuffers, default pixel format, maximum buffer
	   count, wininfo extension. */
	pfbuf = s3c64xx_fbuf;
	pwinext = s3c64xx_wininfo_ext;
	for (win=0, pwi=s3c64xx_wininfo; win < MAX_WINDOWS; win++, pwi++) {
		u_int fbmaxcount = (win < 2) ? 2 : 1;
		pwi->defpix = DEFAULT_PIXEL_FORMAT;
		pwi->pfbuf = pfbuf;
		pwi->fbmaxcount = (u_char)fbmaxcount;
		pfbuf += fbmaxcount;
		pwinext->alpha = 0x00FFF000;
		pwi->ext = pwinext++;
	}
}

