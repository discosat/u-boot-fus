/*
 * LCD Panel Database
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

#include <config.h>
#include <common.h>
#include <lcd_panels.h>			  /* Own interface */

/************************************************************************/
/* LCD Panel Database (may be moved to a separate file someday)		*/
/************************************************************************/
const vidinfo_t panellist[] = {
	{				  /* Panel 0 */
		name: "(no display)",	  /* Name, type and dimensions */
		type:		VI_TYPE_TFT,
		hdim:		0,
		vdim:		0,
		hres:		0,	  /* Resolution */
		vres:		0,
		hfp:		24,	  /* Horizontal timing */
		hsw:		96,
		hbp:		40,
		vfp:		10,	  /* Vertical timing */
		vsw:		2,
		vbp:		33,
		hspol:		CFG_LOW,  /* Signal polarity */
		vspol:		CFG_LOW,
		denpol:		CFG_HIGH,
		clkpol:		CFG_HIGH,
		fps:		60,	  /* Frame rate or pixel clock */
		clk:		0,
		pwmenable:	0,	  /* PWM/Backlight setting */
		pwmvalue:	0,
		pwmfreq:	0,
		strength:	3,	  /* Extra settings */
		dither:		0,
	},
	{				  /* Panel 1 */
		name: "LG.Philips LB064V02", /* Name, type and dimensions */
		type:		VI_TYPE_TFT,
		hdim:		132,
		vdim:		98,
		hres:		640,	  /* Resolution */
		vres:		480,
		hfp:		24,	  /* Horizontal timing */
		hsw:		96,
		hbp:		40,
		vfp:		10,	  /* Vertical timing */
		vsw:		2,
		vbp:		33,
		hspol:		CFG_LOW,  /* Signal polarity */
		vspol:		CFG_LOW,
		denpol:		CFG_HIGH,
		clkpol:		CFG_HIGH,
		fps:		60,	  /* Frame rate or pixel clock */
		clk:		0,
		pwmenable:	0,	  /* PWM/Backlight setting */
		pwmvalue:	0,
		pwmfreq:	0,
		strength:	3,	  /* Extra settings */
		dither:		0,
	},
};

/* Find predefined panel by index (NULL on bad index) */
const vidinfo_t *lcd_getpanel(u_int index)
{
	return (index < ARRAYSIZE(panellist)) ? &panellist[index] : NULL;
}

/* Return index of next panel matching string s (or 0 if no match); start
   search at given index */ 
u_int lcd_searchpanel(char *s, u_int index)
{
	for ( ;index < ARRAYSIZE(panellist); index++) {
		if (strstr(panellist[index].name, s))
		    return index;	  /* match */
	}

	return 0;			  /* no more matches */
}
