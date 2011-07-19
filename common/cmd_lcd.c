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
   1. lcd_getwininfo() wegnehmen, nur noch lcd_getwininfop()
   2. window 0 darf nicht < panelres sein
   3. rgba2col() und col2rgba() in wininfo_t-Struktur einbetten
   4. hardware-dependent includes von lcd.h nach cmd_lcd.h
   5. Nochmal prüfen, dass fbhres und hoffs immer aligned sind
   6. Unnötige includes entfernen
   7. Alten code aus lcd.c/lcd.h entfernen
   8. Evtl. coninfo hierher
   9. LCD-Einschalttiming hinzunehmen

   10. Vorbereitung für mehrere Displays an einem Board: eigenes File
       vidinfo.c, das video-Devices verwaltet. Im init-Teil werden per #ifdef
       die ganzen init-Funktionen der hardware-spez. Module aufgerufen. Ist
       mehr als ein #ifdef aktiv, werden mehrere Devices aktiviert. Dort im
       HW-Init() wird eine Struktur gefüllt, die als panelinfo_t die bisherige
       vidinfo_t hat und zusätzlich Infos über maximale Fensteranzahl,
       maximale Buffer pro Fenster usw. hat. Außerdem werden dort alle
       Funktionen reingesetzt, die hardware-spezifisch sind, z.B. das
       Alignment von hres und hoffs, lcd_hw_enable/disable(), etc. Außerdem
       wird dort das aktuell selektierte Window gemerkt. In die
       wininfo_t-Struktur sollte dann ein Pointer auf die zug. vidinfo_t rein,
       damit man direkt diese Infos referenzieren kann.
       Macht man umgekehrt einen Pointer in die vidinfo_t rein, die auf das
       wininfo_t-Array zeigt, dann kann das wininfo_t-Array vom
       Hardware-spezifischen Modul statisch (im bss) angelegt werden und
       braucht nicht dynamisch auf den Heap.
   11. lcdset, lcdon, lcdoff und lcdlist könnten unter "lcd" zusammengefasst
       werden, lcdwin nach win(dow) und lcddraw nach draw geändert werden.
   12. lcdwin col sollte als draw col kommen (logischer), draw-Befehle sollten
       evtl. FG und BG ändern.
   13. lcdwin ident könnte Farbe des Windows blinken (Hardware-Feature)
   14. Prüfen, was bei LCD-Signalen schon von NBoot gesetzt ist
   15. REG32() durch __REG() ersetzen
   16. PWM settings auf GPF15 setzen.

****/

/*
 * LCD commands
 */

#include <config.h>
#include <common.h>
#include <cmd_lcd.h>			  /* Own interface */
#include <lcd.h>			  /* vidinfo_t, wininfo_t, ... */
#include <lcd_panels.h>			  /* lcd_getpanel(), ... */

/* Always round framebuffer pool size to full pages */
#ifndef PAGE_SIZE
#define PAGE_SIZE	4096
#endif

#ifndef CONFIG_FBPOOL_SIZE
#define CONFIG_FBPOOL_SIZE 0x00100000	  /* 1MB, enough for 800x600@16bpp */
#endif

#define DEFAULT_BG 0x000000FF		  /* Opaque black */
#define DEFAULT_FG 0xFFFFFFFF		  /* Opaque white */


/************************************************************************/
/* ENUMERATIONS								*/
/************************************************************************/

/* Settings that correspond with lcddraw keywords */
typedef enum DRAW_INDEX
{
	DI_PIXEL,
	DI_LINE,
	DI_RECT,
	DI_CIRCLE,
	DI_TEXT,
	DI_BITMAP,

	/* Unknown keyword (must be the last entry!) */
	DI_UNKNOWN
} drawindex_t;

/* Settings that correspond with lcdset and reg set value keywords */
typedef enum SET_INDEX
{
	/* Settings not available with reg set val because they take more than
	   one argument or behave differently */
	SI_PANEL,
	SI_DIM,
	SI_SIZE,
	SI_HTIMING,
	SI_VTIMING,
	SI_POLARITY,
	SI_EXTRA,
	SI_PWM,

	/* Single argument settings, usually also available with reg set val */
	SI_NAME,
	SI_HDIM,
	SI_VDIM,
	SI_TYPE,
	SI_HFP,
	SI_HSW,
	SI_HBP,
	SI_HRES,
	SI_VFP,
	SI_VSW,
	SI_VBP,
	SI_VRES,
	SI_HSPOL,
	SI_VSPOL,
	SI_DENPOL,
	SI_CLKPOL,
	SI_CLK,
	SI_FPS,
	SI_STRENGTH,
	SI_DITHER,
	SI_PWMENABLE,
	SI_PWMVALUE,
	SI_PWMFREQ,

#ifdef CONFIG_FSWINCE_COMPAT
	/* Settings only available with reg set value */
	SI_CONFIG,
#endif

	/* Unknown keyword (must be the last entry!) */
	SI_UNKNOWN
} setindex_t;




/************************************************************************/
/* TYPES AND STRUCTURES							*/
/************************************************************************/


/************************************************************************/
/* LOCAL VARIABLES							*/
/************************************************************************/

DECLARE_GLOBAL_DATA_PTR;

/* Currently selected window */
static WINDOW win_sel = 0;

wininfo_t wininfo[CONFIG_MAX_WINDOWS];	  /* Current window information */
vidinfo_t vidinfo;			  /* Current panel information */

/* Size and base address of the framebuffer pool */
static fbpoolinfo_t fbpool = {
	base:	CFG_UBOOT_BASE - CONFIG_FBPOOL_SIZE,
	size:	CONFIG_FBPOOL_SIZE,
	used:	0
};

/* Display types, corresponding to vidinfo_t.type */
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


/* Keywords available with lcddraw */
static struct kwinfo const drawkeywords[] = {
	{3, 3, DI_PIXEL,  "pixel"},
	{4, 5, DI_LINE,   "line"},
	{4, 6, DI_RECT,   "rectangle"},
	{3, 5, DI_CIRCLE, "circle"},
	{3, 6, DI_TEXT,   "text"},
	{3, 4, DI_BITMAP, "bitmap"},
};


/* Keywords available with lcdwin */
static struct kwinfo const winkeywords[] = {
	{1, 1, WI_SELECT, "select"},
	{1, 2, WI_IMAGE,  "image"},
	{1, 3, WI_FBUF,   "fbuf"},
	{2, 2, WI_RES,    "res"},
	{2, 2, WI_OFFSET, "offset"},
	{2, 2, WI_POS,    "position"},
	{1, 2, WI_COLOR,  "color"},
};

/* Keywords available with lcdset */
static struct kwinfo const setkeywords[] = {
	/* Multiple arguments or argument with multiple types */
	{1, 1, SI_PANEL,     "panel"},
	{1, 2, SI_DIM,       "dimension"},
	{1, 2, SI_SIZE,	     "size"},
	{1, 3, SI_HTIMING,   "htiming"},
	{1, 3, SI_VTIMING,   "vtiming"},
	{1, 4, SI_POLARITY,  "polarity"},
	{1, 3, SI_EXTRA,     "extra"},
	{1, 3, SI_PWM,       "pwm"},

	/* Single argument, SI_NAME must be the first one. */
	{1, 1, SI_NAME,      "name"},
	{1, 1, SI_HDIM,      "hdim"},
	{1, 1, SI_VDIM,      "vdim"},
	{1, 1, SI_TYPE,      "type"},
	{1, 1, SI_HFP,       "hfp"},
	{1, 1, SI_HFP,       "elw"},
	{1, 1, SI_HFP,       "rightmargin"},
	{1, 1, SI_HSW,       "hsw"},
	{1, 1, SI_HBP,       "hbp"},
	{1, 1, SI_HBP,       "blw"},
	{1, 1, SI_HBP,       "leftmargin"},
	{1, 1, SI_HRES,      "hres"},
	{1, 1, SI_VFP,       "vfp"},
	{1, 1, SI_VFP,       "efw"},
	{1, 1, SI_VFP,       "lowermargin"},
	{1, 1, SI_VSW,       "vsw"},
	{1, 1, SI_VBP,       "vbp"},
	{1, 1, SI_VBP,       "bfw"},
	{1, 1, SI_VBP,       "uppermargin"},
	{1, 1, SI_VRES,      "vres"},
	{1, 1, SI_HSPOL,     "hspol"},
	{1, 1, SI_VSPOL,     "vspol"},
	{1, 1, SI_DENPOL,    "denpol"},
	{1, 1, SI_CLKPOL,    "clkpol"},
	{1, 1, SI_CLK,       "clk"},
	{1, 1, SI_FPS,       "fps"},
	{1, 1, SI_STRENGTH,  "strength"},
	{1, 1, SI_DITHER,    "dither"},
	{1, 1, SI_DITHER,    "frc"},
	{1, 1, SI_PWMENABLE, "pwmenable"},
	{1, 1, SI_PWMVALUE,  "pwmvalue"},
	{1, 1, SI_PWMFREQ,   "pwmfreq"},
};

struct winkwtableinfo {
	const struct kwinfo *table;
	u_int entries;
};

static const struct winkwtableinfo const winkwtables[] = {
	{winkeywords, ARRAYSIZE(winkeywords)},
#ifdef CONFIG_LCDWIN_EXT
	{winextkeywords, CONFIG_LCDWIN_EXT}
#endif
};


#ifdef CONFIG_FSWINCE_COMPAT
/* Keywords available with reg set value */
struct regkwinfo
{
	setindex_t si;
	char *keyword;
};

struct regkwinfo const regkeywords[] =
{
	{SI_NAME,      "name"},
	{SI_HDIM,      "width"},
	{SI_VDIM,      "height"},
	{SI_TYPE,      "type"},
	{SI_HFP,       "elw"},
	{SI_HSW,       "hsw"},
	{SI_HBP,       "blw"},
	{SI_HRES,      "columns"},
	{SI_VFP,       "efw"},
	{SI_VSW,       "vsw"},
	{SI_VBP,       "bfw"},
	{SI_VRES,      "rows"},
	{SI_CONFIG,    "config"},
	{SI_CLK,       "lcdclk"},
	{SI_STRENGTH,  "lcdportdrivestrength"},
	{SI_PWMENABLE, "contrastenable"},
	{SI_PWMVALUE,  "contrastvalue"},
	{SI_PWMFREQ,   "contrastfreq"},
};
#endif /*CONFIG_FSWINCE_COMPAT*/

/************************************************************************/
/* Prototypes of local functions					*/
/************************************************************************/
/* Get window information */
static void lcd_getwininfo(wininfo_t *wi, WINDOW win);

/* Set new/updated window information */
static void lcd_setwininfo(wininfo_t *wi, WINDOW win);

/* Get pointer to current window information */
static const wininfo_t *lcd_getwininfop(WINDOW win);


/************************************************************************/
/* Command cls								*/
/************************************************************************/

static int cls(cmd_tbl_t *cmdtp, int flag, int argc, char *argv[])
{
	wininfo_t wi;

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
	lcd_setcolreg  (CONSOLE_COLOR_GREY,	0xAA, 0xAA, 0xAA);
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

	lcd_getwininfo(&wi, win_sel);

	/* If selected window is active, fill it with background color */
	if (wi.active) {
		memset32((unsigned *)wi.fbuf[wi.fbdraw], wi.bg, wi.fbsize/4);
		/* #### TODO
		wi.column = 0;
		wi.row = 0;
		lcd_setwininfo(&wi, win_sel);
		*/
	}

	return 0;
}


U_BOOT_CMD(
	cls,	1,	1,	cls,
	"cls\t- clear screen\n",
	NULL
);


/************************************************************************/
/* Commands lcdon, lcdoff						*/
/************************************************************************/

static int lcdon(cmd_tbl_t *cmdtp, int flag, int argc, char *argv[])
{
	char *pReason = NULL;

	if (!vidinfo.hres || !vidinfo.vres)
		pReason = "No valid lcd panel\n";
	else {
		WINDOW win;
		wininfo_t *pwi;

		for (win=0, pwi=wininfo; win<CONFIG_MAX_WINDOWS; win++, pwi++) {
			if (pwi->active)
				break;
		}
		if (win == CONFIG_MAX_WINDOWS)
			pReason = "No active window\n";
		else if (lcd_hw_enable())
			pReason = "Can't enable display\n";
	}
	if (!pReason)
		return 0;

	puts(pReason);
	return 1;
}

U_BOOT_CMD(
	lcdon,	1,	0,	lcdon,
	"lcdon\t- activate display\n",
	NULL
);

static int lcdoff(cmd_tbl_t *cmdtp, int flag, int argc, char *argv[])
{
	lcd_hw_disable();

	return 0;
}

U_BOOT_CMD(
	lcdoff,	1,	0,	lcdoff,
	"lcdoff\t- deactivate display\n",
	NULL
);


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
/* Command lcdlist							*/
/************************************************************************/

static int lcdlist(cmd_tbl_t *cmdtp, int flag, int argc, char *argv[])
{
	if (argc >= 2) {
		unsigned count;
		unsigned index;
		char c;

		/* Get count if given */
		count = 0xFFFFFFFF;
		if (argc > 2)
			count = simple_strtoul(argv[2], NULL, 0);

		/* Show header line */
		printf("#\tType\thres x vres\thdim x vdim\tName\n"
		       "---------------------------------------------"
		       "-------------------------\n");

		/* If first argument is a number, parse start index */
		c = argv[1][0];
		index = 0;
		if ((c >= '0') & (c <= '9')) {
			c = 0;
			index = simple_strtoul(argv[1], NULL, 0);
		}
		/* If the first argument is an empty string, c is also zero
		   here; this is intentionally handled like a normal index
		   list later because an empty string matches all entries
		   anyway. */

		while (count--)
		{
			const vidinfo_t *pvi;
			const char *p;

			if (c) {
				/* Search next matching panel */
				index = lcd_searchpanel(argv[1], index);
				if (!index)
					break; /* No further match */
			}
			pvi = lcd_getpanel(index);
			if (!pvi)
				break;	  /* Reached end of list */

			/* Show entry */
			if (pvi->type < 4)
				p = "STN";
			else if (pvi->type < 8)
				p = "CSTN";
			else
				p = "TFT";
			printf("%d:\t%s\t%4u x %u\t%4u x %u\t%s\n",
			       index, p, pvi->hres, pvi->vres,
			       pvi->hdim, pvi->vdim, pvi->name);

			/* Next panel */
			index++;
		}
		return 0;
	}

	printf("usage:\n%s ", cmdtp->name);
	puts(cmdtp->help);
	putc('\n');
	return 1;
}

U_BOOT_CMD(
	lcdlist,	3,	1,	lcdlist,
	"lcdlist\t- list predefined LCD panels\n",
	"\n    - List all predefined LCD panels\n"
	"lcdlist substring [count]\n"
	"    - List count entries matching substring\n"
	"lcdlist index [count]\n"
	"    - List count entries starting at index\n"
);

/************************************************************************/
/* Command lcdfbpool							*/
/************************************************************************/

/* Relocate all windows to newaddr, starting at window win */
static void relocbuffers(u_long newaddr, WINDOW win)
{
	/* Relocate all windows, i.e. set all image buffer addresses
	   to the new address and update hardware */
	for (; win < CONFIG_MAX_WINDOWS; win++) {
		u_int buf;
		wininfo_t wi;

		lcd_getwininfo(&wi, win);

		for (buf=0; buf<CONFIG_MAX_BUFFERS_PER_WIN; buf++) {
			if (buf >= wi.fbmaxcount)
				wi.fbuf[buf] = 0;
			else if (wi.fbuf[buf] != newaddr) {
				printf("Window %u: Relocated buffer %u from"
				       " 0x%08lx to 0x%08lx\n",
				       win, buf, wi.fbuf[buf], newaddr);
				wi.fbuf[buf] = newaddr;
			}
			if (buf < wi.fbcount)
				newaddr += wi.fbsize;
		}

		/* Update controller hardware with new wininfo */
		lcd_setwininfo(&wi, win);
	}
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
			printf("Bad address or collision with U-Boot code\n");
			return 1;
		}

		if (pfp->used > newsize) {
			printf("Current framebuffers exceed new size\n");
			return 1;
		}

		/* Move framebuffer content in one go */
		memmove((void *)newbase, (void *)pfp->base, pfp->used);

		/* Relocate the image buffer addresses of all windows */
	        relocbuffers(newbase, 0);

		/* Finally set the new framebuffer pool values */
		pfp->size = newsize;
		pfp->base = newbase;
		gd->fb_base = newbase;
	}

	/* Print current or new settings */
	printf("Framebuffer Pool: 0x%08lx - 0x%08lx (%lu bytes)\n",
	       pfp->base, pfp->base + pfp->size - 1, pfp->size);
	return 0;
}

U_BOOT_CMD(
	lcdfbpool,	5,	1,	lcdfbpool,
	"lcdfbpool\t- set framebuffer pool\n",
	"<size> <address>\n"
	"    - set framebuffer pool of <size> at <address>\n"
	"lcdfbpool <size>\n"
	"    - set framebuffer pool of <size> immediately before U-Boot\n"
	"lcdfbpool\n"
	"    - show framebuffer pool settings"
);


/************************************************************************/
/* Command lcdtest							*/
/************************************************************************/

static int lcdtest(cmd_tbl_t *cmdtp, int flag, int argc, char *argv[])
{
	u_int param;
	wininfo_t wi;

	lcd_getwininfo(&wi, win_sel);
	if (!wi.active)
		return 0;

	param = 0;
	if (argc == 2) {
		/* Do we have a color value? */
		if (*argv[1] == '#') {
			RGBA rgba;

			/* Yes, parse color */
			if (parse_rgb(argv[1], &rgba) == RT_NONE)
				return 1;

			/* Fill screen with single color */
			memset32((unsigned *)wi.fbuf[wi.fbdraw],
				 lcd_rgba2col(&wi, rgba), wi.fbsize/4);
			return 0;
		}
		
		/* No, we have a simple number */
		param = (u_int)simple_strtoul(argv[1], NULL, 0);
	}

	switch (param) {
	case 0:				  /* Test pattern */
		printf("####Test pattern not yet implemented#####\n");
		//break;
		return 1;

	case 1:				  /* Color gradient 1 */
		printf("####Color gradient 1 test not yet implemented#####\n");
		//break;
		return 1;

	case 2:				  /* Color gradient 2 */
		printf("####Color gradient 2 test not yet implemented#####\n");
		//break;
		return 1;

	default:
		printf("Unknown test pattern\n");
		return 1;
	}
	return 0;
}

U_BOOT_CMD(
	lcdtest,	5,	1,	lcdtest,
	"lcdtest\t- show test pattern or color on LCD\n",
	"[<n>]\n"
	"    - show test pattern <n>. <n> may be one of:\n"
	"\t0: test pattern (grid, circle, basic colors)\n"
	"\t1: gradient 1 (horizontal: colors, vertical: brightness)\n"
	"\t2: gradient 2 (colors along screen edges, grey in center)\n"
);


/************************************************************************/
/* Command draw								*/
/************************************************************************/

static int draw(cmd_tbl_t *cmdtp, int flag, int argc, char *argv[])
{
	const wininfo_t *pwi;
	u_int si = DI_UNKNOWN;
	XYPOS x1, y1, x2, y2;
	RGBA rgba;
	COLOR32 col1, col2;
	u_int a;			  /* Attribute */
	

	pwi = lcd_getwininfop(win_sel);

	/* If selected window is not active do nothing */
	if (!pwi->active) {
		printf("Window %d not active\n", win_sel);
		return 1;
	}

	/* Search for keyword in lcddraw keyword list */
	if (argc >= 2) {
		const struct kwinfo *ki;
		size_t len;
		uint i;

		len = strlen(argv[1]);
		for (i=0, ki=drawkeywords;
		     i<ARRAYSIZE(drawkeywords); i++, ki++) {
			if (strncmp(ki->keyword, argv[1], len) == 0)
				break;
		}

		if ((i < ARRAYSIZE(drawkeywords)) && (argc <= ki->argc_max+2)) {
			if ((argc == 2) || (argc >= ki->argc_min+2))
				si = ki->si; /* Keyword found */
			else
				printf("Missing argument\n");
		}
	}

	if (si == DI_UNKNOWN) {
		printf("usage:\n%s ", cmdtp->name);
		puts(cmdtp->help);
		putc('\n');
		return 1;
	}

	/* All commands have at least argument 1: x, argument 2: y */
	x1 = (XYPOS)simple_strtol(argv[2], NULL, 0);
	y1 = (XYPOS)simple_strtol(argv[3], NULL, 0);

	/* The default color is FG */
	col1 = pwi->fg;

	switch (si) {
	case DI_PIXEL:			  /* Draw pixel */
		/* Optional argument 3: color */
		if (argc > 4) {
			if (parse_rgb(argv[4], &rgba) == RT_NONE)
				return 1;
			col1 = lcd_rgba2col(pwi, rgba);
		}
		lcd_pixel(pwi, x1, y1, col1);
		break;

	case DI_LINE:			  /* Draw line */
	case DI_RECT:			  /* Draw rectangle */
		/* Argument 3: x2, argument 4: y2 */
		x2 = (XYPOS)simple_strtol(argv[4], NULL, 0);
		y2 = (XYPOS)simple_strtol(argv[5], NULL, 0);

		/* Optional argument 5: line color */
		if (argc > 6) {
			if (parse_rgb(argv[6], &rgba) == RT_NONE)
				return 1;
			col1 = lcd_rgba2col(pwi, rgba);
		}

		if (si == DI_LINE) {
			/* Draw line */
			lcd_line(pwi, x1, y1, x2, y2, col1);
			break;
		}

		/* Optional argument 6: fill color */
		if (argc > 7) {
			if (parse_rgb(argv[7], &rgba) == RT_NONE)
				return 1;
			col2 = lcd_rgba2col(pwi, rgba);

			/* Draw filled rectangle */
			lcd_rect(pwi, x1, y1, x2, y2, col2);

			/* If line color is the same, we are done */
			if (col1 == col2)
				return 0;
		};

		/* Draw frame in line color */
		lcd_frame(pwi, x1, y1, x2, y2, col1);
		break;

	case DI_CIRCLE:
		/* Argument 3: radius */
		x2 = (XYPOS)simple_strtol(argv[4], NULL, 0);

		/* Optional argument 4: line color */
		if (argc > 5) {
			if (parse_rgb(argv[5], &rgba) == RT_NONE)
				return 1;
			col1 = lcd_rgba2col(pwi, rgba);
		}

		/* Optional argument 5: fill color */
		if (argc > 6) {
			if (parse_rgb(argv[6], &rgba) == RT_NONE)
				return 1;
			col2 = lcd_rgba2col(pwi, rgba);

			/* Draw filled circle */
			lcd_disc(pwi, x1, y1, x2, col2);

			/* If line color is the same, we are done */
			if (col1 == col2)
				return 0;
		}

		/* Draw circle outline in line color */
		lcd_circle(pwi, x1, y1, x2, col1);
		break;
			
	case DI_TEXT:
		/* Optional argument 4: attribute */
		a = 0;
		if (argc > 5)
			a = simple_strtoul(argv[5], NULL, 0);

		/* Optional argument 5: FG color */
		if (argc > 6) {
			if (parse_rgb(argv[6], &rgba) == RT_NONE)
				return 1;
			col1 = lcd_rgba2col(pwi, rgba);
		}

		/* Optional argument 6: BG color */
		col2 = pwi->bg;
		if (argc > 7) {
			if (parse_rgb(argv[7], &rgba) == RT_NONE)
				return 1;
			col2 = lcd_rgba2col(pwi, rgba);
		}

		/* Draw text */
		lcd_text(pwi, x1, y1, argv[4], a, col1, col2);
		break;

	case DI_BITMAP:
		/* Optional argument 4: attribute */
		a = 0;
		if (argc > 5)
			a = simple_strtoul(argv[5], NULL, 0);

		/* Draw bitmap */
		lcd_bitmap(pwi, x1, y1, simple_strtoul(argv[4], NULL, 16), a);
		break;

	default:			  /* Should not happen */
		break;
	}

	return 0;	
}

U_BOOT_CMD(
	draw, 7,	0,	draw,
	"draw\t- draw to selected window\n",
	"pixel x y [#rgba]\n"
	"    - draw a pixel to (x, y)\n"
	"draw line x1 y1 x2 y2 [#rgba]\n"
	"    - draw a line from (x1, y1) to (x2, y2)\n"
	"draw rectangle x1 y1 x2 y2 [#rgba [#rgba]]\n"
	"    - draw a filled or unfilled rectangle from (x1, y1) to (x2, y2)\n"
	"draw circle x y r [#rgba [#rgba]]\n"
	"    - draw a filled or unfilled circle at (x, y) with radius r\n"
	"draw text x y string [a [#rgba [#rgba]]]\n"
	"    - draw text string at (x, y) with attribute a\n"
	"draw bitmap x y addr [a]\n"
	"    - draw bitmap from addr at (x, y) with attribute a\n"
);


/************************************************************************/
/* Command lcdwin							*/
/************************************************************************/

/* Move offset if window would not fit within framebuffer */
static void fix_offset(wininfo_t *wi)
{
	/* Move offset if necessary; window must fit into image buffer */
	if ((u_short)wi->hoffs + wi->hres > wi->fbhres)
		wi->hoffs = (XYPOS)(wi->fbhres - wi->hres);
	if ((u_short)wi->voffs + wi->vres > wi->fbvres)
		wi->voffs = (XYPOS)(wi->fbvres - wi->vres);
}

static int setfbuf(wininfo_t *pwi, u_int fbhres, u_int fbvres, u_char pix,
		   u_char fbcount)
{
	u_short fbmaxhres, fbmaxvres;
	u_long oldsize, newsize;
	u_long linelen, fbsize;
	u_long addr;
	WINDOW win;
	const pixinfo_t *pi;
	fbpoolinfo_t *pfp = &fbpool;

	/* Check if pixel format is valid for this window */
	win = pwi->win;
	pi = lcd_getpixinfo(win, pix);
	if (!pi) {
		printf("Bad pixel format '%u'\n", pix);
		return 1;
	}

	/* Check if framebuffer count is valid for this window */
	if (fbcount > pwi->fbmaxcount) {
		printf("Bad image buffer count '%u'\n", fbcount);
		return 1;
	}

	/* Check if resolution is valid */
	fbhres = lcd_align_hres(win, pix, fbhres);
	fbmaxhres = lcd_getfbmaxhres(win, pix);
	fbmaxvres = lcd_getfbmaxvres(win, pix);
	if ((fbhres > fbmaxhres) || (fbvres > fbmaxvres)) {
		printf("Requested size %ux%u exceeds allowed size %ux%u\n",
		       fbhres, fbvres, fbmaxhres, fbmaxvres);
		return 1;
	}

	/* If there are no changes, we're done */
	if ((fbhres == pwi->fbhres) && (fbvres == pwi->fbvres)
	    && (pix == pwi->pix) && (fbcount == pwi->fbcount))
		return 0;

	/* Compute the size of one framebuffer line (incl. alignment) */
	linelen = ((u_long)fbhres << pi->bpp_shift) >> 3;

	/* Compute the size of one image buffer */
	fbsize = linelen * fbvres;

	newsize = fbsize * fbcount;
	oldsize = pwi->fbsize * pwi->fbcount;

	if (pfp->used - oldsize + newsize > pfp->size) {
		printf("Framebuffer pool too small\n");
		return 1;
	}

	/* OK, the new settings can be made permanent */
	pwi->fbhres = fbhres;
	pwi->fbvres = fbvres;
	pwi->pi = pi;
	pwi->fbcount = fbcount;
	pwi->linelen = linelen;
	pwi->fbsize = fbsize;
	if (pwi->pix != pix) {
		/* New pixel format: set default bg + fg */
		pwi->bg = lcd_rgba2col(pwi, DEFAULT_BG);
		pwi->fg = lcd_rgba2col(pwi, DEFAULT_FG);
	}
	pwi->pix = pix;

	/* If not the last window, relocate all subsequent windows. */
	addr = pwi->fbuf[0];
	if ((win+1 < CONFIG_MAX_WINDOWS) && (oldsize != newsize)) {
		u_long newaddr, oldaddr;
		u_long used;

		oldaddr = addr + oldsize;
		newaddr = addr + newsize;

		/* Used mem for windows 0..win */
		used = oldaddr - pfp->base;

		/* Used mem for windows win+1..CONFIG_MAX_WINDOWS-1 */
		used = pfp->used - used;

		/* Move framebuffers of all subsequent windows in one go */
		memmove((void *)newaddr, (void *)oldaddr, used);
	}

	/* Clear the new framebuffer with background color */
	memset32((unsigned *)addr, pwi->bg, newsize/4);

	/* Then relocate the image buffer addresses of this and all subsequent
	   windows */
	relocbuffers(addr, win);

	/* Update framebuffer pool */
	pfp->used = pfp->used - oldsize + newsize;

	/* Set the active flag of the window */
	pwi->active = (pwi->fbhres && pwi->fbvres && pwi->fbcount);

	/* The console might also be interested in the buffer changes */
	console_update(pwi);

	return 0;
}


static int lcdwin(cmd_tbl_t *cmdtp, int flag, int argc, char *argv[])
{
	wininfo_t wi;
	unsigned si = WI_UNKNOWN;
	unsigned buf;

	/* Get the window info for the currently selected window */
	lcd_getwininfo(&wi, win_sel);
	if (argc < 2)
		si = WI_INFO;
	else {
		const struct kwinfo *ki;
		const struct winkwtableinfo *kt;
		size_t len = strlen(argv[1]);
		unsigned i, j;

		for (j=0, kt=winkwtables; j<ARRAYSIZE(winkwtables); j++, kt++) {
			for (i=0, ki = kt->table; i<kt->entries; i++, ki++) {
				if (strncmp(ki->keyword, argv[1], len) == 0)
					break;
			}
			if (i < kt->entries)
				break;
		}

		if ((j < ARRAYSIZE(winkwtables)) && (argc <= ki->argc_max+2)) {
			if ((argc == 2) || (argc >= ki->argc_min+2))
				si = ki->si;	  /* Command found */
			else
				printf("Missing argument\n");
		}
	}
	/* If not recognized, print usage and return */
	if (si == WI_UNKNOWN) {
		printf("usage:\n%s ", cmdtp->name);
		puts(cmdtp->help);
		putc('\n');
		return 1;
	}

	if ((si != WI_INFO) && (argc > 2)) {
		/* We have our minimum number of arguments */
		switch (si) {
		case WI_SELECT: {
			WINDOW win;
			
			/* Argument 1: window number */
			win = (WINDOW)simple_strtoul(argv[2], NULL, 0);
			if (win >= CONFIG_MAX_WINDOWS) {
				printf("Bad window number '%u'\n", win);
				return 1;
			}
			break;
		}

		case WI_IMAGE: {
			u_char fbdraw, fbshow;

			/* Argument 1: buffer number to draw to */
			fbdraw = (u_char)simple_strtoul(argv[3], NULL, 0);
			if (fbdraw >= wi.fbcount) {
				printf("Bad image buffer '%u'\n", fbdraw);
				return 1;
			}

			/* Argument 2: buffer number to show */
			fbshow = 0;
			if (argc == 4) {
				fbshow = (u_char)simple_strtoul(argv[4],
								NULL, 0);
				if (fbshow >= wi.fbcount) {
					printf("Bad image buffer '%u'\n",
					       fbshow);
					return 1;
				}
			}

			/* Set new values */
			wi.fbdraw = fbdraw;
			wi.fbshow = fbshow;
			break;
		}

		case WI_FBUF: {
			u_short fbhres, fbvres;
			u_char pix, fbcount;

			/* Argument 3: pixel format (index number) */
			pix = DEFAULT_PIXEL_FORMAT;
			if (argc > 4)
				pix = (u_char)simple_strtoul(argv[4], NULL, 0);

			/* Argument 4: buffer count */
			fbcount = 1;
			if (argc == 6)
				fbcount =
				      (u_char)simple_strtoul(argv[5], NULL, 0);

			/* Arguments 1+2: fbhres and fbvres */
			fbhres = (u_short)simple_strtoul(argv[2], NULL, 0);
			fbvres = (u_short)simple_strtoul(argv[3], NULL, 0);

			/* Test validity and set new framebuffer size */
			if (setfbuf(&wi, fbhres, fbvres, pix, fbcount))
				return 1;

			/* Reduce window size if necessary */
			if (wi.hres > wi.fbhres)
				wi.hres = wi.fbhres;
			if (wi.vres > wi.fbvres)
				wi.vres = wi.fbvres;
			fix_offset(&wi);

			break;
		}

		case WI_RES: {
			u_short hres, vres;
			u_short fbhres, fbvres;

			/* Arguments 1+2: hres and vres */
			hres = (u_short)simple_strtoul(argv[2], NULL, 0);
			vres = (u_short)simple_strtoul(argv[3], NULL, 0);
			fbhres = wi.fbhres;
			fbvres = wi.fbvres;
			if ((hres > fbhres) || (vres > fbvres)) {
				/* Framebuffer must be increased */
				if (hres > fbhres)
					fbhres = hres;
				if (vres > fbvres)
					fbvres = vres;
				if (setfbuf(&wi, fbhres, fbvres, wi.pix,
					    wi.fbcount))
					return 1;
			}

			/* Set new window size */
			wi.hres = hres;
			wi.vres = vres;
			fix_offset(&wi);
			break;
		}

		case WI_OFFSET:
			/* Arguments 1+2: hoffs and voffs (non-negative) */
			wi.hoffs = lcd_align_hoffs(&wi,
				      (XYPOS)simple_strtoul(argv[2], NULL, 0));
			wi.voffs = (XYPOS)simple_strtoul(argv[3], NULL, 0);
			fix_offset(&wi);
			break;

		case WI_POS:
			/* Arguments 1+2: hpos and vpos, may be negative */
			wi.hpos = (XYPOS)simple_strtol(argv[2], NULL, 0);
			wi.vpos = (XYPOS)simple_strtol(argv[3], NULL, 0);
			break;

		case WI_COLOR: {
			RGBA rgba;
			COLOR32 fg;

			if (parse_rgb(argv[2], &rgba) == RT_NONE)
				return 1;
			fg = lcd_rgba2col(&wi, rgba);

			rgba = 0x000000FF; /* Opaque black */
			if (argc > 3) {
				if (parse_rgb(argv[3], &rgba) == RT_NONE)
					return 1;
			}
			wi.fg = fg;
			wi.bg = lcd_rgba2col(&wi, rgba);
			break;
		}

		default:
			/* This is one of the extensions */
			if (lcdwin_ext_exec(&wi, argc, argv, si))
				return 1;
			break;
		}

		lcd_setwininfo(&wi, win_sel);
	}

	/* Print the modified or requested settings */
	printf("Sel. Window:\t%u", win_sel);
	switch (si) {
	case WI_SELECT:
		break;			  /* Already done above */

	case WI_INFO:
	case WI_FBUF:
	case WI_IMAGE:
		printf("Framebuffer:\t%d x %d, %lu bytes (%lu bytes/line)\n",
		       wi.fbhres, wi.fbvres, wi.fbsize, wi.linelen);
		for (buf=0; buf<wi.fbcount; buf++) {
			printf("\t\timage buffer %u: 0x%08lx - 0x%08lx\n",
			       buf, wi.fbuf[buf], wi.fbuf[buf]+wi.fbsize-1);
		}
		printf("\t\t%u image buffer(s) available\n", wi.fbmaxcount);
		printf("\t\tdraw to image buffer %u, show image buffer %u\n",
		       wi.fbdraw, wi.fbshow);
		printf("Pixel Format:\t%u, %u/%u bpp, %s\n",
		       wi.pix, wi.pi->depth, 1<<wi.pi->bpp_shift, wi.pi->name);
		if (si != WI_INFO)
			break;
		/* WI_INFO: fall through to case WI_IMAGE */

	case WI_RES:
	case WI_POS:
	case WI_OFFSET:
		printf("Window:\t%u x %u\n"
		       "\t\tfrom framebuffer offset (%d, %d)\n"
		       "\t\tdisplayed at screen position (%d, %d)\n",
		       wi.hres, wi.vres, wi.hoffs, wi.voffs, wi.hpos, wi.vpos);
		if (si != WI_INFO)
			break;
		/* WI_INFO: fall through to case WI_COLOR */

	case WI_COLOR:
		printf("Colors:\t\tFG #%08x, BG #%08x\n",
		       lcd_col2rgba(&wi, wi.fg),
		       lcd_col2rgba(&wi, wi.bg));
		if (si != WI_INFO)
			break;
		/* WI_INFO: fall through to default */

	default:
		/* Call the wininfo extension to print additional status */
		lcdwin_ext_print(&wi, si);

		/* WI_INFO lists the available pixel formats */
		if (si == WI_INFO) {
			u_char pix = 0;

			printf("\nPixel Formats:\tindex\tbpp\tformat\n");
			for (;;) {
				const pixinfo_t *pi;
				pix = lcd_getnextpix(win_sel, pix);
				pi = lcd_getpixinfo(win_sel, pix);
				if (!pi)
					break;
				printf("\t\t%u\t%u/%u\t%s\n", pix, pi->depth,
				       1<<pi->bpp_shift, pi->name);
				pix++;
			}
		}
		break;
	}

	return 0;
}

U_BOOT_CMD(
	lcdwin,	7,	0,	lcdwin,
	"lcdwin\t- set framebuffer and overlay window parameters\n",
	"select [window [draw [show]]]\n"
	"    - Select a window, set buffer to draw and to show\n"
	"lcdwin fbuf [fbhres fbvres [pixelformat [buffers]]]\n"
	"    - Set virtual framebuffer resolution, pixel format and buffer count\n"
	"lcdwin size [hres vres]\n"
	"    - Set window resolution (i.e. OSD size)\n"
	"lcdwin offset [hoffs voffs]\n"
	"    - Set window offset within virtual framebuffer\n"
	"lcdwin pos [x y]\n"
	"    - Set window position on display\n"
	"lcdwin col [#rgba [#rgba]]\n"
	"    - Set foreground and background color\n"
#ifdef CONFIG_LCDWIN_EXT
	LCDWIN_EXT_HELP
#endif
	"lcdwin\n"
	"    - Show all settings of selected window\n"
);


/************************************************************************/
/* Command lcdset							*/
/************************************************************************/

static void setvalue(uint si, char *argv, vidinfo_t *pvi)
{
	uint param = 0;

	/* All parameters but SI_NAME require a number, parse it */
	if (si != SI_NAME)
		param = simple_strtoul(argv, NULL, 0);

	switch (si)
	{
	case SI_NAME:			  /* Parse string */
		strncpy(pvi->name, argv, MAX_NAME);
		pvi->name[MAX_NAME-1] = 0;
		break;

	case SI_HDIM:			  /* Parse u_short */
		pvi->hdim = (u_short)param;
		break;

	case SI_VDIM:			  /* Parse u_short */
		pvi->vdim = (u_short)param;
		break;

	case SI_TYPE:			  /* Parse u_char (0..8) */
		if (param > 8)
			param = 8;
		pvi->type = (u_char)param;
		break;

	case SI_HFP:			  /* Parse u_short */
		pvi->hfp = (u_short)param;
		break;

	case SI_HSW:			  /* Parse u_short */
		pvi->hsw = (u_short)param;
		break;

	case SI_HBP:			  /* Parse u_short */
		pvi->hbp = (u_short)param;
		break;

	case SI_HRES:			  /* Parse u_short */
		pvi->hres = (u_short)param;
		break;

	case SI_VFP:			  /* Parse u_short */
		pvi->hfp = (u_short)param;
		break;

	case SI_VSW:			  /* Parse u_short */
		pvi->vsw = (u_short)param;
		break;

	case SI_VBP:			  /* Parse u_short */
		pvi->vbp = (u_short)param;
		break;

	case SI_VRES:			  /* Parse u_short */
		pvi->vres = (u_short)param;
		break;

	case SI_HSPOL:			  /* Parse flag (u_char 0..1) */
		pvi->hspol = (param != 0);
		break;

	case SI_VSPOL:			  /* Parse flag (u_char 0..1) */
		pvi->vspol = (param != 0);
		break;

	case SI_DENPOL:			  /* Parse flag (u_char 0..1) */
		pvi->denpol = (param != 0);
		break;

	case SI_CLKPOL:			  /* Parse flag (u_char 0..1) */
		pvi->clkpol = (param != 0);
		break;

	case SI_CLK:			  /* Parse u_int; scale appropriately */
		if (param < 1000000) {
			param *= 1000;
			if (param < 1000000)
				param *= 1000;
		}
		pvi->clk = (u_int)param;
		pvi->fps = 0;		  /* Compute fps from clk */
		break;

	case SI_FPS:			  /* Parse u_int */
		pvi->fps = (u_int)param;
		pvi->clk = 0;		  /* Compute clk from fps */
		break;

	case SI_STRENGTH:		  /* Parse u_char */
		pvi->strength = (u_char)param;
		break;

	case SI_DITHER:			  /* Parse flag (u_char 0..1) */
		pvi->dither = (param != 0);
		break;

	case SI_PWMENABLE:		  /* Parse flag (u_char 0..1) */
		pvi->pwmenable = (param != 0);
		break;

	case SI_PWMVALUE:		  /* Parse u_int */
		pvi->pwmvalue = (u_int)param;
		break;

	case SI_PWMFREQ:		  /* Parse u_int */
		pvi->pwmfreq = (u_int)param;
		break;

	default:			  /* Should not happen */
		printf("Unknown setting\n");
		break;
	}
}

static int lcdset(cmd_tbl_t *cmdtp, int flag, int argc, char *argv[])
{
	vidinfo_t *pvi;
	setindex_t si = SI_UNKNOWN;
	char c;
	uint param;

	/* Get the LCD panel info */
	pvi = &vidinfo;
	if (argc < 2)
		si = SI_PANEL; 		  /* No arguments: same as panel */
	else {
		const struct kwinfo *ki;
		uint len = strlen(argv[1]);
		uint i;

		for (i=0, ki=setkeywords; i<ARRAYSIZE(setkeywords); i++, ki++) {
			if (strncmp(ki->keyword, argv[1], len) == 0)
				break;
		}
		if ((i < ARRAYSIZE(setkeywords) && (argc <= ki->argc_max+2))) {
			if ((argc == 2) || (argc >= ki->argc_min+2))
				si = ki->si;
			else
				printf("Missing argument\n");
		}
	}

	if (si == SI_UNKNOWN) {
		printf("usage:\n%s ", cmdtp->name);
		puts(cmdtp->help);
		putc('\n');
		return 1;
	}

	if (argc >= 3) {
		/* We have our minimum number of arguments */
		switch (si) {
		case SI_PANEL: {		  /* Parse number or string */
			const vidinfo_t *pvi_new;

			c = argv[2][0];
			if ((c >= '0') && (c <= '9')) {
				/* Parse panel index number */
				param = simple_strtoul(argv[2], NULL, 0);
			} else if (!(param = lcd_searchpanel(argv[2], 0))) {
				printf("\nNo panel matches '%s'\n", argv[2]);
				break;
			}
			
			pvi_new = lcd_getpanel(param);
			if (!pvi_new)
				printf("\nBad panel index %d\n", param);
			else
				*pvi = *pvi_new;
			break;
		}

		case SI_DIM:		  /* Parse exactly 2 u_shorts */
			param = simple_strtoul(argv[3], NULL, 0);
			pvi->hdim = (u_short)param;
			param = simple_strtoul(argv[4], NULL, 0);
			pvi->vdim = (u_short)param;
			break;

		case SI_SIZE:		  /* Parse exactly 2 u_shorts */
			param = simple_strtoul(argv[3], NULL, 0);
			pvi->hres = (u_short)param;
			param = simple_strtoul(argv[4], NULL, 0);
			pvi->vres = (u_short)param;
			break;

		case SI_HTIMING:	  /* Parse up to 3 u_shorts */
			param = simple_strtoul(argv[2], NULL, 0);
			pvi->hfp = (u_short)param;
			if (argc < 4)
				break;
			param = simple_strtoul(argv[3], NULL, 0);
			pvi->hsw = (u_short)param;
			if (argc < 5)
				break;
			param = simple_strtoul(argv[4], NULL, 0);
			pvi->hbp = (u_short)param;
			break;

		case SI_VTIMING:	  /* Parse up to 3 u_shorts */
			param = simple_strtoul(argv[2], NULL, 0);
			pvi->vfp = (u_short)param;
			if (argc < 4)
				break;
			param = simple_strtoul(argv[3], NULL, 0);
			pvi->vsw = (u_short)param;
			if (argc < 5)
				break;
			param = simple_strtoul(argv[4], NULL, 0);
			pvi->vbp = (u_short)param;
			break;

		case SI_POLARITY:	  /* Parse up to 4 flags */
			param = simple_strtoul(argv[2], NULL, 0);
			pvi->hspol = (param != 0);
			if (argc < 4)
				break;
			param = simple_strtoul(argv[3], NULL, 0);
			pvi->vspol = (param != 0);
			if (argc < 5)
				break;
			param = simple_strtoul(argv[4], NULL, 0);
			pvi->denpol = (param != 0);
			if (argc < 6)
				break;
			param = simple_strtoul(argv[5], NULL, 0);
			pvi->clkpol = (param != 0);
			break;

		case SI_EXTRA:		  /* Parse number + flag */
			param = simple_strtoul(argv[2], NULL, 0);
			pvi->strength = (u_char)param;
			if (argc < 4)
				break;
			param = simple_strtoul(argv[3], NULL, 0);
			pvi->dither = (param != 0);
			break;

		case SI_PWM:		  /* Parse flag + 2 numbers */
			param = simple_strtoul(argv[2], NULL, 0);
			pvi->pwmenable = (param != 0);
			if (argc < 4)
				break;
			param = simple_strtoul(argv[3], NULL, 0);
			pvi->pwmvalue = (u_int)param;
			if (argc < 5)
				break;
			param = simple_strtoul(argv[4], NULL, 0);
			pvi->pwmfreq = (u_int)param;
			break;

		default:		  /* Parse one argument */
			setvalue(si, argv[2], pvi);
			break;
		}

		/* Set new vidinfo to hardware; this may also update vi to
		   the actually used parameters. */
		lcd_hw_vidinfo(pvi);
	}

	/* Print the modified or requested settings */
	switch (si)
	{
	default:
	case SI_PANEL:
	case SI_NAME:
		printf("Display Name:\tname='%s'\n", pvi->name);
		if (si != SI_PANEL)
			break;
		/* SI_PANEL: Fall through to case SI_DIM */

	case SI_DIM:
	case SI_HDIM:
	case SI_VDIM:
		printf("Dimensions:\thdim=%dmm, vdim=%dmm\n",
		       pvi->hdim, pvi->vdim);
		if (si != SI_PANEL)
			break;
		/* SI_PANEL: Fall through to case SI_TYPE */

	case SI_TYPE:
		printf("Display Type:\ttype=%d (%s)\n",
		       pvi->type, typeinfo[pvi->type]);
		if (si != SI_PANEL)
			break;
		/* SI_PANEL: Fall through to case SI_SIZE */

	case SI_SIZE:
	case SI_HRES:
	case SI_VRES:
		printf("Display Size:\thres=%d, vres=%d\n",
		       pvi->hres, pvi->vres);
		if (si != SI_PANEL)
			break;
		/* SI_PANEL: Fall through to case SI_HTIMING */

	case SI_HTIMING:
	case SI_HFP:
	case SI_HSW:
	case SI_HBP:
		printf("Horiz. Timing:\thfp=%d, hsw=%d, hbp=%d\n",
		       pvi->hfp, pvi->hsw, pvi->hbp);
		if (si != SI_PANEL)
			break;
		/* SI_PANEL: Fall through to case SI_VTIMING */

	case SI_VTIMING:
	case SI_VFP:
	case SI_VSW:
	case SI_VBP:
		printf("Vert. Timing:\tvfp=%d, vsw=%d, vbp=%d\n",
		       pvi->vfp, pvi->vsw, pvi->vbp);
		if (si != SI_PANEL)
			break;
		/* SI_PANEL: Fall through to case SI_POLARITY */

	case SI_POLARITY:
	case SI_HSPOL:
	case SI_VSPOL:
	case SI_DENPOL:
	case SI_CLKPOL:
		printf("Polarity:\thspol=%d, vspol=%d, clkpol=%d, denpol=%d\n",
		       pvi->hspol, pvi->vspol, pvi->clkpol, pvi->denpol);
		if (si != SI_PANEL)
			break;
		/* SI_PANEL: Fall through to case SI_CLK */

	case SI_CLK:
	case SI_FPS:
		printf("Display Timing:\tclk=%dHz (%dfps)\n",
		       pvi->clk, pvi->fps);
		if (si != SI_PANEL)
			break;
		/* SI_PANEL: Fall through to case SI_EXTRA */

	case SI_EXTRA:
	case SI_STRENGTH:
	case SI_DITHER:
		printf("Extra Settings:\tstrength=%d, dither=%d\n",
		       pvi->strength, pvi->dither);
		if (si != SI_PANEL)
			break;
		/* SI_PANEL: Fall through to case SI_PWM */

	case SI_PWM:
	case SI_PWMENABLE:
	case SI_PWMVALUE:
	case SI_PWMFREQ:
		printf("PWM:\t\tpwmenable=%d, pwmvalue=%d, pwmfreq=%dHz\n",
		       pvi->pwmenable, pvi->pwmvalue, pvi->pwmfreq);
		break;
	}

	return 0;
}

U_BOOT_CMD(
	lcdset,	7,	0,	lcdset,
	"lcdset\t- set LCD panel parameters\n",
	"panel index\n"
	"    - set one of the predefined panels\n"
	"lcdset panel substring\n"
	"    - set the predefined panel that matches substring\n"
	"lcdset param [value]\n"
	"    - set or show the LCD parameter param; param is one of:\n"
	"\tname, hdim, vdim, hres, vres, hfp, hsw, hpb, vfp, vsw, vbp,\n"
	"\thspol, vspol, denpol, clkpol, clk, fps, type, pixel, strength,\n"
	"\tdither, pwmenable, pwmvalue, pwmfreq\n"
	"lcdset group {values}\n"
	"    - set or show the LCD parameter group; group is one of:\n"
	"\tdimension, size, htiming, vtiming, polarity, pwm, extra\n"
	"lcdset\n"
	"    - show all current panel settings\n"
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
		const struct regkwinfo *ki;
		uint len = strlen(argv[3]);
		uint i;

		/* Search list of keywords */
		for (i=0, ki=regkeywords; i<ARRAYSIZE(regkeywords); i++, ki++) {
			if (strncmp(ki->keyword, argv[1], len) == 0)
				break;
		}
		/* Handle value if keyword was found */
		if (i < ARRAYSIZE(regkeywords))
		{
			vidinfo_t *pvi = &vidinfo;
			uint param;

			switch (ki->si) {
			case SI_TYPE:
				/* "type" is used differently here */
				param = simple_strtoul(argv[5], NULL, 0);
				if (param & 0x0002)
					pvi->type = VI_TYPE_TFT;
				else {
					pvi->type = 0;
					if (param & 0x0001)
						pvi->type |= VI_TYPE_DUALSCAN;
					if (param & 0x0004)
						pvi->type |= VI_TYPE_CSTN;
					if (param & 0x0008)
						pvi->type |= VI_TYPE_8BITBUS;
				}
				break;

			case SI_CONFIG:
				/* "config" is not available with lcdset */
				param = simple_strtoul(argv[5], NULL, 0);
				pvi->vspol = ((param & 0x00100000) != 0);
				pvi->hspol = ((param & 0x00200000) != 0);
				pvi->clkpol = ((param & 0x00400000) != 0);
				pvi->denpol = ((param & 0x00800000) != 0);
				break;

			default:
				/* All other commands have a counterpart in
				   lcdset, use common function */
				setvalue(ki->si, argv[5], pvi);
				break;
			}

			/* Update hardware with new settings */
			lcd_hw_vidinfo(pvi);

			/* Different to lcdset, reg set value does not echo
			   the final result when setting the values */
		}
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
	lcd_hw_wininfo(pwi, &vidinfo);
}

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


const fbpoolinfo_t *lcd_getfbpoolinfo(void)
{
	return &fbpool;
}

/************************************************************************/
/* GENERIC Initialization Routines					*/
/************************************************************************/
void cmd_lcd_init(void)
{
	WINDOW win;
	wininfo_t *pwi;

	/* Set framebuffer pool base into global data */
	gd->fb_base = fbpool.base;

	printf("####a\n");
	/* Initialize LCD controller (GPIOs, clock, etc.) */
	lcd_hw_init();

	printf("####b\n");

	/* Initialize vidinfo with the "no panel" settings */
	vidinfo = *lcd_getpanel(0);

	printf("####c\n");

	lcd_hw_vidinfo(&vidinfo);

	printf("####d\n");

	for (win = 0, pwi = wininfo; win < CONFIG_MAX_WINDOWS; win++, pwi++) {
		unsigned buf;
		u_char fbmaxcount;

		/* Clear structure */
		memset(pwi, 0, sizeof(wininfo_t));

	printf("####e\n");

		/* Initialize some entries */
		pwi->win = win;
		pwi->fbcount = 0;
		pwi->pix = DEFAULT_PIXEL_FORMAT;
		pwi->fg = lcd_rgba2col(pwi, DEFAULT_FG);
		pwi->bg = lcd_rgba2col(pwi, DEFAULT_BG);
		fbmaxcount = lcd_getfbmaxcount(win);
		pwi->fbmaxcount = fbmaxcount;

		for (buf=0; buf<CONFIG_MAX_BUFFERS_PER_WIN; buf++)
			pwi->fbuf[buf] = (buf < fbmaxcount) ? fbpool.base : 0;

	printf("####f\n");

		/* Update controller hardware with new wininfo */
		lcd_hw_wininfo(pwi, &vidinfo);
	printf("####g\n");

	}

	printf("####h\n");

	/* Initialize text console to use window 0 */
	console_init(&wininfo[0]);

	printf("####i\n");
}
