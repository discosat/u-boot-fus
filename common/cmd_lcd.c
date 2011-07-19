/*
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

/*
 * LCD commands
 */

#include <lcd.h>			  /* vidinfo_t, wininfo_t */

#define SETKEYWORDS_COUNT (sizeof(setkeywords)/sizeof(setkeywords[0]))
#ifndef CONFIG_FBPOOL_SIZE
#define CONFIG_FBPOOL_SIZE 0x00100000	  /* 1MB, enough for 800x600@16bpp */
#endif

typedef enum SET_INDEX
{
	/* Keywords not available with reg set val because they take more than
	   one argument or behave differently */
	SI_PANEL,
	SI_INFO,
	SI_HINFO,
	SI_VINFO,
	SI_POLARITY,
	SI_EXTRA,
	SI_PWM,

	/* Keywords that are also available with reg set val */
	SI_NAME,
	SI_HSIZE,
	SI_VSIZE,
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
#ifdef CONFIG_FSWINCE_CONFIG
	SI_CONFIG,
#endif
	SI_CLK,
	SI_FPS,
	SI_PIXEL,
	SI_STRENGTH,
	SI_DITHER,
	SI_DEBUG,
	SI_PWMENABLE,
	SI_PWMVALUE,
	SI_PWMFREQ,

	/* Unknown keyword (must be the last entry!) */
	SI_UNKNOWN,
} setindex_t;

typedef enum WIN_INDEX
{
} winindex_t;

struct keyinfo
{
	uint argc;
	setindex_t si;
	char *keyword;
	char *altkeyword;
};

/* Size and base address of the framebuffer pool */
static u_long fbsize = CONFIG_FBPOOL_SIZE;
static u_long fbpool = CFG_UBOOT_BASE - CONFIG_FBPOOL_SIZE;


char * const typeinfo[8] =
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

/* Keywords available with lcdset */
struct keyinfo const setkeywords[] =
{
	/* Multiple arguments or argument with multiple types */
	{1, SI_PANEL,     "panel",     "display"},
	{3, SI_INFO,      "info",      "dinfo"},
	{4, SI_HINFO,     "hinfo",     "horizontal"},
	{4, SI_VINFO,     "vinfo",     "vertical"},
	{4, SI_POLARITY,  "polarity",  "signal"},
	{3, SI_EXTRA,     "extra",     "special"},
	{3, SI_PWM,       "pwm",       "backlight"},

	/* Single argument, SI_NAME must be the first one. */
	{1, SI_NAME,      "name",      "description"},
	{1, SI_HSIZE,     "hsize",     "width"},
	{1, SI_VSIZE,     "vsize",     "height"},
	{1, SI_TYPE,      "type",      "technology"},
	{1, SI_HFP,       "hfp",       "elw"},
	{1, SI_HSW,       "hsw",       "hspw"},
	{1, SI_HBP,       "hbp",       "blw"},
	{1, SI_HRES,      "hres",      "columns"},
	{1, SI_VFP,       "vfp",       "efw"},
	{1, SI_VSW,       "vsw",       "vspw"},
	{1, SI_VBP,       "vbp",       "bfw"},
	{1, SI_VRES,      "vres",      "rows"},
	{1, SI_HSPOL,     "hspol",     "ihsync"},
	{1, SI_VSPOL,     "vspol",     "ivsync"},
	{1, SI_DENPOL,    "denpol",    "iden"},
	{1, SI_CLKPOL,    "clkpol",    "iclk"},
#ifdef CONFIG_FSWINCE_CONFIG
	{1, SI_CONFIG,    "config",    "cfg"},
#endif
	{1, SI_CLK,       "clk",       "lcdclk"},
	{1, SI_FPS,       "fps",       "framerate"},
	{1, SI_PIXEL,     "pixel",   "bpp"},
	{1, SI_STRENGTH,  "strength",  "lcdportdrivestrength"},
	{1, SI_DITHER,    "dither",    "frc"},
	{1, SI_DEBUG,     "debug",     "verbose"},
	{1, SI_PWMENABLE, "pwmenable", "contrastenable"},
	{1, SI_PWMVALUE,  "pwmvalue",  "contrastvalue"},
	{1, SI_PWMFREQ,   "pwmfreq",   "contrastfreq"},
};

{
	{"config"},		  /* 22 */
	{"type"},		  /* 23 */
	{"msignal"},		  /* 24 */
	{"voltage"},		  /* 25 */
		};

/************************************************************************/
/* Commands lcdon, lcdoff						*/
/************************************************************************/

static int lcdon(cmd_tbl_t * cmdtp, int flag, int argc, char *argv[])
{
	printf("#### lcdon not yet implemented ####\n");
	return 1;
}

U_BOOT_CMD(
	lcdon,	1,	0,	lcdon,
	"lcdon\t- activate display\n",
	NULL
);

static int lcdoff(cmd_tbl_t * cmdtp, int flag, int argc, char *argv[])
{
	printf("#### lcdoff not yet implemented ####\n");
	return 1;
}

U_BOOT_CMD(
	lcdoff,	1,	0,	lcdoff,
	"lcdoff\t- deactivate display\n",
	NULL
);


/************************************************************************/
/* Command lcdset (plus compatibility reg, display, contrast, reboot)	*/
/************************************************************************/

static void setvalue(uint si, char *argv, vidinfo_t *vi)
{
	uint param = 0;

	/* All parameters but SI_NAME require a number, parse it */
	if (si != SI_NAME)
		param = simple_strtoul(argv, NULL, 0);

	switch (si)
	{
	case SI_NAME:			  /* Parse string */
		strncpy(vi->name, argv, MAX_NAME);
		vi->name[MAX_NAME-1] = 0;
		break;

	case SI_HSIZE:			  /* Parse u_short */
		vi->hsize = (u_short)param;
		break;

	case SI_VSIZE:			  /* Parse u_short */
		vi->vsize = (u_short)param;
		break;

	case SI_TYPE:			  /* Parse u_char (0..8) */
		if (param > 8)
			param = 8;
		vi->vsize = (u_char)param;
		break;

	case SI_HFP:			  /* Parse u_short */
		vi->hfp = (u_short)param;
		break;

	case SI_HSW:			  /* Parse u_short */
		vi->hsw = (u_short)param;
		break;

	case SI_HBP:			  /* Parse u_short */
		vi->hbp = (u_short)param;
		break;

	case SI_HRES:			  /* Parse u_short */
		vi->hres = (u_short)param;
		break;

	case SI_VFP:			  /* Parse u_short */
		vi->hfp = (u_short)param;
		break;

	case SI_VSW:			  /* Parse u_short */
		vi->vsw = (u_short)param;
		break;

	case SI_VBP:			  /* Parse u_short */
		vi->vbp = (u_short)param;
		break;

	case SI_VRES:			  /* Parse u_short */
		vi->vres = (u_short)param;
		break;

	case SI_HSPOL:			  /* Parse flag (u_char 0..1) */
		vi->hspol = (param != 0);
		break;

	case SI_VSPOL:			  /* Parse flag (u_char 0..1) */
		vi->vspol = (param != 0);
		break;

	case SI_DENPOL:			  /* Parse flag (u_char 0..1) */
		vi->denpol = (param != 0);
		break;

	case SI_CLKPOL:			  /* Parse flag (u_char 0..1) */
		vi->clkpol = (param != 0);
		break;

#ifdef CONFIG_FSWINCE_CONFIG
	case SI_CONFIG:			  /* Parse F&S compatibility value */
		vi->vspol = ((param & 0x00100000) != 0);
		vi->hspol = ((param & 0x00200000) != 0);
		vi->clkpol = ((param & 0x00400000) != 0);
		vi->denpol = ((param & 0x00800000) != 0);
		break;
#endif

	case SI_CLK:			  /* Parse u_int; scale appropriately */
		if (param < 1000000) {
			param *= 1000;
			if (param < 1000000)
				param *= 1000;
		}
		vi->clk = (u_int)param;
		vi->fps = 0;		  /* Compute fps from clk */
		break;

	case SI_FPS:			  /* Parse u_short */
		vi->fps = (u_short)param;
		vi->clk = 0;		  /* Compute clk from fps */
		break;

	case SI_PIXEL:			  /* Parse u_int, convert to pixel */
		vi->pformat = lcd_getpix(param);
		break;

	case SI_STRENGTH:		  /* Parse u_char */
		vi->strength = (u_char)param;
		break;

	case SI_DITHER:			  /* Parse flag (u_char 0..1) */
		vi->dither = (param != 0);
		break;

	case SI_DEBUG:			  /* Parse u_int */
		vi->debug = (u_int)param;
		break;

	case SI_PWMENABLE:		  /* Parse flag (u_char 0..1) */
		vi->pwmenable = (param != 0);
		break;

	case SI_PWMVALUE:		  /* Parse u_int */
		vi->pwmvalue = (u_int)param;
		break;

	case SI_PWMFREQ:		  /* Parse u_int */
		vi->pwmfreq = (u_int)param;
		break;

	default:			  /* Should not happen */
		break;
	}
}

static int lcdset(cmd_tbl_t *cmdtp, int flag, int argc, char *argv[])
{
	vidinfo_t vi;
	setindex_t si = SI_UNKNOWN;
	char c;
	uint param;

	/* Get the LCD panel info */
	lcd_getvidinfo(&vi);
	if (argc < 2)
		si = SI_PANEL; 		  /* No arguments: same as panel */
	else {
		struct keyinfo *ki;
		uint len = strlen(argv[1]);
		uint i;

		for (i=0, ki=setkeywords; i<SETKEYWORDS_COUNT; i++, ki++) {
			if ((strncmp(ki->keyword, argv[1], len) == 0)
			    || (strncmp(ki->altkeyword, argv[1], len) == 0))
				break;
		}
		if ((i >= SETKEYWORDS_COUNT) || (argc > ki->argc+2))
			si = SI_UNKNOWN;
		else
			si = ki->si;
	}
	if ((si != SI_UNKNOWN) && (argc >= 3)) {
		switch (si) {
		case SI_PANEL:	  /* Parse number or string */
			c = argv[2][0];
			if ((c >= '0') && (c <= '9')) {
				/* Parse panel index number */
				param = simple_strtoul(argv[2], NULL, 0);
			} else if (!(param = lcd_searchpanel(0, argv[2]))) {
				printf("\nNo panel matches '%s'\n", argv[2]);
				break;
			}
			if (!lcd_getpanel(param, &vi))
				printf("\nBad panel index %d\n", param);
			break;

		case SI_INFO:		  /* Parse string and 2 u_shorts */
			strncpy(vi.name, argv[2], MAX_NAME);
			vi.name[MAX_NAME-1] = 0;
			if (argc < 4)
				break;
			param = simple_strtoul(argv[3], NULL, 0);
			vi.hsize = (u_short)param;
			if (argc < 5)
				break;
			param = simple_strtoul(argv[4], NULL, 0);
			vi.vsize = (u_short)param;
			break;

		case SI_HINFO:		  /* Parse 4 u_shorts */
			param = simple_strtoul(argv[2], NULL, 0);
			vi.hfp = (u_short)param;
			if (argc < 4)
				break;
			param = simple_strtoul(argv[3], NULL, 0);
			vi.hsw = (u_short)param;
			if (argc < 5)
				break;
			param = simple_strtoul(argv[4], NULL, 0);
			vi.hbp = (u_short)param;
			if (argc < 6)
				break;
			param = simple_strtoul(argv[5], NULL, 0);
			vi.hres = (u_short)param;
			break;

		case SI_VINFO:		  /* Parse 4 u_shorts */
			param = simple_strtoul(argv[2], NULL, 0);
			vi.vfp = (u_short)param;
			if (argc < 4)
				break;
			param = simple_strtoul(argv[3], NULL, 0);
			vi.vsw = (u_short)param;
			if (argc < 5)
				break;
			param = simple_strtoul(argv[4], NULL, 0);
			vi.vbp = (u_short)param;
			if (argc < 6)
				break;
			param = simple_strtoul(argv[5], NULL, 0);
			vi.vres = (u_short)param;
			break;

		case SI_POLARITY:	  /* Parse 4 flags */
			param = simple_strtoul(argv[2], NULL, 0);
			vi.hspol = (param != 0);
			if (argc < 4)
				break;
			param = simple_strtoul(argv[3], NULL, 0);
			vi.vspol = (param != 0);
			if (argc < 5)
				break;
			param = simple_strtoul(argv[4], NULL, 0);
			vi.denpol = (param != 0);
			if (argc < 6)
				break;
			param = simple_strtoul(argv[5], NULL, 0);
			vi.clkpol = (param != 0);
			break;

		case SI_EXTRA:		  /* Parse number + flag + number */
			param = simple_strtoul(argv[2], NULL, 0);
			vi.strength = (u_char)param;
			if (argc < 4)
				break;
			param = simple_strtoul(argv[3], NULL, 0);
			vi.dither = (param != 0);
			if (argc < 5)
				break;
			param = simple_strtoul(argv[4], NULL, 0);
			vi.debug = (u_int)param;
			break;

		case SI_PWM:		  /* Parse flag + 2 numbers */
			param = simple_strtoul(argv[2], NULL, 0);
			vi.pwmenable = (param != 0);
			if (argc < 4)
				break;
			param = simple_strtoul(argv[3], NULL, 0);
			vi.pwmvalue = (u_int)param;
			if (argc < 5)
				break;
			param = simple_strtoul(argv[4], NULL, 0);
			vi.pwmfreq = (u_int)param;
			break;

		default:		  /* Parse one argument */
			setvalue(si, argv[2], &vi);
			break;
		}

		/* Set new vidinfo to hardware; this may also update vi to
		   the actually used parameters. */
		lcd_update(&vi);
	}

	/* Print the modified or requested settings */
	switch (si)
	{
	default:
		printf("usage:\n%s ", cmdtp->name);
		puts(cmdtp->help);
		putc('\n');
		return 1;

	case SI_PANEL:
	case SI_INFO:
		printf("Display Info:\tname='%s' (%dmm x %dmm)\n",
		       vi.name, vi.hsize, vi.vsize);
		if (si != SI_PANEL)
			break;
		/* SI_PANEL: Fall through to case SI_TYPE */

	case SI_TYPE:
		printf("Display Type:\ttype=%d (%s)\n",
		       vi.type, typeinfo[vi.type]);
		if (si != SI_PANEL)
			break;
		/* SI_PANEL: Fall through to case SI_HINFO */

	case SI_HINFO:
		printf("Horiz. Info:\thfp=%d, hsw=%d, hbp=%d, hres=%d\n",
		       vi.hfp, vi.hsw, vi.hpb, vi.hres);
		if (si != SI_PANEL)
			break;
		/* SI_PANEL: Fall through to case SI_VINFO */

	case SI_VINFO:
		printf("Vertical Info:\tvfp=%d, vsw=%d, vbp=%d, vres=%d\n",
		       vi.vfp, vi.vsw, vi.vpb, vi.vres);
		if (si != SI_PANEL)
			break;
		/* SI_PANEL: Fall through to case SI_POLARITY */

	case SI_POLARITY:
		printf("Polarity:\thspol=%d, vspol=%d, clkpol=%d, denpol=%d\n",
		       vi.hspol, vi.vspol, vi.clkpol, vi.denpol);
		if (si != SI_PANEL)
			break;
		/* SI_PANEL: Fall through to case SI_CLK */

	case SI_CLK:
	case SI_FPS:
		printf("Display Timing:\tclk=%dHz (%dfps)\n", vi.clk, vi.fps);
		if (si != SI_PANEL)
			break;
		/* SI_PANEL: Fall through to case SI_PFORMAT */

	case SI_PFORMAT:
		printf("Pixel Format:\tpixel=%s\n", lcd_getpixformat(vi.pix));
		if (si != SI_PANEL)
			break;
		/* SI_PANEL: Fall through to case SI_EXTRA */

	case SI_EXTRA:
		printf("Extra Settings:\tstrength=%d, dither=%d, debug=0x%x\n",
		       vi.strength, vi.dither, vi.debug);
		if (si != SI_PANEL)
			break;
		/* SI_PANEL: Fall through to case SI_PWM */

	case SI_PWM:
		printf("PWM:\t\tpwmenable=%d, pwmvalue=%d, pwmfreq=%dHz\n",
		       vi.pwmenable, vi.pwmvalue, vi.pwmfreq);
		break;

		/* Single values, in additon to groups above */
	case SI_NAME:
		printf("Display Name:\tname='%s'\n", vi.name);
		break;

	case SI_HSIZE:
		printf("Horiz. Size:\thsize=%dmm\n", vi.hsw);
		break;

	case SI_VSIZE:
		printf("Vertical Size:\tvsize=%dmm\n", vi.hsw);
		break;

	case SI_HFP:
		printf("H. Front Porch:\thfp=%d\n", vi.hfp);
		break;

	case SI_HSW:
		printf("HSync Width:\thsw=%d\n", vi.hsw);
		break;

	case SI_HBP:
		printf("H. Back Porch:\thbp=%d\n", vi.hbp);
		break;

	case SI_HRES:
		printf("H. Resolution:\thres=%d\n", vi.hres);
		break;

	case SI_VFP:
		printf("V. Front Porch:\tvfp=%d\n", vi.vfp);
		break;

	case SI_VSW:
		printf("VSync Width:\tvws=%d\n", vi.vsw);
		break;

	case SI_VBP:
		printf("V. Back Porch:\tvpb=%d\n", vi.vbp);
		break;

	case SI_VRES:
		printf("V. Resolution:\tvres=%d\n", vi.vres);
		break;

	case SI_HSPOL:
		printf("HSync Polarity:\thspol=%d (%s)\n",
		       vi.hspol, vi.hspol ? "inverted" : "normal");
		break;

	case SI_VSPOL:
		printf("VSync Polarity:\tvspol=%d (%s)\n",
		       vi.vspol, vi.vspol ? "inverted" : "normal");
		break;

	case SI_DENPOL:
		printf("DEN Polarity:\tdenpol=%d (%s)\n",
		       vi.denpol, vi.denpol ? "inverted" : "normal");
		break;

	case SI_CLKPOL:
		printf("Clock Polarity:\tclkpol=%d (%s)\n",
		       vi.clkpol, vi.clkpol ? "inverted" : "normal");
		break;

	case SI_STRENGTH:
		printf("Drive Strength:\tstrength=%d\n", vi.hsw);
		break;

	case SI_DITHER:
		printf("Dithering:\tdither=%d\n", vi.dither);
		break;

	case SI_DEBUG:
		printf("Verbosity:\tdebug=0x%x\n", vi.debug);
		break;

	case SI_PWMENABLE:
		printf("PWM:\t\tpwmenable=%d\n", vi.pwmenable);
		break;

	case SI_HSW:
		printf("PWM:\t\tpwmvalue=%d\n", vi.pwmvalue);
		break;

	case SI_HSW:
		printf("PWM:\t\tpwmfreq=%d\n", vi.pwmfreq);
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
	"\tname, hsize, vsize, hfp, hsw, hpb, hres, vfp, vsw, vbp, vres,\n"
	"\thspol, vspol, denpol, clkpol, clk, fps, type, pixel, strength,\n"
	"\tdither, debug, pwmenable, pwmvalue, pwmfreq\n"
	"lcdset group {values}\n"
	"    - set or show the LCD parameter group; group is one of:\n"
	"\tinfo, hinfo, vinfo, polarity, pwm, extra\n"
	"lcdset\n"
	"    - show all current panel settings\n"
);


#ifdef CONFIG_FSWINCE_CONFIG
/* This function parses "reg set value" commands to set display parameters.
   Any other "reg" commands that may be present in F&S WinCE display
   configuration files are silently accepted, but ignored. */
static int reg(cmd_tbl_t * cmdtp, int flag, int argc, char *argv[])
{
	if ((argc == 6)
	    && (strcmp(argv[1], "set") == 0)
	    && (strncmp(argv[2], "value", strlen(argv[2])) == 0)) {
		struct keyinfo *ki;
		uint len = strlen(argv[3]);
		uint i;

		/* Search list of keywords */
		for (i=0, ki=setkeywords; i<SETKEYWORDS_COUNT; i++, ki++) {
			if ((strncmp(ki->keyword, argv[1], len) == 0)
			    || (strncmp(ki->altkeyword, argv[1], len) == 0))
				break;
		}
		/* Handle value if keyword was found */
		if ((i < SETKEYWORDS_COUNT) && (ki->si >= SI_NAME))
		{
			vidinfo_t vi;

			/* Get the LCD panel info */
			lcd_getvidinfo(&vi);

			if (ki->si != SI_TYPE) {
				/* Parse the value and set it */
				setvalue(ki->si, argv[5], &vi);
			} else {
				uint param;

				/* "type" is used differently here */
				param = simple_strtoul(argv[5], NULL, 0);
				if (param & 0x0002)
					vi.type = VI_TYPE_TFT;
				else {
					vi.type = 0;
					if (param & 0x0001)
						vi.type |= VI_TYPE_DUALSCAN;
					if (param & 0x0004)
						vi.type |= VI_TYPE_CSTN;
					if (param & 0x0008)
						vi.type |= VI_TYPE_8BITBUS;
				}
			}

			/* Update hardware with new settings */
			lcd_update(&vi);

			/* Other than lcdset, reg set value does not echo the
			   final result when setting the values */
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
	"\tcontrastvalue, contrastfreq; bpp, msignal and other parameter\n"
	"\tnames are accepted for compatibility reasons, but ignored\n"
	"reg open | create | enum | save ...\n"
	"    - Accepted for compatibility reasons, but ignored\n"
);

/* This function silently accepts, but ignores "contrast", "display" or
   "reboot" commands that may be present in F&S WinCE display configuration
   files. */
static int ignore(cmd_tbl_t * cmdtp, int flag, int argc, char *argv[])
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
#endif /*CONFIG_FSWINCE_CONFIG*/


/************************************************************************/
/* Command lcdwin							*/
/************************************************************************/

static int lcdwin(cmd_tbl_t * cmdtp, int flag, int argc, char *argv[])
{
	printf("#### lcdwin not yet implemented ####\n");
	return 1;
}

U_BOOT_CMD(
	lcdwin,	4,	0,	lcdwin,
	"lcdwin\t- set framebuffer and overlay window parameters\n",
	NULL
);


/************************************************************************/
/* Command lcdclut							*/
/************************************************************************/

static int lcdclut(cmd_tbl_t * cmdtp, int flag, int argc, char *argv[])
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

static int lcdlist(cmd_tbl_t * cmdtp, int flag, int argc, char *argv[])
{
	if ((argc >= 2) && (argc <= 3)) {
		uint count = 0xFFFFFFFF;
		uint index = 0;
		vidinfo_t vi;
		char c;

		/* Get count if given */
		if (argc == 3)
			count = simple_strtoul(argv[2], NULL, 0);

		/* Show header line */
		printf("#\tType\hres x vres\thsiz x vsiz\tName\n"
		       "---------------------------------------------"
		       "-------------------------\n");

		/* If first argument is a number, parse start index */
		c = argv[1][0];
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
			if (c) {
				/* Search next matching panel */
				if (!(index = lcd_searchpanel(index, argv[1])))
					break; /* No further match */
			}
			if (!lcd_getpanel(index, &vi))
				break;	  /* Reached end of list */

			/* Show entry */
			printf("%d:\t%s\t%4d x %d\t%4d x %d\t%s\n", index,
			       vi.hres, vi.vres, vi.hsize, vi.vsize, vi.name);

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
	lcdlist,	2,	1,	lcdlist,
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

static int lcdfbpool(cmd_tbl_t *cmdtp, int flag, int argc, char *argv[])
{
	if (argc > 3)
	{
		printf("usage:\n%s ", cmdtp->name);
		puts(cmdtp->help);
		putc('\n');
		return 1;
	}

	if (argc > 1)
	{
		ulong size;
		ulong base;

		/* Get size and base address */
		size = simple_strtoul(argv[1], NULL, 16);
		if (argc > 2)
			base = simple_strtoul(argv[1], NULL, 16);
		else
			base = CFG_UBOOT_BASE - size;

		/* Check if new values are valid */
		if ((base < MEMORY_BASE_ADDRESS)
		    || (base + size > CFG_UBOOT_BASE)) {
			printf("Can't set fbpool: no RAM or collision with"
			       " U-Boot code\n");
			return 1;
		}

		/* Compute required size for all windows */
		//####TODO####

		/* Relocate all windows, i.e. memmove framebuffer content to
		   new position, add offset to all enabled windows and set
		   windows to hardware */
		//####TODO####

		/* Finally set the new values */
		fbsize = size;
		fbpool = base;
	}

	/* Print current or new settings */
	printf("Framebuffer Pool: 0x%08x - 0x%08x (%d bytes)\n",
		       fbpool, fbpool+fbsize-1, fbsize);
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

static int lcdtest(cmd_tbl_t * cmdtp, int flag, int argc, char *argv[])
{
	printf("#### lcdtest not yet implemented ####\n");
	return 1;
}

U_BOOT_CMD(
	lcdtest,	5,	1,	lcdtest,
	"lcdtest\t- show test pattern or color on LCD\n",
	NULL
);
