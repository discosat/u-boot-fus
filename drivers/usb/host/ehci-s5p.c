/*
 * (C) Copyright 2012
 * F&S Elektronik Systeme GmbH
 * Written-by: H. Keller <keller@fs-net.de>
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
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston,
 * MA 02110-1301 USA
 */

#include <common.h>
#include <asm/io.h>
#include <usb.h>
#include "ehci.h"			  /* struct ehci_{hccr,hcor} */
#include "ehci-core.h"			  /* hccr, hcor */
#include <asm/arch/cpu.h>		  /* samsung_get_base_ehci() */

/*
 * Create the appropriate control structures to manage
 * a new EHCI host controller.
 */
struct s5p_usb_phy {			  /* Offset */
	volatile unsigned int uphypwr;	  /* 0x00 */
	volatile unsigned int uphyclk;	  /* 0x04 */
	volatile unsigned int urstcon;	  /* 0x08 */
	volatile unsigned int res[5];	  /* 0x0c */
	volatile unsigned int uphytune1;  /* 0x20 */
	volatile unsigned int uphytune2;  /* 0x24 */
};


int ehci_hcd_init(void)
{
	unsigned int ehci_base = samsung_get_base_ehci();
	volatile unsigned int *phy_control;
	struct s5p_usb_phy *phy;
	unsigned int rstcon;

	/* EHCI is only available on S5PC110 */
	if (!ehci_base)
		return -1;

	hccr = (struct ehci_hccr *)ehci_base;
	hcor = (struct ehci_hcor *)(ehci_base + 0x10);

	/* Enable PHY1 (USB Host) */
	phy_control = (unsigned int *)samsung_get_base_phy_control();
	writel(readl(phy_control) | (1<<1), phy_control);

	udelay(30000);

	/* Enable PHY1, activate analog power. Remark: bit 8 of UPHYPWR must
	   be set or PHY1 won't work! */
	phy = (struct s5p_usb_phy *)samsung_get_base_phy();
	writel((readl(&phy->uphypwr) & ~(3<<6)) | 0x100, &phy->uphypwr);

	udelay(30000);

	/* Reset PHY1 */
	rstcon = readl(&phy->urstcon);
	rstcon |= (1<<3);
	writel(rstcon, &phy->urstcon);
	udelay(20);
	rstcon &= ~(1<<3);
	writel(rstcon, &phy->urstcon);

	udelay(30000);

	return 0;
}

/*
 * Destroy the appropriate control structures corresponding
 * the the EHCI host controller.
 */
int ehci_hcd_stop(void)
{
	unsigned int *phy_control =
		(unsigned int *)samsung_get_base_phy_control();
	struct s5p_usb_phy *phy = (struct s5p_usb_phy *)samsung_get_base_phy();
	unsigned int rstcon;

	/* Reset PHY1 */
	rstcon = readl(&phy->urstcon);
	rstcon |= (1<<3);
	writel(rstcon, &phy->urstcon);

	/* PHY1: analog power down, suspend */
	writel(readl(&phy->uphypwr) | (3<<6), &phy->uphypwr);

	/* Disable PHY1 (USB Host) */
	writel(readl(phy_control) & ~(1<<1), phy_control);

	return 0;
}
