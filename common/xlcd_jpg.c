/*
 * Hardware independent LCD support for BMP files
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
#include <cmd_lcd.h>			  /* wininfo_t, pixinfo_t */
#include <xlcd_bitmap.h>		  /* bminfo_t, CT_*, ... */
#include <watchdog.h>			  /* WATCHDOG_RESET() */

#ifdef CONFIG_CMD_DRAW
#include <xlcd_draw_ll.h>		  /* draw_ll_row_*() */
#endif

#ifdef CONFIG_CMD_ADRAW
#include <xlcd_adraw_ll.h>		  /* adraw_ll_row_*() */
#endif

#if CONFIG_XLCD_DRAW & XLCD_DRAW_BITMAP

/************************************************************************/
/* DEFINITIONS								*/
/************************************************************************/

/************************************************************************/
/* LOCAL VARIABLES							*/
/************************************************************************/

/************************************************************************/
/* Local Helper Functions						*/
/************************************************************************/

/************************************************************************/
/* Exported functions							*/
/************************************************************************/

/* Draw JPG image */
const char *draw_jpg(imginfo_t *pii, u_long addr)
{
	/* TODO: add JPG support; don't forget to trigger the watchdog with
	   WATCHDOG_RESET() after each row. */
	return "JPG images not yet supported";
}


/* Get a bminfo structure with JPG bitmap information */
int get_bminfo_jpg(bminfo_t *pbi, u_long addr)
{
	if (1)
		return 0;

	/* TODO: add JPG support */
	return 1;
}


/* Scan integrity of a JPG bitmap and return end address */
u_long scan_jpg(u_long addr)
{
	if (1)
		return 0;

	/* TODO: Add support for scanning a JPG bitmap */
	return addr;
}

#endif /* CONFIG_XLCD_DRAW & XLCD_DRAW_BITMAP */
