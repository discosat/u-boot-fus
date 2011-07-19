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
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.	 See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston,
 * MA 02111-1307 USA
 */

#ifndef _CMD_LCD_H_
#define _CMD_LCD_H_

enum RGB_TYPE {
	RT_NONE,
	RT_RGB,
	RT_RGBA
};

/* Settings that correspond with lcdwin */
typedef enum WIN_INDEX {
	WI_INFO,
	WI_SELECT,
	WI_IMAGE,
	WI_FBUF,
	WI_RES,
	WI_OFFSET,
	WI_POS,
	WI_COLOR,
	
	/* Unknown keyword (must be the last entry!) */
	WI_UNKNOWN,
} winindex_t;


/* Info about max. argument count, command index and keyword string of
   commands lcdset and lcdwin */
struct kwinfo
{
	u_char  argc_min;
	u_char  argc_max;
	u_short si;
	char    *keyword;
};



/* Return RT_RGB for #rrggbb, RT_RGBA for #rrggbbaa and RT_NONE otherwise */
extern enum RGB_TYPE parse_rgb_type(char *s);

/* Parse #rrggbb or #rrggbbaa value; return value and type */
extern enum RGB_TYPE parse_rgb(char *s, u_int *prgba);

#endif /*!_CMD_LCD_H_*/
