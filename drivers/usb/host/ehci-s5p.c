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
#include <asm/arch/clock.h>		  /* struct s5pc110_clock */

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
	struct s5pc110_clock *clock = 
		(struct s5pc110_clock *)samsung_get_base_clock();
	volatile unsigned int *phy_control;
	struct s5p_usb_phy *phy;
	unsigned int rstcon;
	unsigned int gate_ip1;

	/* EHCI is only available on S5PC110 */
	if (!ehci_base)
		return -1;

	hccr = (struct ehci_hccr *)ehci_base;
	hcor = (struct ehci_hcor *)(ehci_base + 0x10);

	/* Enable OTG clock while changing PHY registers */
	gate_ip1 = readl(&clock->gate_ip1);
	writel(gate_ip1 | (1 << 16), &clock->gate_ip1);

	/* Enable PHY1 (USB Host) */
	phy_control = (unsigned int *)samsung_get_base_phy_control();
	writel(readl(phy_control) | (1<<1), phy_control);

	phy = (struct s5p_usb_phy *)samsung_get_base_phy();

	/* Configure UPHYCLK for 24 MHz crystal. */
	writel(readl(&phy->uphyclk) | (3<<0), &phy->uphyclk);

	/* Enable PHY1, activate analog power. Remark: bit 8 of UPHYPWR must
	   be set or PHY1 won't work! */
	writel((readl(&phy->uphypwr) & ~(3<<6)) | 0x100, &phy->uphypwr);

	/* Reset PHY1 */
	rstcon = readl(&phy->urstcon);
	writel(rstcon | (1<<3), &phy->urstcon);
	udelay(20);
	writel(rstcon & ~(1<<3), &phy->urstcon);
	udelay(1000);

	/* Restore gate_ip1; this may switch off OTG clock again */
	writel(gate_ip1, &clock->gate_ip1);

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
	struct s5pc110_clock *clock = 
		(struct s5pc110_clock *)samsung_get_base_clock();
	unsigned int gate_ip1;

	/* Enable OTG clock while changing PHY registers */
	gate_ip1 = readl(&clock->gate_ip1);
	writel(gate_ip1 | (1 << 16), &clock->gate_ip1);

	/* Reset PHY1 */
	writel(readl(&phy->urstcon) | (1<<3), &phy->urstcon);

	/* PHY1: analog power down, suspend */
	writel(readl(&phy->uphypwr) | (3<<6), &phy->uphypwr);

	/* Disable PHY1 (USB Host) */
	writel(readl(phy_control) & ~(1<<1), phy_control);

	/* Restore gate_ip1; this may switch off OTG clock again */
	writel(gate_ip1, &clock->gate_ip1);

	return 0;
}
