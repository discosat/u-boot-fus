/*
 * Copyright (C) 2014-2015 Freescale Semiconductor, Inc.
 *
 * SPDX-License-Identifier:	GPL-2.0+
 */

#ifndef __MXSFB_H__
#define __MXSFB_H__

#include <linux/fb.h>

/* The color channels may easily be swapped in the driver */
#define PATTERN_RGB	0
#define PATTERN_RBG	1
#define PATTERN_GBR	2
#define PATTERN_GRB	3
#define PATTERN_BRG	4
#define PATTERN_BGR	5

#ifdef CONFIG_VIDEO_MXS
void lcdif_power_down(void);
int mxs_lcd_panel_setup(uint32_t base_addr, const struct fb_videomode *mode,
			int bpp, int pattern);
#endif

#endif				/* __MXSFB_H__ */
