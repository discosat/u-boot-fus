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
    2. Unnötige includes entfernen, lokale Funktionsprototypen hinzufügen
    3. Alten code aus lcd.c/lcd.h entfernen
    4. LCD-Einschalttiming hinzunehmen
    5. Board-spezifische GPIOs nach picomod6.c (VCFL, VLCD, etc.)
    6. window ident könnte Farbe des Windows blinken (Hardware-Feature)
    7. Prüfen, was bei LCD-Signalen schon von NBoot gesetzt ist
    8. PWM settings auf GPF15 setzen, 
    9. Kommandozeile: Ein String mit "..." sollte als *ein* Argument geparsed
       und die Anführungszeichen entfernt werden (evtl auch nicht entfernen,
       damit z.B. lcd list "320" möglich wird)
   10. Pattern matching bei lcd list evtl. case insensitive
   11. Für jedes Display und jedes Window eine mögliche Console lcdx_y
       anlegen (x: display, y: window). dannauch wieder die Console-Infos ins
       Window zurück. Problem: bei den Console-Funktionen (putc(), puts(),
       etc) müsste ein Pointer auf die wininfo_t mitgegeben werden, damit man
       in einer einheitlichen putc()/puts()-Funktion wieder auf das richtige
       Window referenzieren kann. Mit dem gleichen Konzept könnte man auch
       mehrere serielle Schnittstellen implementieren.
   12. Kleines README.txt schreiben, wie man neuen Displaytreiber in das neue
       Treiberkonzept einbindet
   13. cls entweder weg oder als Löschen des aktuellen Console-Windows
   14. alpha-Werte scheinen noch nicht zu tun, nochmal testen
   15. Vielleicht doch Turtle-Grafik bei draw hinzufügen. Das kann seine Daten
       entweder aus dem Speicher holen (dann Adresse angeben) oder direkt als
       String (dann in Anführungszeichen).
         Trennung: ; oder ,
         Präfix: B: Blank, N: No-update
	 U: Up, D: Down, R: Right, L: Left, E = Up+right, F= down+right,
	 G = down+left, H = up+left, Mx,y: absolute, M+-x,[+-]y: relative,
	 [n...]: repeat, evtl. #rrggbb: color-rgb, %rrggbbaa: color-rgba,
	 $(var): Environmentvariable einbetten, X(addr): Speicher einbetten
       Wenn man X implementiert, kann man sich evtl. oben die Variante mit
       Adresse sparen, weil man immer auch "draw turtle X(nnnnn)" sagen kann.
   16. Color-Map und cmap-Befehle implementieren
   17. Environment-Variablen für Display implementieren
   18. bei lcd panel 0 muss display ausgeschaltet werden.
   19. Befehl bminfo hinzufügen
   20. Befehl adraw hinzufügen dazu alle Ausgaberoutinen anpassen und
       ll-Routinen mit apply_alpha hinzufügen. Dazu braucht es eine
       get_pixel_rgba()-Funktion. Diese muss das Pixel maskieren und in die
       unteren Bits schieben, bevor sie col2rgba() aufruft. Dazu müssen dann
       die col2rgba-Funktionen definitiv nur das unterste Pixel zur Wandlung
       nutzen.
   21. Wenn es adraw gibt, kann ATTR_TRANSP weg. Denn dann muss man nur die
       BG-Color auf transparent stellen und kann dann mit adraw den gleichen
       Effekt erreichen. 
   22. draw bitmap anpassen (Parameter-Reihenfolge), evtl. keyword "bm" statt
       "bitmap", damit ähnlicher zu bminfo
   23. PNG-Analyse kommentieren (File-Aufbau, Filtering).
   24. Bei komplexen Zeichenoperationen zwischendurch Watchdog triggern
   25. draw_ll_row...() optimieren.
   26. lcd_ll_pixel() u.ä. umstellen, so dass Masken wie in draw_bitmap()
       verwendet werden. Das sieht praktisch aus als die ganze Geschichte mit
       x_shift und x_mask. lcd_ll_*() in draw_ll_*() und adraw_ll_*()
       umbenennen. 
   27. draw_ll_row_*() kann evtl. in eine Funktion zusammengefasst werden und
       nur das holen neuer row-Values in spezifische Unterfunktion row_fetch()
       auslagern. Auch BMP kann dann diese Funktion nutzen, mit eigenem
       row_fetch(). Entsprechend auch für adraw_ll_row_*(). row_fetch ist dann
       vermutlich nicht mehr von applyalpha abhängig. Mal noch schauen, ob
       applyalpha oder doch einfacher wieder attr in ii stehen sollte.
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

#if defined(CONFIG_CMD_BMP) || defined(CONFIG_SPLASH_SCREEN)
# include <bmp_layout.h>
# include <asm/byteorder.h>
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

#define DEFAULT_BG 0x000000FF		  /* Opaque black */
#define DEFAULT_FG 0xFFFFFFFF		  /* Opaque white */

#if (CONFIG_DISPLAYS > 1)
#define PRINT_WIN(vid, win) printf("display %u window %u: ", vid, win)
#else
#define PRINT_WIN(vid, win) printf("window %u: ", win)
#endif

/************************************************************************/
/* ENUMERATIONS								*/
/************************************************************************/

/* Settings that correspond with lcddraw keywords */
typedef enum DRAW_INDEX
{
	/* Draw commands with two coordinate pairs x1/y1 and x2/y2 */
	DI_LINE,
	DI_FRAME,
	DI_RECT,
	DI_X1Y1X2Y2 = DI_RECT,

	/* Draw commands with one coordinate pair x1/y1 */
	DI_PIXEL,
	DI_CIRCLE,
	DI_DISC,
	DI_TEXT,
	DI_BITMAP,
	DI_X1Y1 = DI_BITMAP,

	/* Draw commands with no coordinate pair */
	DI_COLOR,
	DI_FILL,
	DI_TEST,

	/* Unknown keyword (must be the last entry!) */
	DI_UNKNOWN
} drawindex_t;

/* Settings that correspond with window */
typedef enum WIN_INDEX {
	WI_DRAWTO,
	WI_FBRES,
	WI_RES,
	WI_OFFSET,
	WI_POS,
	WI_ALL,
	
	/* Unkown window subcommand; must be the last entry! */
	WI_UNKNOWN
} winindex_t;

/* Settings that correspond with lcd and reg set value keywords */
typedef enum SET_INDEX
{
	/* Settings not available with reg set val because they take more than
	   one argument or behave differently */
	LI_PANEL,
	LI_DIM,
	LI_RES,
	LI_HTIMING,
	LI_VTIMING,
	LI_POLARITY,
	LI_EXTRA,
	LI_PWM,
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
	LI_STRENGTH,
	LI_DITHER,
	LI_PWMENABLE,
	LI_PWMVALUE,
	LI_PWMFREQ,

#ifdef CONFIG_FSWINCE_COMPAT
	/* Settings only available with reg set value */
	LI_CONFIG,
#endif

	/* Unknown keyword (must be the last entry!) */
	LI_UNKNOWN
} setindex_t;




/************************************************************************/
/* TYPES AND STRUCTURES							*/
/************************************************************************/


/************************************************************************/
/* LOCAL VARIABLES							*/
/************************************************************************/

DECLARE_GLOBAL_DATA_PTR;

/* Currently selected window */
static VID vid_sel = 0;
static VID vid_count = 0;

vidinfo_t vidinfo[CONFIG_DISPLAYS];	  /* Current display information */

/* Size and base address of the framebuffer pool */
static fbpoolinfo_t fbpool = {
	base:	CFG_UBOOT_BASE - CONFIG_FBPOOL_SIZE,
	size:	CONFIG_FBPOOL_SIZE,
	used:	0
};

/* LCD types, corresponding to lcdinfo_t.type */
static char * const typeinfo[9] =
{
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

/* Bitmap types */
static const char * const btypeinfo[] =
{
	"???",				  /* BT_UNKNOWN */
	"PNG",				  /* BR_PNG */
	"BMP",				  /* BT_BMP */
	"JPG",				  /* BT_JPG ###not yet supported### */
};


/* Bitmap color types */
static const char * const ctypeinfo[] =
{
	"(unknown)",			  /* CT_UNKNOWN */
	"palette",			  /* CT_PALETTE */
	"grayscale",			  /* CT_GRAY */
	"grayscale+alpha",		  /* CT_GRAY_ALPHA */
	"truecolor",			  /* CT_TRUECOL */
	"truecolor+alpha",		  /* CT_TRUECOL_ALPHA */
};


/* Keywords available with lcddraw */
static kwinfo_t const draw_kw[] = {
	{4, 5, DI_LINE,   "line"},	  /* x1 y1 x2 y2 [rgba] */
	{4, 5, DI_FRAME,  "frame"},	  /* x1 y1 x2 y2 [rgba] */
	{4, 6, DI_RECT,   "rectangle"},	  /* x1 y1 x2 y2 [rgba [rgba]] */
	{2, 3, DI_PIXEL,  "pixel"},	  /* x1 y1 [rgba] */
	{3, 4, DI_CIRCLE, "circle"},	  /* x1 y1 r [rgba] */
	{3, 5, DI_DISC,   "disc"},	  /* x1 y1 r [rgba [rgba]] */
	{3, 6, DI_TEXT,   "text"},	  /* x1 y1 text [attr [rgba [rgba]]] */
	{3, 4, DI_BITMAP, "bitmap"},	  /* x1 y1 addr [attr]*/
	{1, 2, DI_COLOR,  "color"},	  /* [rgba [rgba]] */
	{0, 1, DI_FILL,   "fill"},	  /* [rgba] */
	{0, 1, DI_TEST,   "test"},	  /* [n] */
	{0, 0, DI_UNKNOWN,"help"},	  /* (no arguments, show usage) */
};


/* Keywords available with window */
static kwinfo_t const win_kw[] = {
	{1, 2, WI_DRAWTO, "drawto"},	  /* fbdraw [fbshow] */
	{1, 4, WI_FBRES,  "fbresolution"},/* fbhres fbvres [pix [fbcount]] */
	{2, 2, WI_RES,    "resolution"},  /* hres vres */
	{2, 2, WI_OFFSET, "offset"},	  /* hoffs voffs */
	{2, 2, WI_POS,    "position"},	  /* hpos vpos */
	{0, 0, WI_ALL,    "all"},	  /* (no arguments) */
	{0, 0, WI_UNKNOWN,"help"},	  /* (no arguments, show usage) */
};

/* Keywords available with lcd */
static kwinfo_t const lcd_kw[] = {
	/* Multiple arguments or argument with multiple types */
	{1, 1, LI_PANEL,     "panel"},	  /* index | substring */
	{2, 2, LI_DIM,       "dimension"},/* hdim vdim */
	{2, 2, LI_RES,	     "resolution"}, /* hres, vres */
	{1, 3, LI_HTIMING,   "htiming"},  /* hfp [hsw [hbp]] */
	{1, 3, LI_VTIMING,   "vtiming"},  /* vfp [vsw [vbp]] */
	{1, 4, LI_POLARITY,  "polarity"}, /* hspol [vspol [clkpol [denpol]]] */
	{1, 3, LI_PWM,       "pwm"},	  /* pwmenable [pwmvalue [pwmfreq]] */
	{1, 2, LI_EXTRA,     "extra"},	  /* strength [dither] */
	{0, 2, LI_LIST,      "list"},	  /* [index | substring [count]] */
	{0, 0, LI_ON,        "on"},	  /* (no arguments) */
	{0, 0, LI_OFF,       "off"},	  /* (no arguments) */
#if (CONFIG_DISPLAYS > 1)
	{0, 0, LI_ALL,       "all"},	  /* (no arguments) */
#endif
	{0, 0, LI_UNKNOWN,   "help"},	  /* (no arguments, show usage) */

	/* Single argument, LI_NAME must be the first one. */
	{1, 1, LI_NAME,      "name"},	  /* name */
	{1, 1, LI_HDIM,      "hdim"},	  /* hdim */
	{1, 1, LI_VDIM,      "vdim"},	  /* vdim */
	{1, 1, LI_TYPE,      "type"},	  /* type */
	{1, 1, LI_HFP,       "hfp"},	  /* hfp */
	{1, 1, LI_HFP,       "elw"},	  /* hfp */
	{1, 1, LI_HFP,       "rightmargin"}, /* hfp */
	{1, 1, LI_HSW,       "hsw"},	  /* hsw */
	{1, 1, LI_HBP,       "hbp"},	  /* hbp */
	{1, 1, LI_HBP,       "blw"},	  /* hbp */
	{1, 1, LI_HBP,       "leftmargin"}, /* hbp */
	{1, 1, LI_HRES,      "hres"},	  /* hres */
	{1, 1, LI_VFP,       "vfp"},	  /* vfp */
	{1, 1, LI_VFP,       "efw"},	  /* vfp */
	{1, 1, LI_VFP,       "lowermargin"}, /* vfp */
	{1, 1, LI_VSW,       "vsw"},	  /* vsw */
	{1, 1, LI_VBP,       "vbp"},	  /* vbp */
	{1, 1, LI_VBP,       "bfw"},	  /* vbp */
	{1, 1, LI_VBP,       "uppermargin"}, /* vbp */
	{1, 1, LI_VRES,      "vres"},	  /* vres */
	{1, 1, LI_HSPOL,     "hspol"},	  /* hspol */
	{1, 1, LI_VSPOL,     "vspol"},	  /* hspol */
	{1, 1, LI_DENPOL,    "denpol"},	  /* denpol */
	{1, 1, LI_CLKPOL,    "clkpol"},	  /* clkpol */
	{1, 1, LI_CLK,       "clk"},	  /* clk */
	{1, 1, LI_FPS,       "fps"},	  /* fps */
	{1, 1, LI_STRENGTH,  "strength"}, /* strength */
	{1, 1, LI_DITHER,    "dither"},	  /* dither */
	{1, 1, LI_DITHER,    "frc"},	  /* dither */
	{1, 1, LI_PWMENABLE, "pwmenable"}, /* pwmenable */
	{1, 1, LI_PWMVALUE,  "pwmvalue"}, /* pwmvalue */
	{1, 1, LI_PWMFREQ,   "pwmfreq"},  /* pwmfreq */
};

#ifdef CONFIG_FSWINCE_COMPAT
static kwinfo_t const reg_kw[] =
{
	{4, 4, LI_NAME,      "name"},	  /* name */
	{4, 4, LI_HDIM,      "width"},	  /* hdim */
	{4, 4, LI_VDIM,      "height"},	  /* vdim */
	{4, 4, LI_TYPE,      "type"},	  /* type */
	{4, 4, LI_HFP,       "elw"},	  /* hfp */
	{4, 4, LI_HSW,       "hsw"},	  /* hsw */
	{4, 4, LI_HBP,       "blw"},	  /* hbp */
	{4, 4, LI_HRES,      "columns"},  /* hres */
	{4, 4, LI_VFP,       "efw"},	  /* vfp */
	{4, 4, LI_VSW,       "vsw"},	  /* vsw */
	{4, 4, LI_VBP,       "bfw"},	  /* vbp */
	{4, 4, LI_VRES,      "rows"},	  /* vres */
	{4, 4, LI_CONFIG,    "config"},	  /* hspol/vspol/clkpol/denpol */
	{4, 4, LI_CLK,       "lcdclk"},	  /* clk */
	{4, 4, LI_STRENGTH,  "lcdportdrivestrength"}, /* strength */
	{4, 4, LI_PWMENABLE, "contrastenable"},	/* pwmenable */
	{4, 4, LI_PWMVALUE,  "contrastvalue"}, /* pwmvalue */
	{4, 4, LI_PWMFREQ,   "contrastfreq"}, /* pwmfreq */
};
#endif /*CONFIG_FSWINCE_COMPAT*/

/************************************************************************/
/* Prototypes of local functions					*/
/************************************************************************/
/* Get pointer to current lcd panel information */
static vidinfo_t *lcd_get_vidinfo_p(VID vid);

/* Get pointer to current window information */
static wininfo_t *lcd_get_wininfo_p(const vidinfo_t *pvi, WINDOW win);


/************************************************************************/
/* Command cls								*/
/************************************************************************/

#if 0 //####
static int cls(cmd_tbl_t *cmdtp, int flag, int argc, char *argv[])
{
	wininfo_t *pwi;

#if 0 //######
#if LCD_BPP == LCD_MONOCHROME
	/* Setting the palette */
	lcd_initcolregs();

#elif LCD_BPP == LCD_COLOR8
	/* Setting the palette */
	lcd_setcolreg  (CONSOLE_COLOR_BLACK,       0,    0,    0);
	lcd_setcolreg  (CONSOLE_COLOR_RED,	0xFF,    0,    0);
	lcd_setcolreg  (CONSOLE_COLOR_GREEN,       0, 0xFF,    0);
	lcd_setcolreg  (CONSOLE_COLOR_YELLOW,	0xFF, 0xFF,    0);
	lcd_setcolreg  (CONSOLE_COLOR_BLUE,        0,    0, 0xFF);
	lcd_setcolreg  (CONSOLE_COLOR_MAGENTA,	0xFF,    0, 0xFF);
	lcd_setcolreg  (CONSOLE_COLOR_CYAN,	   0, 0xFF, 0xFF);
	lcd_setcolreg  (CONSOLE_COLOR_GRAY,	0xAA, 0xAA, 0xAA);
	lcd_setcolreg  (CONSOLE_COLOR_WHITE,	0xFF, 0xFF, 0xFF);
#endif

#ifndef CFG_WHITE_ON_BLACK
	lcd_setfgcolor (CONSOLE_COLOR_BLACK);
	lcd_setbgcolor (CONSOLE_COLOR_WHITE);
#else
	lcd_setfgcolor (CONSOLE_COLOR_WHITE);
	lcd_setbgcolor (CONSOLE_COLOR_BLACK);
#endif	/* CFG_WHITE_ON_BLACK */

#ifdef	LCD_TEST_PATTERN
	test_pattern();
#else
	printf("####Z\n");

	printf("####lcd_getbgcolor=%d, lcd_line_length=%d, vl_row=%d\n",
	       lcd_getbgcolor(), lcd_line_length, panel_info.vl_row);
	/* set framebuffer to background color */
	memset ((char *)lcd_base,
		COLOR_MASK(lcd_getbgcolor()),
		lcd_line_length*panel_info.vl_row);
	printf("####Y\n");
#endif
	/* Paint the logo and retrieve LCD base address */
	debug ("[LCD] Drawing the logo...\n");
	lcd_console_address = lcd_logo ();
	printf("####X\n");
#endif //0 ####

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
/* Command lcdclut							*/
/************************************************************************/

static int lcdclut(cmd_tbl_t *cmdtp, int flag, int argc, char *argv[])
{
	printf("#### lcdclut not yet implemented ####\n");
	return 1;
}

U_BOOT_CMD(
	lcdclut, 6,	1,	lcdclut,
	"lcdclut\t- set or read the color look-up table (CLUT)\n",
	NULL
);


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
				PRINT_WIN(vid, win);
				printf("relocated buffer %u from 0x%08lx to"
				       " 0x%08lx\n",
				       buf, pwi->pfbuf[buf], newaddr);
				pwi->pfbuf[buf] = newaddr;
			}
			if (buf < pwi->fbcount)
				newaddr += pwi->fbsize;
		}

		/* Update controller hardware with new info */
		pvi->set_wininfo(pwi);
		if (win+1 < pvi->wincount)
			pwi++;		  /* Next window */
		else if (++vid < vid_count) {
			pvi++;		  /* Next display */
			pwi = pvi->pwi;	  /* First window */
		}
	} while (vid < vid_count);
}

static int lcdfbpool(cmd_tbl_t *cmdtp, int flag, int argc, char *argv[])
{
	fbpoolinfo_t *pfp = &fbpool;

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
	return 0;
}

U_BOOT_CMD(
	fbpool,	5,	1,	lcdfbpool,
	"fbpool\t- set framebuffer pool\n",
	"<size> <address>\n"
	"    - set framebuffer pool of <size> at <address>\n"
	"fbpool <size>\n"
	"    - set framebuffer pool of <size> immediately before U-Boot\n"
	"fbpool\n"
	"    - show framebuffer pool settings"
);


/************************************************************************/
/* Command bminfo							*/
/************************************************************************/

static int bminfo(cmd_tbl_t *cmdtp, int flag, int argc, char *argv[])
{
	u_int base, addr;
	u_int i;

	if (argc < 2) {
		printf("Missing argument\n");
		return 1;
	}

	/* Get base address */
	base = simple_strtoul(argv[1], NULL, 16);

	/* Print header line for bitmap info list */
	printf("#\tOffset\t\thres x vres\tbpp\tType\tCIB\tInfo\n");
	printf("--------------------------------------------"
	       "---------------------------\n");
	addr = base;
	for (i=0; ; i++)
	{
		bminfo_t bi;
		u_int bmaddr = addr;

		/* Scan bitmap structure and get end of current bitmap; stop
		   on error, this is usually the end of the (multi-)image */
		addr = lcd_scan_bitmap(addr);
		if (!addr)
			break;

		/* Get bitmap info and show it */
		lcd_get_bminfo(&bi, bmaddr);
		printf("%d\t0x%08x\t%4d x %d\t%d\t%s\t%c%c%c\t%s\n",
		       i, bmaddr - base, bi.hres, bi.vres, bi.bitdepth,
		       btypeinfo[bi.type],
		       (bi.flags & BF_COMPRESSED) ? 'C' : '-',
		       (bi.flags & BF_INTERLACED) ? 'I' : '-',
		       (bi.flags & BF_BOTTOMUP) ? 'B' : '-', 
		       ctypeinfo[bi.colortype]);
	}
	if (!i)
		puts("(no bitmap found)\n");

	return 0;
}

U_BOOT_CMD(
	bminfo, 2,	0,	bminfo,
	"bminfo\t- show (multi-)bitmap information in a list\n",
	"addr\n"
	"    - show information about bitmap(s) stored at addr\n"
);


/************************************************************************/
/* Command draw								*/
/************************************************************************/

static int draw(cmd_tbl_t *cmdtp, int flag, int argc, char *argv[])
{
	wininfo_t *pwi;
	const vidinfo_t *pvi;
	u_short sc;
	XYPOS x1 = 0, y1 = 0, x2 = 0, y2 = 0;
	RGBA rgba;
	COLOR32 col1, col2;
	u_int a;			  /* Attribute */
	int colindex = 2;

	pvi = lcd_get_vidinfo_p(vid_sel);
	pwi = lcd_get_wininfo_p(pvi, pvi->win_sel);

	if (argc < 2) {
		PRINT_WIN(vid_sel, pvi->win_sel);
		printf("FG #%08x, BG #%08x\n",
		       pwi->ppi->col2rgba(pwi, pwi->fg),
		       pwi->ppi->col2rgba(pwi, pwi->bg));
		return 0;
	}
		       
	/* Search for keyword in draw keyword list */
	sc = parse_sc(argc, argv[1], DI_UNKNOWN, draw_kw, ARRAYSIZE(draw_kw));

	/* Print usage if command not valid */
	if (sc == DI_UNKNOWN) {
		printf("usage:\n%s ", cmdtp->name);
		puts(cmdtp->help);
		putc('\n');
		return 1;
	}

	/* If selected window is not active do nothing */
	if (!pwi->active) {
		PRINT_WIN(vid_sel, pvi->win_sel);
		puts("selected window not active\n");
		return 1;
	}

	/* Parse one or two coordinate pairs for commands with coordinates */
	if (sc <= DI_X1Y1) {
		x1 = (XYPOS)simple_strtol(argv[2], NULL, 0);
		y1 = (XYPOS)simple_strtol(argv[3], NULL, 0);
		colindex += 2;

		if (sc <= DI_X1Y1X2Y2) {
			x2 = (XYPOS)simple_strtol(argv[4], NULL, 0);
			y2 = (XYPOS)simple_strtol(argv[5], NULL, 0);
			colindex += 2;
		}
	}

	/* Parse one or two optional colors for commands who may have colors */
	switch (sc) {
	case DI_TEXT:			  /* Color begins in arg[6] */
		colindex++;
		/* Fall through to case DI_CIRCLE */
	case DI_CIRCLE:			  /* Color begins in arg[5] */
	case DI_DISC:			  /* Color begins in arg[5] */
	case DI_BITMAP:			  /* Move index beyond argc_max */
	case DI_TEST:			  /* Move index beyond argc_max */
		colindex++;
		/* fall through to default */
	default:
		break;
	}
	col1 = pwi->fg;
	col2 = pwi->bg;
	if (argc > colindex) {
		/* Parse first color */
		if (parse_rgb(argv[colindex], &rgba) == RT_NONE)
			return 1;
		col1 = pwi->ppi->rgba2col(pwi, rgba);
		colindex++;
		if (argc > colindex) {
			/* Parse second color */
			if (parse_rgb(argv[colindex], &rgba) == RT_NONE)
				return 1;
			col2 = pwi->ppi->rgba2col(pwi, rgba);
		}
	}

	/* Finally execute the drawing command */
	switch (sc) {
	case DI_PIXEL:			  /* Draw pixel */
		lcd_pixel(pwi, x1, y1, col1);
		break;

	case DI_LINE:			  /* Draw line */
		lcd_line(pwi, x1, y1, x2, y2, col1);
		break;

	case DI_RECT:			  /* Draw filled rectangle */
		lcd_rect(pwi, x1, y1, x2, y2, col1);
		if ((argc < 8) || (col1 == col2))
			break;
		col1 = col2;
		/* Fall through to case DI_FRAME */

	case DI_FRAME:			  /* Draw rectangle outline */
		lcd_frame(pwi, x1, y1, x2, y2, col1);
		break;

	case DI_CIRCLE:			  /* Draw circle outline */
	case DI_DISC:			  /* Draw filled circle */
		x2 = (XYPOS)simple_strtol(argv[4], NULL, 0); /* Parse radius */
		if (sc == DI_DISC) {
			lcd_disc(pwi, x1, y1, x2, col1);
			if ((argc < 7) || (col1 == col2))
				break;
			col1 = col2;
		}
		lcd_circle(pwi, x1, y1, x2, col1);
		break;
			
	case DI_TEXT:			  /* Draw text */
		/* Optional argument 4: attribute */
		a = (argc > 5) ? simple_strtoul(argv[5], NULL, 0) : 0;
		lcd_text(pwi, x1, y1, argv[4], a, col1, col2);
		break;

	case DI_BITMAP:			  /* Draw bitmap */
	{
		u_int addr;
		const char *errmsg;

		/* Argument 3: address */
		addr = simple_strtoul(argv[4], NULL, 0);

		/* Optional argument 4: bitmap number (for multi bitmaps) */
		if (argc > 5) {
			u_int n;

			for (n = simple_strtoul(argv[5], NULL, 0); n; n--) {
				addr = lcd_scan_bitmap(addr);
				if (!addr) {
					printf("Bitmap %d not found\n", n);
					return 1;
				}
			}
		}

		/* Optional argument 5: attribute */
		a = (argc > 6) ? simple_strtoul(argv[6], NULL, 0) : 0;
		errmsg = lcd_bitmap(pwi, x1, y1, addr, a);
		if (errmsg) {
			puts(errmsg);
			return 1;
		}
		break;
	}

	case DI_COLOR:			  /* Set FG and BG color */
		pwi->bg = col2;
		break;

	case DI_FILL:			  /* Fill window with color */
		lcd_fill(pwi, col1);
		break;

	case DI_TEST:			  /* Draw test pattern */
		a = (argc > 2) ? simple_strtoul(argv[2], NULL, 0) : 0;
		lcd_test(pwi, a);
		break;

	default:			  /* Should not happen */
		printf("Unhandled draw command '%s'\n", argv[1]);
		return 1;
	}
	pwi->fg = col1;			  /* Set FG color */

	return 0;
}

U_BOOT_CMD(
	draw, 8,	0,	draw,
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
	"draw bitmap x y addr [n [a]]\n"
	"    - draw bitmap n from addr at (x, y) with attribute a\n"
	"draw fill [#rgba]\n"
	"    - fill window with color\n"
	"draw test [n]\n"
	"    - draw test pattern n\n"
	"draw color #rgba [#rgba]\n"
	"    - set FG (and BG) color\n"
	"draw\n"
	"    - show current FG and BG color\n"
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
		PRINT_WIN(pvi->vid, win);
		printf("bad pixel format '%u'\n", pix);
		return 1;
	}

	/* Check if framebuffer count is valid for this window */
	if (fbcount > pwi->fbmaxcount) {
		PRINT_WIN(pvi->vid, win);
		printf("bad image buffer count '%u'\n", fbcount);
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
	pwi->fbcount = fbcount;
	pwi->linelen = linelen;
	pwi->fbsize = fbsize;
	if (pwi->pix != pix) {
		/* New pixel format: set default bg + fg */
		pwi->bg = ppi->rgba2col(pwi, DEFAULT_BG);
		pwi->fg = ppi->rgba2col(pwi, DEFAULT_FG);
	}
	pwi->pix = pix;
	fix_offset(pwi);

	/* If size changed, move framebuffers of all subsequent windows and
	   change framebuffer pool info (used amount) */
	addr = pwi->pfbuf[0];
	if (oldsize == newsize)
		pvi->set_wininfo(pwi);	  /* Only update current window */
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
	memset32((unsigned *)addr, pwi->bg, newsize/4);

	/* The console might also be interested in the buffer changes */
	console_update(pwi);

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
		pwi->pvi->set_wininfo(pwi);
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
	const vidinfo_t *pvi = pwi->pvi;
	u_int buf;

	PRINT_WIN(pvi->vid, pwi->win);
	printf("\nFramebuffer:\t%d x %d pixels, %lu bytes (%lu bytes/line)\n",
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

	/* Call the print function for additional window sub-commands */
	if (pvi->winext_show)
		pvi->winext_show(pwi);

	putc('\n');
}

/* Handle window command */
static int window(cmd_tbl_t *cmdtp, int flag, int argc, char *argv[])
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
	sc = parse_sc(argc, argv[1], WI_UNKNOWN, win_kw, ARRAYSIZE(win_kw));
	if ((sc == WI_UNKNOWN) && pvi->winext_parse) {
		/* Search for extension sub-commands; these must have
		   a value >= WI_UNKNOWN */
		sc = pvi->winext_parse(argc, argv[1]) + WI_UNKNOWN;
	}

	/* If not recognized, print usage and return */
	if (sc == WI_UNKNOWN) {
		printf("usage:\n%s ", cmdtp->name);
		puts(cmdtp->help);

		/* Call display specific extended help */
		if (pvi->winext_help)
			pvi->winext_help();
		putc('\n');
		return 1;
	}

	switch (sc) {
	case WI_DRAWTO: {
		u_char fbdraw, fbshow;

		/* Argument 1: buffer number to draw to */
		fbdraw = (u_char)simple_strtoul(argv[2], NULL, 0);
		if (fbdraw >= pwi->fbcount) {
			printf("Bad image buffer '%u'\n", fbdraw);
			return 1;
		}

		/* Argument 2: buffer number to show */
		fbshow = pwi->fbshow;
		if (argc == 4) {
			fbshow = (u_char)simple_strtoul(argv[3], NULL, 0);
			if (fbshow >= pwi->fbcount) {
				printf("Bad image buffer '%u'\n",
				       fbshow);
				return 1;
				}
		}

		/* Set new values */
		pwi->fbdraw = fbdraw;
		pwi->fbshow = fbshow;
		pvi->set_wininfo(pwi);
		break;
	}

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

	case WI_RES: {
		HVRES hres, vres;

		/* Arguments 1+2: hres and vres */
		hres = (HVRES)simple_strtoul(argv[2], NULL, 0);
		vres = (HVRES)simple_strtoul(argv[3], NULL, 0);

		/* Set window resolution */
		return set_winres(pwi, hres, vres);
	}

	case WI_OFFSET:
		/* Arguments 1+2: hoffs and voffs (non-negative) */
		pwi->hoffs = pvi->align_hoffs(pwi,
				      (XYPOS)simple_strtoul(argv[2], NULL, 0));
		pwi->voffs = (XYPOS)simple_strtoul(argv[3], NULL, 0);
		fix_offset(pwi);
		pvi->set_wininfo(pwi);
		break;

	case WI_POS:
		/* Arguments 1+2: hpos and vpos, may be negative */
		pwi->hpos = (XYPOS)simple_strtol(argv[2], NULL, 0);
		pwi->vpos = (XYPOS)simple_strtol(argv[3], NULL, 0);
		pvi->set_wininfo(pwi);
		break;

	case WI_ALL:
		for (win = 0; win < pvi->wincount; win++)
			show_wininfo(lcd_get_wininfo_p(pvi, win));
		break;

        default:
		/* This is one of the extensions */
		if (pvi->winext_exec
		    && pvi->winext_exec(pwi, argc, argv, sc - WI_UNKNOWN))
			return 1;
		break;
	}

	return 0;
}

/* Remark: we can only show the main set of sub-commands here; any hardware
   specific extensions can only be shown with "window help" as this calls
   vidinfo_t.winext_help() in addition to this text. */
U_BOOT_CMD(
	window,	7,	0,	window,
	"window\t- set framebuffer and overlay window parameters\n",
	"n\n"
	"    - Select window n\n"
	"drawto fbdraw [fbshow]\n"
	"    - Set the buffer to draw to and to show\n"
	"window fbresolution [fbhres fbvres [pix [fbcount]]]\n"
	"    - Set virtual framebuffer resolution, pixel format, buffer count\n"
	"window resolution [hres vres]\n"
	"    - Set window resolution (i.e. OSD size)\n"
	"window offset [hoffs voffs]\n"
	"    - Set window offset within virtual framebuffer\n"
	"window position [x y]\n"
	"    - Set window position on display\n"
	"window all\n"
	"    - List all windows of the current display\n"
	"window help\n"
	"    - Show extended help information\n"
	"window\n"
	"    - Show all settings of selected window\n"
);


/************************************************************************/
/* Command lcd								*/
/************************************************************************/

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
		pvi->lcd.hspol = (param != 0);
		break;

	case LI_VSPOL:			  /* Parse flag (u_char 0..1) */
		pvi->lcd.vspol = (param != 0);
		break;

	case LI_DENPOL:			  /* Parse flag (u_char 0..1) */
		pvi->lcd.denpol = (param != 0);
		break;

	case LI_CLKPOL:			  /* Parse flag (u_char 0..1) */
		pvi->lcd.clkpol = (param != 0);
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
		pvi->lcd.fps = (u_int)param;
		pvi->lcd.clk = 0;	  /* Compute clk from fps */
		break;

	case LI_STRENGTH:		  /* Parse u_char */
		pvi->lcd.strength = (u_char)param;
		break;

	case LI_DITHER:			  /* Parse flag (u_char 0..1) */
		pvi->lcd.dither = (param != 0);
		break;

	case LI_PWMENABLE:		  /* Parse flag (u_char 0..1) */
		pvi->lcd.pwmenable = (param != 0);
		break;

	case LI_PWMVALUE:		  /* Parse u_int */
		pvi->lcd.pwmvalue = (u_int)param;
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
	/* Show LCD panel settings */
	printf("Display Name:\tname='%s'\n", pvi->lcd.name);
	printf("Dimensions:\thdim=%umm, vdim=%umm\n",
	       pvi->lcd.hdim, pvi->lcd.vdim);
	printf("Display Type:\ttype=%u (%s)\n",
	       pvi->lcd.type, typeinfo[pvi->lcd.type]);
	printf("Display Size:\thres=%u, vres=%u\n",
	       pvi->lcd.hres, pvi->lcd.vres);
	printf("Horiz. Timing:\thfp=%d, hsw=%d, hbp=%d\n",
	       pvi->lcd.hfp, pvi->lcd.hsw, pvi->lcd.hbp);
	printf("Vert. Timing:\tvfp=%d, vsw=%d, vbp=%d\n",
	       pvi->lcd.vfp, pvi->lcd.vsw, pvi->lcd.vbp);
	printf("Polarity:\thspol=%d, vspol=%d, clkpol=%d, denpol=%d\n",
	       pvi->lcd.hspol, pvi->lcd.vspol, pvi->lcd.clkpol,
	       pvi->lcd.denpol);
	printf("Display Timing:\tclk=%dHz (%dfps)\n",
	       pvi->lcd.clk, pvi->lcd.fps);
	printf("Extra Settings:\tstrength=%d, dither=%d\n",
	       pvi->lcd.strength, pvi->lcd.dither);
	printf("PWM:\t\tpwmenable=%d, pwmvalue=%d, pwmfreq=%dHz\n",
	       pvi->lcd.pwmenable, pvi->lcd.pwmvalue, pvi->lcd.pwmfreq);

	/* Show driver settings */
	printf("Display driver:\t%s, %u windows, %u pixel formats\n",
	       pvi->driver_name, pvi->wincount, pvi->pixcount);
	printf("State:\t\tdisplay is switched %s\n\n",
	       pvi->is_enabled ? "ON" : "OFF");
}

static int lcd(cmd_tbl_t *cmdtp, int flag, int argc, char *argv[])
{
	vidinfo_t *pvi;
	u_short sc;
	char c;
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
	c = argv[1][0];
	if ((c >= '0') && (c <= '9')) {
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
	sc = parse_sc(argc, argv[1], LI_UNKNOWN, lcd_kw, ARRAYSIZE(lcd_kw));

	/* If not recognized, print usage and return */
	if (sc == LI_UNKNOWN) {
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
		char *pReason = NULL;

		if (!pvi->lcd.hres || !pvi->lcd.vres)
			pReason = "No valid lcd panel\n";
		else {
			WINDOW win;
			wininfo_t *pwi;

			for (win=0, pwi=pvi->pwi; win<pvi->wincount;
			     win++, pwi++) {
				if (pwi->active)
					break;
			}
			if (win >= pvi->wincount)
				pReason = "No active window\n";
			else if (pvi->enable())
				pReason = "Can't enable display\n";
		}
		if (pReason) {
			puts(pReason);
			return 1;
		}
		pvi->is_enabled = 1;
		return 0;
	}

	case LI_OFF:
		pvi->disable();
		pvi->is_enabled = 0;
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
			if ((c >= '0') & (c <= '9')) {
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

		c = argv[2][0];
		if ((c >= '0') && (c <= '9')) {
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
		param = simple_strtoul(argv[3], NULL, 0);
		pvi->lcd.hdim = (u_short)param;
		param = simple_strtoul(argv[4], NULL, 0);
		pvi->lcd.vdim = (u_short)param;
		break;

	case LI_RES:		  /* Parse exactly 2 HVRES'es */
		param = simple_strtoul(argv[3], NULL, 0);
		pvi->lcd.hres = (HVRES)param;
		param = simple_strtoul(argv[4], NULL, 0);
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

	case LI_POLARITY:	  /* Parse up to 4 flags */
		param = simple_strtoul(argv[2], NULL, 0);
		pvi->lcd.hspol = (param != 0);
		if (argc < 4)
			break;
		param = simple_strtoul(argv[3], NULL, 0);
		pvi->lcd.vspol = (param != 0);
		if (argc < 5)
			break;
		param = simple_strtoul(argv[4], NULL, 0);
		pvi->lcd.denpol = (param != 0);
		if (argc < 6)
			break;
		param = simple_strtoul(argv[5], NULL, 0);
		pvi->lcd.clkpol = (param != 0);
		break;

	case LI_EXTRA:		  /* Parse number + flag */
		param = simple_strtoul(argv[2], NULL, 0);
		pvi->lcd.strength = (u_char)param;
		if (argc < 4)
			break;
		param = simple_strtoul(argv[3], NULL, 0);
		pvi->lcd.dither = (param != 0);
		break;

	case LI_PWM:		  /* Parse flag + 2 numbers */
		param = simple_strtoul(argv[2], NULL, 0);
		pvi->lcd.pwmenable = (param != 0);
		if (argc < 4)
			break;
		param = simple_strtoul(argv[3], NULL, 0);
		pvi->lcd.pwmvalue = (u_int)param;
		if (argc < 5)
			break;
		param = simple_strtoul(argv[4], NULL, 0);
		pvi->lcd.pwmfreq = (u_int)param;
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
	pvi->set_vidinfo(pvi);

	return 0;
}

U_BOOT_CMD(
	lcd,	7,	0,	lcd,
	"lcd\t- set lcd panel parameters\n",
	"list\n"
	"    - list all predefined lcd panels\n"
	"lcd list substring [count]\n"
	"    - list at most count panel entries that match substring\n"
	"lcd list index [count]\n"
	"    - list at most count panel entries starting at index\n"
	"lcd panel n\n"
	"    - set the predefined lcd panel n\n"
	"lcd panel substring\n"
	"    - set the predefined panel that matches substring\n"
	"lcd param value\n"
	"    - set the lcd parameter param, which is one of:\n"
	"\tname, hdim, vdim, hres, vres, hfp, hsw, hpb, vfp, vsw, vbp,\n"
	"\thspol, vspol, denpol, clkpol, clk, fps, type, pixel, strength,\n"
	"\tdither, pwmenable, pwmvalue, pwmfreq\n"
	"lcd group {values}\n"
	"    - set the lcd parameter group, which is one of:\n"
	"\tdimension, resolution, htiming, vtiming, polarity, pwm, extra\n"
	"lcd on\n"
	"    - activate lcd\n"
	"lcd off\n"
	"    - deactivate lcd\n"
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
static int reg(cmd_tbl_t *cmdtp, int flag, int argc, char *argv[])
{
	if ((argc == 6)
	    && (strcmp(argv[1], "set") == 0)
	    && (strncmp(argv[2], "value", strlen(argv[2])) == 0)) {
		u_int param;
		u_short sc;
		vidinfo_t *pvi;
		sc = parse_sc(argc, argv[3], LI_UNKNOWN, reg_kw,
			      ARRAYSIZE(reg_kw));

		pvi = lcd_get_vidinfo_p(vid_sel);
		switch (sc) {
		case LI_TYPE:
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

		case LI_CONFIG:
			/* "config" is not available with lcdset */
			param = simple_strtoul(argv[5], NULL, 0);
			pvi->lcd.vspol = ((param & 0x00100000) != 0);
			pvi->lcd.hspol = ((param & 0x00200000) != 0);
			pvi->lcd.clkpol = ((param & 0x00400000) != 0);
			pvi->lcd.denpol = ((param & 0x00800000) != 0);
			break;

		default:
			/* All other commands have a counterpart in
			   lcdset, use common function */
			if (set_value(pvi, argv[5], sc))
				return 1;
			break;
		}

		/* Update hardware with new settings */
		pvi->set_vidinfo(pvi);
	}

	return 0;
}

U_BOOT_CMD(
	reg,	6,	0,	reg,
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
static int ignore(cmd_tbl_t *cmdtp, int flag, int argc, char *argv[])
{
	return 0;			  /* Always succeed */
}

U_BOOT_CMD(
	display,	4,	0,	ignore,
	"display\t- Ignored (F&S WinCE compatibility)\n",
	"...\n"
	"    - Ignored, but accepted for compatibility reasons\n"
);

U_BOOT_CMD(
	contrast,	3,	0,	ignore,
	"contrast\t- Ignored (F&S WinCE compatibility)\n",
	"...\n"
	"    - Ignored, but accepted for compatibility reasons\n"
);

U_BOOT_CMD(
	reboot,		2,	0,	ignore,
	"reboot\t- Ignored (F&S WinCE compatibility)\n",
	"...\n"
	"    - Ignored, but accepted for compatibility reasons\n"
);
#endif /*CONFIG_FSWINCE_COMPAT*/


/************************************************************************/
/* Exported functions							*/
/************************************************************************/

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

#if 0 //#####
void lcd_getwininfo(wininfo_t *pwi, WINDOW win)
{
	*pwi = wininfo[win];
}

void lcd_setwininfo(wininfo_t *pwi, WINDOW win)
{
	/* Set the values to hardware; this updates wininfo[win] */
	lcd_hw_wininfo(pwi, &vidinfo);
}
#endif //#####

/* Return RT_RGB for #rrggbb, RT_RGBA for #rrggbbaa and RT_NONE otherwise */
enum RGB_TYPE parse_rgb_type(char *s)
{
	/* Color must start with '#' */
	if (*s == '#') {
		char *ep;

		s++;
		simple_strtoul(s, &ep, 16);
		if (*ep == 0) {
			/* No parse error, all digits are hex digits */
			switch (ep-s) {
			case 6:
				return RT_RGB;  /* 6 digits are RGB */
			case 8:
				return RT_RGBA; /* 8 digits are RGBA */
			default:
				break;	        /* other length fails */
			}
		}
	}
	return RT_NONE;
}

enum RGB_TYPE parse_rgb(char *s, u_int *prgba)
{
	enum RGB_TYPE rt;
	u_int rgba;

	rt = parse_rgb_type(s);
	if (rt == RT_NONE)
		printf("Bad color '%s'\n", s);
	else {
		rgba = simple_strtoul(s+1, NULL, 16);
		if (rt == RT_RGB)
			rgba = (rgba << 8) | 0xFF;
		*prgba = rgba;
	}

	return rt;
}


/* Parse the given keyword table for a sub-command and check for required
   argument count. Return new sub-command index or sc on error */
u_short parse_sc(int argc, char *s, u_short sc, const kwinfo_t *pki,
		 u_short count)
{
	size_t len = strlen(s);

	for ( ; count; count--, pki++) {
		if (strncmp(pki->keyword, s, len) == 0) {
			if (argc > pki->argc_max+2)
				break;	  /* Too many arguments */
			if (argc >= pki->argc_min+2)
				sc = pki->sc; /* OK */
			else
				puts("Missing argument\n");
			break;
		}
	}
	return sc;
}


const fbpoolinfo_t *lcd_get_fbpoolinfo_p(void)
{
	return &fbpool;
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

	/* Set framebuffer pool base into global data */
	gd->fb_base = fbpool.base;
	memset(&lcddev, 0, sizeof(lcddev));

	printf("####a\n");

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

	printf("####b\n");

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
		pvi->lcd = *lcd_get_lcdinfo_p(0); /* Set panel #0 */
		
		printf("####c\n");
		pvi->set_vidinfo(pvi);
		printf("####d\n");

		/* Initialize the window entries; the fields defpix, pfbuf,
		   fbmaxcount and ext are already set by the controller
		   specific init function(s) above. */
		for (win = 0; win < pvi->wincount; win++) {
			unsigned buf;

			pwi = lcd_get_wininfo_p(pvi, win);

			printf("####e\n");

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
			pwi->fg = pwi->ppi->rgba2col(pwi, DEFAULT_FG);
			pwi->bg = pwi->ppi->rgba2col(pwi, DEFAULT_BG);
			pwi->cmap = NULL; //#####?!?!

			printf("####f\n");

			/* Update controller hardware with new wininfo */
			pvi->set_wininfo(pwi);
			printf("####g\n");

#ifdef CONFIG_MULTIPLE_CONSOLES
			/* Init a console device for each window */
#if (CONFIG_DISPLAYS > 1)
			sprintf(lcddev.name, "lcd%u_%u", vid, win);
#else
			sprintf(lcddev.name, "lcd%u", win);
#endif
			lcddev.ext   = 0;		  /* No extensions */
			lcddev.flags = DEV_FLAGS_OUTPUT;  /* Output only */
			lcddev.putc  = lcd_putc;	  /* 'putc' function */
			lcddev.puts  = lcd_puts;	  /* 'puts' function */
			lcddev.priv  = pwi;		  /* Call-back arg */
			device_register(&lcddev);
#endif /*CONFIG_MULTIPLE_CONSOLES*/
		}
	}

	printf("####h\n");

	/* Default console "lcd" on vid 0, win 0 */
	console_init(lcd_get_wininfo_p(lcd_get_vidinfo_p(0), 0));

	printf("####i\n");

	/* Device initialization */
	memset(&lcddev, 0, sizeof(lcddev));

	strcpy(lcddev.name, "lcd");
	lcddev.ext   = 0;		  /* No extensions */
	lcddev.flags = DEV_FLAGS_OUTPUT;  /* Output only */
	lcddev.putc  = lcd_putc;	  /* 'putc' function */
	lcddev.puts  = lcd_puts;	  /* 'puts' function */

	device_register(&lcddev);
}
