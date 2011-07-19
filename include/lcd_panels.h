/*
 * LCD Panel Database
 *
 * (C) Copyright 2011
 * Hartmut Keller, F&S Elektronik Systeme GmbH, keller@fs-net.de
 *
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

#ifndef _LCD_PANELS_H_
#define _LCD_PANELS_H_

/************************************************************************/
/* DEFINITIONS								*/
/************************************************************************/

/* Maximum length of a panel name */
#define MAX_NAME 32

/* Values for type entry of vidinfo_t; if TFT is set, all other bits must be
   zero. Therefore the range of this value is 0..8 */
#define VI_TYPE_SINGLESCAN 0x00		  /* Bit 0: single or dual scan */
#define VI_TYPE_DUALSCAN   0x01
#define VI_TYPE_4BITBUS    0x00		  /* Bit 1: 4-bit or 8-bit buswidth */
#define VI_TYPE_8BITBUS    0x02
#define VI_TYPE_STN        0x00		  /* Bit 2: STN or CSTN */
#define VI_TYPE_CSTN       0x04
#define VI_TYPE_STN_CSTN   0x00		  /* Bit 3: (C)STN or TFT */
#define VI_TYPE_TFT        0x08

/*
 *  Information about displays we are using. This is for configuring
 *  the LCD controller and memory allocation. Someone has to know what
 *  is connected, as we can't autodetect anything.
 */
#define CFG_HIGH	0	/* Pins are active high	*/
#define CFG_LOW		1	/* Pins are active low */


/************************************************************************/
/* TYPES AND STRUCTURES							*/
/************************************************************************/

/* Common LCD panel information */
typedef struct vidinfo {
	/* Horizontal control */
	u_short	hfp;		/* Front porch (between data and HSYNC) */
	u_short hsw;		/* Horizontal sync pulse (HSYNC) width */
	u_short hbp;		/* Back porch (between HSYNC and data) */
	u_short	hres;		/* Horizontal pixel resolution (i.e. 640) */

	/* Vertical control */
	u_short vfp;		/* Front porch (between data and VSYNC) */
	u_short	vsw;		/* Vertical sync pulse (VSYNC) width */
	u_short vbp;		/* Back porch (between VSYNC and data) */
	u_short	vres;		/* Vertical pixel resolution (i.e. 480) */

	/* Signal polarity */
	u_char  hspol;	        /* HSYNC polarity (0=normal, 1=inverted) */
	u_char  vspol;		/* VSYNC polarity (0=normal, 1=inverted) */
	u_char  denpol;		/* DEN polarity (0=normal, 1=inverted) */
	u_char  clkpol;		/* Clock polarity (0=normal, 1=inverted) */

	/* Timings */
	u_int   fps;		/* Frame rate (in frames per second) */
	u_int   clk;		/* Pixel clock (in Hz) */

	/* Backlight settings */
	u_int   pwmvalue;	/* PWM value (voltage) */
	u_int   pwmfreq;	/* PWM frequency */
	u_char  pwmenable;	/* 0: disabled, 1: enabled */

	/* Display type */
	u_char  type;		/* Bit 0: 0: 4-bit bus, 1: 8-bit bus
				   Bit 1: 0: single-scan, 1: dual-scan
				   Bit 2: 0: STN, 1: CSTN
				   Bit 3: 0: (C)STN, 1: TFT */

	/* Additional settings */
	u_char  strength;	/* Drive strength: 0=2mA, 1=4mA, 2=7mA, 3=9mA */
	u_char	dither;		/* Dither mode (FRC) #### */

	/* General info */
	u_short	hdim;		/* Width of display area in millimeters */
	u_short	vdim;		/* Height of display area in millimeters */
	char    name[MAX_NAME];	/* Manufacturer, display and resolution */
} vidinfo_t;


/************************************************************************/
/* PROTOTYPES OF EXPORTED FUNCTIONS					*/
/************************************************************************/

/* Find predefined panel by index (NULL on bad index) */
extern const vidinfo_t *lcd_getpanel(u_int index);

/* Return index of next panel matching string s (or 0 if no match); start
   search at given index */ 
extern u_int lcd_searchpanel(char *s, u_int index);


#endif /*!_LCD_PANELS_H_*/
