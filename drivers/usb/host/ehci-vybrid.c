/*
 * (C) Copyright 2015
 * F&S Elektronik Systeme GmbH
 * Written-by: H. Keller <keller@fs-net.de>
 *
 * SPDX-License-Identifier:	GPL-2.0+
 */

#include <common.h>
#include <usb.h>
#include <errno.h>
#include <usb/ehci-ci.h>
#include <asm/io.h>

#include "ehci.h"
#include "regs-usbphy-mvf.h"

/* USBCMD */
#define USBCMD_OFFSET	0x140
#define UCMD_RUN_STOP	(1 << 0)	/* controller run/stop */
#define UCMD_RESET	(1 << 1)	/* controller reset */

static void usbh_internal_phy_clock_gate(int id, int on)
{
	void __iomem *phy_reg;

	if (!id)
		phy_reg = (void __iomem *)MVF_USBPHY0_BASE_ADDR;
	else
		phy_reg = (void __iomem *)MVF_USBPHY1_BASE_ADDR;
	
	if (on)
		__raw_writel(BM_USBPHY_CTRL_CLKGATE,
			     phy_reg + HW_USBPHY_CTRL_CLR);
	else
		__raw_writel(BM_USBPHY_CTRL_CLKGATE,
			     phy_reg + HW_USBPHY_CTRL_SET);
}

static void usbh_power_config(int id)
{
	void __iomem *anatop = (void __iomem *)ANATOP_BASE_ADDR;
	void __iomem *pll;
	void __iomem *chrg_det;
	void __iomem *vbus_det;

	if (!id) {
		pll = anatop + ANADIG_USB1_PLL_CTRL;
		chrg_det = anatop + ANADIG_USB1_CHRG_DET;
		vbus_det = anatop + ANADIG_USB1_VBUS_DET;
	} else {
		pll = anatop + ANADIG_USB2_PLL_CTRL;
		chrg_det = anatop + ANADIG_USB2_CHRG_DET;
		vbus_det = anatop + ANADIG_USB2_VBUS_DET;
	}

	/* Power up PLL, wait until PLL is locked, switch off bypass and
	   enable PLL output and USB clocks */
	__raw_writel(ANADIG_PLL_CTRL_POWER, pll + ANADIG_SET_OFFS);
	do {
	} while (!(__raw_readl(pll) & ANADIG_PLL_CTRL_LOCK));
	__raw_writel(ANADIG_PLL_CTRL_BYPASS, pll + ANADIG_CLR_OFFS);
	__raw_writel(ANADIG_PLL_CTRL_ENABLE
		     | ANADIG_PLL_CTRL_EN_USB_CLKS, pll + ANADIG_SET_OFFS);

	/* Disable charger detector or the DP signal will be poor */
	__raw_writel(ANADIG_USB_CHRG_DET_EN_B
		     | ANADIG_USB_CHRG_DET_CHK_CHGR_B
		     | ANADIG_USB_CHRG_DET_CHK_CONTACT, chrg_det);

	/* Disable VBUS detection */
	__raw_writel(0xE8, vbus_det + ANADIG_SET_OFFS);
}

static int usbh_phy_enable(int id)
{
	void __iomem *phy_reg;
	void __iomem *usb_cmd;
	u32 val;

	if (!id) {
		phy_reg = (void __iomem *)MVF_USBPHY0_BASE_ADDR;
		usb_cmd = (void __iomem *)(USBC0_BASE_ADDR + USBCMD_OFFSET);
	} else {
		phy_reg = (void __iomem *)MVF_USBPHY1_BASE_ADDR;
		usb_cmd = (void __iomem *)(USBC1_BASE_ADDR + USBCMD_OFFSET);;
	}

	/* Stop then Reset */
	val = __raw_readl(usb_cmd);
	val &= ~UCMD_RUN_STOP;
	__raw_writel(val, usb_cmd);
	do {
		val = __raw_readl(usb_cmd);
	} while (val & UCMD_RUN_STOP);
	__raw_writel(val | UCMD_RESET, usb_cmd);
	do {
		val = __raw_readl(usb_cmd);
	} while (val & UCMD_RESET);

	/* Reset USBPHY module */
	__raw_writel(BM_USBPHY_CTRL_SFTRST, phy_reg + HW_USBPHY_CTRL_SET);
	udelay(10);

	/* Remove CLKGATE and SFTRST */
	__raw_writel(BM_USBPHY_CTRL_CLKGATE | BM_USBPHY_CTRL_SFTRST,
		     phy_reg + HW_USBPHY_CTRL_CLR);
	udelay(10);

	/* Power up the PHY */
	__raw_writel(0, phy_reg + HW_USBPHY_PWD);

	/* Enable FS/LS device */
	__raw_writel(BM_USBPHY_CTRL_ENUTMILEVEL2 | BM_USBPHY_CTRL_ENUTMILEVEL3,
		     phy_reg + HW_USBPHY_CTRL_SET);

	return 0;
}

static void usbh_oc_config(int id)
{
	/* No overcurrent settings on Vybrid */
}

int __weak board_ehci_hcd_init(int port)
{
	return 0;
}

int ehci_hcd_init(int index, enum usb_init_type init,
		  struct ehci_hccr **hccr, struct ehci_hcor **hcor)
{
	struct usb_ehci *ehci;
	int id = index ? 0 : 1;		/* 1st call: USB1, 2nd call: USB0 */

	if (init != USB_INIT_HOST)
		return -ENODEV;

	if (!id)
		ehci = (struct usb_ehci *)USBC0_BASE_ADDR;
	else
		ehci = (struct usb_ehci *)USBC1_BASE_ADDR;

	/* Do board specific initialization */
	board_ehci_hcd_init(id);

	usbh_power_config(id);
	usbh_oc_config(id);

#if 0
	{
		void __iomem *phy_reg;
		u32 reg;

		if (!id)
			phy_reg = (void __iomem *)MVF_USBPHY0_BASE_ADDR;
		else
			phy_reg = (void __iomem *)MVF_USBPHY1_BASE_ADDR;

		__raw_writel(0x0220C802, phy_reg + HW_USBPHY_CTRL);
		udelay(20);

		/* Enable disconnect detect */
		reg = __raw_readl(phy_reg + HW_USBPHY_CTRL);
		reg &= ~0x040; /* clear OTG ID change IRQ */
		reg |= (0x1 << 9);
		reg &= ~((0xD1 << 11) | 0x6);
		__raw_writel(reg, phy_reg + HW_USBPHY_CTRL);
		__raw_writel(0x1 << 3, phy_reg + HW_USBPHY_CTRL_CLR);

		setbits_le32(&ehci->usbmode, CM_HOST);
		__raw_writel((PORT_PTS_UTMI | PORT_PTS_PTW), &ehci->portsc);
		setbits_le32(&ehci->portsc, USB_EN);
		mdelay(10);
	}
#endif

	usbh_internal_phy_clock_gate(id, 1);
	usbh_phy_enable(id);

	/*
	 * after the phy reset,can not read the value for id/vbus at
	 * the register of otgsc ,cannot  read at once ,need delay 3 ms
	 */
	mdelay(3);

	*hccr = (struct ehci_hccr *)((uint32_t)&ehci->caplength);
	*hcor = (struct ehci_hcor *)((uint32_t)*hccr +
				 HC_LENGTH(ehci_readl(&(*hccr)->cr_capbase)));

	return 0;
}

int ehci_hcd_stop(int index)
{
	return 0;
}
