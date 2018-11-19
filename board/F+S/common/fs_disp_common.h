/*
 * fs_disp_common.h
 *
 * (C) Copyright 2018
 * Hartmut Keller, F&S Elektronik Systeme GmbH, keller@fs-net.de
 *
 * Common display code used on F&S boards
 *
 * SPDX-License-Identifier:	GPL-2.0+
 *
 * Usage
 * -----
 *
 * Provide three callback functions in the board-specific file:
 *
 * - board_display_set_power() to enable or disable power to the display logic
 * - board_display set_backlight() to switch backlight to a display on or off
 * - board_display_start() to initialize the clocks and display controller
 *
 * Define an array of struct fs_display_port that holds all available display
 * ports for this architecture. These may be more than are actually available
 * on a specific board. Implement function board_video_skip() and determine the
 * default display port and a bit vector of actually available display ports
 * on this specific board. For example if display ports at index 0 and 2 are
 * available on the board, and the default display should be the one on index
 * 2, then valid_mask has to have bits 0 and 2 set to 1 and default_port has
 * to be set to 2. The default display port is used if variable disppanel only
 * consists of a dislay name but does not include the port name.
 *
 * Call fs_disp_register(). This function will check the environment if the
 * user has requested a display. If yes, it will use the above callback
 * functions to activate clocks, voltages and controller.
 *
 * All other functions are simply helper functions to handle some common
 * tasks, for example when implementing the callback functions.
 *
 *
 * Environment settings
 * --------------------
 *
 * disppanel: [port:]display
 *
 * Use the given display (from the display database) on the given port. If the
 * display is not in the database, you also have to set variable dispmode to
 * define a user-defined display.
 *
 * dispmode: param=val,param=val,param=val,...
 *
 * Define timing parameters. These values override the values from the display
 * database. Available parameters:
 *
 *   clk:  pixel clock (in Hz)
 *   rate: refresh rate (in Hz) (clk will take precedence if also given)
 *         if neither clk nor rate are given, 60Hz is assumed
 *   hres: horizontal pixel resolution
 *   vres: vertical pixel resolution
 *   hfp:  horizontal front porch (time between end of line and HSYNC)
 *   hbp:  horizontal back porch (time between HSYNC and beginning of line)
 *   hsw:  HSYNC width
 *   vfp:  vertical front porch (time between end of frame and HSYNC)
 *   vbp:  vertical back porch (time between VSYNC and beginning of frame)
 *   vsw:  VSYNC width
 *   hsp:  HSYNC polarity: 0: active low, 1: active high
 *   vsp:  VSYNC polarity: 0: active low, 1: active high
 *   dep:  DE polarity: 0: active low, 1: active high
 *   clkp: clk polarity: data latched on 0: falling edge, 1: rising edge
 *   il:   interlaced mode: 0: progressive format, 1: interlaced format
 *
 * dispflags: flag[=0|1],flag[=0|1],...
 *
 * Define extra settings (boolean). At the moment this is only valid for LVDS
 * displays:
 *
 *   lvds2ch:   Use 2-channel lvds: one channel even, other channel odd pixels
 *   lvdsdup:   Use duplicated image on both channels, for two similar displays
 *   lvds24:    Use 24-bit LVDS (4 data lanes), default: 18-bit (3 data lanes)
 *   lvdsjeida: Use JEIDA bit sorting in 24-bit mode, default: SPWG sorting
 */

#ifndef __FS_DISP_COMMON_H__
#define __FS_DISP_COMMON_H__

#include <linux/fb.h>			/* struct fb_videomode */

/*
 * Extra flags that the user can define
 * 2CH:   2-channel LVDS: even/odd pixels on separate channels (NXP: split)
 * DUP:   Duplicated data for two displays on both LVDS channels (NXP: dual)
 * JEIDA: A different sorting of data bits on LVDS lanes; only for 24 bpp
 */
#define FS_DISP_FLAGS_LVDS_2CH   (1 << 0) /* 0: 1ch display, 1: 2ch display */
#define FS_DISP_FLAGS_LVDS_DUP   (1 << 1) /* 0: one display, 1: two displays */
#define FS_DISP_FLAGS_LVDS_24BPP (1 << 2) /* 0: 18 bpp, 1: 24 bpp */
#define FS_DISP_FLAGS_LVDS_JEIDA (1 << 3) /* 0: 24 bpp SPWG, 1: 24 bpp JEIDA */

struct fs_display_port {
	const char *name;		/* Port name */
	unsigned int flags_mask;	/* Mask which flags are used */
};


/* ---- Call-back functions that need to be implemented by board code ------ */

/* Enable or disable display voltage (VLCD) for a display port */
void board_display_set_power(int port, int on);

/* Enable or disable backlight (incl. backlight voltage) for a display port */
/* ### TODO: set backlight brightness */
void board_display_set_backlight(int port, int on);

/* Start display clocks and prepare display */
int board_display_start(int port, unsigned flags, struct fb_videomode *mode);

/* ---- Functions provided by fs_disp_common module ------------------------ */

/* Enable backlight power and set brightness via I2C on F&S RGB adapter */
void fs_disp_set_i2c_backlight(unsigned int bus, int on);

/* Set VCFL power via GPIO; several displays may share this GPIO */
void fs_disp_set_vcfl(int port, int on, int gpio);

/* Switch display power for all active displays on or off */
void fs_disp_set_power_all(int on);

/* Switch backlight of all active displays on or off */
void fs_disp_set_backlight_all(int on);

/* Check environment and start the display if requested by the user */
int fs_disp_register(const struct fs_display_port *display_ports,
		     unsigned int valid_mask, int default_port);


#endif /* !__FS_DISP_COMMON_H__ */
