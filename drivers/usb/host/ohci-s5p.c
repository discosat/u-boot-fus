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
#include <asm/arch/cpu.h>		  /* samsung_get_base_ehci() */

struct s5p_usb_phy {			  /* Offset */
	volatile unsigned int uphypwr;	  /* 0x00 */
	volatile unsigned int uphyclk;	  /* 0x04 */
	volatile unsigned int urstcon;	  /* 0x08 */
	volatile unsigned int res[5];	  /* 0x0c */
	volatile unsigned int uphytune1;  /* 0x20 */
	volatile unsigned int uphytune2;  /* 0x24 */
};


int usb_cpu_init(void)
{
	volatile unsigned int *phy_control = 
		(unsigned int *)samsung_get_base_phy_control();
	struct s5p_usb_phy *phy = (struct s5p_usb_phy *)samsung_get_base_phy();
	unsigned int rstcon;
	unsigned int reset;

	if (!phy_control) {
		/* We're on an S5PC100, use PHY */
		reset = (1<<0);
	} else {
		/* We're on an S5PC110; use PHY1 */
		writel(readl(phy_control) | (1<<1), phy_control);

		/* Bit 8 of UPHYPWR must be set or PHY1 won't work */
		writel(readl(&phy->uphypwr) | 0x100, &phy->uphypwr);

		reset = (1<<3);
	}

	/* Reset appropriate PHY */
	rstcon = readl(&phy->urstcon);
	rstcon |= reset;
	writel(rstcon, &phy->urstcon);
	udelay(20);
	rstcon &= ~reset;
	writel(rstcon, &phy->urstcon);

	return 0;
}

void usb_cpu_stop(void)
{
	unsigned int *phy_control =
		(unsigned int *)samsung_get_base_phy_control();

	if (phy_control) {
		/* Disable PHY1 (USB Host) */
		writel(readl(phy_control) & ~(1<<1), phy_control);
	}
}

void usb_cpu_init_fail(void)
{
	usb_cpu_stop();
}
