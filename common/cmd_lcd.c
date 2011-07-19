/*
 * Generic LCD commands
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
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston,
 * MA 02111-1307 USA
 */

/* ##### TODO:
    1. Nochmal prüfen, dass fbhres und hoffs immer aligned sind
    2. Lokale Funktionsprototypen hinzufügen
    3. Prüfen, was bei LCD-Signalen schon von NBoot gesetzt ist
    4. PWM settings auf GPF15 setzen, 
    5. Pattern matching bei lcd list evtl. case insensitive
    6. Kleines README.txt schreiben, wie man neuen Displaytreiber in das neue
       Treiberkonzept einbindet
    7. cls als Löschen des aktuellen Console-Windows
    8. Bei komplexen Zeichenoperationen ab und zu WATCHDOG_RESET() aufrufen.
    9. Bei Framebuffer sind im Ende-Reg ja nur die LSBs der Adresse vorhanden.
       Macht das Probleme, wenn fbpool über eine solche Segmentgrenze geht?
   10. Puffer für Kommando-Eingabe? Sonst klappt der Download von Scripten
       nicht.
   11. Bei reg: reg create sollte lockupdate setzen, reg save wieder löschen;
       oder reg-Befehl ganz raus und besser externes Wandlungstool machen.
****/

/*
 * LCD commands
 */

#include <config.h>
#include <common.h>
#include <cmd_lcd.h>			  /* Own interface */
#include <lcd.h>			  /* lcd_line(), lcd_rect(), ... */
#include <lcd_panels.h>			  /* lcdinfo_t, lcd_getlcd(), ... */
#include <devices.h>			  /* device_t, device_register(), ... */
#include <linux/ctype.h>		  /* isdigit(), toupper() */

#if defined(CONFIG_CMD_PNG) || defined(CONFIG_CMD_BMP) || defined(CONFIG_CMD_JPG)
#define _BITMAP_SUPPORT_
#include <bitmap.h>			  /* lcd_scan_bitmap(), lcd_bitmap() */
#endif

/* The following LCD controller hardware specific includes provide extensions
   that are called via the access functions in vidinfo_t; they also must
   provide a global init function that is called in drv_lcd_init() below. */
#if defined CONFIG_PXA250 || defined CONFIG_PXA27X || defined CONFIG_CPU_MONAHANS
#include <asm/arch-pxa/pxafb.h>
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

/* Always round framebuffer pool size to full pages */
#ifndef PAGE_SIZE
#define PAGE_SIZE	4096
#endif

/* Number of display interfaces (usable in parallel); you can override this
   value in your platform specific header file; exactly this many displays
   must be initialized in cmd_lcd_init(). */
#ifndef CONFIG_DISPLAYS
#define CONFIG_DISPLAYS 1
#endif

#ifndef CONFIG_FBPOOL_SIZE
#define CONFIG_FBPOOL_SIZE 0x00100000	  /* 1MB, enough for 800x600@16bpp */
#endif

/* Default window colors */
#define DEFAULT_BG 0x000000FF		  /* Opaque black */
#define DEFAULT_FG 0xFFFFFFFF		  /* Opaque white */

/* Default console colors */
#define DEFAULT_CON_BG 0x000000FF	  /* Opaque black */
#define DEFAULT_CON_FG 0xFFFFFFFF	  /* Opaque white */

/* Default alpha values */
#define DEFAULT_ALPHA0 0x00000000	  /* Transparent */
#define DEFAULT_ALPHA1 0xFFFFFFFF	  /* Opaque */

/* Settings for win ident blinking */
#define IDENT_BLINK_COUNT 5		  /* How often */
#define IDENT_BLINK_DELAY 100000	  /* How fast */


/************************************************************************/
/* ENUMERATIONS								*/
/************************************************************************/

/* Settings that correspond with commands "draw" and "adraw"; the order
   decides in what sequence the commands are searched for, which may be
   important for sub-commands with the same prefix. */
enum DRAW_INDEX {
	/* Draw commands with two coordinate pairs x1/y1 and x2/y2 */
	DI_LINE,
	DI_FRAME,
	DI_RECT,

	/* Draw commands with one coordinate pair x1/y1 */
	DI_PIXEL,
	DI_CIRCLE,
	DI_DISC,
	DI_TEXT,
	DI_TURTLE,
#ifdef _BITMAP_SUPPORT_
	DI_BITMAP,
#endif

	/* Draw commands with no coordinate pair */
	DI_COLOR,
	DI_FILL,
	DI_TEST,
	DI_CLEAR,

	/* Unknown draw sub-command (must be the last entry!) */
	DI_HELP
};

/* Settings that correspond with command "cmap"; the order decides in what
   sequence the commands are searched for, which may be important for
   sub-commands with the same prefix. */
enum CMAP_INDEX {
	CI_VIEW,
	CI_SET,
	CI_LOAD,
	CI_STORE,
	CI_DEFAULT,

	/* Unkown cmap sub-command */
	CI_HELP
};

/* Settings that correspond with command "window"; the order decides in what
   sequence the commands are searched for, which may be important for
   sub-commands with the same prefix. */
enum WIN_INDEX {
	WI_FBRES,
	WI_SHOW,
	WI_RES,
	WI_OFFS,
	WI_POS,
	WI_ALPHA,
	WI_COLKEY,
	WI_IDENT,
	WI_ALL,
	
	/* Unkown window sub-command; must be the last entry or the window
	   extension commands won't work */
	WI_HELP
};

/* Settings that correspond with command "lcd"; the order decides in what
   sequence the commands are searched for, which may be important for
   sub-commands with the same prefix. */
enum LCD_INDEX {
	/* Settings not available with reg set val because they take more than
	   one argument or behave differently */
	LI_PANEL,
	LI_DIM,
	LI_RES,
	LI_HTIMING,
	LI_VTIMING,
	LI_POL,
	LI_EXTRA,
	LI_PWM,
	LI_PONSEQ,
	LI_POFFSEQ,
	LI_LIST,
	LI_ON,
	LI_OFF,
#if (CONFIG_DISPLAYS > 1)
	LI_ALL,
#endif

	/* Single argument settings, usually also available with reg set val */
	LI_NAME,
	LI_HDIM,
	LI_VDIM,
	LI_TYPE,
	LI_HFP,
	LI_HSW,
	LI_HBP,
	LI_HRES,
	LI_VFP,
	LI_VSW,
	LI_VBP,
	LI_VRES,
	LI_HSPOL,
	LI_VSPOL,
	LI_DENPOL,
	LI_CLKPOL,
	LI_CLK,
	LI_FPS,
	LI_DRIVE,
	LI_FRC,
	LI_PWMVALUE,
	LI_PWMFREQ,
	LI_PONLOGIC,
	LI_PONDISP,
	LI_PONCONTR,
	LI_PONPWM,
	LI_PONBL,
	LI_POFFBL,
	LI_POFFPWM,
	LI_POFFCONTR,
	LI_POFFDISP,
	LI_POFFLOGIC,

	/* Unknown lcd sub-command (must be the last entry!) */
	LI_HELP
};

#ifdef CONFIG_FSWINCE_COMPAT
/* Settings that correspond with command "reg set value"; the order decides in
   what sequence the commands are searched for, which may be important for
   sub-commands with the same prefix. */
enum REG_INDEX {
	RI_NAME,
	RI_WIDTH,
	RI_HEIGHT,
	RI_COLUMNS,
	RI_ROWS,
	RI_BLW,
	RI_HSW,
	RI_ELW,
	RI_BFW,
	RI_VSW,
	RI_EFW,
	RI_LCDCLK,
	RI_LCDPDS,
	RI_CONTRENA,
	RI_CONTRVAL,
	RI_CONTRFREQ,
	RI_PONLCDPOW,
	RI_PONLCDENA,
	RI_PONBUFENA,
	RI_PONVEEON,
	RI_PONCFLPOW,
	RI_TYPE,
	RI_CONFIG,
	RI_UNKNOWN
};
#endif /*CONFIG_FSWINCE_COMPAT*/


/************************************************************************/
/* TYPES AND STRUCTURES							*/
/************************************************************************/


/************************************************************************/
/* LOCAL VARIABLES							*/
/************************************************************************/

DECLARE_GLOBAL_DATA_PTR;

/* Currently selected window */
static VID vid_sel;
static VID vid_count;
static int lockupdate;

vidinfo_t vidinfo[CONFIG_DISPLAYS];	  /* Current display information */

/* Size and base address of the framebuffer pool */
static fbpoolinfo_t fbpool = {
	base:	CFG_UBOOT_BASE - CONFIG_FBPOOL_SIZE,
	size:	CONFIG_FBPOOL_SIZE,
	used:	0
};

/* LCD types, corresponding to lcdinfo_t.type */
static char * const typeinfo[9] = {
	"Single-scan 4-bit STN",
	"Dual-scan 4-bit STN",
	"Single-scan 8-bit STN",
	"Dual-scan 8-bit STN",
	"Single-scan 4-bit CSTN",
	"Dual-scan 4-bit CSTN",
	"Single-scan 8-bit CSTN",
	"Dual-scan 8-bit CSTN",
	"TFT"
};

/* Power-on sequence variable names */
static const char * const ponseq_name[] = {
	"ponlogic",
	"pondisp",
	"poncontr",
	"ponpwm",
	"ponbl"
};

/* Power-off sequence variable names */
static const char * const poffseq_name[] = {
	"poffbl",
	"poffpwm",
	"poffcontr",
	"poffdisp",
	"pofflogic",
};

/* Palette signature */
static const u_char palsig[6] = {
	'P', 'A', 'L', 0,		  /* "PAL" */
	1, 0				  /* V1.0 */
};

/* Keywords available with draw and adraw; info1 holds the number of
   coordinate pairs, info2 holds the index for the first rgba value (use a
   value higher than argc_max if no rgba value at all) */
static kwinfo_t const draw_kw[] = {
	[DI_LINE] =   {4, 5, 2, 4, "line"},   /* x1 y1 x2 y2 [rgba] */
	[DI_FRAME] =  {4, 5, 2, 4, "frame"},  /* x1 y1 x2 y2 [rgba] */
	[DI_RECT] =   {4, 6, 2, 4, "rect"},   /* x1 y1 x2 y2 [rgba [rgba]] */
	[DI_PIXEL] =  {2, 3, 1, 2, "pixel"},  /* x1 y1 [rgba] */
	[DI_CIRCLE] = {3, 4, 1, 3, "circle"}, /* x1 y1 r [rgba] */
	[DI_DISC] =   {3, 5, 1, 3, "disc"},   /* x1 y1 r [rgba [rgba]] */
	[DI_TEXT] =   {3, 6, 1, 4, "text"},   /* x1 y1 s [attr [rgba [rgba]]] */
	[DI_TURTLE] = {3, 4, 1, 3, "turtle"}, /* x1 y1 s [rgba] */
#ifdef _BITMAP_SUPPORT_
	[DI_BITMAP] = {3, 5, 1, 9, "bm"},     /* x1 y1 addr [n [attr]] */
#endif
	[DI_COLOR] =  {1, 2, 0, 0, "color"},  /* rgba [rgba] */
	[DI_FILL] =   {0, 1, 0, 0, "fill"},   /* [rgba] */
	[DI_TEST] =   {0, 1, 0, 9, "test"},   /* [n] */
	[DI_CLEAR] =  {0, 0, 0, 9, "clear"},  /* (no args) */
	[DI_HELP] =   {0, 0, 0, 9, "help"},   /* (no args, show usage) */
};

/* Keywords available with cmap; info1 holds the index of the start index,
   info2 holds the index of the end index; use a value > argc_max if
   start/end is not used in the sub-command.  */
static kwinfo_t const cmap_kw[] = {
	[CI_VIEW] =    {0, 0, 0, 1, ""},	/* (we need info1+2) */
	[CI_SET] =     {2, 2, 0, 9, "set"},	/* index #rgba */
	[CI_LOAD] =    {1, 3, 1, 2, "load"},	/* addr [start [end]] */
	[CI_STORE] =   {1, 3, 1, 2, "store"},	/* addr [start [end]] */
	[CI_DEFAULT] = {0, 0, 9, 9, "default"}, /* (no args) */
	[CI_HELP] =    {0, 0, 9, 9, "help"},	/* (no args, show usage) */
};

/* Keywords available with command "window"; info1 and info2 are unused */
static kwinfo_t const win_kw[] = {
	[WI_FBRES] =  {1, 4, 0, 0, "fbres"}, /* fbhres fbvres [pix [fbcount]] */
	[WI_SHOW] =   {1, 2, 0, 0, "show"},  /* fbshow [fbdraw] */
	[WI_RES] =    {2, 2, 0, 0, "res"},   /* hres vres */
	[WI_OFFS] =   {2, 2, 0, 0, "offs"},  /* hoffs voffs */
	[WI_POS] =    {2, 2, 0, 0, "pos"},   /* hpos vpos */
	[WI_ALPHA] =  {1, 3, 0, 0, "alpha"}, /* alpha0 [alpha1 [mode]] */
	[WI_COLKEY] = {1, 3, 0, 0, "colkey"},/* value [mask [mode]] */
	[WI_IDENT] =  {0, 0, 0, 0, "ident"}, /* (no args) */
	[WI_ALL] =    {0, 0, 0, 0, "all"},   /* (no args) */
	[WI_HELP] =   {0, 0, 0, 0, "help"},  /* (no args, show usage) */
};

/* Keywords available with command "lcd"; info1 and info2 are unused */
static kwinfo_t const lcd_kw[] = {
	/* Multiple arguments or argument with multiple types */
	[LI_PANEL] =     {1, 1, 0, 0, "panel"},	   /* index | substring */
	[LI_DIM] =       {2, 2, 0, 0, "dim"},      /* hdim vdim */
	[LI_RES] =       {2, 2, 0, 0, "res"},      /* hres, vres */
	[LI_HTIMING] =   {1, 3, 0, 0, "htiming"},  /* hfp [hsw [hbp]] */
	[LI_VTIMING] =   {1, 3, 0, 0, "vtiming"},  /* vfp [vsw [vbp]] */
	[LI_POL] =       {1, 4, 0, 0, "pol"},      /* hspol [vspol [clkpol
						      [denpol]]]*/
	[LI_PWM] =       {1, 2, 0, 0, "pwm"},      /* pwmvalue [pwmfreq]*/
	[LI_PONSEQ] =    {1, 5, 0, 0, "ponseq"},   /* logic [disp [contr
						      [pwm [bl]]]] */
	[LI_POFFSEQ] =   {1, 5, 0, 0, "poffseq"},  /* bl [pwm [contr [disp
						      [logic]]]] */
	[LI_EXTRA] =     {1, 2, 0, 0, "extra"},	   /* dither [drive] */
	[LI_LIST] =      {0, 2, 0, 0, "list"},     /* [index | substring
						      [count]]*/
	[LI_ON] =        {0, 0, 0, 0, "on"},	   /* (no args) */
	[LI_OFF] =       {0, 0, 0, 0, "off"},	   /* (no args) */
#if (CONFIG_DISPLAYS > 1)
	[LI_ALL] =       {0, 0, 0, 0, "all"},	   /* (no args) */
#endif
	[LI_NAME] =      {1, 1, 0, 0, "name"},	   /* name */
	[LI_HDIM] =      {1, 1, 0, 0, "hdim"},	   /* hdim */
	[LI_VDIM] =      {1, 1, 0, 0, "vdim"},	   /* vdim */
	[LI_TYPE] =      {1, 1, 0, 0, "type"},	   /* type */
	[LI_HFP] =       {1, 1, 0, 0, "hfp"},	   /* hfp */
	[LI_HSW] =       {1, 1, 0, 0, "hsw"},	   /* hsw */
	[LI_HBP] =       {1, 1, 0, 0, "hbp"},	   /* hbp */
	[LI_HRES] =      {1, 1, 0, 0, "hres"},	   /* hres */
	[LI_VFP] =       {1, 1, 0, 0, "vfp"},	   /* vfp */
	[LI_VSW] =       {1, 1, 0, 0, "vsw"},	   /* vsw */
	[LI_VBP] =       {1, 1, 0, 0, "vbp"},	   /* vbp */
	[LI_VRES] =      {1, 1, 0, 0, "vres"},	   /* vres */
	[LI_HSPOL] =     {1, 1, 0, 0, "hspol"},	   /* hspol */
	[LI_VSPOL] =     {1, 1, 0, 0, "vspol"},	   /* hspol */
	[LI_DENPOL] =    {1, 1, 0, 0, "denpol"},   /* denpol */
	[LI_CLKPOL] =    {1, 1, 0, 0, "clkpol"},   /* clkpol */
	[LI_CLK] =       {1, 1, 0, 0, "clk"},	   /* clk */
	[LI_FPS] =       {1, 1, 0, 0, "fps"},	   /* fps */
	[LI_DRIVE] =     {1, 1, 0, 0, "drive"},    /* strength */
	[LI_FRC] =       {1, 1, 0, 0, "frc"},	   /* dither */
	[LI_PWMVALUE] =  {1, 1, 0, 0, "pwmvalue"}, /* pwmvalue */
	[LI_PWMFREQ] =   {1, 1, 0, 0, "pwmfreq"},  /* pwmfreq */
	[LI_PONLOGIC] =  {1, 1, 0, 0, "ponlogic"}, /* ponlogic */
	[LI_PONDISP] =   {1, 1, 0, 0, "pondisp"},  /* pondisp */
	[LI_PONCONTR] =  {1, 1, 0, 0, "poncontr"}, /* pondisp */
	[LI_PONPWM] =    {1, 1, 0, 0, "ponpwm"},   /* ponpwm */
	[LI_PONBL] =     {1, 1, 0, 0, "ponbl"},    /* ponbl */
	[LI_POFFLOGIC] = {1, 1, 0, 0, "pofflogic"},/* pofflogic */
	[LI_POFFDISP] =  {1, 1, 0, 0, "poffdisp"}, /* poffdisp */
	[LI_POFFCONTR] = {1, 1, 0, 0, "poffcontr"},/* poffdisp */
	[LI_POFFPWM] =   {1, 1, 0, 0, "poffpwm"},  /* poffpwm */
	[LI_POFFBL] =    {1, 1, 0, 0, "poffbl"},   /* poffbl */
	[LI_HELP] =      {0, 0, 0, 0, "help"},	   /* (no args, show usage) */
};

#ifdef CONFIG_FSWINCE_COMPAT
/* Keywords available with command "reg"; info1 holds the corresponding
   sub-command for command "lcd", so that the common function set_value() can
   be used, info2 is unused. */
static kwinfo_t const reg_kw[] =
{
	[RI_NAME] =      {4, 4, LI_NAME,     0, "name"},
	[RI_WIDTH] =     {4, 4, LI_HDIM,     0, "width"},
	[RI_HEIGHT] =    {4, 4, LI_VDIM,     0, "height"},
	[RI_COLUMNS] =   {4, 4, LI_HRES,     0, "columns"},
	[RI_ROWS] =      {4, 4, LI_VRES,     0, "rows"},
	[RI_BLW] =       {4, 4, LI_HBP,      0, "blw"},
	[RI_HSW] =       {4, 4, LI_HSW,      0, "hsw"},
	[RI_ELW] =       {4, 4, LI_HFP,      0, "elw"},
	[RI_BFW] =       {4, 4, LI_VBP,      0, "bfw"},
	[RI_VSW] =       {4, 4, LI_VSW,      0, "vsw"},
	[RI_EFW] =       {4, 4, LI_VFP,      0, "efw"},
	[RI_LCDCLK] =    {4, 4, LI_CLK,      0, "lcdclk"},
	[RI_LCDPDS] =    {4, 4, LI_DRIVE,    0, "lcdportdrivestrength"},
	[RI_CONTRVAL] =  {4, 4, LI_PWMVALUE, 0, "contrastvalue"},
	[RI_CONTRFREQ] = {4, 4, LI_PWMFREQ,  0, "contrastfreq"},
	[RI_PONLCDPOW] = {4, 4, LI_PONLOGIC, 0, "ponlcdpow"},
	[RI_PONLCDENA] = {4, 4, LI_PONDISP,  0, "ponlcdena"},
	[RI_PONBUFENA] = {4, 4, LI_PONCONTR, 0, "ponlcdbufena"},
	[RI_PONVEEON] =  {4, 4, LI_PONPWM,   0, "ponveeon"},
	[RI_PONCFLPOW] = {4, 4, LI_PONBL,    0, "poncflpow"},

	/* Special reg values with no corresponding lcd sub-command; type is
	   used differently here as in command lcd. */
	[RI_TYPE] =      {4, 4, LI_HELP,     0, "type"},   /* lcd type */
	[RI_CONFIG] =    {4, 4, LI_HELP,     0, "config"}, /* polarities */
	[RI_CONTRENA] =  {4, 4, LI_HELP,     0, "contrastenable"},
};
#endif /*CONFIG_FSWINCE_COMPAT*/

/* Default color map for 1 bpp */
static const RGBA defcmap2[] = {
	0x000000FF,			  /* black */
	0xFFFFFFFF			  /* white */
};

/* Default color map for 2 bpp */
static const RGBA defcmap4[] = {
	0x000000FF,			  /* black */
	0xFF0000FF,			  /* red */
	0x00FF00FF,			  /* green */
	0xFFFFFFFF			  /* white */
};

/* Default color map for 4 bpp */
static const RGBA defcmap16[] = {
	0x000000FF,			  /* black */
	0x000080FF,			  /* dark blue */
	0x008000FF,			  /* dark green */
	0x008080FF,			  /* dark cyan */
	0x800000FF,			  /* dark red */
	0x800080FF,			  /* dark magenta */
	0x808000FF,			  /* dark yellow */
	0x555555FF,			  /* dark gray */
	0xAAAAAAFF,			  /* light gray */
	0x0000FFFF,			  /* blue */
	0x00FF00FF,			  /* green */
	0x00FFFFFF,			  /* cyan */
	0xFF0000FF,			  /* red */
	0xFF00FFFF,			  /* magenta */
	0xFFFF00FF,			  /* yellow */
	0xFFFFFFFF			  /* white */
};


/************************************************************************/
/* Prototypes of local functions					*/
/************************************************************************/

/* Get pointer to current lcd panel information */
static vidinfo_t *lcd_get_vidinfo_p(VID vid);

/* Get pointer to current window information */
static wininfo_t *lcd_get_wininfo_p(const vidinfo_t *pvi, WINDOW win);


/************************************************************************/
/* Local Helper Functions						*/
/************************************************************************/

/* Switch display on; return error message on error or NULL on success */
static char *lcd_on(vidinfo_t *pvi)
{
	WINDOW win;
	wininfo_t *pwi;

	if (!pvi->lcd.hres || !pvi->lcd.vres)
		return "No valid lcd panel\n";

	for (win=0, pwi=pvi->pwi; win<pvi->wincount; win++, pwi++) {
		if (pwi->active)
			break;
	}
	if (win >= pvi->wincount)
		return "No active window\n";

	pvi->enable(pvi);
	pvi->is_enabled = 1;

	return NULL;
}


/* Switch display off */
static void lcd_off(vidinfo_t *pvi)
{
	pvi->disable(pvi);
	pvi->is_enabled = 0;
}


/* Get a pointer to the vidinfo structure */
static vidinfo_t *lcd_get_vidinfo_p(VID vid)
{
	return (vid < vid_count) ? &vidinfo[vid] : NULL;
}


/* Get a pointer to the wininfo structure */
static wininfo_t *lcd_get_wininfo_p(const vidinfo_t *pvi, WINDOW win)
{
	return (win < pvi->wincount) ? &pvi->pwi[win] : NULL;
}


/* If not locked, update lcd hardware and set environment variable */
static void set_vidinfo(vidinfo_t *pvi)
{
	char buf[CFG_CBSIZE];
	char *cmd = "lcd";
	char *s = buf;

	/* If update should not take place, return immediately */
	if (lockupdate)
		return;

	/* If new display has hres or vres 0 and display was switched on,
	   switch it off now. The most common case is if panel #0 is set. */
	if ((!pvi->lcd.hres || !pvi->lcd.vres) && pvi->is_enabled)
		lcd_off(pvi);

	/* Call hardware specific function to update controller hardware */
	pvi->set_vidinfo(pvi);

	/* If display is not active (hres or vres is 0), unset environment
	   variable */
	if (!pvi->lcd.hres || !pvi->lcd.vres) {
		setenv(pvi->name, NULL);
		return;
	}

	/* Prepare environment string for this lcd panel */
	s += sprintf(s, "%s %s \"%s\"", cmd, lcd_kw[LI_NAME].keyword,
		     pvi->lcd.name);
	s += sprintf(s, "; %s %s %u", cmd, lcd_kw[LI_TYPE].keyword,
		     pvi->lcd.type);
	s += sprintf(s, "; %s %s %u %u", cmd, lcd_kw[LI_DIM].keyword,
		     pvi->lcd.hdim, pvi->lcd.vdim);
	s += sprintf(s, "; %s %s %u %u", cmd, lcd_kw[LI_RES].keyword,
		     pvi->lcd.hres, pvi->lcd.vres);
	s += sprintf(s, "; %s %s %u %u %u", cmd, lcd_kw[LI_HTIMING].keyword,
		     pvi->lcd.hfp, pvi->lcd.hsw, pvi->lcd.hbp);
	s += sprintf(s, "; %s %s %u %u %u", cmd, lcd_kw[LI_VTIMING].keyword,
		     pvi->lcd.vfp, pvi->lcd.vsw, pvi->lcd.vbp);
	s += sprintf(s, "; %s %s %u %u %u %u", cmd, lcd_kw[LI_POL].keyword,
		     ((pvi->lcd.pol & HS_LOW) != 0),
		     ((pvi->lcd.pol & VS_LOW) != 0),
		     ((pvi->lcd.pol & DEN_LOW) != 0),
		     ((pvi->lcd.pol & CLK_FALLING) != 0));
	s += sprintf(s, "; %s %s %u", cmd, lcd_kw[LI_FPS].keyword,
		     pvi->lcd.fps);
	s += sprintf(s, "; %s %s %u %u", cmd, lcd_kw[LI_PWM].keyword,
		     pvi->lcd.pwmvalue, pvi->lcd.pwmfreq);
	s += sprintf(s, "; %s %s %u %u %u %u %u", cmd,
		     lcd_kw[LI_PONSEQ].keyword, pvi->lcd.ponseq[0],
		     pvi->lcd.ponseq[1], pvi->lcd.ponseq[2],
		     pvi->lcd.ponseq[3], pvi->lcd.ponseq[4]);
	s += sprintf(s, "; %s %s %u %u %u %u %u", cmd,
		     lcd_kw[LI_POFFSEQ].keyword, pvi->lcd.poffseq[0],
		     pvi->lcd.poffseq[1], pvi->lcd.poffseq[2],
		     pvi->lcd.poffseq[3], pvi->lcd.poffseq[4]);
	s += sprintf(s, "; %s %s %u %u", cmd, lcd_kw[LI_EXTRA].keyword,
		     pvi->frc, pvi->drive);

	/* Set the environment variable */
	setenv(pvi->name, buf);
}


/* If not locked, update window hardware and set environment variable */
static void set_wininfo(const wininfo_t *pwi)
{
	char buf[CFG_CBSIZE];
	char *cmd = "win";
	char *s = buf;

	/* If update should not take place, return immediately */
	if (lockupdate)
		return;

	/* Call hardware specific function to update controller hardware */
	pwi->pvi->set_wininfo(pwi);

	/* If window is not active, unset the environment variable */
	if (!pwi->active) {
		setenv((char *)pwi->name, NULL);
		return;
	}

	/* Prepare environment string for this window */
	s += sprintf(s, "%s %s %u %u %u %u", cmd, win_kw[WI_FBRES].keyword,
		     pwi->fbhres, pwi->fbvres, pwi->pix, pwi->fbcount);
	s += sprintf(s, "; %s %s %u %u", cmd, win_kw[WI_SHOW].keyword,
		     pwi->fbshow, pwi->fbdraw);
	s += sprintf(s, "; %s %s %u %u", cmd, win_kw[WI_RES].keyword,
		     pwi->fbhres, pwi->fbvres);
	s += sprintf(s, "; %s %s %u %u", cmd, win_kw[WI_OFFS].keyword,
		     pwi->hoffs, pwi->voffs);
	s += sprintf(s, "; %s %s %d %d", cmd, win_kw[WI_POS].keyword,
		     pwi->hpos, pwi->vpos);
	s += sprintf(s, "; %s %s #%08x #%08x %u", cmd, win_kw[WI_ALPHA].keyword,
		     pwi->alpha0, pwi->alpha1, pwi->alphamode);
	s += sprintf(s, "; %s %s #%08x #%08x %u", cmd,
		     win_kw[WI_COLKEY].keyword, pwi->ckvalue, pwi->ckmask,
		     pwi->ckmode);

	/* Set the environment variable */
	setenv((char *)pwi->name, buf);
}


/************************************************************************/
/* Command cls								*/
/************************************************************************/

#if 0 //####
static int cls(cmd_tbl_t *cmdtp, int flag, int argc, char *argv[])
{
	wininfo_t *pwi;

	pwi = lcd_getwininfo(win_sel);

	/* If selected window is active, fill it with background color */
	if (pwi->active) {
		memset32((unsigned *)pwi->pfbuf[pwi->fbdraw], pwi->bg,
			 pwi->fbsize/4);
		/* #### TODO
		wi.column = 0;
		wi.row = 0;
		lcd_setwininfo(pwi, win_sel);
		*/
	}

	return 0;
}


U_BOOT_CMD(
	cls,	1,	1,	cls,
	"cls\t- clear screen\n",
	NULL
);
#endif //0 #####


/************************************************************************/
/* Command fbpool							*/
/************************************************************************/

/* Relocate all windows to newaddr, starting at given window (via pwi) */
static void relocbuffers(wininfo_t *pwi, u_long newaddr)
{
	const vidinfo_t *pvi = pwi->pvi;
	VID vid = pvi->vid;

	/* Relocate all windows, i.e. set all image buffer addresses
	   to the new address and update hardware */
	do {
		WINDOW win = pwi->win;
		u_int buf;

		for (buf = 0; buf < pwi->fbmaxcount; buf++) {
			if (pwi->pfbuf[buf] != newaddr) {
#ifdef DEBUG
				printf("%s: relocated buffer %u from 0x%08lx"
				       " to 0x%08lx\n", pwi->name,
				       buf, pwi->pfbuf[buf], newaddr);
#endif
				pwi->pfbuf[buf] = newaddr;
			}
			if (buf < pwi->fbcount)
				newaddr += pwi->fbsize;
		}

		/* Update controller hardware with new info */
		set_wininfo(pwi);
		if (win+1 < pvi->wincount)
			pwi++;		  /* Next window */
		else if (++vid < vid_count) {
			pvi++;		  /* Next display */
			pwi = pvi->pwi;	  /* First window */
		}
	} while (vid < vid_count);
}

static int do_fbpool(cmd_tbl_t *cmdtp, int flag, int argc, char *argv[])
{
	fbpoolinfo_t *pfp = &fbpool;
	WINDOW win;
	VID vid;
	u_int buf;

	if (argc > 1)
	{
		ulong newsize;
		ulong newbase;

		/* Get size and round to full pages */
		newsize = simple_strtoul(argv[1], NULL, 16);
		newsize = (((newsize - 1)/PAGE_SIZE)+1)*PAGE_SIZE;

		/* Get base address */
		if (argc > 2)
			newbase = simple_strtoul(argv[1], NULL, 16);
		else
			newbase = CFG_UBOOT_BASE - newsize;

		/* Check if new values are valid */
		if ((newbase < MEMORY_BASE_ADDRESS)
		    || (newbase + newsize > CFG_UBOOT_BASE)) {
			puts("Bad address or collision with U-Boot code\n");
			return 1;
		}

		if (pfp->used > newsize) {
			puts("Current framebuffers exceed new size\n");
			return 1;
		}

		/* Move framebuffer content in one go */
		memmove((void *)newbase, (void *)pfp->base, pfp->used);

		/* Relocate the image buffer addresses of all windows */
	        relocbuffers(lcd_get_wininfo_p(lcd_get_vidinfo_p(0), 0),
			     newbase);

		/* Finally set the new framebuffer pool values */
		pfp->size = newsize;
		pfp->base = newbase;
		gd->fb_base = newbase;
	}

	/* Print current or new settings */
	printf("Framebuffer Pool: 0x%08lx - 0x%08lx (%lu bytes total,"
	       " %lu bytes used)\n",
	       pfp->base, pfp->base + pfp->size - 1, pfp->size, pfp->used);

	/* List all buffers */
	puts("\nBuffer list:\n");
	for (vid = 0; vid < vid_count; vid++) {
		const vidinfo_t *pvi;

		pvi = lcd_get_vidinfo_p(vid);
		for (win = 0; win < pvi->wincount; win++) {
			const wininfo_t *pwi;

			pwi = lcd_get_wininfo_p(pvi, win);
			for (buf = 0; buf < pwi->fbcount; buf++) {
				printf(" %s buffer %u: 0x%08lx - 0x%08lx\n",
				       pwi->name, buf, pwi->pfbuf[buf],
				       pwi->pfbuf[buf] + pwi->fbsize - 1);
			}
		}
	}
	putc('\n');

	return 0;
}

U_BOOT_CMD(
	fbpool,	5,	1,	do_fbpool,
	"fbpool\t- set framebuffer pool\n",
	"<size> <address>\n"
	"    - set framebuffer pool of <size> at <address>\n"
	"fbpool <size>\n"
	"    - set framebuffer pool of <size> immediately before U-Boot\n"
	"fbpool\n"
	"    - show framebuffer pool settings"
);


/************************************************************************/
/* Command draw								*/
/************************************************************************/

/* Draw turtle graphics until end of string, closing bracket or error */
static int lcd_turtle(const wininfo_t *pwi, XYPOS *px, XYPOS *py, char *s,
		      u_int count)
{
	XYPOS x = *px;
	XYPOS y = *py;
	u_char flagB = 0;
	u_char flagN = 0;
	char *s_new;
	int param;
	char c;
	XYPOS nx;
	XYPOS ny;
	int i = 0;
	char *errmsg = NULL;

	do {
		c = toupper(s[i++]);
		if (c == 'B')		  /* "Blank" prefix */
			flagB = 1;
		else if (c == 'N')	  /* "No-update" prefix */
			flagN = 1;
		else if (c == 0) {	  /* End of string */
			if (!count) {
				i--;
				goto DONE;
			}
			errmsg = "Missing ']'";
		} else if (c == ']') {	  /* End of repeat */
			if (!count)
				errmsg = "Invalid ']'";
			else if (!--count)
				goto DONE;/* Loop completed */
			i = 0;		  /* Next repeat iteration */
		} else if (c == '(') {	  /* Call address */
			u_int addr;
			int ret;

			addr = simple_strtoul(s+i, &s_new, 16);
			if (!s_new)
				errmsg = "Missing address";
			else {
				i = s_new - s;
				if (s[i++] != ')')
					errmsg = "Missing ')'";
				ret = lcd_turtle(pwi, &x, &y, (char *)addr, 0);
				if (ret < 0) {
					printf(" in substring at 0x%08x\n",
					       addr);
					errmsg = "called";
				}
			}
		} else {
			/* Parse number; if no number found, use 1 */
			param = (XYPOS)simple_strtol(s+i, &s_new, 0);
			if (s_new)
				i = s_new - s;
			else
				param = 0;
			nx = x;
			ny = y;
			if ((c != 'M') && (param == 0))
				param = 1;

			if (c == '[') {
				int ret = lcd_turtle(pwi, &x, &y, s+i, param);

				if (ret < 0)
					return -1;
				i += ret;
				continue;
			}
			switch (c) {
			case 'E':
				ny -= param;
				/* Fall through to case 'R' */
			case 'R':
				nx += param;
				break;

			case 'F':
				nx += param;
				/* Fall through to case 'D' */
			case 'D':
				ny += param;
				break;

			case 'G':
				ny += param;
				/* Fall through to case 'L' */
			case 'L':
				nx -= param;
				break;

			case 'H':
				nx -= param;
				/* Fall through to case 'U' */
			case 'U':
				ny -= param;
				break;

			case 'M':
				if (s[i++] != ',') {
					errmsg = "Missing ','";
					break;
				}
				nx += param;
				param = (XYPOS)simple_strtol(s+i, &s_new, 0);
				if (s_new)
					i = s_new - s;
				else
					param = 0;
				ny += param;
				break;

			default:
				errmsg = "Unknown turtle command";
				break;
			}

			if (!errmsg) {
				if (!flagB)
					lcd_line(pwi, x, y, nx, ny);
				if (!flagN) {
					x = nx;
					y = ny;
				}
				flagB = 0;
				flagN = 0;
			}
		}
	} while (!errmsg);

	/* Handle error message */
	i--;
	printf("%s at offset %i\n", errmsg, i);

DONE:
	*px = x;
	*py = y;
	return i;
}

static int do_draw(cmd_tbl_t *cmdtp, int flag, int argc, char *argv[])
{
	wininfo_t *pwi;
	const vidinfo_t *pvi;
	u_short sc;
	XYPOS x1 = 0, y1 = 0, x2 = 0, y2 = 0;
	u_int a;			  /* Attribute */
	RGBA rgba2;
	RGBA rgba_save;
	u_char coord_pairs, colindex;

	pvi = lcd_get_vidinfo_p(vid_sel);
	pwi = lcd_get_wininfo_p(pvi, pvi->win_sel);

	if (argc < 2) {
		printf("%s: FG #%08x (#%08x), BG #%08x (#%08x)\n", pwi->name,
		       pwi->fg.rgba, pwi->ppi->col2rgba(pwi, pwi->fg.col),
		       pwi->bg.rgba, pwi->ppi->col2rgba(pwi, pwi->bg.col));
		return 0;
	}

	/* Set "apply alpha" attribute if command was "adraw" */
	pwi->attr = (argv[0][0] == 'a') ? ATTR_ALPHA : 0;

	/* Search for keyword in draw keyword list */
	sc = parse_sc(argc, argv[1], DI_HELP, draw_kw, ARRAYSIZE(draw_kw));

	/* Print usage if command not valid */
	if (sc == DI_HELP) {
		printf("usage:\n%s ", cmdtp->name);
		puts(cmdtp->help);
		putc('\n');
		return 1;
	}

	/* If selected window is not active do nothing */
	if (!pwi->active) {
		//PRINT_WIN(vid_sel, pvi->win_sel);
		printf("Selected %s is not active\n", pwi->name);
		return 1;
	}

	/* Parse one or two coordinate pairs for commands with coordinates */
	coord_pairs = draw_kw[sc].info1;
	if (coord_pairs > 0) {
		x1 = (XYPOS)simple_strtol(argv[2], NULL, 0);
		y1 = (XYPOS)simple_strtol(argv[3], NULL, 0);

		if (coord_pairs > 1) {
			x2 = (XYPOS)simple_strtol(argv[4], NULL, 0);
			y2 = (XYPOS)simple_strtol(argv[5], NULL, 0);
		}
	}

	/* Parse one or two optional colors for commands who may have colors */
	colindex = draw_kw[sc].info2 + 2;
	rgba2 = pwi->bg.rgba;
	if (argc > colindex) {
		RGBA rgba1;

		/* Parse first color */
		if (parse_rgb(argv[colindex], &rgba1))
			return 1;
		colindex++;
		if (argc > colindex) {
			/* Parse second color */
			if (parse_rgb(argv[colindex], &rgba2))
				return 1;
		}
		lcd_set_fg(pwi, rgba1); /* Set FG color */
	}

	/* Finally execute the drawing command */
	switch (sc) {
	case DI_PIXEL:			  /* Draw pixel */
		lcd_pixel(pwi, x1, y1);
		break;

	case DI_LINE:			  /* Draw line */
		lcd_line(pwi, x1, y1, x2, y2);
		break;

	case DI_RECT:			  /* Draw filled rectangle */
		lcd_rect(pwi, x1, y1, x2, y2);
		if ((argc < 8) || (pwi->fg.rgba == rgba2))
			break;
		lcd_set_fg(pwi, rgba2);
		/* Fall through to case DI_FRAME */

	case DI_FRAME:			  /* Draw rectangle outline */
		lcd_frame(pwi, x1, y1, x2, y2);
		break;

	case DI_CIRCLE:			  /* Draw circle outline */
	case DI_DISC:			  /* Draw filled circle */
		x2 = (XYPOS)simple_strtol(argv[4], NULL, 0); /* Parse radius */
		if (sc == DI_DISC) {
			lcd_disc(pwi, x1, y1, x2);
			if ((argc < 7) || (pwi->fg.rgba == rgba2))
				break;
			lcd_set_fg(pwi, rgba2);
		}
		lcd_circle(pwi, x1, y1, x2);
		break;
			
	case DI_TEXT:			  /* Draw text */
		rgba_save = pwi->bg.rgba;
		lcd_set_bg(pwi, rgba2);

		/* Optional argument 4: attribute */
		a = (argc > 5) ? simple_strtoul(argv[5], NULL, 0) : 0;
		pwi->attr |= a;
		lcd_text(pwi, x1, y1, argv[4]);
		lcd_set_bg(pwi, rgba_save);
		break;

#ifdef _BITMAP_SUPPORT_
	case DI_BITMAP:			  /* Draw bitmap */
	{
		u_int addr;
		const char *errmsg;

		/* Argument 3: address */
		addr = simple_strtoul(argv[4], NULL, 16);

		/* Optional argument 4: bitmap number (for multi bitmaps) */
		if (argc > 5) {
			u_int n = simple_strtoul(argv[5], NULL, 0);
			u_int i;

			for (i = 0; i < n; i++) {
				addr = lcd_scan_bitmap(addr);
				if (!addr)
					break;
			}

			if (!addr || !lcd_scan_bitmap(addr)) {
				printf("Bitmap %d not found\n", n);
				return 1;
			}
		}

		/* Optional argument 5: attribute */
		a = (argc > 6) ? simple_strtoul(argv[6], NULL, 0) : 0;
		pwi->attr |= a;
		errmsg = lcd_bitmap(pwi, x1, y1, addr);
		if (errmsg) {
			puts(errmsg);
			return 1;
		}
		break;
	}
#endif /*_BITMAP_SUPPORT_*/

	case DI_TURTLE:			  /* Draw turtle graphics */
		if (lcd_turtle(pwi, &x1, &y1, argv[4], 0) < 0)
			puts(" in argument string\n");
		break;

	case DI_COLOR:			  /* Set FG and BG color */
		lcd_set_bg(pwi, rgba2);
		break;

	case DI_FILL:			  /* Fill window with FG color */
		lcd_fill(pwi);
		break;

	case DI_CLEAR:			  /* Fill window with BG color */
		lcd_clear(pwi);
		break;

	case DI_TEST:			  /* Draw test pattern */
		a = (argc > 2) ? simple_strtoul(argv[2], NULL, 0) : 0;
		rgba_save = pwi->fg.rgba;
		lcd_set_fg(pwi, 0xFFFFFFFF); /* White for grid & circles */
		lcd_test(pwi, a);
		lcd_set_fg(pwi, rgba_save);
		break;

	default:			  /* Should not happen */
		printf("Unhandled draw command '%s'\n", argv[1]);
		return 1;
	}

	return 0;
}

U_BOOT_CMD(
	draw, 8,	1,	do_draw,
	"draw\t- draw to selected window\n",
	"pixel x y [#rgba]\n"
	"    - draw pixel at (x, y)\n"
	"draw line x1 y1 x2 y2 [#rgba]\n"
	"    - draw line from (x1, y1) to (x2, y2)\n"
	"draw frame x1 y1 x2 y2 [#rgba]\n"
	"    - draw unfilled rectangle from (x1, y1) to (x2, y2)\n"
	"draw rectangle x1 y1 x2 y2 [#rgba [#rgba]]\n"
	"    - draw filled rectangle (with outline) from (x1, y1) to (x2, y2)\n"
	"draw circle x y r [#rgba]\n"
	"    - draw unfilled circle at (x, y) with radius r\n"
	"draw disc x y r [#rgba [#rgba]]\n"
	"    - draw filled circle (with outline) at (x, y) with radius r\n"
	"draw text x y string [a [#rgba [#rgba]]]\n"
	"    - draw text string at (x, y) with attribute a\n"
#ifdef _BITMAP_SUPPORT_
	"draw bitmap x y addr [n [a]]\n"
	"    - draw bitmap n from addr at (x, y) with attribute a\n"
#endif
	"draw turtle x y string [#rgba]\n"
	"    - draw turtle graphic command from string at (x, y)\n"
	"draw fill [#rgba]\n"
	"    - fill window with color\n"
	"draw test [n]\n"
	"    - draw test pattern n\n"
	"draw clear\n"
	"    - clear window\n"
	"draw color #rgba [#rgba]\n"
	"    - set FG (and BG) color\n"
	"draw\n"
	"    - show current FG and BG color\n"
);

U_BOOT_CMD(
	adraw, 8,	1,	do_draw,
	"adraw\t- draw to selected window, directly applying alpha\n",
	"arguments\n"
	"    - see 'help draw' for a description of the 'adraw' arguments\n"
);

/************************************************************************/
/* Command cmap								*/
/************************************************************************/

static void set_default_cmap(const wininfo_t *pwi)
{
	u_int bpp_shift;
	u_int cmapsize;
	u_int end;
	RGBA *defcmap;
	static const RGBA * const defcmap_table[] = {
		defcmap2,		  /* 1bpp, predefined map */
		defcmap4,		  /* 2bpp, predefined map */
		defcmap16,		  /* 4bpp, predefined map */
		NULL			  /* 8bpp, compute here */
	};

	if (!(pwi->ppi->flags & PIF_CMAP))
		return;			  /* No color map pixel format */

	bpp_shift = pwi->ppi->bpp_shift;
	if (bpp_shift > 3)
		return;			  /* No support for 16/32 bpp cmaps */

	cmapsize = 1 << (1 << bpp_shift);
	end = cmapsize - 1;
	defcmap = (RGBA *)defcmap_table[bpp_shift];
	if (!defcmap) {
		u_int index;

		defcmap = pwi->cmap;
		for (index = 0; index <= end; index++) {
			RGBA rgba;
			RGBA b;

			/* Color map with 8bpp. Handle index as RGBA3320
			   value, i.e. RRRGGGBB. We could store these values
			   also in a predefined RGBA array, but that would be
			   1KB (256 entries a 4 bytes). The following code
			   computing the RGBA value from the index is much
			   shorter. */
			rgba = (index & 0xE0) << 24; /* R[7:5] */
			rgba |= (index & 0x1C) << 19;/* G[7:5] */
			rgba |= rgba >> 3;	     /* R[7:2], G[7:2] */
			rgba |= (rgba & 0xC0C00000) >> 6; /* R[7:0], G[7:0] */
		        b = index & 0x03;		  /* B[1:0] */
			b |= b << 2;			  /* B[3:0] */
			b |= b << 4;			  /* B[7:0] */
			rgba |= (b << 8) | 0xFF;
			defcmap[index] = rgba;
		}
	}

	/* Set the color map to hardware */
	pwi->set_cmap(pwi, 0, end, defcmap);
}

static int do_cmap(cmd_tbl_t *cmdtp, int flag, int argc, char *argv[])
{
	vidinfo_t *pvi;
	wininfo_t *pwi;
	u_short sc;
	u_int index = 0;
	u_int end = ~0;
	RGBA rgba;
	RGBA *cmap;
	u_int cmapsize;

	/* Get the info for the currently selected display and window */
	pvi = lcd_get_vidinfo_p(vid_sel);
	pwi = lcd_get_wininfo_p(pvi, pvi->win_sel);
	if (!(pwi->ppi->flags & PIF_CMAP)) {
		printf("%s: no color map based pixel format\n", pwi->name);
		return 1;
	}

	if (argc < 2)
		sc = CI_VIEW;
	else {
		if ((argc <= 3) && isdigit(argv[1][0]))
			sc = CI_VIEW;
		else
			sc = parse_sc(argc, argv[1], CI_HELP, cmap_kw,
				      ARRAYSIZE(cmap_kw));
	}

	if (sc == CI_HELP) {
		printf("usage:\n%s ", cmdtp->name);
		puts(cmdtp->help);
		putc('\n');
		return 1;
	}

	cmapsize = 1 << (1 << pwi->ppi->bpp_shift);

	/* info1 holds the argument index of the color (start) index */
	if (argc > cmap_kw[sc].info1 + 2) {
		index = simple_strtoul(argv[cmap_kw[sc].info1 + 2], NULL, 0);
		if (index >= cmapsize) {
			puts("Bad color index\n");
			return 1;
		}
	}

	/* info2 holds the argument index of the color end index */
	if (argc > cmap_kw[sc].info2 + 2)
		end = simple_strtoul(argv[cmap_kw[sc].info2 + 2], NULL, 0);
	if (end >= cmapsize)
	        end = cmapsize-1;
	else if (end < index)
		end = index;

	cmap = pwi->cmap;
	switch (sc) {
	case CI_SET:
		/* Second argument is RGBA color */
		if (parse_rgb(argv[3], &rgba))
			return 1;

		pwi->set_cmap(pwi, index, index, &rgba);
		break;

	case CI_VIEW:
		/* List colors */
		do {
			printf("%3u (0x%02x): #%08x\n", index, index,
			       cmap[index]);
		} while (++index <= end);
		break;

	case CI_LOAD:
	case CI_STORE:
	{
		u_char *p;
		u_int count;
		u_int palcount;
		u_int i;

		/* First argument: memory address */
		p = (u_char *)simple_strtoul(argv[2], NULL, 16);

		count = end - index + 1;

		/* Should we store the color map? */
		if (sc == CI_STORE) {
			printf("Storing PAL V1.0 color map with %u entries"
			       " at %p\n", count, p);

			/* Yes, store palette header with number of entries */
			memcpy(p, palsig, 6);
			p[6] = count >> 8;
			p[7] = count & 255;
			p += 8;

			/* Write RGBA values in big endian order */
			do {
				rgba = cmap[index];
				p[0] = (u_char)(rgba >> 24);
				p[1] = (u_char)(rgba >> 16);
				p[2] = (u_char)(rgba >> 8);
				p[3] = (u_char)rgba;
				index++;
				p += 4;
			} while (index <= end);
			break;
		}

		/* No: Load palette: check PAL signature  */
		if (memcmp(p, palsig, 6) != 0) {
			printf("No PAL V1.0 color map found at %p\n", p);
			return 1;
		}

		palcount = (p[6] << 8) | p[7];

		/* Adjust count if new palette has fewer colors */
		if (palcount < count) {
			count = palcount;
			end = index + count - 1;
		}

		printf("Reading %u entries from PAL V1.0 color map at %p\n",
		       count, p);

		p += 8;
		for (i=index; i<=end; i++, p+=4) {
			rgba = p[0] << 24;
			rgba |= p[1] << 16;
			rgba |= p[2] << 8;
			rgba |= p[3];
			cmap[i] = rgba;
		}
		pwi->set_cmap(pwi, index, end, &cmap[index]);
		break;
	}

	case CI_DEFAULT:		  /* Set default color map */
		set_default_cmap(pwi);
		break;

	default:			  /* Unhandled command?!?!? */
		break;
	}

	return 0;
}

U_BOOT_CMD(
	cmap,	7,	1,	do_cmap,
	"cmap\t- handle color map entries\n",
	"set n #rgba\n"
	"    - set color map entry n to new color\n"
	"cmap load | store addr [start [end]]\n"
	"    - load/store whole or part of color map from/to memory\n"
	"cmap default\n"
	"    - Set default color map\n"
	"cmap [start [end]]\n"
	"    - list whole or part of color map\n"
);

/************************************************************************/
/* Command window							*/
/************************************************************************/

/* Move offset if window would not fit within framebuffer */
static void fix_offset(wininfo_t *wi)
{
	/* Move offset if necessary; window must fit into image buffer */
	if ((HVRES)wi->hoffs + wi->hres > wi->fbhres)
		wi->hoffs = (XYPOS)(wi->fbhres - wi->hres);
	if ((HVRES)wi->voffs + wi->vres > wi->fbvres)
		wi->voffs = (XYPOS)(wi->fbvres - wi->vres);
}

/* Set new framebuffer resolution, pixel format, and/or framebuffer count for
   the given window */
static int setfbuf(wininfo_t *pwi, HVRES hres, HVRES vres,
		   HVRES fbhres, HVRES fbvres, PIX pix, u_char fbcount)
{
	HVRES fbmaxhres, fbmaxvres;
	u_long oldsize, newsize;
	u_long linelen, fbsize;
	u_long addr;
	WINDOW win;
	const pixinfo_t *ppi;
	const vidinfo_t *pvi;
	fbpoolinfo_t *pfp = &fbpool;

	/* Check if pixel format is valid for this window */
	win = pwi->win;
	pvi = pwi->pvi;
	ppi = pvi->get_pixinfo_p(win, pix);
	if (!ppi) {
		printf("%s: bad pixel format '%u'\n", pwi->name, pix);
		return 1;
	}

	/* Check if framebuffer count is valid for this window */
	if (fbcount > pwi->fbmaxcount) {
		printf("%s: bad image buffer count '%u'\n", pwi->name, fbcount);
		return 1;
	}

	/* Check if resolution is valid */
	fbhres = pvi->align_hres(win, pix, fbhres);
	fbmaxhres = pvi->get_fbmaxhres(win, pix);
	fbmaxvres = pvi->get_fbmaxvres(win, pix);
	if ((fbhres > fbmaxhres) || (fbvres > fbmaxvres)) {
		printf("Requested size %u x %u exceeds allowed size %u x %u"
		       " for pixel format #%u\n",
		       fbhres, fbvres, fbmaxhres, fbmaxvres, pix);
		return 1;
	}

	/* Compute the size of one framebuffer line (incl. alignment) */
	linelen = ((u_long)fbhres << ppi->bpp_shift) >> 3;

	/* Compute the size of one image buffer */
	fbsize = linelen * fbvres;

	newsize = fbsize * fbcount;
	oldsize = pwi->fbsize * pwi->fbcount;

	if (pfp->used - oldsize + newsize > pfp->size) {
		puts("Framebuffer pool too small\n");
		return 1;
	}

	/* OK, the new settings can be made permanent */
	pwi->hres = hres;
	pwi->vres = vres;
	pwi->fbhres = fbhres;
	pwi->fbvres = fbvres;
	pwi->active = (fbhres && fbvres && fbcount);
	pwi->ppi = ppi;
	pwi->fbcount = pwi->active ? fbcount : 0;
	pwi->linelen = linelen;
	pwi->fbsize = fbsize;
	pwi->fbdraw = 0;
	pwi->fbshow = 0;
	if (pwi->pix != pix) {
		/* New pixel format: set default bg + fg */
		pwi->pix = pix;
		set_default_cmap(pwi);
		lcd_set_fg(pwi, DEFAULT_FG);
		lcd_set_bg(pwi, DEFAULT_BG);
		pwi->alpha0 = DEFAULT_ALPHA0;
		pwi->alpha1 = DEFAULT_ALPHA1;
		pwi->alphamode = (pwi->ppi->flags & PIF_ALPHA) ? 2 : 1;
	}
	fix_offset(pwi);

	/* If size changed, move framebuffers of all subsequent windows and
	   change framebuffer pool info (used amount) */
	addr = pwi->pfbuf[0];
	if (oldsize == newsize)
		set_wininfo(pwi);	  /* Only update current window */
	else {
		u_long newaddr, oldaddr;
		u_long used;

		oldaddr = addr + oldsize;
		newaddr = addr + newsize;

		/* Used mem for all windows up to current */
		used = oldaddr - pfp->base;

		/* Used mem for all subsequent windows */
		used = pfp->used - used;

		/* Update framebuffer pool */
		pfp->used = pfp->used - oldsize + newsize;

		/* Move framebuffers of all subsequent windows in one go */
		memmove((void *)newaddr, (void *)oldaddr, used);

		/* Then relocate the image buffer addresses of this and all
		   subsequent windows */
		relocbuffers(pwi, addr);
	}

	/* Clear the new framebuffer with background color */
	lcd_clear(pwi);

#ifdef CONFIG_MULTIPLE_CONSOLES
	pwi->ci.x = 0;
	pwi->ci.y = 0;
	pwi->ci.fg = pwi->ppi->rgba2col(pwi, DEFAULT_CON_FG);
	pwi->ci.bg = pwi->ppi->rgba2col(pwi, DEFAULT_CON_BG);
#else
	/* The console might also be interested in the buffer changes */
	console_update(pwi, DEFAULT_CON_FG, DEFAULT_CON_BG);
#endif

	return 0;
}

/* Set window resolution */
static int set_winres(wininfo_t *pwi, HVRES hres, HVRES vres)
{
	HVRES fbhres, fbvres;
	u_char fbcount;

	/* Check if framebuffer must be increased */
	fbhres = pwi->fbhres;
	fbvres = pwi->fbvres;
	fbcount = pwi->fbcount;
	if ((hres <= fbhres) && (vres <= fbvres) && (fbcount > 0)) {
		/* No, simply set new window size */
		pwi->hres = hres;
		pwi->vres = vres;
		fix_offset(pwi);
		set_wininfo(pwi);
		return 0;
	}

	/* Yes, set new framebuffer size */
	if (hres > fbhres)
		fbhres = hres;
	if (vres > fbvres)
		fbvres = vres;
	if (fbcount < 1)
		fbcount = 1;
	return setfbuf(pwi, hres, vres, fbhres, fbvres, pwi->pix, fbcount);
}

/* Print the window information */
static void show_wininfo(const wininfo_t *pwi)
{
	u_int buf;

	printf("%s:\n", pwi->name);
	printf("Framebuffer:\t%d x %d pixels, %lu bytes (%lu bytes/line)\n",
	       pwi->fbhres, pwi->fbvres, pwi->fbsize, pwi->linelen);
	printf("\t\t%u buffer(s) available", pwi->fbmaxcount);
	if (pwi->fbcount) {
		printf(", draw to #%u, show #%u", pwi->fbdraw, pwi->fbshow);
		for (buf=0; buf<pwi->fbcount; buf++) {
			printf("\n\t\timage buffer %u: 0x%08lx - 0x%08lx", buf,
			       pwi->pfbuf[buf], pwi->pfbuf[buf]+pwi->fbsize-1);
		}
	}
	printf("\nPixel Format:\t#%u, %u/%u bpp, %s\n", pwi->pix,
	       pwi->ppi->depth, 1 << pwi->ppi->bpp_shift, pwi->ppi->name);
	printf("Window:\t\t%u x %u pixels, from offset (%d, %d)"
	       " to pos (%d, %d)\n", pwi->hres, pwi->vres,
	       pwi->hoffs, pwi->voffs, pwi->hpos, pwi->vpos);
	printf("Alpha Setting:\talpha0=#%08x, alpha1=#%08x, alphamode=0x%x\n",
	       pwi->alpha0, pwi->alpha1, pwi->alphamode);
	printf("Color Keying:\tckvalue=#%08x, ckmask=#%08x, ckmode=0x%x\n\n",
	       pwi->ckvalue, pwi->ckmask, pwi->ckmode);
}

/* Handle window command */
static int do_win(cmd_tbl_t *cmdtp, int flag, int argc, char *argv[])
{
	vidinfo_t *pvi;
	wininfo_t *pwi;
	WINDOW win;
	u_short sc;
	char c;

	/* Get the info for the currently selected display and window */
	pvi = lcd_get_vidinfo_p(vid_sel);
	pwi = lcd_get_wininfo_p(pvi, pvi->win_sel);

	/* If no parameter, show current window info and valid pixel formats */ 
	if (argc < 2) {
		PIX pix;
		const pixinfo_t *ppi;

		/* Show the main window info */
		show_wininfo(pwi);

		/* Show the valid pixel formats */
		puts("Pixel Formats:\t#\tbpp\tformat\n");
		for (pix = 0; pix < pvi->pixcount; pix++) {
			ppi = pvi->get_pixinfo_p(pvi->win_sel, pix);
			if (ppi)
				printf("\t\t%u\t%u/%u\t%s\n", pix, ppi->depth,
				       1 << ppi->bpp_shift, ppi->name);
		}
		return 0;		  /* Done */
	}

	/* If the first argument is a number, this selects a new window */
	c = argv[1][0];
	if ((c >= '0') && (c <= '9')) {
		win = (WINDOW)simple_strtoul(argv[1], NULL, 0);
		if (win >= pvi->wincount) {
			printf("Bad window number '%u'\n", win);
			return 1;
		}
		pvi->win_sel = win;
		return 0;		  /* Done */
	}
		
	/* Search for regular window sub-commands */
	sc = parse_sc(argc, argv[1], WI_HELP, win_kw, ARRAYSIZE(win_kw));

	/* If not recognized, print usage and return */
	if (sc == WI_HELP) {
		printf("usage:\n%s ", cmdtp->name);
		puts(cmdtp->help);
		putc('\n');
		return 1;
	}

	switch (sc) {
	case WI_FBRES: {
		HVRES fbhres, fbvres;
		HVRES hres, vres;
		u_char pix, fbcount;

		/* Arguments 1+2: fbhres and fbvres */
		fbhres = (HVRES)simple_strtoul(argv[2], NULL, 0);
		fbvres = (HVRES)simple_strtoul(argv[3], NULL, 0);

		/* Argument 3: pixel format (index number) */
		pix = pwi->defpix;
		if (argc > 4)
			pix = (u_char)simple_strtoul(argv[4], NULL, 0);

		/* Argument 4: buffer count */
		fbcount = 1;
		if (argc == 6)
			fbcount = (u_char)simple_strtoul(argv[5], NULL, 0);

		/* Reduce window size if necessary */
		hres = pwi->hres;
		vres = pwi->vres;
		if (hres > fbhres)
			hres = fbhres;
		if (vres > fbvres)
			vres = fbvres;

		/* Test validity and set new framebuffer size */
		if (setfbuf(pwi, hres, vres, fbhres, fbvres, pix, fbcount))
			return 1;
		break;
	}

	case WI_SHOW: {
		u_char fbshow;

		/* Argument 1: buffer number to show */
		fbshow = (u_char)simple_strtoul(argv[2], NULL, 0);
		if (fbshow >= pwi->fbcount) {
			printf("Bad image buffer '%u'\n", fbshow);
			return 1;
		}

		/* Argument 2: buffer number to draw to */
		if (argc > 3) {
			u_char fbdraw;

			fbdraw = (u_char)simple_strtoul(argv[3], NULL, 0);
			if (fbdraw >= pwi->fbcount) {
				printf("Bad image buffer '%u'\n", fbdraw);
				return 1;
			}
			pwi->fbdraw = fbdraw;
		}

		/* Set new values */
		pwi->fbshow = fbshow;
		set_wininfo(pwi);
		break;
	}

	case WI_RES: {
		HVRES hres, vres;

		/* Arguments 1+2: hres and vres */
		hres = (HVRES)simple_strtoul(argv[2], NULL, 0);
		vres = (HVRES)simple_strtoul(argv[3], NULL, 0);

		/* Set window resolution */
		return set_winres(pwi, hres, vres);
	}

	case WI_OFFS:
		/* Arguments 1+2: hoffs and voffs (non-negative) */
		pwi->hoffs = pvi->align_hoffs(pwi,
				      (XYPOS)simple_strtoul(argv[2], NULL, 0));
		pwi->voffs = (XYPOS)simple_strtoul(argv[3], NULL, 0);
		fix_offset(pwi);
		set_wininfo(pwi);
		break;

	case WI_POS:
		/* Arguments 1+2: hpos and vpos, may be negative */
		pwi->hpos = (XYPOS)simple_strtol(argv[2], NULL, 0);
		pwi->vpos = (XYPOS)simple_strtol(argv[3], NULL, 0);
		set_wininfo(pwi);
		break;

	case WI_ALPHA:
	{
		RGBA alpha0;

		if (parse_rgb(argv[2], &alpha0))
			return 1;
		if (argc > 3) {
			RGBA alpha1;
			if (parse_rgb(argv[3], &alpha1))
				return 1;
			if (argc > 4) {
				u_char alphamode;
				alphamode = (u_char)simple_strtol(argv[4],
								  NULL, 0);
				if (alphamode > 2) {
					puts("Illegal alpha mode\n");
					return 1;
				}
				pwi->alphamode = alphamode;
			}
			pwi->alpha1 = alpha1;
		}
		pwi->alpha0 = alpha0;

		/* Actually set hardware */
		set_wininfo(pwi);
		break;
	}

	case WI_COLKEY:
	{
		RGBA ckvalue;

		if (parse_rgb(argv[2], &ckvalue))
			return 1;
		if (argc > 3) {
			RGBA ckmask;
			if (parse_rgb(argv[3], &ckmask))
				return 1;
			if (argc > 4) {
				u_char ckmode;
			        ckmode = (u_char)simple_strtol(argv[4],
							       NULL, 0);
				if (ckmode > 3) {
					puts("Illegal color key mode\n");
					return 1;
				}
				pwi->ckmode = ckmode;
			}
			pwi->ckmask = ckmask;
		}
		pwi->ckvalue = ckvalue;

		/* Actually set hardware */
		set_wininfo(pwi);
		break;
	}

	case WI_IDENT:
	{
		u_int i;

		for (i=0; i<IDENT_BLINK_COUNT; i++) {
			pwi->replace = 0x00FF00FF; /* Green */
			set_wininfo(pwi);
			udelay(IDENT_BLINK_DELAY);
			pwi->replace = 0xFFFFFF00; /* Transparent (=off) */
			set_wininfo(pwi);
			udelay(IDENT_BLINK_DELAY);
		}
		break;
	}

	case WI_ALL:
		for (win = 0; win < pvi->wincount; win++)
			show_wininfo(lcd_get_wininfo_p(pvi, win));
		break;

        default:			  /* Unhandled sub-command?!?!? */
		break;
	}

	return 0;
}

U_BOOT_CMD(
	win,	7,	0,	do_win,
	"win\t- set framebuffer and overlay window parameters\n",
	"n\n"
	"    - Select window n\n"
	"show fbshow [fbdraw]\n"
	"    - Set the buffer to show and to draw to\n"
	"win fbres [fbhres fbvres [pix [fbcount]]]\n"
	"    - Set virtual framebuffer resolution, pixel format, buffer count\n"
	"win res [hres vres]\n"
	"    - Set window resolution (i.e. OSD size)\n"
	"win offs [hoffs voffs]\n"
	"    - Set window offset within virtual framebuffer\n"
	"win pos [x y]\n"
	"    - Set window position on display\n"
	"win ident\n"
	"    - Identify the window by blinking it a few times\n"
	"win alpha alpha0 [alpha1 [mode]]\n"
	"    - Set per-window alpha values and mode\n"
	"win colkey rgbaval [rgbamask [mode]]"
	"    - Set per-window color key value, mask and mode\n"
	"win all\n"
	"    - List all windows of the current display\n"
	"win\n"
	"    - Show all settings of selected window\n"
);


/************************************************************************/
/* Command lcd								*/
/************************************************************************/

static void set_delay(u_short *delays, int index, u_short value)
{
	u_short oldval = delays[index];
	int i;

	/* Set new value */
	delays[index] = value;

	/* Modify all values with larger delays (that are executed after this
	   entry) accordingly */
	for (i=0; i<POW_COUNT; i++) {
		if ((i != index) && (delays[i] > oldval))
			delays[i] += value - oldval;
	}
}

static int set_value(vidinfo_t *pvi, char *argv, u_int sc)
{
	u_int param = 0;
	HVRES hres, vres;

	/* All parameters but LI_NAME require a number, parse it */
	if (sc != LI_NAME)
		param = simple_strtoul(argv, NULL, 0);

	switch (sc)
	{
	case LI_NAME:			  /* Parse string */
		strncpy(pvi->lcd.name, argv, MAX_NAME);
		pvi->lcd.name[MAX_NAME-1] = 0;
		break;

	case LI_HDIM:			  /* Parse u_short */
		pvi->lcd.hdim = (u_short)param;
		break;

	case LI_VDIM:			  /* Parse u_short */
		pvi->lcd.vdim = (u_short)param;
		break;

	case LI_TYPE:			  /* Parse u_char (0..8) */
		if (param > 8)
			param = 8;
		pvi->lcd.type = (u_char)param;
		break;

	case LI_HFP:			  /* Parse u_short */
		pvi->lcd.hfp = (u_short)param;
		break;

	case LI_HSW:			  /* Parse u_short */
		pvi->lcd.hsw = (u_short)param;
		break;

	case LI_HBP:			  /* Parse u_short */
		pvi->lcd.hbp = (u_short)param;
		break;

	case LI_HRES:			  /* Parse HVRES */
		hres = (HVRES)param;
		vres = pvi->lcd.vres;

		/* This also sets resolution of window 0 of this display to
		   the same size */
		if (set_winres(lcd_get_wininfo_p(pvi, 0), hres, vres))
			return 1;
			
		pvi->lcd.hres = hres;
		break;

	case LI_VFP:			  /* Parse u_short */
		pvi->lcd.hfp = (u_short)param;
		break;

	case LI_VSW:			  /* Parse u_short */
		pvi->lcd.vsw = (u_short)param;
		break;

	case LI_VBP:			  /* Parse u_short */
		pvi->lcd.vbp = (u_short)param;
		break;

	case LI_VRES:			  /* Parse HVRES */
		hres = pvi->lcd.hres;
		vres = (HVRES)param;

		/* This also sets resolution of window 0 of this display to
		   the same size */
		if (set_winres(lcd_get_wininfo_p(pvi, 0), hres, vres))
			return 1;
			
		pvi->lcd.vres = vres;
		break;

	case LI_HSPOL:			  /* Parse flag (u_char 0..1) */
		if (param)
			pvi->lcd.pol |= HS_LOW;
		else
			pvi->lcd.pol &= ~HS_LOW;
		break;

	case LI_VSPOL:			  /* Parse flag (u_char 0..1) */
		if (param)
			pvi->lcd.pol |= VS_LOW;
		else
			pvi->lcd.pol &= ~VS_LOW;
		break;

	case LI_DENPOL:			  /* Parse flag (u_char 0..1) */
		if (param)
			pvi->lcd.pol |= DEN_LOW;
		else
			pvi->lcd.pol &= ~DEN_LOW;
		break;

	case LI_CLKPOL:			  /* Parse flag (u_char 0..1) */
		if (param)
			pvi->lcd.pol |= CLK_FALLING;
		else
			pvi->lcd.pol &= ~CLK_FALLING;
		break;

	case LI_CLK:			  /* Parse u_int; scale appropriately */
		if (param < 1000000) {
			param *= 1000;
			if (param < 1000000)
				param *= 1000;
		}
		pvi->lcd.clk = (u_int)param;
		pvi->lcd.fps = 0;	  /* Compute fps from clk */
		break;

	case LI_FPS:			  /* Parse u_int */
		pvi->lcd.fps = (u_short)param;
		pvi->lcd.clk = 0;	  /* Compute clk from fps */
		break;

	case LI_PONLOGIC:		  /* Power-on delay for VLCD */
		set_delay(pvi->lcd.ponseq, PON_LOGIC, (u_short)param);
		break;

	case LI_PONDISP:		  /* Power-on delay for DEN */
		set_delay(pvi->lcd.ponseq, PON_DISP, (u_short)param);
		break;

	case LI_PONCONTR:		  /* Power-on delay for controller */
		set_delay(pvi->lcd.ponseq, PON_CONTR, (u_short)param);
		break;

	case LI_PONPWM:			 /* Power-on delay for PWM */
		set_delay(pvi->lcd.ponseq, PON_PWM, (u_short)param);
		break;

	case LI_PONBL:			 /* Power-on delay for backlight */
		set_delay(pvi->lcd.ponseq, PON_BL, (u_short)param);
		break;

	case LI_POFFBL:			 /* Power-off delay for backlight */
		set_delay(pvi->lcd.poffseq, POFF_BL, (u_short)param);
		break;

	case LI_POFFPWM:		  /* Power-off delay for PWM */
		set_delay(pvi->lcd.poffseq, POFF_PWM, (u_short)param);
		break;

	case LI_POFFCONTR:		  /* Power-off delay for controller */
		set_delay(pvi->lcd.poffseq, POFF_CONTR, (u_short)param);
		break;

	case LI_POFFDISP:		  /* Power-off delay for DEN */
		set_delay(pvi->lcd.poffseq, POFF_DISP, (u_short)param);
		break;

	case LI_POFFLOGIC:		  /* Power-off delay for VLCD */
		set_delay(pvi->lcd.poffseq, POFF_LOGIC, (u_short)param);
		break;

	case LI_DRIVE:			  /* Parse u_char */
		pvi->drive = (u_char)param;
		break;

	case LI_FRC:			  /* Parse flag (u_char 0..1) */
		pvi->frc = (param != 0);
		break;

	case LI_PWMVALUE:		  /* Parse u_int */
		if (param > MAX_PWM)
			param = MAX_PWM;
		pvi->lcd.pwmvalue = (u_short)param;
		break;

	case LI_PWMFREQ:		  /* Parse u_int */
		pvi->lcd.pwmfreq = (u_int)param;
		break;

	default:			  /* Should not happen */
		puts("Unhandled lcd setting\n");
		return 1;
	}
	return 0;
}

static void show_vidinfo(const vidinfo_t *pvi)
{
	u_int i;

	/* Show LCD panel settings */
	printf("Display Name:\tname='%s'\n", pvi->lcd.name);
	printf("Dimensions:\thdim=%umm, vdim=%umm\n",
	       pvi->lcd.hdim, pvi->lcd.vdim);
	printf("Display Type:\ttype=%u (%s)\n",
	       pvi->lcd.type, typeinfo[pvi->lcd.type]);
	printf("Display Size:\thres=%u, vres=%u\n",
	       pvi->lcd.hres, pvi->lcd.vres);
	printf("Horiz. Timing:\thfp=%u, hsw=%u, hbp=%u\n",
	       pvi->lcd.hfp, pvi->lcd.hsw, pvi->lcd.hbp);
	printf("Vert. Timing:\tvfp=%u, vsw=%u, vbp=%u\n",
	       pvi->lcd.vfp, pvi->lcd.vsw, pvi->lcd.vbp);
	printf("Polarity:\thspol=%u, vspol=%u, denpol=%u, clkpol=%u\n",
	       ((pvi->lcd.pol & HS_LOW) != 0),
	       ((pvi->lcd.pol & VS_LOW) != 0),
	       ((pvi->lcd.pol & DEN_LOW) != 0),
	       ((pvi->lcd.pol & CLK_FALLING) != 0));
	printf("Display Timing:\tfps=%u (clk=%u)\n",
	       pvi->lcd.fps, pvi->lcd.clk);
	printf("PWM:\t\tpwmvalue=%u, pwmfreq=%u\n",
	       pvi->lcd.pwmvalue, pvi->lcd.pwmfreq);
	puts("Power-on Seq.:\t");
	for (i=0; i<POW_COUNT; i++)
		printf("%s=%u%s", ponseq_name[i], pvi->lcd.ponseq[i],
		       (i<POW_COUNT-1) ? ", " : "\n");
	puts("Power-off Seq.:\t");
	for (i=0; i<POW_COUNT; i++)
		printf("%s=%u%s", poffseq_name[i], pvi->lcd.poffseq[i],
		       (i<POW_COUNT-1) ? ", " : "\n");

	/* Show driver settings */
	printf("\nDisplay driver:\t%s, %u windows, %u pixel formats\n",
	       pvi->driver_name, pvi->wincount, pvi->pixcount);
	printf("State:\t\tdisplay is switched %s\n",
	       pvi->is_enabled ? "ON" : "OFF");
	printf("Extra Settings:\tfrc=%u, drive=%umA\n\n",
	       pvi->frc, pvi->drive);
}

static int do_lcd(cmd_tbl_t *cmdtp, int flag, int argc, char *argv[])
{
	vidinfo_t *pvi;
	u_short sc;
	uint param;

	/* Get the LCD panel info */
	pvi = lcd_get_vidinfo_p(vid_sel);

	/* If no parameter is given, show current display info */
	if (argc < 2) {
		show_vidinfo(pvi);
		return 0;
	}

#if (CONFIG_DISPLAYS > 1)
	/* If the first argument is a number, this selects a new display */
	if (isdigit(argv[1][0])) {
		VID vid;
		vid = (u_int)simple_strtoul(argv[1], NULL, 0);
		if (vid >= vid_count) {
			printf("Bad display number '%u'\n", vid);
			return 1;
		}
		vid_sel = vid;
		return 0;		  /* Done */
	}
#endif /* CONFIG_DISPLAYS > 1 */

	/* Search for regular lcd sub-commands */
	sc = parse_sc(argc, argv[1], LI_HELP, lcd_kw, ARRAYSIZE(lcd_kw));

	/* If not recognized, print usage and return */
	if (sc == LI_HELP) {
		printf("usage:\n%s ", cmdtp->name);
		puts(cmdtp->help);
		putc('\n');
		return 1;
	}

	/* Execute the specific command; if the command wants to call
	   set_vidinfo() at the end, use break, if it should not call
	   set_vidinfo(), directly use return 0. */
	switch (sc) {
	case LI_ON: {
		char *errmsg;
		errmsg = lcd_on(pvi);
		if (errmsg)
			puts(errmsg);
		return 0;
	}

	case LI_OFF:
		lcd_off(pvi);
		return 0;

	case LI_LIST: {
		unsigned count = 0xFFFFFFFF;
		unsigned index = 0;
		char c = 0;
		unsigned shown = 0;

		if (argc > 2) {
			/* Get count if given */
			if (argc > 3)
				count = simple_strtoul(argv[3], NULL, 0);

			/* If first argument is a number, parse start index */
			c = argv[2][0];
			index = 0;
			if (isdigit(c)) {
				c = 0;
				index = simple_strtoul(argv[2], NULL, 0);
			}
			/* If the first argument is an empty string, c is also
			   zero here; this is intentionally handled like a
			   normal index list later because an empty string
			   matches all entries anyway. */
		}

		/* Show header line */
		puts("#\tType\thres x vres\thdim x vdim\tName\n"
		     "---------------------------------------------"
		     "-------------------------\n");

		while (count--)
		{
			const char *p;
			const lcdinfo_t *pli;

			if (c) {
				/* Search next matching panel */
				index = lcd_search_lcd(argv[2], index);
				if (!index)
					break;	  /* No further match */
			}
			pli = lcd_get_lcdinfo_p(index);
			if (!pli)
				break;		  /* Reached end of list */

			/* Show entry */
			if (pli->type < 4)
				p = "STN";
			else if (pli->type < 8)
				p = "CSTN";
			else
				p = "TFT";
			printf("%d:\t%s\t%4u x %u\t%4u x %u\t%s\n",
			       index, p, pli->hres, pli->vres,
			       pli->hdim, pli->vdim, pli->name);
			shown++;

			/* Next panel */
			index++;
		}
		if (!shown)
			puts("(no match)\n");

		return 0;
	}

	case LI_PANEL: {		  /* Parse number or string */
		const lcdinfo_t *pli;

		if (isdigit(argv[2][0])) {
			/* Parse panel index number */
			param = simple_strtoul(argv[2], NULL, 0);
		} else if (!(param = lcd_search_lcd(argv[2], 0))) {
			printf("\nNo panel matches '%s'\n", argv[2]);
			break;
		}
			
		pli = lcd_get_lcdinfo_p(param);
		if (!pli)
			printf("\nBad lcd panel index '%d'\n", param);
		else {
			lcdinfo_t old_lcd = pvi->lcd;

			/* Try to set the resolution for window 0 to the new
			   panel resolution; if this fails restore old panel */
			pvi->lcd = *pli;
			if (set_winres(lcd_get_wininfo_p(pvi, 0),
				       pli->hres, pli->vres)) {
				pvi->lcd = old_lcd;
				return 1;
			}
		}
		break;
	}

	case LI_DIM:		  /* Parse exactly 2 u_shorts */
		param = simple_strtoul(argv[2], NULL, 0);
		pvi->lcd.hdim = (u_short)param;
		param = simple_strtoul(argv[3], NULL, 0);
		pvi->lcd.vdim = (u_short)param;
		break;

	case LI_RES:		  /* Parse exactly 2 HVRES'es */
		param = simple_strtoul(argv[2], NULL, 0);
		pvi->lcd.hres = (HVRES)param;
		param = simple_strtoul(argv[3], NULL, 0);
		pvi->lcd.vres = (HVRES)param;
		break;

	case LI_HTIMING:	  /* Parse up to 3 u_shorts */
		param = simple_strtoul(argv[2], NULL, 0);
		pvi->lcd.hfp = (u_short)param;
		if (argc < 4)
			break;
		param = simple_strtoul(argv[3], NULL, 0);
		pvi->lcd.hsw = (u_short)param;
		if (argc < 5)
			break;
		param = simple_strtoul(argv[4], NULL, 0);
		pvi->lcd.hbp = (u_short)param;
		break;

	case LI_VTIMING:	  /* Parse up to 3 u_shorts */
		param = simple_strtoul(argv[2], NULL, 0);
		pvi->lcd.vfp = (u_short)param;
		if (argc < 4)
			break;
		param = simple_strtoul(argv[3], NULL, 0);
		pvi->lcd.vsw = (u_short)param;
		if (argc < 5)
			break;
		param = simple_strtoul(argv[4], NULL, 0);
		pvi->lcd.vbp = (u_short)param;
		break;

	case LI_POL:			  /* Parse up to 4 flags */
		if (simple_strtoul(argv[2], NULL, 0))
			pvi->lcd.pol |= HS_LOW;
		else
			pvi->lcd.pol &= ~HS_LOW;
		if (argc < 4)
			break;
		if (simple_strtoul(argv[3], NULL, 0))
			pvi->lcd.pol |= VS_LOW;
		else
			pvi->lcd.pol &= ~VS_LOW;
		if (argc < 5)
			break;
		if (simple_strtoul(argv[4], NULL, 0))
			pvi->lcd.pol |= DEN_LOW;
		else
			pvi->lcd.pol &= ~DEN_LOW;
		break;
		if (argc < 6)
			break;
		if (simple_strtoul(argv[5], NULL, 0))
			pvi->lcd.pol |= CLK_FALLING;
		else
			pvi->lcd.pol &= ~CLK_FALLING;
		break;

	case LI_PWM:		  /* Parse u_short and optional number */
		param = simple_strtoul(argv[2], NULL, 0);
		if (param > MAX_PWM)
			param = MAX_PWM;
		pvi->lcd.pwmvalue = (u_short)param;
		if (argc < 4)
			break;
		param = simple_strtoul(argv[3], NULL, 0);
		pvi->lcd.pwmfreq = (u_int)param;
		break;

	case LI_PONSEQ:
	case LI_POFFSEQ: {	  /* Parse one to five u_shorts */
		int i;

		param = 0;
		for (i=0; i<POW_COUNT; i++) {
			if (argc > i+2)
				param = simple_strtoul(argv[2], NULL, 0);
			else
				param += 10;
			if (sc == LI_PONSEQ)
				pvi->lcd.ponseq[i] = (u_short)param;
			else
				pvi->lcd.poffseq[i] = (u_short)param;
		}
		break;
	}

	case LI_EXTRA:		  /* Parse flag + number */
		param = simple_strtoul(argv[2], NULL, 0);
		pvi->frc = (param != 0);
		if (argc < 4)
			break;
		param = simple_strtoul(argv[3], NULL, 0);
		pvi->drive = (u_char)param;
		break;

#if (CONFIG_DISPLAYS > 1)
	case LI_ALL: {
		VID vid;

		for (vid=0; vid<vid_count; vid++)
			show_vidinfo(lcd_get_vidinfo_p(vid));
		return 0;
	}
#endif

	default:		  /* Parse one argument */
		if (set_value(pvi, argv[2], sc))
			return 1;
		break;
	}

	/* Update controller hardware with new settings */
	set_vidinfo(pvi);

	return 0;
}

U_BOOT_CMD(
	lcd,	7,	0,	do_lcd,
	"lcd\t- set or show lcd panel parameters\n",
	"list [index [count]]\n"
	"    - list predefined lcd panels\n"
	"lcd list substring [count]\n"
	"    - list panel entries matching substring\n"
	"lcd panel n\n"
	"    - set the predefined lcd panel n\n"
	"lcd panel substring\n"
	"    - set the (first) predefined panel that matches substring\n"
	"lcd param value\n"
	"    - set the lcd parameter param, which is one of:\n"
	"\tname, hdim, vdim, hres, vres, hfp, hsw, hpb, vfp, vsw, vbp,\n"
	"\thspol, vspol, denpol, clkpol, clk, fps, type, pixel, strength,\n"
	"\tdither, pwmvalue, pwmfreq, ponlogic, pondisp, poncontr, ponpwm,\n"
	"\tponbl, poffbl, poffpwm, poffcontr, poffdisp, pofflogic\n"
	"lcd group {values}\n"
	"    - set the lcd parameter group, which is one of:\n"
	"\tdim, res, htiming, vtiming, pol, pwm, ponseq, poffseq, extra\n"
	"lcd on | off\n"
	"    - activate or deactivate lcd\n"
#if (CONFIG_DISPLAYS > 1)
	"lcd all\n"
	"    - list information of all displays\n"
	"lcd n\n"
	"    - select display n\n"
#endif
	"lcd\n"
	"    - show all current lcd panel settings\n"
);


/************************************************************************/
/* Commands reg, display, command, reboot (F&S WinCE Compatibility)	*/
/************************************************************************/

#ifdef CONFIG_FSWINCE_COMPAT
/* This function parses "reg set value" commands to set display parameters;
   the type (dword or string) is ignored. Any other "reg" commands that may be
   present in F&S WinCE display configuration files are silently accepted, but
   ignored. */
static int do_reg(cmd_tbl_t *cmdtp, int flag, int argc, char *argv[])
{
	if ((argc == 6)
	    && (strcmp(argv[1], "set") == 0)
	    && (strncmp(argv[2], "value", strlen(argv[2])) == 0)) {
		u_int param;
		u_short sc;
		vidinfo_t *pvi;
		sc = parse_sc(argc, argv[3], RI_UNKNOWN, reg_kw,
			      ARRAYSIZE(reg_kw));

		pvi = lcd_get_vidinfo_p(vid_sel);
		switch (sc) {
		case RI_TYPE:
			/* "type" is used differently here */
			param = simple_strtoul(argv[5], NULL, 0);
			if (param & 0x0002)
				pvi->lcd.type = VI_TYPE_TFT;
			else {
				pvi->lcd.type = 0;
				if (param & 0x0001)
					pvi->lcd.type |= VI_TYPE_DUALSCAN;
				if (param & 0x0004)
					pvi->lcd.type |= VI_TYPE_CSTN;
				if (param & 0x0008)
					pvi->lcd.type |= VI_TYPE_8BITBUS;
			}
			break;

		case RI_CONFIG:
			/* "config" is not available with lcd */
			param = simple_strtoul(argv[5], NULL, 0);
			if (param & 0x00100000)
				pvi->lcd.pol |= VS_LOW;
			else
				pvi->lcd.pol &= ~VS_LOW;
			if (param & 0x00200000)
				pvi->lcd.pol |= HS_LOW;
			else
				pvi->lcd.pol &= ~HS_LOW;
			if (param & 0x00400000)
				pvi->lcd.pol |= CLK_FALLING;
			else
				pvi->lcd.pol &= ~CLK_FALLING;
			if (param & 0x00800000)
				pvi->lcd.pol |= DEN_LOW;
			else
				pvi->lcd.pol &= ~DEN_LOW;
			break;

		case RI_CONTRENA:
			param = simple_strtoul(argv[5], NULL, 0);
			if (param == 0)
				pvi->lcd.pwmvalue = 0;
			break;

		default:
			/* All other commands have a counterpart in
			   lcdset, use common function */
			if (set_value(pvi, argv[5], reg_kw[sc].info1))
				return 1;
			break;
		}

		/* Update hardware with new settings */
		set_vidinfo(pvi);
	}

	return 0;
}

U_BOOT_CMD(
	reg,	6,	0,	do_reg,
	"reg\t- set LCD panel parameters (F&S WinCE compatibility)\n",
	"set value name string <name>\n"
	"    - set the display name\n"
	"reg set value <param> dword <value>\n"
	"    - set the LCD parameter param; <param> is one of:\n"
	"\twidth, height, type, rows, columns, blw, elw, bfw, efw, config,\n"
	"\thsw, vsw, lcdclk, verbose, lcdportdrivestrength, contrastenable,\n"
	"\tcontrastvalue, contrastfreq; msignal, bpp and other parameter\n"
	"\tnames are accepted for compatibility reasons, but ignored\n"
	"reg open | create | enum | save ...\n"
	"    - Accepted for compatibility reasons, but ignored\n"
);

/* This function silently accepts, but ignores "contrast", "display" or
   "reboot" commands that may be present in F&S WinCE display configuration
   files. */
static int do_ignore(cmd_tbl_t *cmdtp, int flag, int argc, char *argv[])
{
	return 0;			  /* Always succeed */
}

U_BOOT_CMD(
	display,	4,	0,	do_ignore,
	"display\t- Ignored (F&S WinCE compatibility)\n",
	"...\n"
	"    - Ignored, but accepted for compatibility reasons\n"
);

U_BOOT_CMD(
	contrast,	3,	0,	do_ignore,
	"contrast\t- Ignored (F&S WinCE compatibility)\n",
	"...\n"
	"    - Ignored, but accepted for compatibility reasons\n"
);

U_BOOT_CMD(
	reboot,		2,	0,	do_ignore,
	"reboot\t- Ignored (F&S WinCE compatibility)\n",
	"...\n"
	"    - Ignored, but accepted for compatibility reasons\n"
);
#endif /*CONFIG_FSWINCE_COMPAT*/


/************************************************************************/
/* Exported functions							*/
/************************************************************************/

/* Parse RGB or RGBA value; return 1 on error, 0 on success */
int parse_rgb(char *s, RGBA *prgba)
{
	/* Color must start with '#' */
	if (*s == '#') {
		char *ep;
		RGBA rgba;

		/* Parse color as hex number */
		rgba = simple_strtoul(s+1, &ep, 16);
		if (*ep == 0) {
			/* No parse error, all digits are hex digits */
			switch (ep-s) {
			case 7:		        /* #RRGGBB, set as opaque */
				rgba = (rgba << 8) | 0xFF;
				/* Fall through to case 9 */
			case 9:	                /* #RRGGBBAA */
				*prgba = rgba;
				return 0;       /* Success */
			}
		}
	}

	printf("Bad color '%s'\n", s);

	return 1;			  /* Error */
}


/* Parse the given keyword table for a sub-command and check for required
   argument count. Return new sub-command index or sc on error */
u_short parse_sc(int argc, char *s, u_short sc, const kwinfo_t *pki,
		 u_short count)
{
	size_t len = strlen(s);
	u_int i = 0;

	for (i=0; i<count; i++, pki++) {
		if (strncmp(pki->keyword, s, len) == 0) {
			/* We have a string match */
			if (argc < pki->argc_min+2)
				puts("Missing argument\n");
			if (argc <= pki->argc_max+2)
				sc = i; /* OK */
			break;
		}
	}

	return sc;
}


const fbpoolinfo_t *lcd_get_fbpoolinfo_p(void)
{
	return &fbpool;
}


int find_delay_index(const u_short *delays, int index, u_short value)
{
	int found;

	/* Search the next entry with the same value */
	for (; index<POW_COUNT; index++) {
		if (delays[index] == value)
			return index;
	}

	/* Search the next higher value */
	found = -1;
	for (index=0; index<POW_COUNT; index++) {
		if (delays[index] > value) {
			if ((found < 0) || (delays[index] < delays[found]))
				found = index;
		}
	}
	return found;
}


/************************************************************************/
/* GENERIC Initialization Routines					*/
/************************************************************************/
void drv_lcd_init(void)
{
	WINDOW win;
	VID vid;
	wininfo_t *pwi;
	vidinfo_t *pvi;
	device_t lcddev;
	u_long fbuf;
	char *s;

	/* Set framebuffer pool base into global data */
	gd->fb_base = fbpool.base;
	memset(&lcddev, 0, sizeof(lcddev));

	/* Initialize LCD controller(s) (GPIOs, clock, etc.) */
#if defined CONFIG_PXA250 || defined CONFIG_PXA27X || defined CONFIG_CPU_MONAHANS
	if (vid_count < CONFIG_DISPLAYS)
		pxa_lcd_init(lcd_get_vidinfo_p(vid_count++));
#endif

#ifdef CONFIG_MPC823
	if (vid_count < CONFIG_DISPLAYS)
		mpc823_lcd_init(lcd_get_vidinfo_p(vid_count++));
#endif

#ifdef CONFIG_ATMEL_LCD
	if (vid_count < CONFIG_DISPLAYS)
		atmel_lcd_init(lcd_get_vidinfo_p(vid_count++));
#endif

#ifdef CONFIG_S3C64XX
	if (vid_count < CONFIG_DISPLAYS)
		s3c64xx_lcd_init(lcd_get_vidinfo_p(vid_count++));
#endif

	/* Initialize all display entries and window entries */
	fbuf = fbpool.base;
	for (vid = 0; vid < vid_count; vid++) {
		pvi = lcd_get_vidinfo_p(vid);

		/* Init all displays with the "no panel" LCD panel, and set
		   vid, win_sel and is_enabled entries; all other entries are
		   already set by the controller specific init function(s)
		   above */
		pvi->vid = vid;
		pvi->is_enabled = 0;
		pvi->win_sel = 0;
#if (CONFIG_DISPLAYS > 1)
		sprintf(pvi->name, "lcd%u", vid);
#else
		sprintf(pvi->name, "lcd");
#endif
		pvi->lcd = *lcd_get_lcdinfo_p(0); /* Set panel #0 */

		/* Initialize the window entries; the fields defpix, pfbuf,
		   fbmaxcount and ext are already set by the controller
		   specific init function(s) above. */
		for (win = 0; win < pvi->wincount; win++) {
			unsigned buf;

			pwi = lcd_get_wininfo_p(pvi, win);

			/* Fill remaining entries with default values */
			pwi->pvi = pvi;
			pwi->win = win;
			pwi->active = 0;
			pwi->pix = pwi->defpix;
			pwi->ppi = pvi->get_pixinfo_p(win, pwi->defpix);
			pwi->hres = 0;
			pwi->vres = 0;
			pwi->hpos = 0;
			pwi->vpos = 0;
			for (buf = 0; buf < pwi->fbmaxcount; buf++)
				pwi->pfbuf[buf] = fbpool.base;
			pwi->fbsize = 0;
			pwi->linelen = 0;
			pwi->fbcount = 0;
			pwi->fbdraw = 0;
			pwi->fbshow = 0;
			pwi->fbhres = 0;
			pwi->fbvres = 0;
			pwi->hoffs = 0;
			pwi->voffs = 0;
			pwi->alpha0 = DEFAULT_ALPHA0;
			pwi->alpha1 = DEFAULT_ALPHA1;
			pwi->alphamode = (pwi->ppi->flags & PIF_ALPHA) ? 2 : 1;
			pwi->ckvalue = 0; /* Off */
			pwi->ckmask = 0;  /* Bits must match */
			pwi->ckmode = 0;  /* Check window pixels, no blend */
			lcd_set_fg(pwi, DEFAULT_FG);
			lcd_set_bg(pwi, DEFAULT_BG);

#if (CONFIG_DISPLAYS > 1)
			sprintf(pwi->name, "win%u_%u", vid, win);
#else
			sprintf(pwi->name, "win%u", win);
#endif
		}
	}

	/* Search if there are environment variables for the lcd panel(s) and
	   windows; if yes, update the settings. In any case set the new
	   information to the hardware */
	for (vid = 0; vid < vid_count; vid++) {
		int enable = 0;

		pvi = lcd_get_vidinfo_p(vid);

		s = getenv(pvi->name);
		if (s) {
			/* Execute the commands that are in the lcd variable.
			   These commands must not modify the environment.
			   This is only a danger if the user sets or adds own
			   commands. The 'lcd' commands automatically
			   generated by the driver don't change the
			   environment as long as lockupdate is set. */
			lockupdate = 1;
			run_command(s, 0);
			lockupdate = 0;
			enable = 1;
		}
		
		set_vidinfo(pvi);

		for (win = 0; win < pvi->wincount; win++) {
			pvi->win_sel = win;
			pwi = lcd_get_wininfo_p(pvi, win);

			/* Is there an environment variable for this window? */
			s = getenv(pwi->name);
			if (s) {
				/* Execute the commands that are in the window
				   variable. These commands must not modify
				   the environment. This is only a danger if
				   the user sets or adds own commands. The
				   'win' commands automatically generated by
				   the driver don't change the environment as
				   long as lockupdate is set. */
				lockupdate = 1;
				run_command(s, 0);
				lockupdate = 0;
			}

			/* Set new wininfo to controller hardware */
			set_wininfo(pwi);

#ifdef CONFIG_MULTIPLE_CONSOLES
			/* Set console info for this window */
			pwi->ci.fg = pwi->ppi->rgba2col(pwi, DEFAULT_CON_FG);
			pwi->ci.bg = pwi->ppi->rgba2col(pwi, DEFAULT_CON_BG);
			pwi->ci.x = 0;
			pwi->ci.y = 0;

			/* Init a console device for each window */
			strcpy(lcddev.name, pwi->name);
			lcddev.ext   = 0;		  /* No extensions */
			lcddev.flags = DEV_FLAGS_OUTPUT;  /* Output only */
			lcddev.putc  = lcd_putc;	  /* 'putc' function */
			lcddev.puts  = lcd_puts;	  /* 'puts' function */
			lcddev.priv  = pwi;		  /* Call-back arg */
			device_register(&lcddev);
#endif /*CONFIG_MULTIPLE_CONSOLES*/
		}

		pvi->win_sel = 0;

		/* Add a device for each display */
		memset(&lcddev, 0, sizeof(lcddev));

		strcpy(lcddev.name, pvi->name);
		lcddev.ext   = 0;		  /* No extensions */
		lcddev.flags = DEV_FLAGS_OUTPUT;  /* Output only */
		lcddev.putc  = lcd_putc;	  /* 'putc' function */
		lcddev.puts  = lcd_puts;	  /* 'puts' function */
		lcddev.priv  = lcd_get_wininfo_p(pvi, 0);  /* Call-back arg */

		device_register(&lcddev);

		/* If an lcd setting was loaded from environment, switch it on
		   now; ignore any errors */
		if (enable) {
			lcd_on(pvi);
			
			/* If there is an environment variable "splashcmd",
			   run it to show the splash screen */
			s = getenv("splashcmd");
			if (s)
				run_command(s, 0);
		}

	}

#ifndef CONFIG_MULTIPLE_CONSOLES
	/* Default console "lcd" on vid 0, win 0 */
	console_init(lcd_get_wininfo_p(lcd_get_vidinfo_p(0), 0),
		     DEFAULT_CON_FG, DEFAULT_CON_BG);
#endif
}
