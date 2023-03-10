/*
 * fs_disp_common.c
 *
 * (C) Copyright 2018
 * Hartmut Keller, F&S Elektronik Systeme GmbH, keller@fs-net.de
 *
 * Common display code used on F&S boards
 *
 * SPDX-License-Identifier:	GPL-2.0+
 */

#include <config.h>

#if defined(CONFIG_VIDEO_IPUV3) || defined(CONFIG_VIDEO_MXS)

#include <common.h>			/* types */
#include <asm/gpio.h>			/* gpio_direction_output() */
#include <i2c.h>			/* i2c_set_bus_num(), i2c_reg_read() */
#include "fs_disp_common.h"		/* Own interface */

/* Display states */
#define FS_DISP_STATE_ACTIVE	(1 << 0) /* 0: not used, 1: used */
#define FS_DISP_STATE_VLCD_ON	(1 << 1) /* 0: VLCD off, 1: VLCD_ON */
#define FS_DISP_STATE_BL_ON	(1 << 2) /* 0: backlight off, 1: backlight on */

struct fs_display {
	unsigned int port;		/* Port index */
	const char *name;		/* Port name */
	unsigned int state;		/* Port state */
	unsigned int flags_mask;	/* Mask which user flags are valid */
	unsigned int flags;		/* User settable flags */
	struct fb_videomode mode;	/* Display resolution and timings */
};

/* This is a set of used display ports */
static struct fs_display displays[CONFIG_FS_DISP_COUNT];
static int display_count;
static int display_index[CONFIG_FS_DISP_COUNT];

static const char *parse_pos;

/*
 * Have a small display data base.
 *
 * drivers/video/mxcfb.h defines additional values to set DE polarity and
 * clock sensitivity. However these values are not valid in Linux and the file
 * can not easily be included here. So we (F&S) misuse some existing defines
 * from include/linux/fb.h to handle these two cases. This may change in the
 * future.
 *
 *   FB_SYNC_COMP_HIGH_ACT: DE signal active high
 *   FB_SYNC_ON_GREEN:      Latch on rising edge of pixel clock
 */
const struct fb_videomode display_db[] = {
	{
		.name           = "EDT-ET070080DH6",
		.refresh        = 60,
		.xres           = 800,
		.yres           = 480,
		.pixclock       = 30066, // picoseconds
		.left_margin    = 88,
		.right_margin   = 40,
		.upper_margin   = 33,
		.lower_margin   = 10,
		.hsync_len      = 128,
		.vsync_len      = 2,
		.sync           = FB_SYNC_ON_GREEN | FB_SYNC_COMP_HIGH_ACT,
		.vmode          = FB_VMODE_NONINTERLACED
	},
	{
		.name           = "ChiMei-G070Y2-L01",
		.refresh        = 60,
		.xres           = 800,
		.yres           = 480,
		.pixclock       = 33500, // picoseconds
		.left_margin    = 88,
		.right_margin   = 40,
		.upper_margin   = 33,
		.lower_margin   = 10,
		.hsync_len      = 128,
		.vsync_len      = 2,
		.sync           = FB_SYNC_ON_GREEN | FB_SYNC_COMP_HIGH_ACT,
		.vmode          = FB_VMODE_NONINTERLACED
	},
	{
		.name           = "Jinghua-J070WVTC0211",
		.refresh        = 60,
		.xres           = 800,
		.yres           = 480,
		.pixclock       = 33500, // picoseconds
		.left_margin    = 44,
		.right_margin   = 4,
		.upper_margin   = 21,
		.lower_margin   = 11,
		.hsync_len      = 2,
		.vsync_len      = 2,
		.sync           = FB_SYNC_ON_GREEN | FB_SYNC_COMP_HIGH_ACT,
		.vmode          = FB_VMODE_NONINTERLACED
	},
#if 0
	{
		.name           = "HDMI",
		.refresh        = 60,
		.xres           = 640,
		.yres           = 480,
		.pixclock       = 39721,
		.left_margin    = 48,
		.right_margin   = 16,
		.upper_margin   = 33,
		.lower_margin   = 10,
		.hsync_len      = 96,
		.vsync_len      = 2,
		.sync           = 0,
		.vmode          = FB_VMODE_NONINTERLACED
	},
#endif
};

/* Always use serial for U-Boot console */
int overwrite_console(void)
{
	return 1;
}

/* Run variable splashprepare to load bitmap image for splash */
int splash_screen_prepare(void)
{
	char *prep;

	prep = env_get("splashprepare");
	if (prep)
		run_command(prep, 0);

	return 0;
}

/* Enable backlight power and set brightness via I2C on RGB adapter */
void fs_disp_set_i2c_backlight(unsigned int bus, int on)
{
	u8 val;
#if 0
	printf("### ID = 0x%x\n", i2c_reg_read(0x60, 0));
#endif

	i2c_set_bus_num(bus);

	/*
	 * Talk to the PCA9632 via I2C, this is a 4 channel LED driver.
	 *  Channel 0: Used as GPIO to switch backlight power
	 *  Channel 1: Used as PWM to set backlight brightness
	 *  Channel 2: Used as GPIO to set display rotation
	 *  Channel 3: Unused
	 * Channels use inverted logic, i.e. ON outputs 0, OFF outputs 1, and
	 * the higher the PWM value, the lower the duty cycle.
	 */
	i2c_reg_write(0x60, 0x0, 0x0);	/* Normal mode, no auto-increment */
	i2c_reg_write(0x60, 0x1, 0x5);	/* Grp dimming, no inv., Totem pole */
	i2c_reg_write(0x60, 0x3, 0xf0);	/* PWM duty cycle for Channel 1 */
	if (on)
		val = 0x18;		/* CH2: ON=0, CH1: PWM, CH0: OFF=1 */
	else
		val = 0x11;		/* CH2: ON=0, CH1: OFF=1, CH0: ON=0 */
	i2c_reg_write(0x60, 0x8, val);
}

/* Set VCFL power via GPIO, several displays may share this GPIO */
void fs_disp_set_vcfl(int port, int on, int gpio)
{
	static unsigned int vcfl_users;
	int index = display_index[port];

	/* Switch on when first user enables and off when last user disables */
	if (!on)
		vcfl_users &= ~(1 << index);
	if (!vcfl_users) {
		if (displays[index].flags & FS_DISP_FLAGS_LVDS_VCFL_INV)
			gpio_direction_output(gpio, !on);
		else
			gpio_direction_output(gpio, on);
		if (on)
			mdelay(1);
	}
	if (on)
		vcfl_users |= (1 << index);
}

/* Set BKLT_PWM via GPIO, several displays may share this GPIO */
void fs_disp_set_bklt_pwm(int port, int on, int gpio)
{
	int index = display_index[port];

	if (displays[index].flags & FS_DISP_FLAGS_LVDS_BL_INV)
		on = !on;

	gpio_direction_output(gpio, on);
	mdelay(1);
}

#if 0 //###
static void show_dispflags(unsigned int flags)
{
	static char dispflags[200] = ",";
	char *p = &dispflags[0];

	if (flags & FS_DISP_FLAGS_LVDS_2CH) {
		strcpy(p, ",lvds2ch");
		p += strlen(p);
	}
	if (flags & FS_DISP_FLAGS_LVDS_DUP) {
		strcpy(p, ",lvdsdup");
		p += strlen(p);
	}
	if (flags & FS_DISP_FLAGS_LVDS_24BPP) {
		strcpy(p, ",lvds24");
		p += strlen(p);
	}
	if (flags & FS_DISP_FLAGS_LVDS_JEIDA) {
		strcpy(p, ",lvdsjeida");
		p += strlen(p);
	}
	if (flags & FS_DISP_FLAGS_LVDS_BL_INV) {
		strcpy(p, ",bl_inv");
		p += strlen(p);
	}
	if (flags & FS_DISP_FLAGS_LVDS_VCFL_INV) {
		strcpy(p, ",vcfl_inv");
		p += strlen(p);
	}

	printf("dispflags: %s\n", &dispflags[1]);
}

static void show_dispmode(const struct fb_videomode *mode)
{
	printf("dispmode: clk=%lu,rate=%u,hres=%u,vres=%u,hfp=%u,hbp=%u,vfp=%u,"
	       "vbp=%u,hsw=%u,vsw=%u,hsp=%u,vsp=%u,dep=%u,clkp=%u,il=%u\n",
	       PICOS2KHZ(mode->pixclock)*1000, mode->refresh, mode->xres,
	       mode->yres, mode->right_margin, mode->left_margin,
	       mode->lower_margin, mode->upper_margin, mode->hsync_len,
	       mode->vsync_len,
	       (mode->sync & FB_SYNC_HOR_HIGH_ACT) ? 1 : 0,
	       (mode->sync & FB_SYNC_VERT_HIGH_ACT) ? 1 : 0,
	       (mode->sync & FB_SYNC_COMP_HIGH_ACT) ? 1 : 0,
	       (mode->sync & FB_SYNC_ON_GREEN) ? 1 : 0,
	       (mode->vmode & FB_VMODE_INTERLACED) ? 1 : 0);
}
#endif //###

/* Parse <param>[=0|1]; returns 1 for error, 0 for match and no match */
static int parse_flag(const char *param, unsigned int *val, unsigned int mask)
{
	int len = strlen(param);
	const char *p = parse_pos;
	char *endp;
	unsigned int tmp = 1;

	if (strncmp(param, p, len))
		return 0;
	p += len;
	if (*p == '=') {
		tmp = simple_strtoul(++p, &endp, 0);
		if ((endp == p) || (tmp > 1))
			return 1;
		p = endp;
	}
	if (*p == ',')
		p++;
	else if (*p != '\0')
		return 1;
	parse_pos = p;
	if (tmp)
		*val |= mask;
	else
		*val &= ~mask;

	return 0;
}

/* Parse <param>=<uint>; returns 1 for error, 0 for match and no match */
static int parse_uint(const char *param, unsigned int *val)
{
	int len = strlen(param);
	const char *p = parse_pos;
	char *endp;
	unsigned int tmp;

	if (strncmp(param, p, len))
		return 0;
	p += len;
	if (*p != '=')
		return 0;
	tmp = simple_strtoul(++p, &endp, 0);
	if (endp == p)
		return 1;
	if (*endp == ',')
		endp++;
	else if (*endp != '\0')
		return 1;
	parse_pos = endp;
	*val = tmp;

	return 0;
}

static int parse_error(const char *varname)
{
	printf("%s: Invalid argument at '%s'\n", varname, parse_pos);

	return -1;
}

static int parse_dispflags(unsigned int *flags, unsigned int flags_mask)
{
	const char *s = env_get("dispflags");

	if (!s || !*s)
		return 0;

	/* Parse given string for paramters */
	parse_pos = s;
	do {
		s = parse_pos;
		if ((flags_mask & FS_DISP_FLAGS_LVDS_2CH)
		    && parse_flag("lvds2ch", flags, FS_DISP_FLAGS_LVDS_2CH))
			break;
		if ((flags_mask & FS_DISP_FLAGS_LVDS_DUP)
		    && parse_flag("lvdsdup", flags, FS_DISP_FLAGS_LVDS_DUP))
			break;
		if ((flags_mask & FS_DISP_FLAGS_LVDS_24BPP)
		    && parse_flag("lvds24", flags, FS_DISP_FLAGS_LVDS_24BPP))
			break;
		if ((flags_mask & FS_DISP_FLAGS_LVDS_JEIDA)
		    && parse_flag("lvdsjeida", flags, FS_DISP_FLAGS_LVDS_JEIDA))
			break;
		if ((flags_mask & FS_DISP_FLAGS_LVDS_BL_INV)
		    && parse_flag("bl_inv", flags, FS_DISP_FLAGS_LVDS_BL_INV))
			break;
		if ((flags_mask & FS_DISP_FLAGS_LVDS_VCFL_INV)
		    && parse_flag("vcfl_inv", flags, FS_DISP_FLAGS_LVDS_VCFL_INV))
			break;

		if (!*parse_pos)
			return 0;	/* Success, reached the end */
	} while (s != parse_pos);	/* We need progress in each cycle */

	return parse_error("dispflags");
}

static int parse_dispmode(struct fb_videomode *mode, const char *s)
{
	unsigned int tmp;

	if (!s || !*s)
		return 0;

	/* Parse given string for paramters */
	parse_pos = s;
	do {
		s = parse_pos;
		if (parse_uint("clk", &tmp) < 0)
			break;
		if (parse_pos != s)
			mode->pixclock = KHZ2PICOS(tmp/1000);
		if (parse_uint("rate", &mode->refresh)
		    || parse_uint("hres", &mode->xres)
		    || parse_uint("vres", &mode->yres)
		    || parse_uint("hfp", &mode->right_margin)
		    || parse_uint("hbp", &mode->left_margin)
		    || parse_uint("vfp", &mode->lower_margin)
		    || parse_uint("vbp", &mode->upper_margin)
		    || parse_uint("hsw", &mode->hsync_len)
		    || parse_uint("vsw", &mode->vsync_len)
		    || parse_flag("hsp", &mode->sync, FB_SYNC_HOR_HIGH_ACT)
		    || parse_flag("vsp", &mode->sync, FB_SYNC_VERT_HIGH_ACT)
		    || parse_flag("dep", &mode->sync, FB_SYNC_COMP_HIGH_ACT)
		    || parse_flag("clkp", &mode->sync, FB_SYNC_ON_GREEN)
		    || parse_flag("il", &mode->vmode, FB_VMODE_INTERLACED)) {
			break;
		}
		if (!*parse_pos)
			return 0;	/* Success, reached the end */
	} while (s != parse_pos);	/* We need progress in each cycle */

	return parse_error("dispmode");
}


/* Switch display power for all active displays */
void fs_disp_set_power_all(int on)
{
	int i;

	for (i = 0; i < display_count; i++)
		if (displays[i].state & FS_DISP_STATE_ACTIVE)
			board_display_set_power(displays[i].port, on);
}

/* Switch backlight of all active displays */
void fs_disp_set_backlight_all(int on)
{
	int i;

	for (i = 0; i < display_count; i++)
		if (displays[i].state & FS_DISP_STATE_ACTIVE)
			board_display_set_backlight(displays[i].port, on);
}

/* Check environment for display, return: 0: valid display, 1: no display */
int fs_disp_register(const struct fs_display_port *display_ports,
		     unsigned int valid_mask, int port)
{
	int i;
	unsigned int freq;
	const char *disppanel, *dispmode, *tmp;
	struct fs_display *disp;
	struct fb_videomode *m;
	const struct fb_videomode *orig;

	if ((port >= CONFIG_FS_DISP_COUNT) || !(valid_mask & (1 << port)))
		return 1;

	/* Skip all invalid ports when copying data to displays[] */
	display_count = 0;
	disp = &displays[0];
	for (i = 0; i < CONFIG_FS_DISP_COUNT; i++) {
		display_index[i] = -1;
		if (valid_mask & (1 << i)) {
			if (port == i)
				port = display_count;
			display_index[i] = display_count;
			disp->port = i;
			disp->name = display_ports->name;
			disp->flags_mask = display_ports->flags_mask;
			disp++;
			display_count++;
		}
		display_ports++;
	}

	disppanel = env_get("disppanel");
	if (!disppanel || !disppanel[0])
		return 1;
	dispmode = env_get("dispmode");

	/* Parse port name: Syntax: disppanel = [<port_name>:]<panel-name> */
	tmp = strchr(disppanel, ':');
	if (tmp) {
		size_t len = tmp - disppanel;

		disp = NULL;
		for (i = 0; i < display_count; i++) {
			if ((strlen(displays[i].name) == len)
			    && !strncmp(disppanel, displays[i].name, len)) {
				disp = &displays[i];
				break;
			}
		}
		if (!disp) {
			puts("disppanel: Unknown display port '");
			for (i = 0; i < len; i++)
				putc(disppanel[i]);
			puts("'\n");

			return 1;
		}
		disppanel += len + 1;
	} else
		disp = &displays[port];

	/* Look for panel in display database */
	for (i = 0; i < ARRAY_SIZE(display_db); i++) {
		if (!strcmp(disppanel, display_db[i].name))
			break;
	}
	if ((i >= ARRAY_SIZE(display_db))) {
		if (!dispmode) {
			printf("disppanel: Unknown display '%s'\n"
			       "For a user-defined display set variable"
			       " 'dispmode' with appropriate timings\n",
			       disppanel);
			return 1;
		}
		/* Use first entry for default parameters */
		i = 0;
	}

	/* Take mode default parameters from display database */
	orig = &display_db[i];
	disp->mode = *orig;
	m = &disp->mode;

	/* Parse mode parameters to override defaults and optional flags */
	if ((parse_dispmode(m, dispmode) < 0)
	    || (parse_dispflags(&disp->flags, disp->flags_mask) < 0))
		return 1;

	/*
	 * If pixelclock is given, use it to compute frame rate. If pixelclock
	 * is missing, compute it from frame rate. If frame rate is missing,
	 * too, assume 60 fps.
	 */
	freq = (m->xres + m->left_margin + m->right_margin + m->hsync_len)
		* (m->yres + m->upper_margin + m->lower_margin + m->vsync_len);
	if (m->pixclock) {
		m->refresh = (PICOS2KHZ(m->pixclock) * 1000 + freq / 2) / freq;
	} else {
		if (!m->refresh)
			m->refresh = 60;
		m->pixclock = KHZ2PICOS(freq * m->refresh / 1000);
	}

	/* Switch off backlight */
	board_display_set_backlight(disp->port, 0);
	disp->state &= ~FS_DISP_STATE_BL_ON;

	/* Start LCD voltage */
	board_display_set_power(disp->port, 1);
	disp->state |= FS_DISP_STATE_VLCD_ON;

	/* Start display */
	if (board_display_start(disp->port, disp->flags, m))
		return 1;
	disp->state |= FS_DISP_STATE_ACTIVE;

	printf("Disp.: %s (%ux%u", disppanel, m->xres, m->yres);
	if (memcmp(orig, m, sizeof(*m)))
		puts(", modified timings");
	printf(") on %s port\n", disp->name);

#if 0 //###
	show_dispmode(m);
	show_dispflags(disp->flags & disp->flags_mask);
#endif

	return 0;
}

#endif /* CONFIG_VIDEO_IPUV3 || CONFIG_VIDEO_MXS */
