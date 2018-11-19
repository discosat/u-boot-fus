/*
 * Copyright (C) 2014-2015 Freescale Semiconductor, Inc.
 *
 * SPDX-License-Identifier:	GPL-2.0+
 */

#ifndef __MXSFB_H__
#define __MXSFB_H__

#include <linux/fb.h>

#ifdef CONFIG_VIDEO_MXS
void lcdif_power_down(void);
int mxs_lcd_panel_setup(uint32_t base_addr,
			const struct fb_videomode *mode, int bpp);
#endif

#endif				/* __MXSFB_H__ */
