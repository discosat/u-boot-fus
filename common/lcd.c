/*
 * Hardware independent LCD controller part
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
#include <video_font.h>			  /* Get font data, width and height */
#include <stdarg.h>
#include <linux/types.h>
#include <devices.h>
#if defined(CONFIG_POST)
#include <post.h>
#endif
#include <watchdog.h>


/************************************************************************/
/* DEFINITIONS								*/
/************************************************************************/

#ifndef CONFIG_FBPOOL_SIZE
#define CONFIG_FBPOOL_SIZE 0x00100000	  /* 1MB, enough for 800x600@16bpp */
#endif

/* ###### Noch einzubauen:
   CONFIG_LCD_LOGO
   CONFIG_LCD_INFO_BELOW_LOGO
   ######
*/


/************************************************************************/
/* ** LOGO DATA								*/
/************************************************************************/
#ifdef CONFIG_LCD_LOGO
# include <bmp_logo.h>		/* Get logo data, width and height	*/
# if (CONSOLE_COLOR_WHITE >= BMP_LOGO_OFFSET) && (LCD_BPP != LCD_COLOR16)
#  error Default Color Map overlaps with Logo Color Map
# endif
#endif

DECLARE_GLOBAL_DATA_PTR;

wininfo_t wininfo[MAX_WINDOWS];		  /* Current window information */
vidinfo_t vidinfo;			  /* Current panel information */
coninfo_t coninfo;			  /* Console information */

vidinfo_t panellist[] = {
	{				  /* Panel 0 */
		name: "(no display)",	  /* Name, type and dimensions */
		type:		VI_TYPE_TFT,
		hdim:		0,
		vdim:		0,
		hres:		0,	  /* Resolution */
		vres:		0,
		hfp:		24,	  /* Horizontal timing */
		hsw:		96,
		hbp:		40,
		vfp:		10,	  /* Vertical timing */
		vsw:		2,
		vbp:		33,
		hspol:		CFG_LOW,  /* Signal polarity */
		vspol:		CFG_LOW,
		denpol:		CFG_HIGH,
		clkpol:		CFG_HIGH,
		fps:		60,	  /* Frame rate or pixel clock */
		clk:		0,
		pwmenable:	0,	  /* PWM/Backlight setting */
		pwmvalue:	0,
		pwmfreq:	0,
		strength:	3,	  /* Extra settings */
		dither:		0,
	},
	{				  /* Panel 1 */
		name: "LG.Philips LB064V02", /* Name, type and dimensions */
		type:		VI_TYPE_TFT,
		hdim:		132,
		vdim:		98,
		hres:		640,	  /* Resolution */
		vres:		480,
		hfp:		24,	  /* Horizontal timing */
		hsw:		96,
		hbp:		40,
		vfp:		10,	  /* Vertical timing */
		vsw:		2,
		vbp:		33,
		hspol:		CFG_LOW,  /* Signal polarity */
		vspol:		CFG_LOW,
		denpol:		CFG_HIGH,
		clkpol:		CFG_HIGH,
		fps:		60,	  /* Frame rate or pixel clock */
		clk:		0,
		pwmenable:	0,	  /* PWM/Backlight setting */
		pwmvalue:	0,
		pwmfreq:	0,
		strength:	3,	  /* Extra settings */
		dither:		0,
	},
};

/* Size and base address of the framebuffer pool */
static fbpoolinfo_t fbpool = {
	base:	CFG_UBOOT_BASE - CONFIG_FBPOOL_SIZE,
	size:	CONFIG_FBPOOL_SIZE,
	used:	0
};
   
static int lcd_init (void *lcdbase);

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

static void console_putc(char c);

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
	u_int shift = pwi->pi->bpp_shift;
	u_int bpp = 1 << shift;
	u_int x_shift = 5-shift;
	u_int x_mask = (1 << x_shift) - 1;
	u_long fbuf;
	COLOR32 mask;
	COLOR32 *p;

	/* Compute framebuffer address of the beginning of the row */
	fbuf = pwi->linelen * y + pwi->fbuf[pwi->fbdraw];

	/* We build the basic mask by using arithmetic right shift */ 
	mask = (COLOR32)((signed)0x80000000 >> (bpp-1));

	/* Shift the mask to the appropriate pixel */
	mask >>= (x & x_mask)*bpp;

	/* Remove old pixel and fill in new pixel */
	p = (COLOR32 *)fbuf + (x >> x_shift);
	*p = (*p & ~mask) | (color & mask);
}


/* Draw filled rectangle; given region is definitely visible */
static void lcd_ll_rect(const wininfo_t *pwi, XYPOS x1, XYPOS y1,
			XYPOS x2, XYPOS y2, COLOR32 color)
{
	u_int shift = pwi->pi->bpp_shift;
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
	fbuf = linelen * y1 + pwi->fbuf[pwi->fbdraw];

	count = y2 - y1;
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
	printf("####lcd_ll_char() not yet implemented####\n");
}


/************************************************************************/
/* ** Console support							*/
/************************************************************************/

#define TABWIDTH (8 * VIDEO_FONT_WIDTH)	  /* 8 chars for tab */
static void console_putc(char c)
{
	int x = coninfo.x;
	int y = coninfo.y;
	COLOR32 fg = coninfo.fg;
	COLOR32 bg = coninfo.bg;
	wininfo_t *pwi = &wininfo[coninfo.win];
	int fbhres = pwi->fbhres;
	int fbvres = pwi->fbvres;

	switch (c) {
	case '\t':			  /* Tab */
		x = ((x / TABWIDTH) + 1) * TABWIDTH;
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
		if (y + VIDEO_FONT_HEIGHT <= fbvres)
			y += VIDEO_FONT_HEIGHT;
		else {
			u_long fbuf = pwi->fbuf[pwi->fbdraw];
			u_long linelen = pwi->linelen;
			
			/* Scroll everything up */
			memcpy((void *)fbuf,
			       (void *)fbuf + linelen * VIDEO_FONT_HEIGHT,
			       (fbvres - VIDEO_FONT_HEIGHT) * linelen);

			/* Clear bottom line to end of screen with console
			   background color */
			memset32((unsigned *)fbuf + y*linelen, bg,
				 (fbvres - y) * linelen);
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

void lcd_putc(const char c)
{
	if (!lcd_is_enabled)
		serial_putc(c);
	else
		console_putc(c);
}

/*----------------------------------------------------------------------*/

void lcd_puts(const char *s)
{
	if (!lcd_is_enabled)
		serial_puts(s);
	else {
		for (;;)
		{
			char c = *s++;

			if (!c)
				break;
			console_putc(c);
		}
	}
}

/*----------------------------------------------------------------------*/

void lcd_printf(const char *fmt, ...)
{
	va_list args;
	char buf[CFG_PBSIZE];

	va_start(args, fmt);
	vsprintf(buf, fmt, args);
	va_end(args);

	lcd_puts(buf);
}




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

	printf ("[LCD] Test Pattern: %d x %d [%d x %d]\n",
		h_max, v_max, h_step, v_step);

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


/************************************************************************/
/* ** GENERIC Initialization Routines					*/
/************************************************************************/

int drv_lcd_init(void)
{
	device_t lcddev;
	int rc;
	WINDOW win;
	wininfo_t *pwi;

	vidinfo = panellist[0];

	for (win = 0, pwi=wininfo; win < MAX_WINDOWS; win++, pwi++) {
		u_int buf;

		memset(pwi, 0, sizeof(wininfo_t));

		/* Initialize some entries */
		pwi->fbmaxcount = lcd_getfbmaxcount(win);
		pwi->pix = DEFAULT_PIXEL_FORMAT;
		pwi->fg = lcd_rgba2col(pwi, 0xFFFFFFFF);   /* Opaque white */
		pwi->bg = lcd_rgba2col(pwi, 0x000000FF);   /* Opaque black */

		/* Set all valid buffer pointers to the beginning of the
		   framebuffer pool, all others to NULL */
		for (buf=0; buf < MAX_BUFFERS_PER_WIN; buf++)
			pwi->fbuf[buf] =
				(buf < pwi->fbmaxcount) ? fbpool.base : 0;
	}

	lcd_ctrl_init();		  /* Initialize LCD controller */
	printf("####b\n");
//###	lcd_is_enabled = 1;
	printf("####c\n");
//###	lcd_enable ();
	printf("####d\n");

	printf("####e\n");

	/* Initialize the console */
	coninfo.win = 0;
	coninfo.x = 0;
	coninfo.y = 0;
	coninfo.fg = lcd_rgba2col(&wininfo[coninfo.win], 0xFFFFFFFF);
	coninfo.bg = lcd_rgba2col(&wininfo[coninfo.win], 0x000000FF);

	/* Device initialization */
	memset (&lcddev, 0, sizeof (lcddev));

	strcpy (lcddev.name, "lcd");
	lcddev.ext   = 0;		  /* No extensions */
	lcddev.flags = DEV_FLAGS_OUTPUT;  /* Output only */
	lcddev.putc  = lcd_putc;	  /* 'putc' function */
	lcddev.puts  = lcd_puts;	  /* 'puts' function */

	rc = device_register (&lcddev);
	if (rc == 0)
		rc = 1;

	return rc;
}


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



/************************************************************************/
/* Framebuffer Pool							*/
/************************************************************************/

/* Relocate all windows to newaddr, starting at window win */
void lcd_relocbuffers(u_long newaddr, WINDOW win)
{
	/* Relocate all windows, i.e. set all image buffer addresses
	   to the new address and update hardware */
	for (; win < MAX_WINDOWS; win++) {
		u_int buf;
		wininfo_t wi;

		lcd_getwininfo(&wi, win);

		for (buf=0; buf<wi.fbmaxcount; buf++) {
			printf("Window %u: Relocated buffer %u from 0x%08lx to"
			       " 0x%08lx\n", win, buf, wi.fbuf[buf], newaddr);
			wi.fbuf[buf] = newaddr;
			if (buf < wi.fbcount)
				newaddr += wi.fbsize;
		}

		/* Update controller hardware with new wininfo */
		lcd_setwininfo(&wi, win);
	}
}


/* Resize framebuffer for current window, relocate all subsequent windows */
int lcd_setfbuf(wininfo_t *pwi, WINDOW win, u_short fbhres, u_short fbvres,
		u_char pix, u_char fbcount)
{
	u_int bpp;			  /* Bits per pixel */
	u_long linelen;			  /* New line length */
	u_long fbsize;			  /* New size for one image buffer */
	u_long oldsize;			  /* Old size for all buffers */
	u_long newsize;			  /* New size for all buffers */
	u_int buf;
	u_long addr;

	if ((fbhres == pwi->fbhres) && (fbvres == pwi->fbvres)
	    && (pix == pwi->pix) && (fbcount == pwi->fbcount))
		return 0;		  /* Success, nothing to do */

	/* Align horizontal resolution to word boundary. */
	bpp = 1 << pwi->pi->bpp_shift;
	linelen = ((u_long)fbhres * bpp + 31) & ~31; /* in bits */
	fbhres = (u_short)(linelen/bpp);	     /* in pixels */
	linelen /= 8;				     /* in bytes */
	fbsize = linelen * fbvres;

	oldsize = pwi->fbsize * pwi->fbcount;
	newsize = fbsize * fbcount;
	if (fbpool.used - oldsize + newsize > fbpool.size)
		return 1;		  /* Framebufferpool too small */

	/* If we are not the last window, we have to relocate the subsequent
	   windows. */
	addr = pwi->fbuf[0];
	if (win+1 < MAX_WINDOWS) {
		u_long newaddr, oldaddr;
		u_long used;

		/* Move the framebuffer contents of all subsequent windows in
		   one go */
		oldaddr = addr + oldsize;
		newaddr = addr + newsize;
		used = oldaddr - fbpool.base;/* Used mem for windows 0..win */
		used = fbpool.used - used;   /* Used mem for windows win+1.. */
		memmove((void *)newaddr, (void *)oldaddr, used);

		/* Then relocate the image buffer addresses of these windows */
		lcd_relocbuffers(newaddr, win+1);
	}

	/* Now clear the new framebuffer with background color */
	memset32((unsigned *)addr, pwi->bg, newsize/4);

	/* Set new image buffer addresses for this window */
	for (buf = 0; buf < pwi->fbmaxcount; buf++) {
		pwi->fbuf[buf] = addr;
		if (buf < fbcount)
			addr += fbsize;
	}

	/* Set new window data */
	pwi->fbhres = fbhres;
	pwi->fbvres = fbvres;
	pwi->fbcount = fbcount;
	pwi->pix = pix;

	/* Change the amount of used framepuffer pool */
	fbpool.used = fbpool.used - oldsize + newsize;

	return 0;			  /* Success */
}


/* Get current framebuffer pool information */
void lcd_getfbpool(fbpoolinfo_t *pfp)
{
	*pfp = fbpool;
}


/* Set new framebuffer pool information */
void lcd_setfbpool(fbpoolinfo_t *pfp)
{
	fbpool = *pfp;
}

/************************************************************************/
/* Graphics Primitives							*/
/************************************************************************/

/* Draw pixel at (x, y) with color */
void lcd_pixel(const wininfo_t *pwi, XYPOS x, XYPOS y, COLOR32 color)
{
	if ((x >= 0) && (x < pwi->fbhres) && (y >= 0) && (y < pwi->fbvres))
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


/* Draw circle outline at (x, y) with radius r and color */
void lcd_circle(const wininfo_t *pwi, XYPOS x, XYPOS y, XYPOS r, COLOR32 color)
{
	printf("####lcd_circle() not yet implemented####\n");
}

/* Draw filled circle at (x, y) with radius r and color */
void lcd_disc(const wininfo_t *pwi, XYPOS x, XYPOS y, XYPOS r, COLOR32 color)
{
	printf("####lcd_disc() not yet implemented####\n");
}

/* Draw text string s at (x, y) with alignment/attribute a and colors fg/bg */
void lcd_text(const wininfo_t *pwi, XYPOS x, XYPOS y, char *s, u_int a,
	      COLOR32 fg, COLOR32 bg)
{
	printf("####lcd_text() not yet implemented####\n");
}

/* Draw bitmap from address addr at (x, y) with alignment/attribute a */
void lcd_bitmap(const wininfo_t *pwi, XYPOS x, XYPOS y, u_long addr, u_int a)
{
	printf("####lcd_bitmap() not yet implemented####\n");
}


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

/************************************************************************/
/* Video Information Handling						*/
/************************************************************************/

void lcd_getvidinfo(vidinfo_t *pvi)
{
	*pvi = vidinfo;
}

void lcd_setvidinfo(vidinfo_t *pvi)
{
	/* Set new settings to hardware; this updates vidinfo */
	lcd_hw_vidinfo(pvi, &vidinfo);
}

/* Find predefined panel by index, returns 0 (and vi unchanged) on bad index */
int lcd_getpanel(vidinfo_t *pvi, u_int index)
{
	if (index >= ARRAYSIZE(panellist))
		return 1;

	*pvi = panellist[index];
	return 0;
}

/* Search panel by string, start at index; return index (or 0 if no match) */ 
u_int lcd_searchpanel(char *s, u_int index)
{
	while (++index < ARRAYSIZE(panellist)) {
		if (strstr(panellist[index].name, s))
		    return index;	  /* match */
	}

	return 0;			  /* no more matches */
}



/************************************************************************/
/* Window Information Handling						*/
/************************************************************************/

/* Get a pointer to the wininfo structure; the structure should be considered
   as read-only here; this is more efficient than lcd_getwininfo() as the
   structure needs not to be copied. lcd_getwininfop() should be prefered in
   places where window information is required but not changed. */
const wininfo_t *lcd_getwininfop(WINDOW win)
{
	return &wininfo[win];
}

void lcd_getwininfo(wininfo_t *pwi, WINDOW win)
{
	*pwi = wininfo[win];
}

void lcd_setwininfo(wininfo_t *pwi, WINDOW win)
{
	/* Set the values to hardware; this updates wininfo[win] */
	lcd_hw_wininfo(pwi, win, &wininfo[win]);
}

