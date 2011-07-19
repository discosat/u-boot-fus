/*
 * Hardware specific LCD controller part for S3C64XX
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

#ifndef _LCD_S3C64XX_H_
#define _LCD_S3C64XX_H_

/* Default pixel format is RGBA5650 */
#define DEFAULT_PIXEL_FORMAT 5

/* S3C64XX has five hardware windows; you can override CONFIG_MAX_WINDOWS with
   a lower value in your platform specific configuration header file */
#ifndef CONFIG_MAX_WINDOWS
#define CONFIG_MAX_WINDOWS 5
#endif

/* We have at most two buffers per window (on windows 0 & 1); you can override
   this with a lower value in your platform specific header file */
#ifndef CONFIG_MAX_BUFFERS_PER_WIN
#define CONFIG_MAX_BUFFERS_PER_WIN 2
#endif

/* We extend the WININFO structure and lcdwin by 2 commands */
#define CONFIG_LCDWIN_EXT 2

/* Extended help message for lcdwin */
#define LCDWIN_EXT_HELP \
	"lcdwin alpha [alpha0 [alpha1 [pix [sel]]]]\n" \
	"    - Set per-window alpha values\n" \
	"lcdwin colkey [enable [dir [blend [value [mask]]]]]\n" \
	"    - Set per-window color key values\n"

/* This is the extension part, available as wininfo_t.ext */
typedef struct wininfo_ext {
	/* Alpha information */
	u_int  alpha;			  /* Alpha R/G/B AEN=0/1 */
	u_char bld_pix;			  /* Blend 0: per plane, 1: per pixel */
	u_char alpha_sel;		  /* Alpha selection */

	/* Color key information */
	u_char ckenable;		  /* 0: disabled, 1: enabled */
	u_char ckdir;			  /* compare to 0: fg, 1: bg */
	u_char ckblend;			  /* 0: no blending, 1: blend */
	u_int  ckvalue;			  /* Color key value */
	u_int  ckmask;			  /* Mask which value bits matter */
} wininfo_ext_t;

#endif /*!_LCD_S3C64XX_H_*/
