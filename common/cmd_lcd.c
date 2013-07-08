/*
 * Command lcd and general LCD support
 *
 * (C) Copyright 2012
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

/* ##### TODO:
    1. Prüfen, was bei LCD-Signalen schon von NBoot gesetzt ist
    2. PWM settings auf GPF15 setzen,
    3. Pattern matching bei lcd list evtl. case insensitive
    4. Kleines README.txt schreiben, wie man neuen Displaytreiber in das neue
       Treiberkonzept einbindet
    5. cls als Löschen des aktuellen Console-Windows nach console.c. Dazu im
       device (ähnlich getc/putc) auch eine cls-Funktion einfügen und diese
       dann beim Kommando aufrufen. Entsprechend müssten auch Kommandos zum
       Setzen der Console-Farben dort hin. --> als eigenen ChangeSet ins GIT
    6. Puffer für Kommando-Eingabe? Sonst klappt der Download von Scripten
       nicht. Ein solcher Software-Puffer ist sogar schon vorbereitet.
    7. Externes Tool schreiben, das eine WinCE-Displaydatei in die passenden
       U-Boot-Settings wandelt.
    8. Macht es Sinn, set_value() wieder nach do_lcd() einzubetten?
    9. Einfache Console weg und nur Multiple Console hin.
   10. Statt 1x, 2x, 3x, 4x sollte doch besser 1x, 2x, 4x, 8x bei Breite und
       Höhe hin. Oder noch besser: drei Bit: 0..7 entspricht 1x bis 8x.
   11. Idee: Statt ein eigenes Argument für ein Attribut zu nehmen (Text,
       Progress Bar, Bitmap), könnte man es mit Punkt an den Befehlsnamen
       anhängen. Es bestünde aus einer Buchstaben/Ziffern-Kombination:
         {l|r}:       l=left, r=right, lr=left of right, none: hcenter
	 {u|d}:       u=up, d=down, ud=over down, none: vcenter
         {e|f|g|h|c}: e=ru, f=rd, g=ld, h=lu, c=center
	 {1|2|4|8}:   Breite (x1, x2, x4, x8), negativ: Höhe
         b:           bold
         i:           inverse
	 u:           underline
         s:           strike-through
         n:           no background
       Beispiele:
         draw text.2-4buc: 2x breit, 4x hoch, bold, underlined, centered
	 draw bm.-22r:     2x breit, 2x hoch, links von der gegebenen x-Koord.
       Sinnvollerweise sollte man dann auch diese Namen im Expression-Parser
       verwenden.
   12. FG- und BG-Color mit . aneinander hängen.
       Beispiele:
         #ff0000.00ff00:     FG: rot+alpha 255, BG: grün+alpha 255
	 #0000ffff.00ffff00: FG: blau+alpha 255, BG: cyan+alpha 0
	 #ffff00.ffffff00:   FG: gelb+alpha 255, BG: weiß+alpha 0
	 #ffffff:            FG: weiß+alpha 255, BG: unverändert
	 #.000000:           FG: unverändert, BG: schwarz+alpha 255
   13. Neuer Befehl "draw font <addr>". Dann muss an der gegebenen Adresse ein
       Font mit dem gleichen Aufbau wie der interne Font abgelegt sein. Ab
       jetzt wird dann dieser Font verwendet (nur für draw-Befehle, nicht für
       Console!). Ohne Adresse oder mit Adresse 0 wird wieder der interne Font
       genutzt. Als Header sollte ein struct verwendet werden, in dem die
       Anzahl Zeichen, Breite, Höhe, Unterstreichungszeile und
       Durchstreichungszeile angegeben sind. Als Breite sollte dabei 32 das
       Maximum sein, damit eine Characterzeile immer in ein Register passt.
       Alternativ können diese Werte auch im Befehl draw font angegeben werden.
   14. Neuen Befehl oder neues Attributbit, das es erlaubt, nur in eine
       Alpha-Ebene zu zeichnen (speziell wenn nur ein A-Bit da ist). Durch das
       A-Bit kann man sozusagen zwei Bilder (disjunkt) überlagern. Beim
       Zeichnen von normalen Grafikelementen kann man ja durch Angabe
       einer Farbe mit passenden Alpha in die entsprechende Ebene zeichnen,
       aber bei Grafiken wird es schwieriger. Erstens ist es schwer, die
       konkrete Farbe bei transparenten Stellen in einem Grafikprogramm
       anzugeben und außerdem möchte man vielleicht einfach zwei Grafiken ohne
       Transparenz in die beiden Alpha-Ebenen legen. Idee: neuer Befehl idraw,
       der einen inversen Alpha-Wert annimmt, sich aber sonst wie draw
       verhält. iadraw macht auch Sinn, z.B. um zwei Bilder übereinander
       zeichnen zu können. Beim einen landen die sichtbaren Pixel in A1, beim
       andern in A0.
       Denkbar ist auch ein externes Programm, mit dem man solche Bilder
       kombinieren kann, um PNG-Grafiken mit den richtigen Alpha-Werten zu
       erzeugen.
   15. Wird eine geclippte schräge Linie mit gleichen Koordinaten über eine
       ungeclippte Linie gemalt, werden nicht alle Pixel perfekt überdeckt.
       Bei der Berechnung des dd-Offsets am Clipping-Rand stimmt also was
       nicht. Nochmal nachprüfen.
   16. alphamode 0 und 1 scheinen nicht zu funktionieren.
   17. FRC ist noch weitgehend ungetestet. Es tut sich was, aber noch nicht so
       wie erwartet.
   18. Konzept umstellen, dass bei Initialisierung ein board_init_xlcd()
       aufgerufen wird, das seinerseits die spezifischen LCD-Initialisierungen
       aufruft (in unserem Fall also s3c64xx_xlcd_init()), das wiederum dann
       ein register_xlcd_dev() aufruft, das dann hier wäre. Dann braucht es
       hier nicht alle möglichen Treiber-inits. Nach Rückkehr von
       board_init_xlcd() können alle LCDs aufgelistet werden. Dann wird die
       Initialisierungsfunktion jedes Displays aufgerufen, was wiederum die
       Windows registriert. Auf diese Weise könnte man auch eine lineare Liste
       aller Windows erzeugen, falls das besser ist. Diese Windows sollten dann
       bei lcd (ohne Argumente) ausgegeben werden. Eine lineare Liste hätte
       den Vorteil, dass mit einem "win n"-Befehl sowohl Display als auch
       Window gewechselt werden kann und bei der Angabe der Console nur das
       Window, nicht das Display angegeben werden muss.
   19. Bei win auch wie bei draw Befehlsgruppen konfigurierbar machen, Maske
       in CONFIG_XLCD_WIN und mögliche Werte als XLCD_WIN_* definieren.
   20. cmd_lcd.h -> xlcd.h umbenennen? Evtl. andere XLCD-Headers dort
       eingliedern.
   21. Die Zeiteinheit bei win fade funktioniert nicht, da man nur für eine
       gewisse Zeit warten kann und nicht schon die durch die Berechnung
       (set_wininfo()) verbrauchte Zeit abziehen kann. ***geändert, tut es
       jetzt???***
   22. Überlegen, ob man bei cmd_lcd.c noch was weglassen kann, wenn
       CONFIG_CMD_WIN nicht gesetzt ist.
   23. JPG-Support ist vorbereitet, aber noch nicht implementiert.
   24. Prüfen, ob man die [a]draw_ll-Funktionen optimieren kann. Zum Beispiel
       gibt es bei ARMv6 spezielle Assemblerbefehle, mit denen man die Bytes
       eines RGBA-Wertes zerteilen kann.
   25. Prüfen, ob man bei den [a]draw-ll-Funktionen die 2D-Beschleunigung
       nutzen kann. So kann man bei der bitblt-Befehl beim S3C6410 im
       Host-Modus betrieben werden, wo man jedes Pixel einzeln von der CPU
       nachschieben kann. Das könnte evtl. (zumindest bei den unterstützten
       Pixelformaten) eine enorme Beschleunigung bedeuten, da die Bitshifterei
       von der 2D-Einheit übernommen wird. Auch das Alpha-Blending könnte damit
       erledigt werden. Vielleicht könnte man auch *nur* das Alpha-Blending
       im RGBA8888-Modus damit machen und zumindest diese Zeit verbessern.
   26. Liste der LCD panels in lcd_panels.c erweitern.

****/

/*
 * LCD commands
 */

#include <config.h>
#include <common.h>
#include <cmd_lcd.h>			  /* Own interface */
#include <xlcd_panels.h>		  /* lcdinfo_t, lcd_getlcd(), ... */
#include <stdio_dev.h>			  /* stdio_dev, stdio_register(), ... */
#include <serial.h>			  /* serial_putc(), serial_puts() */
#include <linux/ctype.h>		  /* isdigit() */
#include <video_font.h>			  /* Get font data, width and height */

#if defined(CONFIG_S3C64XX)
#include <s3c64xx_xlcd.h>		  /* s3c64xx_xlcd_init() */
#endif

#ifdef CONFIG_CMD_WIN
extern void win_setenv(const  wininfo_t *pwi);
#endif

#ifdef CONFIG_CMD_CMAP
extern void set_default_cmap(const wininfo_t *pwi);
#endif

#ifdef CONFIG_XLCD_CONSOLE
extern void draw_ll_char(const wininfo_t *pwi, XYPOS x, XYPOS y, char c,
			 COLOR32 fg, COLOR32 bg);
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
#ifndef CONFIG_XLCD_DISPLAYS
#define CONFIG_XLCD_DISPLAYS 1
#endif

#ifndef CONFIG_XLCD_FBSIZE
#define CONFIG_XLCD_FBSIZE 0x00100000	  /* 1MB, enough for 800x600@16bpp */
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
#if (CONFIG_XLCD_DISPLAYS > 1)
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

/************************************************************************/
/* TYPES AND STRUCTURES							*/
/************************************************************************/


/************************************************************************/
/* LOCAL VARIABLES							*/
/************************************************************************/

/* Currently selected lcd panel */
static VID vid_sel;
static VID vid_count;
static int lockupdate;

static vidinfo_t vidinfo[CONFIG_XLCD_DISPLAYS]; /* Current display info */

static u_long fbused;			  /* Used part of frambuffer pool */

#ifndef CONFIG_XLCD_CONSOLE_MULTI
coninfo_t coninfo;			  /* Console information */
wininfo_t *console_pwi;			  /* Pointer to window with console */
#endif

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

/* Keywords available with command "lcd"; info1 and info2 are unused */
static kwinfo_t const lcd_kw[] = {
	/* Multiple arguments or argument with multiple types */
	[LI_PANEL] =	 {1, 1, 0, 0, "panel"},	   /* index | substring */
	[LI_DIM] =	 {2, 2, 0, 0, "dim"},	   /* hdim vdim */
	[LI_RES] =	 {2, 2, 0, 0, "res"},	   /* hres, vres */
	[LI_HTIMING] =	 {1, 3, 0, 0, "htiming"},  /* hfp [hsw [hbp]] */
	[LI_VTIMING] =	 {1, 3, 0, 0, "vtiming"},  /* vfp [vsw [vbp]] */
	[LI_POL] =	 {1, 4, 0, 0, "pol"},	   /* hspol [vspol [clkpol
						      [denpol]]]*/
	[LI_PWM] =	 {1, 2, 0, 0, "pwm"},	   /* pwmvalue [pwmfreq]*/
	[LI_PONSEQ] =	 {1, 5, 0, 0, "ponseq"},   /* logic [disp [contr
						      [pwm [bl]]]] */
	[LI_POFFSEQ] =	 {1, 5, 0, 0, "poffseq"},  /* bl [pwm [contr [disp
						      [logic]]]] */
	[LI_EXTRA] =	 {1, 2, 0, 0, "extra"},	   /* dither [drive] */
	[LI_LIST] =	 {0, 2, 0, 0, "list"},	   /* [index | substring
						      [count]]*/
	[LI_ON] =	 {0, 0, 0, 0, "on"},	   /* (no args) */
	[LI_OFF] =	 {0, 0, 0, 0, "off"},	   /* (no args) */
#if (CONFIG_XLCD_DISPLAYS > 1)
	[LI_ALL] =	 {0, 0, 0, 0, "all"},	   /* (no args) */
#endif
	[LI_NAME] =	 {1, 1, 0, 0, "name"},	   /* name */
	[LI_HDIM] =	 {1, 1, 0, 0, "hdim"},	   /* hdim */
	[LI_VDIM] =	 {1, 1, 0, 0, "vdim"},	   /* vdim */
	[LI_TYPE] =	 {1, 1, 0, 0, "type"},	   /* type */
	[LI_HFP] =	 {1, 1, 0, 0, "hfp"},	   /* hfp */
	[LI_HSW] =	 {1, 1, 0, 0, "hsw"},	   /* hsw */
	[LI_HBP] =	 {1, 1, 0, 0, "hbp"},	   /* hbp */
	[LI_HRES] =	 {1, 1, 0, 0, "hres"},	   /* hres */
	[LI_VFP] =	 {1, 1, 0, 0, "vfp"},	   /* vfp */
	[LI_VSW] =	 {1, 1, 0, 0, "vsw"},	   /* vsw */
	[LI_VBP] =	 {1, 1, 0, 0, "vbp"},	   /* vbp */
	[LI_VRES] =	 {1, 1, 0, 0, "vres"},	   /* vres */
	[LI_HSPOL] =	 {1, 1, 0, 0, "hspol"},	   /* hspol */
	[LI_VSPOL] =	 {1, 1, 0, 0, "vspol"},	   /* hspol */
	[LI_DENPOL] =	 {1, 1, 0, 0, "denpol"},   /* denpol */
	[LI_CLKPOL] =	 {1, 1, 0, 0, "clkpol"},   /* clkpol */
	[LI_CLK] =	 {1, 1, 0, 0, "clk"},	   /* clk */
	[LI_FPS] =	 {1, 1, 0, 0, "fps"},	   /* fps */
	[LI_DRIVE] =	 {1, 1, 0, 0, "drive"},	   /* strength */
	[LI_FRC] =	 {1, 1, 0, 0, "frc"},	   /* dither */
	[LI_PWMVALUE] =	 {1, 1, 0, 0, "pwmvalue"}, /* pwmvalue */
	[LI_PWMFREQ] =	 {1, 1, 0, 0, "pwmfreq"},  /* pwmfreq */
	[LI_PONLOGIC] =	 {1, 1, 0, 0, "ponlogic"}, /* ponlogic */
	[LI_PONDISP] =	 {1, 1, 0, 0, "pondisp"},  /* pondisp */
	[LI_PONCONTR] =	 {1, 1, 0, 0, "poncontr"}, /* pondisp */
	[LI_PONPWM] =	 {1, 1, 0, 0, "ponpwm"},   /* ponpwm */
	[LI_PONBL] =	 {1, 1, 0, 0, "ponbl"},	   /* ponbl */
	[LI_POFFLOGIC] = {1, 1, 0, 0, "pofflogic"},/* pofflogic */
	[LI_POFFDISP] =	 {1, 1, 0, 0, "poffdisp"}, /* poffdisp */
	[LI_POFFCONTR] = {1, 1, 0, 0, "poffcontr"},/* poffdisp */
	[LI_POFFPWM] =	 {1, 1, 0, 0, "poffpwm"},  /* poffpwm */
	[LI_POFFBL] =	 {1, 1, 0, 0, "poffbl"},   /* poffbl */
	[LI_HELP] =	 {0, 0, 0, 0, "help"},	   /* (no args, show usage) */
};

/************************************************************************/
/* Prototypes of local functions					*/
/************************************************************************/

/* Switch display on; return error message on error or NULL on success */
static char *lcd_on(vidinfo_t *pvi);

/* Switch display off */
static void lcd_off(vidinfo_t *pvi);

/* Get a pointer to the lcd panel information */
static vidinfo_t *lcd_get_vidinfo_p(VID vid);

/* If not locked, update lcd hardware and set environment variable */
static void set_vidinfo(vidinfo_t *pvi);

/* Draw character to console */
static void console_putc(wininfo_t *pwi, coninfo_t *pci, char c);

/* Relocate all windows to newaddr, starting at given window (via pwi) */
static void relocbuffers(wininfo_t *pwi, u_long newaddr);

/* Set window resolution */
static int set_winres(wininfo_t *pwi, XYPOS hres, XYPOS vres);

/* Increment all PONSEQ or POFFSEQ entries that come after the current one */
static void set_delay(u_short *delays, int index, u_short value);

/* Set lcd parameters with one numeric argument */
static int set_value(vidinfo_t *pvi, u_int param, u_int sc);

/* Show vidinfo as text */
static void show_vidinfo(const vidinfo_t *pvi);

/* Handle lcd command */
static int do_lcd(cmd_tbl_t *cmdtp, int flag, int argc, char *const argv[]);

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


/* If not locked, update lcd hardware and set environment variable */
static void set_vidinfo(vidinfo_t *pvi)
{
	char buf[CONFIG_SYS_CBSIZE];
	char *cmd = "lcd";
	char *s = buf;
	const lcdinfo_t *lcd;		  /* Current display */
	const lcdinfo_t *def;		  /* Default display */

	/* If update should not take place, return immediately */
	if (lockupdate)
		return;

	/* If new display has hres or vres 0 and display was switched on,
	   switch it off now. The most common case is if panel #0 is set. */
	lcd = &pvi->lcd;
	if ((!lcd->hres || !lcd->vres) && pvi->is_enabled)
		lcd_off(pvi);

	/* Call hardware specific function to update controller hardware */
	pvi->set_vidinfo(pvi);

	/* If display is not active (hres or vres is 0), unset environment
	   variable */
	if (!lcd->hres || !lcd->vres) {
		setenv(pvi->name, NULL);
		return;
	}

	/* Only set values that differ from the default panel #0 */ 
	def = lcd_get_lcdinfo_p(0);

	/* Prepare environment string for this lcd panel */
	s += sprintf(s, "%s %s \"%s\"", cmd, lcd_kw[LI_NAME].keyword,
		     lcd->name);
	if (lcd->type != def->type) {
		s += sprintf(s, "; %s %s %u", cmd, lcd_kw[LI_TYPE].keyword,
			     lcd->type);
	}
	if ((lcd->hdim != def->hdim) || (lcd->vdim != def->vdim)) {
		s += sprintf(s, "; %s %s %u %u", cmd, lcd_kw[LI_DIM].keyword,
			     lcd->hdim, lcd->vdim);
	}
	if ((lcd->hres != def->hres) || (lcd->vres != def->vres)) {
		s += sprintf(s, "; %s %s %u %u", cmd, lcd_kw[LI_RES].keyword,
			     lcd->hres, lcd->vres);
	}
	if ((lcd->hfp != def->hfp) || (lcd->hsw != def->hsw)
	    || (lcd->hbp != def->hbp)) {
		s += sprintf(s, "; %s %s %u %u %u", cmd,
			     lcd_kw[LI_HTIMING].keyword,
			     lcd->hfp, lcd->hsw, lcd->hbp);
	}
	if ((lcd->vfp != def->vfp) || (lcd->vsw != def->vsw)
	    || (lcd->vbp != def->vbp)) {
		s += sprintf(s, "; %s %s %u %u %u", cmd,
			     lcd_kw[LI_VTIMING].keyword,
			     lcd->vfp, lcd->vsw, lcd->vbp);
	}
	if (lcd->pol != def->pol) {
		s += sprintf(s, "; %s %s %u %u %u %u", cmd,
			     lcd_kw[LI_POL].keyword,
			     ((lcd->pol & HS_LOW) != 0),
			     ((lcd->pol & VS_LOW) != 0),
			     ((lcd->pol & DEN_LOW) != 0),
			     ((lcd->pol & CLK_FALLING) != 0));
	}
	if (lcd->fps != def->fps) {
		s += sprintf(s, "; %s %s %u", cmd, lcd_kw[LI_FPS].keyword,
			     lcd->fps);
	}
	if ((lcd->pwmvalue != def->pwmvalue) || (lcd->pwmfreq != def->pwmfreq)){
		s += sprintf(s, "; %s %s %u %u", cmd, lcd_kw[LI_PWM].keyword,
			     lcd->pwmvalue, lcd->pwmfreq);
	}
	if ((lcd->ponseq[0] != def->ponseq[0])
	    || (lcd->ponseq[1] != def->ponseq[1])
	    || (lcd->ponseq[1] != def->ponseq[2])
	    || (lcd->ponseq[1] != def->ponseq[3])
	    || (lcd->ponseq[1] != def->ponseq[4])) {
		s += sprintf(s, "; %s %s %u %u %u %u %u", cmd,
			     lcd_kw[LI_PONSEQ].keyword, lcd->ponseq[0],
			     lcd->ponseq[1], lcd->ponseq[2],
			     lcd->ponseq[3], lcd->ponseq[4]);
	}
	if ((lcd->poffseq[0] != def->poffseq[0])
	    || (lcd->poffseq[1] != def->poffseq[1])
	    || (lcd->poffseq[1] != def->poffseq[2])
	    || (lcd->poffseq[1] != def->poffseq[3])
	    || (lcd->poffseq[1] != def->poffseq[4])) {
		s += sprintf(s, "; %s %s %u %u %u %u %u", cmd,
			     lcd_kw[LI_POFFSEQ].keyword, lcd->poffseq[0],
			     lcd->poffseq[1], lcd->poffseq[2],
			     lcd->poffseq[3], lcd->poffseq[4]);
	}
	s += sprintf(s, "; %s %s %u %u", cmd, lcd_kw[LI_EXTRA].keyword,
		     pvi->frc, pvi->drive);

	/* Set the environment variable */
	setenv(pvi->name, buf);
}


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
	cls, 1, 1, cls,
	"clear screen",
	NULL
);
#endif //0 #####


/************************************************************************/
/* CONSOLE SUPPORT							*/
/************************************************************************/

#ifndef CONFIG_XLCD_CONSOLE_MULTI
/* Initialize the console with the given window */
void console_init(wininfo_t *pwi, RGBA fg, RGBA bg)
{
	/* Initialize the console */
	console_pwi = pwi;
	coninfo.x = 0;
	coninfo.y = 0;
	coninfo.fg = pwi->ppi->rgba2col(pwi, fg);
	coninfo.bg = pwi->ppi->rgba2col(pwi, bg);
	console_cls(pwi, coninfo.bg);
}


/* If the window is the console window, re-initialize console */
void console_update(wininfo_t *pwi, RGBA fg, RGBA bg)
{
	if (console_pwi->win == pwi->win)
		console_init(pwi, fg, bg);
}
#endif /* CONFIG_XLCD_CONSOLE_MULTI */

/* Clear the console window with given color */
void console_cls(const wininfo_t *pwi, COLOR32 col)
{
	memset32((unsigned *)pwi->pfbuf[pwi->fbdraw], col2col32(pwi, col),
		 pwi->fbsize/4);
}


#define TABWIDTH (8 * VIDEO_FONT_WIDTH)	  /* 8 chars for tab */
static void console_putc(wininfo_t *pwi, coninfo_t *pci, char c)
{
	int x = pci->x;
	int y = pci->y;
	int xtab;
	COLOR32 fg = pci->fg;
	COLOR32 bg = pci->bg;
	int fbhres = pwi->fbhres;
	int fbvres = pwi->fbvres;

	pwi->attr = 0;
	switch (c) {
	case '\t':			  /* Tab */
		xtab = ((x / TABWIDTH) + 1) * TABWIDTH;
		while ((x + VIDEO_FONT_WIDTH <= fbhres) && (x < xtab)) {
			draw_ll_char(pwi, x, y, ' ', fg, bg);
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
		draw_ll_char(pwi, x, y, ' ', fg, bg);
		break;

	default:			  /* Character */
		draw_ll_char(pwi, x, y, c, fg, bg);
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

	pci->x = (u_short)x;
	pci->y = (u_short)y;
}


/*----------------------------------------------------------------------*/

#ifdef CONFIG_XLCD_CONSOLE_MULTI
void lcd_putc(const struct stdio_dev *pdev, const char c)
{
	wininfo_t *pwi = (wininfo_t *)pdev->priv;
	vidinfo_t *pvi = pwi->pvi;

	if (pvi->is_enabled && pwi->active)
		console_putc(pwi, &pwi->ci, c);
	else
		serial_putc(NULL, c);
}
#else
void lcd_putc(const struct stdio_dev *pdev, const char c)
{
	wininfo_t *pwi = console_pwi;
	vidinfo_t *pvi = pwi->pvi;

	if (pvi->is_enabled && pwi->active)
		console_putc(pwi, &coninfo, c);
	else
		serial_putc(NULL, c);
}
#endif /*CONFIG_XLCD_CONSOLE_MULTI*/

/*----------------------------------------------------------------------*/

#ifdef CONFIG_XLCD_CONSOLE_MULTI
void lcd_puts(const struct stdio_dev *pdev, const char *s)
{
	wininfo_t *pwi = (wininfo_t *)pdev->priv;
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
		serial_puts(NULL, s);
}
#else
void lcd_puts(const struct stdio_dev *pdev, const char *s)
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
		serial_puts(NULL, s);
}
#endif /*CONFIG_XLCD_CONSOLE_MULTI*/


/************************************************************************/
/* Window support as far as required for general LCD function		*/
/************************************************************************/

/* Set window resolution */
static int set_winres(wininfo_t *pwi, XYPOS hres, XYPOS vres)
{
	XYPOS fbhres, fbvres;
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



/************************************************************************/
/* Command lcd								*/
/************************************************************************/

/* Increment all PONSEQ or POFFSEQ entries that come after the current one */
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


/* Handle all lcd commands with a numeric parameter */
static int set_value(vidinfo_t *pvi, u_int param, u_int sc)
{
	XYPOS hres, vres;

	switch (sc)
	{
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

	case LI_HRES:			  /* Parse XYPOS */
		hres = (XYPOS)param;
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

	case LI_VRES:			  /* Parse XYPOS */
		hres = pvi->lcd.hres;
		vres = (XYPOS)param;

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


/* Show vidinfo as text */
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


/* Handle lcd command */
static int do_lcd(cmd_tbl_t *cmdtp, int flag, int argc, char *const argv[])
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

#if (CONFIG_XLCD_DISPLAYS > 1)
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
#endif /* CONFIG_XLCD_DISPLAYS > 1 */

	/* Search for regular lcd sub-commands */
	sc = parse_sc(argc, argv[1], LI_HELP, lcd_kw, ARRAY_SIZE(lcd_kw));

	/* If not recognized, print usage and return */
	if (sc == LI_HELP) {
		cmd_usage(cmdtp);
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
		pvi->lcd.hdim = (u_short)simple_strtoul(argv[2], NULL, 0);
		pvi->lcd.vdim = (u_short)simple_strtoul(argv[3], NULL, 0);
		break;

	case LI_RES:		  /* Parse exactly 2 XYPOS'es */
		pvi->lcd.hres = (XYPOS)simple_strtoul(argv[2], NULL, 0);
		pvi->lcd.vres = (XYPOS)simple_strtoul(argv[3], NULL, 0);
		break;

	case LI_HTIMING:	  /* Parse up to 3 u_shorts */
		pvi->lcd.hfp = (u_short)simple_strtoul(argv[2], NULL, 0);
		if (argc < 4)
			break;
		pvi->lcd.hsw = (u_short)simple_strtoul(argv[3], NULL, 0);
		if (argc < 5)
			break;
		pvi->lcd.hbp = (u_short)simple_strtoul(argv[4], NULL, 0);
		break;

	case LI_VTIMING:	  /* Parse up to 3 u_shorts */
		pvi->lcd.vfp = (u_short)simple_strtoul(argv[2], NULL, 0);
		if (argc < 4)
			break;
		pvi->lcd.vsw = (u_short)simple_strtoul(argv[3], NULL, 0);
		if (argc < 5)
			break;
		pvi->lcd.vbp = (u_short)simple_strtoul(argv[4], NULL, 0);
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
				param = simple_strtoul(argv[i+2], NULL, 0);
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
		pvi->frc = ( simple_strtoul(argv[2], NULL, 0) != 0);
		if (argc < 4)
			break;
		pvi->drive = (u_char)simple_strtoul(argv[3], NULL, 0);
		break;

	case LI_NAME:		  /* Parse string */
		strncpy(pvi->lcd.name, argv[2], MAX_NAME);
		pvi->lcd.name[MAX_NAME-1] = 0;
		break;

#if (CONFIG_XLCD_DISPLAYS > 1)
	case LI_ALL: {
		VID vid;

		for (vid=0; vid<vid_count; vid++)
			show_vidinfo(lcd_get_vidinfo_p(vid));
		return 0;
	}
#endif

	default:		  /* Parse one numeric argument */
		if (set_value(pvi, simple_strtoul(argv[2], NULL, 0), sc))
			return 1;
		break;
	}

	/* Update controller hardware with new settings */
	set_vidinfo(pvi);

	return 0;
}

U_BOOT_CMD(
	lcd, 7, 0, do_lcd,
	"set or show lcd panel parameters",
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
#if (CONFIG_XLCD_DISPLAYS > 1)
	"lcd all\n"
	"    - list information of all displays\n"
	"lcd n\n"
	"    - select display n\n"
#endif
	"lcd\n"
	"    - show all current lcd panel settings\n"
);


/************************************************************************/
/* Exported functions							*/
/************************************************************************/

/* Move offset if window would not fit within framebuffer */
void fix_offset(wininfo_t *wi)
{
	/* Move offset if necessary; window must fit into image buffer */
	if (wi->hoffs + wi->hres > wi->fbhres)
		wi->hoffs = (wi->fbhres - wi->hres);
	if (wi->voffs + wi->vres > wi->fbvres)
		wi->voffs = (wi->fbvres - wi->vres);
}


/* Set new framebuffer resolution, pixel format, and/or framebuffer count for
   the given window */
int setfbuf(wininfo_t *pwi, XYPOS hres, XYPOS vres,
	    XYPOS fbhres, XYPOS fbvres, PIX pix, u_char fbcount)
{
	XYPOS fbmaxhres, fbmaxvres;
	u_long oldsize, newsize;
	u_long linelen, fbsize;
	u_long addr;
	WINDOW win;
	const pixinfo_t *ppi;
	const vidinfo_t *pvi;
	DECLARE_GLOBAL_DATA_PTR;

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

	if (fbused - oldsize + newsize > gd->fb_size) {
		puts("Framebuffer pool too small\n");
		return 1;
	}

	/* OK, the new settings can be made permanent */
	pwi->hres = hres;
	pwi->vres = vres;
	pwi->fbhres = fbhres;
	pwi->fbvres = fbvres;
	pwi->clip_left = 0;
	pwi->clip_top = 0;
	pwi->clip_right = fbhres - 1;
	pwi->clip_bottom = fbvres - 1;
	pwi->pbi.x1 = 0;
	pwi->pbi.x2 = fbhres - 1;
	pwi->pbi.y1 = 0;
	pwi->pbi.y2 = fbvres - 1;
	pwi->horigin = 0;
	pwi->vorigin = 0;
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
#ifdef CONFIG_CMD_CMAP
		set_default_cmap(pwi);
#endif
		lcd_set_fg(pwi, DEFAULT_FG);
		lcd_set_bg(pwi, DEFAULT_BG);
		lcd_set_col(pwi, DEFAULT_FG, &pwi->pbi.rect_fg);
		lcd_set_col(pwi, DEFAULT_BG, &pwi->pbi.rect_bg);
		lcd_set_col(pwi, DEFAULT_FG, &pwi->pbi.text_fg);
		lcd_set_col(pwi, DEFAULT_BG, &pwi->pbi.text_bg);
		pwi->ai[0].alpha = DEFAULT_ALPHA0;
		pwi->ai[0].time = 0;
		pwi->ai[1].alpha = DEFAULT_ALPHA1;
		pwi->ai[1].time = 0;
		pwi->alphamode = (pwi->ppi->flags & PIF_ALPHA) ? 2 : 1;
		pwi->text_attr = 0;
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
		used = oldaddr - gd->fb_base;

		/* Used mem for all subsequent windows */
		used = fbused - used;

		/* Update framebuffer pool */
		fbused = fbused - oldsize + newsize;

		/* Move framebuffers of all subsequent windows in one go */
		memmove((void *)newaddr, (void *)oldaddr, used);

		/* Then relocate the image buffer addresses of this and all
		   subsequent windows */
		relocbuffers(pwi, addr);
	}

	/* Clear the new framebuffer with console color */
#ifdef CONFIG_XLCD_CONSOLE_MULTI
	pwi->ci.x = 0;
	pwi->ci.y = 0;
	pwi->ci.fg = pwi->ppi->rgba2col(pwi, DEFAULT_CON_FG);
	pwi->ci.bg = pwi->ppi->rgba2col(pwi, DEFAULT_CON_BG);
	console_cls(pwi, pwi->ci.bg);
#else
	/* The console might also be interested in the buffer changes */
	console_update(pwi, DEFAULT_CON_FG, DEFAULT_CON_BG);
#endif

	return 0;
}


/* If not locked, update window hardware and set environment variable */
void set_wininfo(const wininfo_t *pwi)
{
	/* If update should not take place, return immediately */
	if (lockupdate)
		return;

	/* Call hardware specific function to update controller hardware */
	pwi->pvi->set_wininfo(pwi);

#ifdef CONFIG_CMD_WIN
	win_setenv(pwi);
#endif
}


/* Get a pointer to the wininfo structure */
wininfo_t *lcd_get_wininfo_p(const vidinfo_t *pvi, WINDOW win)
{
	return (win < pvi->wincount) ? &pvi->pwi[win] : NULL;
}


/* Get a pointer to the vidinfo structure of the currently selected display */
vidinfo_t *lcd_get_sel_vidinfo_p(void)
{
	return lcd_get_vidinfo_p(vid_sel);
}


/* Set colinfo structure */
void lcd_set_col(wininfo_t *pwi, RGBA rgba, colinfo_t *pci)
{
#ifdef CONFIG_XLCD_ADRAW
	RGBA alpha1;

	/* Premultiply alpha for apply_alpha functions */
	alpha1 = rgba & 0x000000FF;
	pci->A256 = 256 - alpha1;
	alpha1++;
	pci->RA1 = (rgba >> 24) * alpha1;
	pci->GA1 = ((rgba >> 16) & 0xFF) * alpha1;
	pci->BA1 = ((rgba >> 8) & 0xFF) * alpha1;
#endif

	/* Store RGBA value */
	pci->rgba = rgba;

	/* Store COLOR32 value */
	pci->col = pwi->ppi->rgba2col(pwi, rgba);
}


/* Repeat color value so that it fills the whole 32 bits */
COLOR32 col2col32(const wininfo_t *pwi, COLOR32 color)
{
	switch (pwi->ppi->bpp_shift) {
	case 0:
		color |= color << 1;
		/* Fall through to case 1 */
	case 1:
		color |= color << 2;
		/* Fall through to case 2 */
	case 2:
		color |= color << 4;
		/* Fall through to case 3 */
	case 3:
		color |= color << 8;
		/* Fall through to case 4 */
	case 4:
		color |= color << 16;
		/* Fall through to default */
	default:
		break;
	}

	return color;
}


/* Search for nearest color in the color map */
COLOR32 lcd_rgbalookup(RGBA rgba, RGBA *cmap, unsigned count)
{
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
}


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
			case 7:			/* #RRGGBB, set as opaque */
				rgba = (rgba << 8) | 0xFF;
				/* Fall through to case 9 */
			case 9:			/* #RRGGBBAA */
				*prgba = rgba;
				return 0;	/* Success */
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
			else if (argc <= pki->argc_max+2)
				sc = i; /* OK */
			break;
		}
	}

	return sc;
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

/* This function is called rather early during initialization. No memory is
   available yet, just a preliminary stack. So don't use global or static
   variables, just automatic variables on the stack. Return the address of the
   framebuffer by decreasing the given address by the framebuffer size. The
   framebuffer size can be set with environment variable fbsize. */
ulong lcd_setmem(ulong addr)
{
	ulong fbsize;

	/* If environment variable fbsize is set, use it as size for the
	   framebuffer pool size (in KB, decimal) */
	fbsize = getenv_ulong("fbsize", 10, CONFIG_XLCD_FBSIZE);

	fbsize = (fbsize + (PAGE_SIZE - 1)) & ~(PAGE_SIZE - 1);

	return addr - fbsize;
}

void drv_lcd_init(void)
{
	DECLARE_GLOBAL_DATA_PTR;
	WINDOW win;
	VID vid;
	wininfo_t *pwi;
	vidinfo_t *pvi;
	struct stdio_dev lcddev;
	u_long fbuf;
	char *s;

	memset(&lcddev, 0, sizeof(lcddev));

	/* Initialize LCD controller(s) (GPIOs, clock, etc.) */
#ifdef CONFIG_S3C64XX
	if (vid_count < CONFIG_XLCD_DISPLAYS)
		s3c64xx_xlcd_init(lcd_get_vidinfo_p(vid_count++));
#endif

	/* Initialize all display entries and window entries */
	fbuf = gd->fb_base;
	for (vid = 0; vid < vid_count; vid++) {
		pvi = lcd_get_vidinfo_p(vid);

		/* Init all displays with the "no panel" LCD panel, and set
		   vid, win_sel and is_enabled entries; all other entries are
		   already set by the controller specific init function(s)
		   above */
		pvi->vid = vid;
		pvi->is_enabled = 0;
		pvi->win_sel = 0;
#if (CONFIG_XLCD_DISPLAYS > 1)
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

			/* Window information */
			pwi->win = win;
			pwi->active = 0;
			pwi->pix = pwi->defpix;
			pwi->ppi = pvi->get_pixinfo_p(win, pwi->defpix);
			pwi->hres = 0;
			pwi->vres = 0;
			pwi->hpos = 0;
			pwi->vpos = 0;

			/* Framebuffer information */
			for (buf = 0; buf < pwi->fbmaxcount; buf++)
				pwi->pfbuf[buf] = gd->fb_base;
			pwi->fbsize = 0;
			pwi->linelen = 0;
			pwi->fbcount = 0;
			pwi->fbdraw = 0;
			pwi->fbshow = 0;
			pwi->fbhres = 0;
			pwi->fbvres = 0;
			pwi->hoffs = 0;
			pwi->voffs = 0;

			/* Drawing information */
			pwi->text_attr = 0;
			pwi->clip_left = 0;
			pwi->clip_top = 0;
			pwi->clip_right = 0;
			pwi->clip_bottom = 0;
			pwi->horigin = 0;
			pwi->vorigin = 0;
			lcd_set_fg(pwi, DEFAULT_FG);
			lcd_set_bg(pwi, DEFAULT_BG);
			pwi->pbi.x1 = 0;
			pwi->pbi.y1 = 0;
			pwi->pbi.x2 = 0;
			pwi->pbi.y2 = 0;
			lcd_set_col(pwi, DEFAULT_FG, &pwi->pbi.rect_fg);
			lcd_set_col(pwi, DEFAULT_BG, &pwi->pbi.rect_bg);
			lcd_set_col(pwi, DEFAULT_FG, &pwi->pbi.text_fg);
			lcd_set_col(pwi, DEFAULT_BG, &pwi->pbi.text_bg);
			pwi->pbi.attr = ATTR_HFOLLOW | ATTR_VCENTER;
			pwi->pbi.prog = 0;

			/* Alpha and color keying information */
			pwi->ai[0].alpha = DEFAULT_ALPHA0;
			pwi->ai[0].time = 0;
			pwi->ai[1].alpha = DEFAULT_ALPHA1;
			pwi->ai[1].time = 0;
			pwi->alphamode = (pwi->ppi->flags & PIF_ALPHA) ? 2 : 1;
			pwi->ckvalue = 0; /* Off */
			pwi->ckmask = 0;  /* Bits must match */
			pwi->ckmode = 0;  /* Check window pixels, no blend */

#if (CONFIG_XLCD_DISPLAYS > 1)
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

#ifdef CONFIG_CMD_WIN
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
#endif

			/* Set new wininfo to controller hardware */
			set_wininfo(pwi);

#ifdef CONFIG_XLCD_CONSOLE_MULTI
			/* Set console info for this window */
			pwi->ci.fg = pwi->ppi->rgba2col(pwi, DEFAULT_CON_FG);
			pwi->ci.bg = pwi->ppi->rgba2col(pwi, DEFAULT_CON_BG);
			pwi->ci.x = 0;
			pwi->ci.y = 0;

			/* Init a stdio device for each window */
			strcpy(lcddev.name, pwi->name);
			lcddev.ext   = 0;		  /* No extensions */
			lcddev.flags = DEV_FLAGS_OUTPUT;  /* Output only */
			lcddev.putc  = lcd_putc;	  /* 'putc' function */
			lcddev.puts  = lcd_puts;	  /* 'puts' function */
			lcddev.priv  = pwi;		  /* Call-back arg */
			stdio_register(&lcddev);
#endif /*CONFIG_XLCD_CONSOLE_MULTI*/
		}

		pvi->win_sel = 0;

		/* Add a stdio device for each display */
		memset(&lcddev, 0, sizeof(lcddev));

		strcpy(lcddev.name, pvi->name);
		lcddev.ext   = 0;		  /* No extensions */
		lcddev.flags = DEV_FLAGS_OUTPUT;  /* Output only */
		lcddev.putc  = lcd_putc;	  /* 'putc' function */
		lcddev.puts  = lcd_puts;	  /* 'puts' function */
		lcddev.priv  = lcd_get_wininfo_p(pvi, 0);  /* Call-back arg */

		stdio_register(&lcddev);

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

#ifndef CONFIG_XLCD_CONSOLE_MULTI
	/* Default console "lcd" on vid 0, win 0 */
	console_init(lcd_get_wininfo_p(lcd_get_vidinfo_p(0), 0),
		     DEFAULT_CON_FG, DEFAULT_CON_BG);
#endif
}
