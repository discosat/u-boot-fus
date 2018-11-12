/*
 * fs_usb_common.c
 *
 * (C) Copyright 2018
 * Hartmut Keller, F&S Elektronik Systeme GmbH, keller@fs-net.de
 *
 * Common USB code for OTG mode, bus power, polarities, environment handling
 *
 * SPDX-License-Identifier:	GPL-2.0+
 */

#include <config.h>

#ifdef CONFIG_USB_EHCI_MX6

#include <common.h>
#include <usb.h>			/* USB_INIT_HOST, USB_INIT_DEVICE */
#include <asm/gpio.h>			/* gpio_direction_input() */
#include <asm/io.h>			/* readl(), writel() */
#include <asm/arch/mx6-pins.h>		/* MX6UL_PAD_*, SETUP_IOMUX_PADS() */
#include "fs_board_common.h"		/* fs_board_issue_reset() */
#include "fs_usb_common.h"		/* Own interface */

#define USB_OTHERREGS_OFFSET	0x800
#define UCTRL_PWR_POL		(1 << 9)
#define UCTRL_OVER_CUR_DIS	(1 << 7)

#define MAX_USB_PORTS \
	(CONFIG_USB_MAX_CONTROLLER_COUNT * CONFIG_SYS_USB_EHCI_MAX_ROOT_PORTS)

struct fs_usb_port {
	struct fs_usb_port_cfg cfg;
	int usb_mode;
	int index;
};

static struct fs_usb_port usb_port[MAX_USB_PORTS];

/* Setup pad for USB hub reset signal and issue 2ms reset */
static void fs_usb_reset_hub(struct fs_usb_port *port)
{
	if (!port->cfg.reset_pad)
		return;

	imx_iomux_v3_setup_multiple_pads(port->cfg.reset_pad, 1);
	if (port->cfg.reset_gpio >= 0)
		fs_board_issue_reset(2000, 0, port->cfg.reset_gpio, ~0, ~0);
}

/* Check if OTG port should be started in HOST or DEVICE mode */
static void fs_usb_get_otg_mode(struct fs_usb_port *port)
{
	char mode_name[] = "usb0mode";
	const char *envvar;
	unsigned int mode = port->cfg.mode - 2;

	mode_name[3] = port->index + '0';
	envvar = getenv(mode_name);
	if (envvar) {
		/* Check if user requested HOST or DEVICE */
		if (!strcmp(envvar, "peripheral") || !strcmp(envvar, "device"))
			mode = FS_USB_DEVICE;
		else if (!strcmp(envvar, "host"))
			mode = FS_USB_HOST;
		else if (!strcmp(envvar, "otg")
			 && port->cfg.id_pad && (port->cfg.id_gpio >= 0)) {
			/* OTG mode, check ID pin (as GPIO) to decide */
			imx_iomux_v3_setup_multiple_pads(port->cfg.id_pad, 1);
			gpio_direction_input(port->cfg.id_gpio);
			udelay(100);			/* Let voltage settle */
			if (gpio_get_value(port->cfg.id_gpio))
				mode = FS_USB_DEVICE;	/* Pulled high */
			else
				mode = FS_USB_HOST;	/* Connected to GND */
		}
	}

	port->cfg.mode = mode;
}

/* Determine USB host power polarity */
static void fs_usb_get_pwr_pol(struct fs_usb_port *port)
{
	char pwr_name[] = "usb0pol";
	const char *envvar;

	pwr_name[3] = port->index + '0';
	envvar = getenv(pwr_name);
	if (envvar) {
		/* Skip optional prefix "active", "active-" or "active_" */
		if (!strncmp(envvar, "active", 6)) {
			envvar += 6;
			if ((*envvar == '-') || (*envvar == '_'))
				envvar++;
		}

		if (!strcmp(envvar, "high"))
			port->cfg.pwr_pol = 0;
		if (!strcmp(envvar, "low"))
			port->cfg.pwr_pol = 1;
	}
}

/* Set up power pad and polarity; if GPIO, switch off for now */
static void fs_usb_config_pwr(struct fs_usb_port *port)
{
	if (!port->cfg.pwr_pad)
		return;			/* No power switching available */

	fs_usb_get_pwr_pol(port);

	/* Configure pad */
	imx_iomux_v3_setup_multiple_pads(port->cfg.pwr_pad, 1);

	/* Use PWR pad as GPIO or set power polarity in USB controller */
	if (port->cfg.pwr_gpio >= 0)
		gpio_direction_output(port->cfg.pwr_gpio, port->cfg.pwr_pol);
#ifdef CONFIG_FS_USB_PWR_USBNC
	else {
		u32 *usbnc_usb_ctrl;
		u32 val;

		usbnc_usb_ctrl = (u32 *)(USB_BASE_ADDR + USB_OTHERREGS_OFFSET +
					 port->index * 4);
		val = readl(usbnc_usb_ctrl);
		if (port->cfg.pwr_pol)
			val &= ~UCTRL_PWR_POL;
		else
			val |= UCTRL_PWR_POL;

		/* Disable over-current detection */
		val |= UCTRL_OVER_CUR_DIS;
		writel(val, usbnc_usb_ctrl);
	}
#endif /* CONFIG_FS_USB_PWR_USBNC */
}

/* Check if port is Host or Device */
int board_usb_phy_mode(int index)
{
	if ((index >= MAX_USB_PORTS) || (!usb_port[index].cfg.mode))
		return USB_INIT_HOST;	/* Port unknown or not initialized */

	return usb_port[index].usb_mode;
}

/* Switch VBUS power via GPIO */
int board_ehci_power(int index, int on)
{
	struct fs_usb_port *port;

	if (index >= MAX_USB_PORTS)
		return 0;		/* Unknown port */

	port = &usb_port[index];
	if (port->cfg.mode != FS_USB_HOST)
		return 0;		/* Port not in host mode */

	if (port->cfg.pwr_gpio < 0)
		return 0;		/* PWR not handled by GPIO */

	if (port->cfg.pwr_pol)
		on = !on;		/* Invert polarity */

	gpio_set_value(port->cfg.pwr_gpio, on);

	return 0;
}

/* Initialize one port, set role if OTG, switch on power if host */
int fs_usb_set_port(int index, struct fs_usb_port_cfg *cfg)
{
	struct fs_usb_port *port = &usb_port[index];

	port->cfg = *cfg;
	port->index = index;

	if (port->cfg.mode >= FS_USB_OTG_DEVICE)
		fs_usb_get_otg_mode(port);

	if (port->cfg.mode == FS_USB_DEVICE)
		port->usb_mode = USB_INIT_DEVICE;
	else {
		port->usb_mode = USB_INIT_HOST;
		fs_usb_reset_hub(port);
		fs_usb_config_pwr(port);
	}

	return 0;
}

#endif /* CONFIG_USB_EHCI_MX6 */
