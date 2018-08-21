/*
 * (C) Copyright 2012
 * F&S Elektronik Systeme GmbH
 * Written-by: H. Keller <keller@fs-net.de>
 *
 * SPDX-License-Identifier:	GPL-2.0+
 */

#include <common.h>
#include <asm/arch/cpu.h>		  /* samsung_get_base_phy(), ... */
#include <asm/arch/clock.h>		  /* struct s5pc110_clock */

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

	if (phy_control) {
		/* We're on an S5PC110 */
		struct s5pc110_clock *clock =
			(struct s5pc110_clock *)samsung_get_base_clock();
		unsigned int gate_ip1;

		/* Enable OTG clock while changing PHY registers */
		gate_ip1 = readl(&clock->gate_ip1);
		writel(gate_ip1 | (1 << 16), &clock->gate_ip1);

		/* On S5PC110 use PHY1 */
		writel(readl(phy_control) | (1<<1), phy_control);

		/* Configure UPHYCLK for 24 MHz crystal. */
		writel(readl(&phy->uphyclk) | (3<<0), &phy->uphyclk);

		/* Enable PHY1, activate analog power. Remark: bit 8 of
		   UPHYPWR must be set or PHY1 won't work! */
		writel((readl(&phy->uphypwr) & ~(3<<6)) | 0x100, &phy->uphypwr);

		/* Reset PHY1 */
		rstcon = readl(&phy->urstcon);
		writel(rstcon | (1<<3), &phy->urstcon);
		udelay(20);
		writel(rstcon & ~(1<<3), &phy->urstcon);

		/* Restore gate_ip1; this may switch off OTG clock again */
		writel(gate_ip1, &clock->gate_ip1);
	} else {
		/* We're on an S5PC100, use PHY */

		/* ### FIXME: Do we also have to switch on OTG clock on
		   S5PC100 before writing to USB PHY registers? ### */

		/* Reset PHY */
		rstcon = readl(&phy->urstcon);
		writel(rstcon | (1<<0), &phy->urstcon);
		udelay(20);
		writel(rstcon & ~(1<<0), &phy->urstcon);

		/* ### FIXME: If we switched on OTG clock above, switch it off
		   here again ### */
	}
	udelay(1000);

	return 0;
}

void usb_cpu_stop(void)
{
	unsigned int *phy_control =
		(unsigned int *)samsung_get_base_phy_control();

	if (phy_control) {
		/* We're on an S5PC110 */
		struct s5pc110_clock *clock =
			(struct s5pc110_clock *)samsung_get_base_clock();
		unsigned int gate_ip1;

		/* Enable OTG clock while changing PHY registers */
		gate_ip1 = readl(&clock->gate_ip1);
		writel(gate_ip1 | (1 << 16), &clock->gate_ip1);

		/* Disable PHY1 (USB Host) */
		writel(readl(phy_control) & ~(1<<1), phy_control);

		/* Restore gate_ip1; this may switch off OTG clock again */
		writel(gate_ip1, &clock->gate_ip1);
	}
}

void usb_cpu_init_fail(void)
{
	usb_cpu_stop();
}
