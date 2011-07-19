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
/* LCD Panel Database 							*/
/************************************************************************/
const lcdinfo_t lcdlist[] = {
	{				  /* Panel 0 */
		name: "(no display)",	  /* Name and type */
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
		clkpol:		CFG_LOW,
		fps:		60,	  /* Frame rate or pixel clock */
		clk:		0,
		pwmenable:	0,	  /* PWM/Backlight setting */
		pwmvalue:	0,
		pwmfreq:	0,
		strength:	3,	  /* Extra settings */
		dither:		0,
	},
	{				  /* Panel 1 */
		name: "LG.Philips LB064V02", /* Name and type */
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
		clkpol:		CFG_LOW,
		fps:		60,	  /* Frame rate or pixel clock */
		clk:		0,
		pwmenable:	0,	  /* PWM/Backlight setting */
		pwmvalue:	0,
		pwmfreq:	0,
		strength:	3,	  /* Extra settings */
		dither:		0,
	},
	{				  /* Panel 1 */
		name: "Ampire AM320240LTNQW",	  /* Name and type */
		type:		VI_TYPE_TFT,
		hdim:		70,
		vdim:		53,
		hres:		320,	  /* Resolution */
		vres:		240,
		hfp:		20,	  /* Horizontal timing */
		hsw:		30,
		hbp:		38,
		vfp:		5,	  /* Vertical timing */
		vsw:		3,
		vbp:		15,
		hspol:		CFG_LOW, /* Signal polarity */
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
	{				  /* Panel ? */
		name: "Ampire AM320240N1TMQW",	  /* Name and type */
		type:		VI_TYPE_TFT,
		hdim:		115,
		vdim:		86,
		hres:		320,	  /* Resolution */
		vres:		241,
		hfp:		20,	  /* Horizontal timing */
		hsw:		30,
		hbp:		38,
		vfp:		5,	  /* Vertical timing */
		vsw:		3,
		vbp:		15,
		hspol:		CFG_LOW, /* Signal polarity */
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
	{				  /* Panel ? */
		name: "Ampire AM640480GHTNQW",	  /* Name and type */
		type:		VI_TYPE_TFT,
		hdim:		115,
		vdim:		86,
		hres:		640,	  /* Resolution */
		vres:		480,
		hfp:		16,	  /* Horizontal timing */
		hsw:		30,
		hbp:		114,
		vfp:		7,	  /* Vertical timing */
		vsw:		3,
		vbp:		35,
		hspol:		CFG_LOW, /* Signal polarity */
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
	{				  /* Panel ? */
		name: "Ampire AM800480E2TMQW",	  /* Name and type */
		type:		VI_TYPE_TFT,
		hdim:		152,
		vdim:		91,
		hres:		800,	  /* Resolution */
		vres:		480,
		hfp:		40,	  /* Horizontal timing */
		hsw:		48,
		hbp:		40,
		vfp:		13,	  /* Vertical timing */
		vsw:		3,
		vbp:		29,
		hspol:		CFG_LOW, /* Signal polarity */
		vspol:		CFG_LOW,
		denpol:		CFG_HIGH,
		clkpol:		CFG_LOW,
		fps:		60,	  /* Frame rate or pixel clock */
		clk:		0,
		pwmenable:	0,	  /* PWM/Backlight setting */
		pwmvalue:	0,
		pwmfreq:	0,
		strength:	3,	  /* Extra settings */
		dither:		0,
	},
	{				  /* Panel ? */
		name: "EDT ET035009DH6",  /* Name and type */
		type:		VI_TYPE_TFT,
		hdim:		70,
		vdim:		53,
		hres:		320,	  /* Resolution */
		vres:		240,
		hfp:		20,	  /* Horizontal timing */
		hsw:		30,
		hbp:		38,
		vfp:		4,	  /* Vertical timing */
		vsw:		2,
		vbp:		16,
		hspol:		CFG_LOW, /* Signal polarity */
		vspol:		CFG_LOW,
		denpol:		CFG_LOW,
		clkpol:		CFG_HIGH,
		fps:		60,	  /* Frame rate or pixel clock */
		clk:		0,
		pwmenable:	0,	  /* PWM/Backlight setting */
		pwmvalue:	0,
		pwmfreq:	0,
		strength:	3,	  /* Extra settings */
		dither:		0,
	},
	{				  /* Panel ? */
		name: "EDT ET035080DH6",  /* Name and type */
		type:		VI_TYPE_TFT,
		hdim:		70,
		vdim:		53,
		hres:		320,	  /* Resolution */
		vres:		240,
		hfp:		20,	  /* Horizontal timing */
		hsw:		30,
		hbp:		38,
		vfp:		4,	  /* Vertical timing */
		vsw:		2,
		vbp:		16,
		hspol:		CFG_LOW, /* Signal polarity */
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
	{				  /* Panel ? */
		name: "EDT ET043000DH6",  /* Name and type */
		type:		VI_TYPE_TFT,
		hdim:		95,
		vdim:		54,
		hres:		480,	  /* Resolution */
		vres:		272,
		hfp:		2,	  /* Horizontal timing */
		hsw:		41,
		hbp:		2,
		vfp:		2,	  /* Vertical timing */
		vsw:		10,
		vbp:		2,
		hspol:		CFG_LOW, /* Signal polarity */
		vspol:		CFG_LOW,
		denpol:		CFG_HIGH,
		clkpol:		CFG_LOW,
		fps:		60,	  /* Frame rate or pixel clock */
		clk:		0,
		pwmenable:	0,	  /* PWM/Backlight setting */
		pwmvalue:	0,
		pwmfreq:	0,
		strength:	3,	  /* Extra settings */
		dither:		0,
	},
	{				  /* Panel ? */
		name: "EDT ET050080DH6",  /* Name and type */
		type:		VI_TYPE_TFT,
		hdim:		109,
		vdim:		65,
		hres:		800,	  /* Resolution */
		vres:		480,
		hfp:		40,	  /* Horizontal timing */
		hsw:		128,
		hbp:		88,
		vfp:		10,	  /* Vertical timing */
		vsw:		2,
		vbp:		33,
		hspol:		CFG_LOW, /* Signal polarity */
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
	{				  /* Panel ? */
		name: "EDT ET057080DH6",  /* Name and type */
		type:		VI_TYPE_TFT,
		hdim:		115,
		vdim:		86,
		hres:		320,	  /* Resolution */
		vres:		240,
		hfp:		20,	  /* Horizontal timing */
		hsw:		30,
		hbp:		38,
		vfp:		5,	  /* Vertical timing */
		vsw:		3,
		vbp:		15,
		hspol:		CFG_LOW, /* Signal polarity */
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
	{				  /* Panel ? */
		name: "EDT ET057090DHU",  /* Name and type */
		type:		VI_TYPE_TFT,
		hdim:		115,
		vdim:		86,
		hres:		640,	  /* Resolution */
		vres:		480,
		hfp:		16,	  /* Horizontal timing */
		hsw:		30,
		hbp:		114,
		vfp:		10,	  /* Vertical timing */
		vsw:		3,
		vbp:		32,
		hspol:		CFG_LOW, /* Signal polarity */
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
	{				  /* Panel ? */
		name: "EDT ET070080DH6",  /* Name and type */
		type:		VI_TYPE_TFT,
		hdim:		152,
		vdim:		91,
		hres:		800,	  /* Resolution */
		vres:		480,
		hfp:		40,	  /* Horizontal timing */
		hsw:		128,
		hbp:		88,
		vfp:		10,	  /* Vertical timing */
		vsw:		2,
		vbp:		33,
		hspol:		CFG_LOW, /* Signal polarity */
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

/* Find predefined lcd panel by index (NULL on bad index) */
const lcdinfo_t *lcd_get_lcdinfo_p(u_int index)
{
	return (index < ARRAYSIZE(lcdlist)) ? &lcdlist[index] : NULL;
}

/* Return index of next lcd panel matching string s (or 0 if no match); start
   search at given index */ 
u_int lcd_search_lcd(char *s, u_int index)
{
	for ( ; index < ARRAYSIZE(lcdlist); index++) {
		if (strstr(lcdlist[index].name, s))
		    return index;	  /* match */
	}

	return 0;			  /* no more matches */
}
