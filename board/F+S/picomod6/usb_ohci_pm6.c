#include <common.h>

#ifdef CONFIG_USB_OHCI_NEW

#include <regs.h>

int usb_board_init(void)
{
	/* Clock source 48MHz for USB */
	CLK_SRC_REG &= ~(3<<5);

	/* Activate USB Host clock */
	HCLK_GATE_REG |=  (1<<29);

	/* Activate USB OTG clock */
	HCLK_GATE_REG |= (1<<20);

	/* Set OTG special flag */
	OTHERS_REG |= 1<<16;

	/* Power up OTG PHY */
	__REG(S3C_OTG_PHYPWR) = 0;

	/* Use UTMI interface of OTG or serial 1 interface of USB1.1 host */
	__REG(S3C_OTG_PHYCTRL) = 0x00; /* 0x40 for serial 2, or 0x00 for OTG */

	/* Reset PHY */
	__REG(S3C_OTG_RSTCON) = 1;
	udelay(50);
	__REG(S3C_OTG_RSTCON) = 0;
	udelay(50);

	/* Switch PWR1 on */
	GPKDAT_REG |= (1<<7);		  /* Out 1 */
	GPKCON0_REG = (GPKCON0_REG & ~0xf0000000) | 0x10000000; /* Output */
	GPKPUD_REG &= 0xffff3fff;	  /* No Pullup/down */

	/* Let the power voltage settle */
	//udelay(10000);
udelay(100000);

	return 0;
}

int usb_board_init_fail(void)
{
	return 0;
}

int usb_board_stop(void)
{
	GPKDAT_REG &= ~(1<<7);		  /* Out 0 */

	return 0;
}
#endif
