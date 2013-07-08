/*
 * Command cmap
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
#include <cmd_lcd.h>			  /* vidinfo_t, wininfo_t */
#include <linux/ctype.h>		  /* isdigit(), toupper() */
#include <watchdog.h>			  /* WATCHDOG_RESET */

/************************************************************************/
/* DEFINITIONS								*/
/************************************************************************/

/************************************************************************/
/* ENUMERATIONS								*/
/************************************************************************/

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


/************************************************************************/
/* TYPES AND STRUCTURES							*/
/************************************************************************/


/************************************************************************/
/* LOCAL VARIABLES							*/
/************************************************************************/

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

/* Palette signature */
static const u_char palsig[6] = {
	'P', 'A', 'L', 0,		  /* "PAL" */
	1, 0				  /* V1.0 */
};

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


/************************************************************************/
/* Exported Functions							*/
/************************************************************************/

/* Set the default color map */
void set_default_cmap(const wininfo_t *pwi)
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


/************************************************************************/
/* Command cmap								*/
/************************************************************************/

/* Handle cmap command */
static int do_cmap(cmd_tbl_t *cmdtp, int flag, int argc, char *const argv[])
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
	pvi = lcd_get_sel_vidinfo_p();
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
				      ARRAY_SIZE(cmap_kw));
	}

	if (sc == CI_HELP) {
		cmd_usage(cmdtp);
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
	cmap, 7, 1, do_cmap,
	"handle color map entries",
	"set n #rgba\n"
	"    - set color map entry n to new color\n"
	"cmap load | store addr [start [end]]\n"
	"    - load/store whole or part of color map from/to memory\n"
	"cmap default\n"
	"    - Set default color map\n"
	"cmap [start [end]]\n"
	"    - list whole or part of color map\n"
);
