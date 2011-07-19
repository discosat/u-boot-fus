/*
 * MPC823 and PXA LCD Controller
 *
 * Modeled after video interface by Paolo Scaffardi
 *
 *
 * (C) Copyright 2001
 * Wolfgang Denk, DENX Software Engineering, wd@denx.de.
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

#ifndef _LCD_H_
#define _LCD_H_

/* Define some types to be used with displays */
typedef unsigned int COLORVAL;
typedef unsigned int RGBA;
typedef short XYPOS;

/************************************************************************/
/* HEADER FILES								*/
/************************************************************************/
#if defined(CONFIG_CMD_BMP) || defined(CONFIG_SPLASH_SCREEN)
# include <bmp_layout.h>
# include <asm/byteorder.h>
#endif

/* Architecture specific includes */
#if defined CONFIG_PXA250 || defined CONFIG_PXA27X || defined CONFIG_CPU_MONAHANS
#include <asm/byteorder.h>
#endif

#if defined(CONFIG_MPC823)
#include <lcdvideo.h>
#endif

#if defined(CONFIG_ATMEL_LCD)
#include <atmel_lcdc.h>
#include <nand.h>
#endif

#if defined(CONFIG_S3C64XX)
#include <lcd_s3c64xx.h>
#endif

/************************************************************************/
/* DEFINITIONS								*/
/************************************************************************/

#ifndef PAGE_SIZE
# define PAGE_SIZE	4096
#endif

#define MAX_NAME 32

/* Values for type entry of vidinfo_t; if TFT is set, all other bits must be
   zero. Therefore the range of this value is 0..8 */
#define VI_TYPE_SINGLESCAN 0x00		  /* Bit 0: single or dual scan */
#define VI_TYPE_DUALSCAN   0x01
#define VI_TYPE_4BITBUS    0x00		  /* Bit 1: 4-bit or 8-bit buswidth */
#define VI_TYPE_8BITBUS    0x02
#define VI_TYPE_STN        0x00		  /* Bit 2: STN or CSTN */
#define VI_TYPE_CSTN       0x04
#define VI_TYPE_STN_CSTN   0x00		  /* Bit 3: (C)STN or TFT */
#define VI_TYPE_TFT        0x08

/*
 *  Information about displays we are using. This is for configuring
 *  the LCD controller and memory allocation. Someone has to know what
 *  is connected, as we can't autodetect anything.
 */
#define CFG_HIGH	0	/* Pins are active high			*/
#define CFG_LOW		1	/* Pins are active low			*/

#define LCD_MONOCHROME	0
#define LCD_COLOR2	1
#define LCD_COLOR4	2
#define LCD_COLOR8	3
#define LCD_COLOR16	4	/* e.g. R5G5B5A1 or R5G6B5 */
#define LCD_COLOR32	5	/* unpacked formats with >16 bpp, e.g.
				   R6G6B6A1, R8G8B8, R8G8B8A4, etc. */
#define LCD_COLOR_MANY  -1      /* Support different pixel formats */

/*----------------------------------------------------------------------*/
#if defined(CONFIG_LCD_INFO_BELOW_LOGO)
# define LCD_INFO_X		0
# define LCD_INFO_Y		(BMP_LOGO_HEIGHT + VIDEO_FONT_HEIGHT)
#elif defined(CONFIG_LCD_LOGO)
# define LCD_INFO_X		(BMP_LOGO_WIDTH + 4 * VIDEO_FONT_WIDTH)
# define LCD_INFO_Y		(VIDEO_FONT_HEIGHT)
#else
# define LCD_INFO_X		(VIDEO_FONT_WIDTH)
# define LCD_INFO_Y		(VIDEO_FONT_HEIGHT)
#endif

/* Default to 8bpp if bit depth not specified */
#ifndef LCD_BPP
# define LCD_BPP			LCD_COLOR8
#endif
#ifndef LCD_DF
# define LCD_DF			1
#endif

/* Calculate nr. of bits per pixel  and nr. of colors */
#define NBITS(bit_code)		(1 << (bit_code))
#define NCOLORS(bit_code)	(1 << NBITS(bit_code))

/************************************************************************/
/* CONSOLE CONSTANTS							*/
/************************************************************************/
#if LCD_BPP == LCD_MONOCHROME

/*
 * Simple black/white definitions
 */
# define CONSOLE_COLOR_BLACK	0
# define CONSOLE_COLOR_WHITE	1	/* Must remain last / highest	*/

#elif LCD_BPP == LCD_COLOR8

/*
 * 8bpp color definitions
 */
# define CONSOLE_COLOR_BLACK	0
# define CONSOLE_COLOR_RED	1
# define CONSOLE_COLOR_GREEN	2
# define CONSOLE_COLOR_YELLOW	3
# define CONSOLE_COLOR_BLUE	4
# define CONSOLE_COLOR_MAGENTA	5
# define CONSOLE_COLOR_CYAN	6
# define CONSOLE_COLOR_GREY	14
# define CONSOLE_COLOR_WHITE	15	/* Must remain last / highest	*/

#else

/*
 * 16bpp color definitions
 */
# define CONSOLE_COLOR_BLACK	0x0000
# define CONSOLE_COLOR_WHITE	0xffff	/* Must remain last / highest	*/

#endif /* color definitions */

/************************************************************************/
/* CONSOLE DEFINITIONS & FUNCTIONS					*/
/************************************************************************/
#if 0 //#####

#if defined(CONFIG_LCD_LOGO) && !defined(CONFIG_LCD_INFO_BELOW_LOGO)
# define CONSOLE_ROWS		((panel_info.vl_row-BMP_LOGO_HEIGHT) \
					/ VIDEO_FONT_HEIGHT)
#else
# define CONSOLE_ROWS		(panel_info.vl_row / VIDEO_FONT_HEIGHT)
#endif

#define CONSOLE_COLS		(panel_info.vl_col / VIDEO_FONT_WIDTH)
#define CONSOLE_ROW_SIZE	(VIDEO_FONT_HEIGHT * lcd_line_length)
#define CONSOLE_ROW_FIRST	(lcd_console_address)
#define CONSOLE_ROW_SECOND	(lcd_console_address + CONSOLE_ROW_SIZE)
#define CONSOLE_ROW_LAST	(lcd_console_address + CONSOLE_SIZE \
					- CONSOLE_ROW_SIZE)
#define CONSOLE_SIZE		(CONSOLE_ROW_SIZE * CONSOLE_ROWS)
#define CONSOLE_SCROLL_SIZE	(CONSOLE_SIZE - CONSOLE_ROW_SIZE)

#if LCD_BPP == LCD_MONOCHROME
# define COLOR_MASK(c)		((c)	  | (c) << 1 | (c) << 2 | (c) << 3 | \
				 (c) << 4 | (c) << 5 | (c) << 6 | (c) << 7))
#else
# define COLOR_MASK(c)		(c)
#endif

#endif //0####

/************************************************************************/
/* ENUMERATIONS								*/
/************************************************************************/

/* Available TrueColor formats; Format: LCD_BPP<BPP>_<RGBA> */
typedef enum LCD_BPP_FORMATS
{
	LCD_BPP1_CLUT,
	LCD_BPP2_CLUT,
	LCD_BPP4_CLUT,
	LCD_BPP8_CLUT,
	LCD_BPP8_2321,
	LCD_BPP16_565,
	LCD_BPP16_5551,
	LCD_BPP32_666,
	LCD_BPP32_6651,
	LCD_BPP32_6661,
	LCD_BPP32_888,
	LCD_BPP32_8871,
	LCD_BPP32_8881,
	LCD_BPP32_8884
} LCD_BPP_FORMATS;


/************************************************************************/
/* TYPES AND STRUCTURES							*/
/************************************************************************/

#ifdef CONFIG_PXA250
/*
 * PXA LCD DMA descriptor
 */
struct pxafb_dma_descriptor {
	u_long	fdadr;		/* Frame descriptor address register */
	u_long	fsadr;		/* Frame source address register */
	u_long	fidr;		/* Frame ID register */
	u_long	ldcmd;		/* Command register */
};

/*
 * PXA LCD info
 */
struct pxafb_info {

	/* Misc registers */
	u_long	reg_lccr3;
	u_long	reg_lccr2;
	u_long	reg_lccr1;
	u_long	reg_lccr0;
	u_long	fdadr0;
	u_long	fdadr1;

	/* DMA descriptors */
	struct	pxafb_dma_descriptor *	dmadesc_fblow;
	struct	pxafb_dma_descriptor *	dmadesc_fbhigh;
	struct	pxafb_dma_descriptor *	dmadesc_palette;

	u_long	screen;		/* physical address of frame buffer */
	u_long	palette;	/* physical address of palette memory */
	u_int	palette_size;
	u_char	datapol;	/* DATA polarity (0=normal, 1=inverted) */
};
#endif /*CONFIG_PXA250*/

/*
 * Common LCD panel information
 */
typedef struct vidinfo {
	/* Horizontal control */
	u_short	hfp;		/* Front porch (between data and HSYNC) */
	u_short hsw;		/* Horizontal sync pulse (HSYNC) width */
	u_short hbp;		/* Back porch (between HSYNC and data) */
	u_short	hres;		/* Horizontal pixel resolution (i.e. 640) */

	/* Vertical control */
	u_short vfp;		/* Front porch (between data and VSYNC) */
	u_short	vsw;		/* Vertical sync pulse (VSYNC) width */
	u_short vbp;		/* Back porch (between VSYNC and data) */
	u_short	vres;		/* Vertical pixel resolution (i.e. 480) */

	/* Signal polarity */
	u_char  hspol;	        /* HSYNC polarity (0=normal, 1=inverted) */
	u_char  vspol;		/* VSYNC polarity (0=normal, 1=inverted) */
	u_char  denpol;		/* DEN polarity (0=normal, 1=inverted) */
	u_char  clkpol;		/* Clock polarity (0=normal, 1=inverted) */

	/* Timings */
	u_int   fps;		/* Frame rate (in frames per second) */
	u_int   clk;		/* Pixel clock (in Hz) */

	/* Backlight settings */
	u_int   pwmvalue;	/* PWM value (voltage) */
	u_int   pwmfreq;	/* PWM frequency */
	u_char  pwmenable;	/* 0: disabled, 1: enabled */

	/* Display type */
	u_char  type;		/* Bit 0: 0: 4-bit bus, 1: 8-bit bus
				   Bit 1: 0: single-scan, 1: dual-scan
				   Bit 2: 0: STN, 1: CSTN
				   Bit 3: 0: (C)STN, 1: TFT */

	/* Additional settings */
	u_char  strength;	/* Drive strength: 0=2mA, 1=4mA, 2=7mA, 3=9mA */
	u_char	dither;		/* Dither mode (FRC) #### */
	u_int   debug;		/* Debug configuration */

	/* General info */
	u_short	hsize;		/* Width of display area in millimeters */
	u_short	vsize;		/* Height of display area in millimeters */
	u_char  name[MAX_NAME];	/* Manufacturer, display and resolution */

#ifdef CONFIG_PXA250
	/* PXA LCD controller params */
	struct	pxafb_info pxa;
#endif
#ifdef CONFIG_ATMEL_LCD
	u_long	mmio;		/* Memory mapped registers */
#endif
} vidinfo_t;


typedef struct wininfo
{
	void *fbbuf[2];			  /* Pointers to primary/backing fb */
	ulong fbsize;			  /* Size of this framebuffer (bytes) */
	ulong linelen;			  /* Bytes per hres_v line */
	XYPOS hres_v;			  /* Virtual size of framebuffer */
	XYPOS vres_v;
	XYPOS hoffs;			  /* Offset within framebuffer */
	XYPOS voffs;
	XYPOS hres;			  /* Size of visible window */
	XYPOS vres;
	XYPOS hpos;			  /* Position of window on display */
	XYPOS vpos;
	XYPOS column;			  /* Console info (character based) */
	XYPOS row;
	uint fbmodify;			  /* Index of framebuffer to modify */
	uint fbshare;			  /* Mask of shared framebuffers */
	LCD_BPP_FORMATS pixformat;	  /* Pixel format */
	COLORVAL fg_col;		  /* Foreground color */
	COLORVAL bg_col;		  /* Background color */

	/* Function to draw a pixel */
	void (*lcd_pixel)(const struct wininfo *wininfo, XYPOS x, XYPOS y,
			  COLORVAL col);

	/* Function to draw a filled rectangle */
	void (*lcd_rect)(const struct wininfo *wininfo, XYPOS x, XYPOS y,
			 ushort width, ushort height, COLORVAL col);

	/* Function to draw a character */
	void (*lcd_char)(const struct wininfo *wininfo, XYPOS x, XYPOS y,
			 uchar c);
} wininfo_t;


/************************************************************************/
/* EXPORTED VARIABLES							*/
/************************************************************************/

extern vidinfo_t panel_info;
extern char lcd_is_enabled;





/************************************************************************/
/* PROTOTYPES OF EXPORTED FUNCTIONS IMPLEMENTED BY GENERIC PART lcd.c   */
/************************************************************************/

/* Video functions */

#if defined(CONFIG_RBC823)
void	lcd_disable	(void);
#endif

/* int	lcd_init(void *lcdbase); */
extern void lcd_putc(const char c);
extern void lcd_puts(const char *s);
extern void lcd_printf(const char *fmt, ...);

/* Enable or disable the display */
extern void lcd_enable(uchar enable);

/* Find predefined panel by index, returns 0 (and vi unchanged) on bad index */
extern uint lcd_getpanel(uint index, vidinfo_t *vi);

/* Search panel by string, start at index; return index (or 0 if no match) */ 
extern uint lcd_searchpanel(uint index, char *s);

/* Set new framebuffer pool information */
extern void lcd_setfbpool(ulong size, ulong base);

extern 

/* Clear selected window(s) */
extern void lcd_clear(void);

/* Get a copy of the panel info */
extern void lcd_getvidinfo(vidinfo_t *vi);

/* Set/update panel info */
extern void lcd_setvidinfo(vidinfo_t *vi);

/* Select a window */
extern void lcd_winselect(uint window);

/* Select a window and get window information */
extern void lcd_getwininfo(uint window, wininfo_t *wi);

/* Set/update window information for currently selected */
extern void lcd_winsize(uint hres, uint vres);


/************************************************************************/
/* EXPORTED FUNCTIONS IMPLEMENTED BY CONTROLLER SPECIFIC PART		*/
/************************************************************************/

/* Get pointer to a string describing the pixel format */
extern const char *lcd_getpixformat(u_short pix);

/* Get the closest matching pixel format for the given number:
   1..32: bits per pixel, 111..888: RGB, 1111..8888: RGBA */
extern u_short lcd_getpix(u_int n);

/* Update controller hardware with new vidinfo */
extern void lcd_update(vidinfo_t *vi);

/* Get a COLORVAL entry with the given RGBA value */
extern COLORVAL lcd_getcolorval(u_int rgba);


#endif	/* _LCD_H_ */
