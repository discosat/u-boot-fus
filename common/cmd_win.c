/*
 * Command win
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

#include <config.h>
#include <common.h>
#include <cmd_lcd.h>			  /* parse_sc(), wininfo_t, ... */
#include <linux/ctype.h>		  /* isdigit(), toupper() */
#include <watchdog.h>			  /* WATCHDOG_RESET */


/************************************************************************/
/* DEFINITIONS								*/
/************************************************************************/

/* Default alpha values */
#define DEFAULT_ALPHA0 0x00000000	  /* Transparent */
#define DEFAULT_ALPHA1 0xFFFFFFFF	  /* Opaque */

/* Settings for win ident blinking */
#define IDENT_BLINK_COUNT 5		  /* How often */
#define IDENT_BLINK_DELAY 100000	  /* How fast */


/************************************************************************/
/* ENUMERATIONS								*/
/************************************************************************/

/* Settings that correspond with command "window"; the order decides in what
   sequence the commands are searched for, which may be important for
   sub-commands with the same prefix. */
enum WIN_INDEX {
	WI_FBRES,
	WI_SHOW,
	WI_RES,
	WI_OFFS,
	WI_POS,
	WI_ALPHA0,
	WI_ALPHA1,
	WI_ALPHAM,
	WI_COLKEY,
	WI_FADE,
	WI_IDENT,
	WI_ALL,

	/* Unkown window sub-command; must be the last entry or the window
	   extension commands won't work */
	WI_HELP
};


/************************************************************************/
/* TYPES AND STRUCTURES							*/
/************************************************************************/


/************************************************************************/
/* LOCAL VARIABLES							*/
/************************************************************************/

/* Keywords available with command "window"; info1 and info2 are unused */
static kwinfo_t const win_kw[] = {
	[WI_FBRES] =  {1, 4, 0, 0, "fbres"}, /* fbhres fbvres [pix [fbcount]] */
	[WI_SHOW] =   {1, 2, 0, 0, "show"},  /* fbshow [fbdraw] */
	[WI_RES] =    {2, 2, 0, 0, "res"},   /* hres vres */
	[WI_OFFS] =   {2, 2, 0, 0, "offs"},  /* hoffs voffs */
	[WI_POS] =    {2, 2, 0, 0, "pos"},   /* hpos vpos */
	[WI_ALPHA0] = {1, 2, 0, 0, "alpha0"},/* alpha0 [time] */
	[WI_ALPHA1] = {1, 2, 0, 0, "alpha1"},/* alpha1 [time] */
	[WI_ALPHAM] = {1, 1, 0, 0, "alpham"},/* mode */
	[WI_COLKEY] = {1, 3, 0, 0, "colkey"},/* value [mask [mode]] */
	[WI_FADE] =   {0, 0, 0, 0, "fade"},  /* (no args) */
	[WI_IDENT] =  {0, 0, 0, 0, "ident"}, /* (no args) */
	[WI_ALL] =    {0, 0, 0, 0, "all"},   /* (no args) */
	[WI_HELP] =   {0, 0, 0, 0, "help"},  /* (no args, show usage) */
};


/************************************************************************/
/* Prototypes of local functions					*/
/************************************************************************/

/* Print the alpha information for channel alpha0 or alpha1 */
static void show_alphainfo(const wininfo_t *pwi, int channel);

/* Print the window information */
static void show_wininfo(const wininfo_t *pwi);

/* Fade alpha value of all windows */
static void fade_alpha(void);

/* Handle win command */
static int do_win(cmd_tbl_t *cmdtp, int flag, int argc, char *const argv[]);



/************************************************************************/
/* Command window							*/
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

/* Print the alpha information for alpha0 or alpha1 */
static void show_alphainfo(const wininfo_t *pwi, int channel)
{
	const alphainfo_t *pai = &pwi->ai[channel];

	printf("Alpha%d Setting:\t#%06x", channel, pai->alpha>>8);
	if (pai->time) {
		printf(", fade from #%06x to #%06x @ %u/%u ms",
		       pai->from>>8, pai->to>>8, pai->now, pai->time);
	}
	puts("\n");
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
	printf("Alphamode:\t%u\n", pwi->alphamode);
	show_alphainfo(pwi, 0);
	show_alphainfo(pwi, 1);
	printf("Color Keying:\tckvalue=#%08x, ckmask=#%08x, ckmode=0x%x\n\n",
	       pwi->ckvalue, pwi->ckmask, pwi->ckmode);
}

/* Fade alpha value of all windows of the selected display */
static void fade_alpha(void)
{
	int done;
	WINDOW win;
	vidinfo_t *pvi = lcd_get_sel_vidinfo_p();

	do {
		ulong timer = get_timer(0);

		done = 1;
		for (win = 0; win < pvi->wincount; win++) {
			int a;
			int update = 0;
			wininfo_t *pwi = lcd_get_wininfo_p(pvi, win);

			for (a = 0; a < 2; a++) {
				alphainfo_t *pai = &pwi->ai[a];
				RGBA alpha;
				int from, to;
				int time, now;

				if (!pai->time)
					continue;

				now = ++(pai->now);
				time = pai->time;

				/* Handle R */
				to = (int)(pai->to >> 24);
				from = (int)(pai->from >> 24);
				from += (to - from) * now / time;
				alpha = ((RGBA)from) << 24;

				/* Handle G */
				to = (int)((pai->to & 0xFF0000) >> 16);
				from = (int)((pai->from&0xFF0000)>>16);
				from += (to - from) * now / time;
				alpha |= ((RGBA)from) << 16;

				/* Handle B */
				to = (int)((pai->to & 0xFF00) >> 8);
				from = (int)((pai->from & 0xFF00) >> 8);
				from += (to - from) * now / time;
				alpha |= ((RGBA)from) << 8;

				if (alpha != pai->alpha) {
					pai->alpha = alpha;
					update = 1;
				}
				if (now >= time)
					pai->time = 0;
				else
					done = 0;
			} /* for (a) */
			if (update)
				set_wininfo(pwi);
		} /* for (win) */

		/* Wait the remaining time to 1ms */
		while (get_timer(timer) < 1000)
			;

		WATCHDOG_RESET();
	} while (!done && !ctrlc());
}

/* Handle win command */
static int do_win(cmd_tbl_t *cmdtp, int flag, int argc, char *const argv[])
{
	vidinfo_t *pvi;
	wininfo_t *pwi;
	WINDOW win;
	u_short sc;
	char c;

	/* Get the info for the currently selected display and window */
	pvi = lcd_get_sel_vidinfo_p();
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
		cmd_usage(cmdtp);
		return 1;
	}

	switch (sc) {
	case WI_FBRES: {
		XYPOS fbhres, fbvres;
		XYPOS hres, vres;
		u_char pix, fbcount;

		/* Arguments 1+2: fbhres and fbvres */
		fbhres = (XYPOS)simple_strtoul(argv[2], NULL, 0);
		fbvres = (XYPOS)simple_strtoul(argv[3], NULL, 0);

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
		XYPOS hres, vres;

		/* Arguments 1+2: hres and vres */
		hres = (XYPOS)simple_strtoul(argv[2], NULL, 0);
		vres = (XYPOS)simple_strtoul(argv[3], NULL, 0);

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

	case WI_ALPHA0:
	case WI_ALPHA1:
	{
		RGBA alpha;
		u_int time;
		alphainfo_t *pai;

		if (parse_rgb(argv[2], &alpha))
			return 1;
		time = (argc > 3) ? simple_strtol(argv[3], NULL, 0) : 0;
		if (time < 0)
			time = 0;
		pai = (sc== WI_ALPHA0) ? &pwi->ai[0] : &pwi->ai[1];
		pai->to = alpha;
		pai->from = pai->alpha;
		pai->time = time;
		pai->now = 0;
		if (time == 0) {
			/* Set hardware immediately */
			pai->alpha = alpha;
			set_wininfo(pwi);
		}
		break;
	}

	case WI_ALPHAM:
	{
		u_int alphamode;

		alphamode = simple_strtoul(argv[2], NULL, 0);
		if (alphamode > 2) {
			puts("Illegal alpha mode\n");
			return 1;
		}
		pwi->alphamode = (u_char)alphamode;
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

	case WI_FADE:
		fade_alpha();
		break;

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
	win, 7, 0, do_win,
	"set framebuffer and overlay window parameters",
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
	"win colkey rgbaval [rgbamask [mode]]"
	"    - Set per-window color key value, mask and mode\n"
	"win alpham mode\n"
	"    - Set alpha mode\n"
	"win alpha0 val time\n"
	"    - Set alpha0 value and fading time (0: set immediately)\n"
	"win alpha1 val time\n"
	"    - Set alpha1 value and fading time (0: set immediately)\n"
	"win fade\n"
	"    - Fade alpha values for all windows\n"
	"win all\n"
	"    - List all windows of the current display\n"
	"win\n"
	"    - Show all settings of selected window\n"
);


/************************************************************************/
/* Exported functions							*/
/************************************************************************/

/* Set the environment variable according to this window */
void win_setenv(const wininfo_t *pwi)
{
	char buf[CONFIG_SYS_CBSIZE];
	char *cmd = "win";
	char *s = buf;

	/* If window is not active, unset the environment variable */
	if (!pwi->active) {
		setenv((char *)pwi->name, NULL);
		return;
	}

	/* Prepare environment string for this window */
	s += sprintf(s, "%s %s %u %u %u %u", cmd, win_kw[WI_FBRES].keyword,
		     pwi->fbhres, pwi->fbvres, pwi->pix, pwi->fbcount);
	s += sprintf(s, "; %s %s %u %u", cmd, win_kw[WI_RES].keyword,
		     pwi->hres, pwi->vres);
	if (pwi->fbshow || pwi->fbdraw) {
		s += sprintf(s, "; %s %s %u %u", cmd, win_kw[WI_SHOW].keyword,
			     pwi->fbshow, pwi->fbdraw);
	}
	if (pwi->hoffs || pwi->voffs) {
		s += sprintf(s, "; %s %s %u %u", cmd, win_kw[WI_OFFS].keyword,
			     pwi->hoffs, pwi->voffs);
	}
	if (pwi->hpos || pwi->vpos) {
		s += sprintf(s, "; %s %s %d %d", cmd, win_kw[WI_POS].keyword,
			     pwi->hpos, pwi->vpos);
	}
	if (pwi->ai[0].alpha != DEFAULT_ALPHA0) {
		s += sprintf(s, "; %s %s #%08x", cmd, win_kw[WI_ALPHA0].keyword,
			     pwi->ai[0].alpha);
	}
	if (pwi->ai[1].alpha != DEFAULT_ALPHA1) {
		s += sprintf(s, "; %s %s #%08x", cmd, win_kw[WI_ALPHA1].keyword,
			     pwi->ai[1].alpha);
	}
	if (pwi->alphamode != ((pwi->ppi->flags & PIF_ALPHA) ? 2 : 1)) {
		s += sprintf(s, "; %s %s %u", cmd, win_kw[WI_ALPHAM].keyword,
			     pwi->alphamode);
	}
	if (pwi->ckvalue || pwi->ckmask || pwi->ckmode) {
		s += sprintf(s, "; %s %s #%08x #%08x %u", cmd,
			     win_kw[WI_COLKEY].keyword, pwi->ckvalue,
			     pwi->ckmask, pwi->ckmode);
	}

	/* Set the environment variable */
	setenv((char *)pwi->name, buf);
}
