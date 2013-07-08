/*
 * (C) Copyright 2009 Samsung Electronics
 * Minkyu Kang <mk7.kang@samsung.com>
 * Heungjun Kim <riverful.kim@samsung.com>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of
 * the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston,
 * MA 02111-1307 USA
 *
 */

#ifndef _S5PC1XX_CPU_H
#define _S5PC1XX_CPU_H

#define S5P_CPU_NAME		"S5P"
#define S5PC1XX_ADDR_BASE	0xE0000000

/* S5PC100 */
#define S5PC100_PRO_ID		0xE0000000
#define S5PC100_CLOCK_BASE	0xE0100000
#define S5PC100_GPIO_BASE	0xE0300000
#define S5PC100_VIC0_BASE	0xE4000000
#define S5PC100_VIC1_BASE	0xE4100000
#define S5PC100_VIC2_BASE	0xE4200000
#define S5PC100_DMC_BASE	0xE6000000
#define S5PC100_SROMC_BASE	0xE7000000
#define S5PC100_ONENAND_BASE	0xE7100000
#define S5PC100_NFCON_BASE	0xE7200000
#define S5PC100_PWMTIMER_BASE	0xEA000000
#define S5PC100_WATCHDOG_BASE	0xEA200000
#define S5PC100_RTC_BASE	0xEA300000
#define S5PC100_UART_BASE	0xEC000000
#define S5PC100_USB_PHY_BASE    0xED300000
#define S5PC100_USB_OHCI_BASE	0xED400000
#define S5PC100_USB_PHY_CONTROL 0x00000000
#define S5PC100_USB_EHCI_BASE	0x00000000 /* No USB 2.0 support */
#define S5PC100_MMC_BASE	0xED800000

/* S5PC110, S5PV210 */
#define S5PC110_PRO_ID		0xE0000000
#define S5PC110_CLOCK_BASE	0xE0100000
#define S5PC110_GPIO_BASE	0xE0200000
#define S5PC110_PWMTIMER_BASE	0xE2500000
#define S5PC110_WATCHDOG_BASE	0xE2700000
#define S5PC110_RTC_BASE	0xE2800000
#define S5PC110_UART_BASE	0xE2900000
#define S5PC110_SROMC_BASE	0xE8000000
#define S5PC110_MMC_BASE	0xEB000000
#define S5PC110_DMC0_BASE	0xF0000000
#define S5PC110_DMC1_BASE	0xF1400000
#define S5PC110_VIC0_BASE	0xF2000000
#define S5PC110_VIC1_BASE	0xF2100000
#define S5PC110_VIC2_BASE	0xF2200000
#define S5PC110_VIC3_BASE	0xF2300000
#define S5PC110_OTG_BASE	0xEC000000
#define S5PC110_USB_PHY_BASE	0xEC100000
#define S5PC110_USB_EHCI_BASE   0xEC200000
#define S5PC110_USB_OHCI_BASE   0xEC300000
#define S5PC110_USB_PHY_CONTROL 0xE010E80C
#define S5PC110_NFCON_BASE	0xB0E00000


#ifndef __ASSEMBLY__
#include <asm/io.h>
/* CPU detection macros */

/* RAM is not initialized yet, so don't use a variable for cpu_id */
static inline void s5p_set_cpu_id(void)
{
	DECLARE_GLOBAL_DATA_PTR;
	gd->cpu_id = (readl(S5PC100_PRO_ID) & 0x00FFFFFF) >> 12;
}

#define IS_SAMSUNG_TYPE(type, id)			\
static inline int cpu_is_##type(void)			\
{							\
        DECLARE_GLOBAL_DATA_PTR;			\
	return (gd->cpu_id == id);			\
}

IS_SAMSUNG_TYPE(s5pc100, 0x100)		  /* S5PC100 */
IS_SAMSUNG_TYPE(s5pc110, 0x110)		  /* S5PC110, S5PC111, S5PV210 */

static inline const char *s5p_get_cpu_name(void)
{
	const char const *s5pc110_dev_id[] = {
		"S5PV210",
		"S5PC110",
		"S5PC111",
	};

	if (cpu_is_s5pc100())
		return "S5PC100";
	if (cpu_is_s5pc110())
		return s5pc110_dev_id[readl(S5PC100_PRO_ID) & 0x0f];
	return S5P_CPU_NAME;
}

#define SAMSUNG_BASE(device, base)			\
static inline unsigned int samsung_get_base_##device(void)	\
{							\
	if (cpu_is_s5pc100())				\
		return S5PC100_##base;			\
	else if (cpu_is_s5pc110())			\
		return S5PC110_##base;			\
	else						\
		return 0;				\
}

SAMSUNG_BASE(clock, CLOCK_BASE)
SAMSUNG_BASE(gpio, GPIO_BASE)
SAMSUNG_BASE(pro_id, PRO_ID)
SAMSUNG_BASE(mmc, MMC_BASE)
SAMSUNG_BASE(sromc, SROMC_BASE)
SAMSUNG_BASE(timer, PWMTIMER_BASE)
SAMSUNG_BASE(uart, UART_BASE)
SAMSUNG_BASE(rtc, RTC_BASE)
SAMSUNG_BASE(nfcon, NFCON_BASE)
SAMSUNG_BASE(ohci, USB_OHCI_BASE)
SAMSUNG_BASE(ehci, USB_EHCI_BASE)
SAMSUNG_BASE(phy, USB_PHY_BASE)
SAMSUNG_BASE(phy_control, USB_PHY_CONTROL)
SAMSUNG_BASE(watchdog, WATCHDOG_BASE)
#endif

#endif	/* _S5PC1XX_CPU_H */
