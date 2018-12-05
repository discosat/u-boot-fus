/*
 * Copyright 2012 Freescale Semiconductor, Inc.
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
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston,
 * MA 02111-1307 USA
 */

#ifndef __ASM_ARCH_VYBRID_REGS_H__
#define __ASM_ARCH_VYBRID_REGS_H__

#define IRAM_BASE_ADDR		0x3F000000	/* internal ram */
#define AIPS0_BASE_ADDR		0x40000000
#define AIPS1_BASE_ADDR		0x40080000
#define CSD0_BASE_ADDR		0x80000000	/* ddr 0 */
#define CSD1_BASE_ADDR		0xa0000000	/* ddr 1 */

#define IRAM_SIZE		0x00040000	/* 256 KB */

/* AIPS 0 */
#define MSCM_BASE_ADDR		(AIPS0_BASE_ADDR + 0x00001000)
#define CA5SCU_BASE_ADDR	(AIPS0_BASE_ADDR + 0x00002000)
#define CA5_INTD_BASE_ADDR	(AIPS0_BASE_ADDR + 0x00003000)
#define CA5_L2C_BASE_ADDR	(AIPS0_BASE_ADDR + 0x00006000)
#define NIC0_BASE_ADDR		(AIPS0_BASE_ADDR + 0x00008000)
#define NIC1_BASE_ADDR		(AIPS0_BASE_ADDR + 0x00009000)
#define NIC2_BASE_ADDR		(AIPS0_BASE_ADDR + 0x0000A000)
#define NIC3_BASE_ADDR		(AIPS0_BASE_ADDR + 0x0000B000)
#define NIC4_BASE_ADDR		(AIPS0_BASE_ADDR + 0x0000C000)
#define NIC5_BASE_ADDR		(AIPS0_BASE_ADDR + 0x0000D000)
#define NIC6_BASE_ADDR		(AIPS0_BASE_ADDR + 0x0000E000)
#define NIC7_BASE_ADDR		(AIPS0_BASE_ADDR + 0x0000F000)
#define AHBTZASC_BASE_ADDR	(AIPS0_BASE_ADDR + 0x00010000)
#define TZASC_SYS0_BASE_ADDR	(AIPS0_BASE_ADDR + 0x00011000)
#define TZASC_SYS1_BASE_ADDR	(AIPS0_BASE_ADDR + 0x00012000)
#define TZASC_GFX_BASE_ADDR	(AIPS0_BASE_ADDR + 0x00013000)
#define TZASC_DDR0_BASE_ADDR	(AIPS0_BASE_ADDR + 0x00014000)
#define TZASC_DDR1_BASE_ADDR	(AIPS0_BASE_ADDR + 0x00015000)
#define CSU_BASE_ADDR		(AIPS0_BASE_ADDR + 0x00017000)
#define DMA0_BASE_ADDR		(AIPS0_BASE_ADDR + 0x00018000)
#define DMA0_TCD_BASE_ADDR	(AIPS0_BASE_ADDR + 0x00019000)
#define SEMA4_BASE_ADDR		(AIPS0_BASE_ADDR + 0x0001D000)
#define FB_BASE_ADDR		(AIPS0_BASE_ADDR + 0x0001E000)
#define DMA_MUX0_BASE_ADDR	(AIPS0_BASE_ADDR + 0x00024000)
#define UART0_BASE		(AIPS0_BASE_ADDR + 0x00027000)
#define UART1_BASE		(AIPS0_BASE_ADDR + 0x00028000)
#define UART2_BASE		(AIPS0_BASE_ADDR + 0x00029000)
#define UART3_BASE		(AIPS0_BASE_ADDR + 0x0002A000)
#define SPI0_BASE_ADDR		(AIPS0_BASE_ADDR + 0x0002C000)
#define SPI1_BASE_ADDR		(AIPS0_BASE_ADDR + 0x0002D000)
#define SAI0_BASE_ADDR		(AIPS0_BASE_ADDR + 0x0002F000)
#define SAI1_BASE_ADDR		(AIPS0_BASE_ADDR + 0x00030000)
#define SAI2_BASE_ADDR		(AIPS0_BASE_ADDR + 0x00031000)
#define SAI3_BASE_ADDR		(AIPS0_BASE_ADDR + 0x00032000)
#define CRC_BASE_ADDR		(AIPS0_BASE_ADDR + 0x00033000)
#define USBC0_BASE_ADDR		(AIPS0_BASE_ADDR + 0x00034000)
#define PDB_BASE_ADDR		(AIPS0_BASE_ADDR + 0x00036000)
#define PIT_BASE_ADDR		(AIPS0_BASE_ADDR + 0x00037000)
#define FTM0_BASE_ADDR		(AIPS0_BASE_ADDR + 0x00038000)
#define FTM1_BASE_ADDR		(AIPS0_BASE_ADDR + 0x00039000)
#define ADC_BASE_ADDR		(AIPS0_BASE_ADDR + 0x0003B000)
#define TCON0_BASE_ADDR		(AIPS0_BASE_ADDR + 0x0003D000)
#define WDOG_A5_BASE_ADDR	(AIPS0_BASE_ADDR + 0x0003E000)
#define WDOG_M4_BASE_ADDR	(AIPS0_BASE_ADDR + 0x0003E000)
#define LPTMR_BASE_ADDR		(AIPS0_BASE_ADDR + 0x00040000)
#define RLE_BASE_ADDR		(AIPS0_BASE_ADDR + 0x00042000)
#define MLB_BASE_ADDR		(AIPS0_BASE_ADDR + 0x00043000)
#define QSPI0_BASE_ADDR		(AIPS0_BASE_ADDR + 0x00044000)
#define IOMUXC_BASE_ADDR	(AIPS0_BASE_ADDR + 0x00048000)
#define ANATOP_BASE_ADDR	(AIPS0_BASE_ADDR + 0x00050000)
#define SCSCM_BASE_ADDR		(AIPS0_BASE_ADDR + 0x00052000)
#define ASRC_BASE_ADDR		(AIPS0_BASE_ADDR + 0x00060000)
#define SPDIF_BASE_ADDR		(AIPS0_BASE_ADDR + 0x00061000)
#define ESAI_BASE_ADDR		(AIPS0_BASE_ADDR + 0x00062000)
#define ESAI_FIFO_BASE_ADDR	(AIPS0_BASE_ADDR + 0x00063000)
#define EWDOG_BASE_ADDR		(AIPS0_BASE_ADDR + 0x00065000)
#define I2C0_BASE_ADDR		(AIPS0_BASE_ADDR + 0x00066000)
#define WKUP_BASE_ADDR		(AIPS0_BASE_ADDR + 0x0006A000)
#define CCM_BASE_ADDR		(AIPS0_BASE_ADDR + 0x0006B000)
#define GPC_BASE_ADDR		(AIPS0_BASE_ADDR + 0x0006C000)
#define VREG_DIG_BASE_ADDR	(AIPS0_BASE_ADDR + 0x0006D000)
#define SRC_BASE_ADDR		(AIPS0_BASE_ADDR + 0x0006E000)
#define CMU_BASE_ADDR		(AIPS0_BASE_ADDR + 0x0006F000)
#define GPIO0_BASE_ADDR		(AIPS0_BASE_ADDR + 0x000FF000)
#define GPIO1_BASE_ADDR		(AIPS0_BASE_ADDR + 0x000FF040)
#define GPIO2_BASE_ADDR		(AIPS0_BASE_ADDR + 0x000FF080)
#define GPIO3_BASE_ADDR		(AIPS0_BASE_ADDR + 0x000FF0C0)
#define GPIO4_BASE_ADDR		(AIPS0_BASE_ADDR + 0x000FF100)

/* AIPS 1 */
#define OTP_BASE_ADDR		(AIPS1_BASE_ADDR + 0x00025000)
#define UART4_BASE		(AIPS1_BASE_ADDR + 0x00029000)
#define UART5_BASE		(AIPS1_BASE_ADDR + 0x0002A000)
#define DDR_BASE_ADDR		(AIPS1_BASE_ADDR + 0x0002E000)
#define ESDHC0_BASE_ADDR	(AIPS1_BASE_ADDR + 0x00031000)
#define ESDHC1_BASE_ADDR	(AIPS1_BASE_ADDR + 0x00032000)
#define USBC1_BASE_ADDR		(AIPS1_BASE_ADDR + 0x00034000)
#define QSPI1_BASE_ADDR		(AIPS1_BASE_ADDR + 0x00044000)
#define MACNET0_BASE_ADDR	(AIPS1_BASE_ADDR + 0x00050000)
#define MACNET1_BASE_ADDR	(AIPS1_BASE_ADDR + 0x00051000)
#define NFC_BASE_ADDR		(AIPS1_BASE_ADDR + 0x00060000)

#define MVF_GPIO1_BASE_ADDR	(0x400FF000)
#define MVF_GPIO2_BASE_ADDR	(0x400FF040)
#define MVF_GPIO3_BASE_ADDR	(0x400FF080)
#define MVF_GPIO4_BASE_ADDR	(0x400FF0C0)
#define MVF_GPIO5_BASE_ADDR	(0x400FF100)

#define MVF_USBPHY0_BASE_ADDR		0x40050800
#define MVF_USBPHY1_BASE_ADDR		0x40050C00

#define FEC_QUIRK_ENET_MAC

/* Number of GPIO pins per port */
#define GPIO_NUM_PIN		32

#define IIM_SREV		0x24
#define ROM_SI_REV		0x80

#define NFC_BUF_SIZE		0x1000

#define CHIP_REV_1_0		0x10
#define CHIP_REV_1_1		0x11
#define CHIP_REV_2_0		0x20
#define CHIP_REV_2_5		0x25
#define CHIP_REV_3_0		0x30

#define BOARD_REV_1_0		0x0
#define BOARD_REV_2_0		0x1

/*#define IMX_IIM_BASE		(IIM_BASE_ADDR)*/

#if !(defined(__KERNEL_STRICT_NAMES) || defined(__ASSEMBLY__))
#include <asm/types.h>

#define __REG(x)		(*((volatile u32 *)(x)))
#define __REG16(x)		(*((volatile u16 *)(x)))
#define __REG8(x)		(*((volatile u8 *)(x)))

struct clkctl {
	u32 ccr;	/* 0x00 */
	u32 csr;	/* 0x04 */
	u32 ccsr;	/* 0x08 */
	u32 cacrr;	/* 0x0C */
	u32 cscmr1;	/* 0x10 */
	u32 cscdr1;	/* 0x14 */
	u32 cscdr2;	/* 0x18 */
	u32 cscdr3;	/* 0x1C */
	u32 cscmr2;	/* 0x20 */
	u32 cscdr4;	/* 0x24 */
	u32 ctor;	/* 0x28 */
	u32 clpcr;	/* 0x2C */
	u32 cisr;	/* 0x30 */
	u32 cimr;	/* 0x34 */
	u32 ccosr;	/* 0x38 */
	u32 cgpr;	/* 0x3C */
	u32 ccgr0;	/* 0x40 */
	u32 ccgr1;	/* 0x44 */
	u32 ccgr2;	/* 0x48 */
	u32 ccgr3;	/* 0x4C */
	u32 ccgr4;	/* 0x50 */
	u32 ccgr5;	/* 0x54 */
	u32 ccgr6;	/* 0x58 */
	u32 ccgr7;	/* 0x5C */
	u32 ccgr8;	/* 0x60 */
	u32 ccgr9;	/* 0x64 */
	u32 ccgr10;	/* 0x68 */
	u32 ccgr11;	/* 0x6C */
	u32 cmeor0;	/* 0x70 */
	u32 cmeor1;	/* 0x74 */
	u32 cmeor2;	/* 0x78 */
	u32 cmeor3;	/* 0x7C */
	u32 cmeor4;	/* 0x80 */
	u32 cmeor5;	/* 0x84 */
	u32 cppdsr;	/* 0x88 */
	u32 ccowr;	/* 0x8C */
	u32 ccpgr0;	/* 0x90 */
	u32 ccpgr1;	/* 0x94 */
	u32 ccpgr2;	/* 0x98 */
	u32 ccpgr3;	/* 0x9C */
};

struct iomuxc {
	u32	gpr0;
	u32	gpr1;
	u32	omux0;
	u32	omux1;
	u32	omux2;
	u32	omux3;
	u32	omux4;
};

/* System Reset Controller (SRC) */
struct src {
	u32 scr;	/* 0x00 */
	u32 sbmr1;	/* 0x04 */
	u32 srsr;	/* 0x08 */
	u32 secr;	/* 0x0C */
	u32 gpsr;	/* 0x10 */
	u32 sicr;	/* 0x14 */
	u32 simr;	/* 0x18 */
	u32 sbmr2;	/* 0x1C */
	u32 gpr0;	/* 0x20 */
	u32 gpr1;	/* 0x24 */
	u32 gpr2;	/* 0x28 */
	u32 gpr3;	/* 0x2C */
	u32 gpr4;	/* 0x30 */
	u32 hab0;	/* 0x34 */
	u32 hab1;	/* 0x38 */
	u32 hab2;	/* 0x3C */
	u32 hab3;	/* 0x40 */
	u32 hab4;	/* 0x44 */
	u32 hab5;	/* 0x48 */
	u32 misc0;	/* 0x4C */
	u32 misc1;	/* 0x50 */
	u32 misc2;	/* 0x54 */
	u32 misc3;	/* 0x58 */
};

struct fuse_bank1_regs {
	u32	fuse0_8[9];
	u32	mac_addr[6];
	u32	fuse15_31[0x11];
};

#define ANADIG_USB1_PLL_CTRL	(0x10)
#define ANADIG_USB2_PLL_CTRL	(0x20)
#define ANADIG_PLL_528_CTRL	(0x30)
#define ANADIG_PLL_528_SS	(0x40)
#define ANADIG_PLL_528_NUM	(0x50)
#define ANADIG_PLL_528_DENOM	(0x60)
#define ANADIG_PLL_AUD_CTRL	(0x70)
#define ANADIG_PLL_AUD_NUM	(0x80)
#define ANADIG_PLL_AUD_DENOM	(0x90)
#define ANADIG_PLL_VID_CTRL	(0xA0)
#define ANADIG_PLL_VID_NUM	(0xB0)
#define ANADIG_PLL_VID_DENOM	(0xC0)
#define ANADIG_PLL_ENET_CTRL	(0xE0)
#define ANADIG_PLL_PFD_480_USB1	(0xF0)
#define ANADIG_PLL_PFD_528	(0x100)
#define ANADIG_REG_1P1		(0x110)
#define ANADIG_REG_3P0		(0x120)
#define ANADIG_REG_2P5		(0x130)
#define ANADIG_ANA_MISC0	(0x150)
#define ANADIG_ANA_MISC1	(0x160)
#define ANADIG_TEMPSENS0	(0x180)
#define ANADIG_USB1_VBUS_DET	(0x1A0)
#define ANADIG_USB1_CHRG_DET	(0x1B0)
#define ANADIG_USB1_VBUS_DETSTA	(0x1C0)
#define ANADIG_UAB1_CHRG_DETSTA	(0x1D0)
#define ANADIG_USB1_LOOPBACK	(0x1E0)
#define ANADIG_USB1_MISC	(0x1F0)
#define ANADIG_USB2_VBUS_DET	(0x200)
#define ANADIG_USB2_CHRG_DET	(0x210)
#define ANADIG_USB2_VBUS_DETSTA	(0x220)
#define ANADIG_USB2_CHRG_DETSTA	(0x230)
#define ANADIG_USB2_LOOPBACK	(0x240)
#define ANADIG_USB2_MISC	(0x250)
#define ANADIG_DIGPROG		(0x260)
#define ANADIG_PLL_SYS_CTRL	(0x270)
#define ANADIG_PLL_SYS_SS	(0x280)
#define ANADIG_PLL_SYS_NUM	(0x290)
#define ANADIG_PLL_SYS_DENOM	(0x2A0)
#define ANADIG_PFD_528_SYS	(0x2B0)
#define ANADIG_PLL_LOCK		(0x2C0)

/* Each ANADIG register has offsets to set, clear or toggle a subset of bits */
#define ANADIG_SET_OFFS 0x4
#define ANADIG_CLR_OFFS 0x8
#define ANADIG_TGL_OFFS 0xc

#define ANADIG_PLL_CTRL_LOCK (1u << 31)
#define ANADIG_PLL_CTRL_BYPASS	(1 << 16)
#define ANADIG_PLL_CTRL_ENABLE  (1 << 13)
#define ANADIG_PLL_CTRL_POWER_DOWN (1 << 12)
#define ANADIG_PLL_CTRL_POWER (1 << 12)
#define ANADIG_PLL_CTRL_EN_USB_CLKS (1 << 6)

#define ANADIG_PLL_CTRL_DIV_SELECT_BIT 0x01
#define ANADIG_PLL_CTRL_DIV_SELECT_MASK 0x7F

#define ANADIG_PLL_PFD_CLKGATE	(1 << 7)
#define ANADIG_PLL_PFD_FRAC_MASK 0x3F

#define ANADIG_USB_CHRG_DET_EN_B (1 << 20)
#define ANADIG_USB_CHRG_DET_CHK_CHGR_B (1 << 19)
#define ANADIG_USB_CHRG_DET_CHK_CONTACT (1 << 18)

/* CCM Clock Switcher Register (CCSR) */
#define CCSR_PLL3_PFD4_EN	(1u << 31)
#define CCSR_PLL3_PFD3_EN	(1 << 30)
#define CCSR_PLL3_PFD2_EN	(1 << 29)
#define CCSR_PLL3_PFD1_EN	(1 << 28)
#define CCSR_DAP_EN		(1 << 24)
#define CCSR_PLL2_CLK_SEL_MASK	(0x7 << 19)
#define CCSR_PLL2_CLK_SEL_SHIFT	19
#define CCSR_PLL1_CLK_SEL_MASK	(0x7 << 16)
#define CCSR_PLL1_CLK_SEL_SHIFT	16
#define CCSR_PLL2_PFD4_EN	(1 << 15)
#define CCSR_PLL2_PFD3_EN	(1 << 14)
#define CCSR_PLL2_PFD2_EN	(1 << 13)
#define CCSR_PLL2_PFD1_EN	(1 << 12)
#define CCSR_PLL1_PFD4_EN	(1 << 11)
#define CCSR_PLL1_PFD3_EN	(1 << 10)
#define CCSR_PLL1_PFD2_EN	(1 << 9)
#define CCSR_PLL1_PFD1_EN	(1 << 8)
#define CCSR_DDRC_CLK_SEL	(1 << 6)
#define CCSR_FAST_CLK_SEL	(1 << 5)
#define CCSR_SLOW_CLK_SEL	(1 << 4)
#define CCSR_SYS_CLK_SEL_MASK	(0x7 << 0)
#define CCSR_SYS_CLK_SEL_SHIFT	0

#define CACRR_IPG_CLK_DIV_MASK	(0x3 << 11)
#define CACRR_IPG_CLK_DIV_SHIFT	11
#define CACRR_PLL4_CLK_DIV_MASK	(0x7 << 6)
#define CACRR_PLL4_CLK_DIV_SHIFT 6
#define CACRR_BUS_CLK_DIV_MASK	(0x7 << 3)
#define CACRR_BUS_CLK_DIV_SHIFT	3
#define CACRR_ARM_CLK_DIV_MASK	(0x7 << 0)
#define CACRR_ARM_CLK_DIV_SHIFT	0

#define CSCMR1_ESDHC1_CLK_SEL_MASK	(0x3 << 18)
#define CSCMR1_ESDHC1_CLK_SEL_SHIFT	18
#define CSCMR1_ESDHC0_CLK_SEL_MASK	(0x3 << 16)
#define CSCMR1_ESDHC0_CLK_SEL_SHIFT	16

#define CSCMR2_RMII_CLK_SEL_MASK	(0x3 << 4)
#define CSCMR2_RMII_CLK_SEL_SHIFT	4

#define CSCDR2_ESDHC1_EN	(1 << 29)
#define CSCDR2_ESDHC0_EN	(1 << 28)
#define CSCDR2_ESDHC1_DIV_MASK	(0xF << 20)
#define CSCDR2_ESDHC1_DIV_SHIFT	20
#define CSCDR2_ESDHC0_DIV_MASK	(0xF << 16)
#define CSCDR2_ESDHC0_DIV_SHIFT	16

/* Low Power Timer */
#define LPTMR0_CSR		(LPTMR_BASE_ADDR + 0x000)
#define LPTMR0_PSR		(LPTMR_BASE_ADDR + 0x004)
#define LPTMR0_CMR		(LPTMR_BASE_ADDR + 0x008)
#define LPTMR0_CNR		(LPTMR_BASE_ADDR + 0x00C)

#define LPTMR_CSR_TCF		(1 << 7)
#define LPTMR_CSR_TIE		(1 << 6)
#define LPTMR_CSR_TPS_MASK	(0x3 << 4)
#define LPTMR_CSR_TPS_SHIFT	4
#define LPTMR_CSR_TPP		(1 << 3)
#define LPTMR_CSR_TFC		(1 << 2)
#define LPTMR_CSR_TMS		(1 << 1)
#define LPTMR_CSR_TEN		(1 << 0)

#define LPTMR_PSR_PRESCALE_MASK (0xF << 3)
#define LPTMR_PSR_PRESCALE_SHIFT 3
#define LPTMR_PSR_PBYP		(1 << 2)
#define LPTMR_PSR_PCS_MASK	(0x3 << 0)
#define LPTMR_PSR_PCS_SHIFT	0

#define LPTMR_CMR_COMPARE_MASK  (0xFFFF << 0)
#define LPTMR_CMR_COMPARE_SHIFT	0

#define LPTMR_CNR_COUNTER_MASK  (0xFFFF << 0)
#define LPTMR_CNR_COUNTER_SHIFT	0


/* DDR */

#define DDR_CR_BASE		DDR_BASE_ADDR
#define DDR_PHY_BASE		(DDR_BASE_ADDR + 0x400)

#define DDR_CR000		(DDR_CR_BASE + 0x000)
#define DDR_CR001		(DDR_CR_BASE + 0x004)
#define DDR_CR002		(DDR_CR_BASE + 0x008)
#define DDR_CR003		(DDR_CR_BASE + 0x00C)
#define DDR_CR004		(DDR_CR_BASE + 0x010)
#define DDR_CR005		(DDR_CR_BASE + 0x014)
#define DDR_CR006		(DDR_CR_BASE + 0x018)
#define DDR_CR007		(DDR_CR_BASE + 0x01C)
#define DDR_CR008		(DDR_CR_BASE + 0x020)
#define DDR_CR009		(DDR_CR_BASE + 0x024)

#define DDR_CR010		(DDR_CR_BASE + 0x028)
#define DDR_CR011		(DDR_CR_BASE + 0x02C)
#define DDR_CR012		(DDR_CR_BASE + 0x030)
#define DDR_CR013		(DDR_CR_BASE + 0x034)
#define DDR_CR014		(DDR_CR_BASE + 0x038)
#define DDR_CR015		(DDR_CR_BASE + 0x03C)
#define DDR_CR016		(DDR_CR_BASE + 0x040)
#define DDR_CR017		(DDR_CR_BASE + 0x044)
#define DDR_CR018		(DDR_CR_BASE + 0x048)
#define DDR_CR019		(DDR_CR_BASE + 0x04C)

#define DDR_CR020		(DDR_CR_BASE + 0x050)
#define DDR_CR021		(DDR_CR_BASE + 0x054)
#define DDR_CR022		(DDR_CR_BASE + 0x058)
#define DDR_CR023		(DDR_CR_BASE + 0x05C)
#define DDR_CR024		(DDR_CR_BASE + 0x060)
#define DDR_CR025		(DDR_CR_BASE + 0x064)
#define DDR_CR026		(DDR_CR_BASE + 0x068)
#define DDR_CR027		(DDR_CR_BASE + 0x06C)
#define DDR_CR028		(DDR_CR_BASE + 0x070)
#define DDR_CR029		(DDR_CR_BASE + 0x074)

#define DDR_CR030		(DDR_CR_BASE + 0x078)
#define DDR_CR031		(DDR_CR_BASE + 0x07C)
#define DDR_CR032		(DDR_CR_BASE + 0x080)
#define DDR_CR033		(DDR_CR_BASE + 0x084)
#define DDR_CR034		(DDR_CR_BASE + 0x088)
#define DDR_CR035		(DDR_CR_BASE + 0x08C)
#define DDR_CR036		(DDR_CR_BASE + 0x090)
#define DDR_CR037		(DDR_CR_BASE + 0x094)
#define DDR_CR038		(DDR_CR_BASE + 0x098)
#define DDR_CR039		(DDR_CR_BASE + 0x09C)

#define DDR_CR040		(DDR_CR_BASE + 0x0A0)
#define DDR_CR041		(DDR_CR_BASE + 0x0A4)
#define DDR_CR042		(DDR_CR_BASE + 0x0A8)
#define DDR_CR043		(DDR_CR_BASE + 0x0AC)
#define DDR_CR044		(DDR_CR_BASE + 0x0B0)
#define DDR_CR045		(DDR_CR_BASE + 0x0B4)
#define DDR_CR046		(DDR_CR_BASE + 0x0B8)
#define DDR_CR047		(DDR_CR_BASE + 0x0BC)
#define DDR_CR048		(DDR_CR_BASE + 0x0C0)
#define DDR_CR049		(DDR_CR_BASE + 0x0C4)

#define DDR_CR050		(DDR_CR_BASE + 0x0C8)
#define DDR_CR051		(DDR_CR_BASE + 0x0CC)
#define DDR_CR052		(DDR_CR_BASE + 0x0D0)
#define DDR_CR053		(DDR_CR_BASE + 0x0D4)
#define DDR_CR054		(DDR_CR_BASE + 0x0D8)
#define DDR_CR055		(DDR_CR_BASE + 0x0DC)
#define DDR_CR056		(DDR_CR_BASE + 0x0E0)
#define DDR_CR057		(DDR_CR_BASE + 0x0E4)
#define DDR_CR058		(DDR_CR_BASE + 0x0E8)
#define DDR_CR059		(DDR_CR_BASE + 0x0EC)

#define DDR_CR060		(DDR_CR_BASE + 0x0F0)
#define DDR_CR061		(DDR_CR_BASE + 0x0F4)
#define DDR_CR062		(DDR_CR_BASE + 0x0F8)
#define DDR_CR063		(DDR_CR_BASE + 0x0FC)
#define DDR_CR064		(DDR_CR_BASE + 0x100)
#define DDR_CR065		(DDR_CR_BASE + 0x104)
#define DDR_CR066		(DDR_CR_BASE + 0x108)
#define DDR_CR067		(DDR_CR_BASE + 0x10C)
#define DDR_CR068		(DDR_CR_BASE + 0x110)
#define DDR_CR069		(DDR_CR_BASE + 0x114)

#define DDR_CR070		(DDR_CR_BASE + 0x118)
#define DDR_CR071		(DDR_CR_BASE + 0x11C)
#define DDR_CR072		(DDR_CR_BASE + 0x120)
#define DDR_CR073		(DDR_CR_BASE + 0x124)
#define DDR_CR074		(DDR_CR_BASE + 0x128)
#define DDR_CR075		(DDR_CR_BASE + 0x12C)
#define DDR_CR076		(DDR_CR_BASE + 0x130)
#define DDR_CR077		(DDR_CR_BASE + 0x134)
#define DDR_CR078		(DDR_CR_BASE + 0x138)
#define DDR_CR079		(DDR_CR_BASE + 0x13C)

#define DDR_CR080		(DDR_CR_BASE + 0x140)
#define DDR_CR081		(DDR_CR_BASE + 0x144)
#define DDR_CR082		(DDR_CR_BASE + 0x148)
#define DDR_CR083		(DDR_CR_BASE + 0x14C)
#define DDR_CR084		(DDR_CR_BASE + 0x150)
#define DDR_CR085		(DDR_CR_BASE + 0x154)
#define DDR_CR086		(DDR_CR_BASE + 0x158)
#define DDR_CR087		(DDR_CR_BASE + 0x15C)
#define DDR_CR088		(DDR_CR_BASE + 0x160)
#define DDR_CR089		(DDR_CR_BASE + 0x164)

#define DDR_CR090		(DDR_CR_BASE + 0x168)
#define DDR_CR091		(DDR_CR_BASE + 0x16C)
#define DDR_CR092		(DDR_CR_BASE + 0x170)
#define DDR_CR093		(DDR_CR_BASE + 0x174)
#define DDR_CR094		(DDR_CR_BASE + 0x178)
#define DDR_CR095		(DDR_CR_BASE + 0x17C)
#define DDR_CR096		(DDR_CR_BASE + 0x180)
#define DDR_CR097		(DDR_CR_BASE + 0x184)
#define DDR_CR098		(DDR_CR_BASE + 0x188)
#define DDR_CR099		(DDR_CR_BASE + 0x18C)

#define DDR_CR100		(DDR_CR_BASE + 0x190)
#define DDR_CR101		(DDR_CR_BASE + 0x194)
#define DDR_CR102		(DDR_CR_BASE + 0x198)
#define DDR_CR103		(DDR_CR_BASE + 0x19C)
#define DDR_CR104		(DDR_CR_BASE + 0x1A0)
#define DDR_CR105		(DDR_CR_BASE + 0x1A4)
#define DDR_CR106		(DDR_CR_BASE + 0x1A8)
#define DDR_CR107		(DDR_CR_BASE + 0x1AC)
#define DDR_CR108		(DDR_CR_BASE + 0x1B0)
#define DDR_CR109		(DDR_CR_BASE + 0x1B4)

#define DDR_CR110		(DDR_CR_BASE + 0x1B8)
#define DDR_CR111		(DDR_CR_BASE + 0x1BC)
#define DDR_CR112		(DDR_CR_BASE + 0x1C0)
#define DDR_CR113		(DDR_CR_BASE + 0x1C4)
#define DDR_CR114		(DDR_CR_BASE + 0x1C8)
#define DDR_CR115		(DDR_CR_BASE + 0x1CC)
#define DDR_CR116		(DDR_CR_BASE + 0x1D0)
#define DDR_CR117		(DDR_CR_BASE + 0x1D4)
#define DDR_CR118		(DDR_CR_BASE + 0x1D8)
#define DDR_CR119		(DDR_CR_BASE + 0x1DC)

#define DDR_CR120		(DDR_CR_BASE + 0x1E0)
#define DDR_CR121		(DDR_CR_BASE + 0x1E4)
#define DDR_CR122		(DDR_CR_BASE + 0x1E8)
#define DDR_CR123		(DDR_CR_BASE + 0x1EC)
#define DDR_CR124		(DDR_CR_BASE + 0x1F0)
#define DDR_CR125		(DDR_CR_BASE + 0x1F4)
#define DDR_CR126		(DDR_CR_BASE + 0x1F8)
#define DDR_CR127		(DDR_CR_BASE + 0x1FC)
#define DDR_CR128		(DDR_CR_BASE + 0x200)
#define DDR_CR129		(DDR_CR_BASE + 0x204)

#define DDR_CR130		(DDR_CR_BASE + 0x208)
#define DDR_CR131		(DDR_CR_BASE + 0x20C)
#define DDR_CR132		(DDR_CR_BASE + 0x210)
#define DDR_CR133		(DDR_CR_BASE + 0x214)
#define DDR_CR134		(DDR_CR_BASE + 0x218)
#define DDR_CR135		(DDR_CR_BASE + 0x21C)
#define DDR_CR136		(DDR_CR_BASE + 0x220)
#define DDR_CR137		(DDR_CR_BASE + 0x224)
#define DDR_CR138		(DDR_CR_BASE + 0x228)
#define DDR_CR139		(DDR_CR_BASE + 0x22C)

#define DDR_CR140		(DDR_CR_BASE + 0x230)
#define DDR_CR141		(DDR_CR_BASE + 0x234)
#define DDR_CR142		(DDR_CR_BASE + 0x238)
#define DDR_CR143		(DDR_CR_BASE + 0x23C)
#define DDR_CR144		(DDR_CR_BASE + 0x240)
#define DDR_CR145		(DDR_CR_BASE + 0x244)
#define DDR_CR146		(DDR_CR_BASE + 0x248)
#define DDR_CR147		(DDR_CR_BASE + 0x24C)
#define DDR_CR148		(DDR_CR_BASE + 0x250)
#define DDR_CR149		(DDR_CR_BASE + 0x254)

#define DDR_CR150		(DDR_CR_BASE + 0x258)
#define DDR_CR151		(DDR_CR_BASE + 0x25C)
#define DDR_CR152		(DDR_CR_BASE + 0x260)
#define DDR_CR153		(DDR_CR_BASE + 0x264)
#define DDR_CR154		(DDR_CR_BASE + 0x268)
#define DDR_CR155		(DDR_CR_BASE + 0x26C)
#define DDR_CR156		(DDR_CR_BASE + 0x270)
#define DDR_CR157		(DDR_CR_BASE + 0x274)
#define DDR_CR158		(DDR_CR_BASE + 0x278)
#define DDR_CR159		(DDR_CR_BASE + 0x27C)

#define DDR_CR160		(DDR_CR_BASE + 0x280)
#define DDR_CR161		(DDR_CR_BASE + 0x284)
#define DDR_CR162		(DDR_CR_BASE + 0x288)
#define DDR_CR163		(DDR_CR_BASE + 0x28C)
#define DDR_CR164		(DDR_CR_BASE + 0x290)
#define DDR_CR165		(DDR_CR_BASE + 0x294)
#define DDR_CR166		(DDR_CR_BASE + 0x298)
#define DDR_CR167		(DDR_CR_BASE + 0x29C)
#define DDR_CR168		(DDR_CR_BASE + 0x2A0)
#define DDR_CR169		(DDR_CR_BASE + 0x2A4)

#define DDR_CR170		(DDR_CR_BASE + 0x2A8)
#define DDR_CR171		(DDR_CR_BASE + 0x2AC)
#define DDR_CR172		(DDR_CR_BASE + 0x2B0)
#define DDR_CR173		(DDR_CR_BASE + 0x2B4)
#define DDR_CR174		(DDR_CR_BASE + 0x2B8)
#define DDR_CR175		(DDR_CR_BASE + 0x2BC)
#define DDR_CR176		(DDR_CR_BASE + 0x2C0)
#define DDR_CR177		(DDR_CR_BASE + 0x2C4)
#define DDR_CR178		(DDR_CR_BASE + 0x2C8)
#define DDR_CR179		(DDR_CR_BASE + 0x2CC)

/*
 * PHY
 */
#define DDR_PHY000		(DDR_PHY_BASE + 0x000)
#define DDR_PHY001		(DDR_PHY_BASE + 0x004)
#define DDR_PHY002		(DDR_PHY_BASE + 0x008)
#define DDR_PHY003		(DDR_PHY_BASE + 0x00C)
#define DDR_PHY004		(DDR_PHY_BASE + 0x010)
#define DDR_PHY005		(DDR_PHY_BASE + 0x014)
#define DDR_PHY006		(DDR_PHY_BASE + 0x018)
#define DDR_PHY007		(DDR_PHY_BASE + 0x01C)
#define DDR_PHY008		(DDR_PHY_BASE + 0x020)
#define DDR_PHY009		(DDR_PHY_BASE + 0x024)

#define DDR_PHY010		(DDR_PHY_BASE + 0x028)
#define DDR_PHY011		(DDR_PHY_BASE + 0x02C)
#define DDR_PHY012		(DDR_PHY_BASE + 0x030)
#define DDR_PHY013		(DDR_PHY_BASE + 0x034)
#define DDR_PHY014		(DDR_PHY_BASE + 0x038)
#define DDR_PHY015		(DDR_PHY_BASE + 0x03C)
#define DDR_PHY016		(DDR_PHY_BASE + 0x040)
#define DDR_PHY017		(DDR_PHY_BASE + 0x044)
#define DDR_PHY018		(DDR_PHY_BASE + 0x048)
#define DDR_PHY019		(DDR_PHY_BASE + 0x04C)

#define DDR_PHY020		(DDR_PHY_BASE + 0x050)
#define DDR_PHY021		(DDR_PHY_BASE + 0x054)
#define DDR_PHY022		(DDR_PHY_BASE + 0x058)
#define DDR_PHY023		(DDR_PHY_BASE + 0x05C)
#define DDR_PHY024		(DDR_PHY_BASE + 0x060)
#define DDR_PHY025		(DDR_PHY_BASE + 0x064)
#define DDR_PHY026		(DDR_PHY_BASE + 0x068)
#define DDR_PHY027		(DDR_PHY_BASE + 0x06C)
#define DDR_PHY028		(DDR_PHY_BASE + 0x070)
#define DDR_PHY029		(DDR_PHY_BASE + 0x074)

#define DDR_PHY030		(DDR_PHY_BASE + 0x078)
#define DDR_PHY031		(DDR_PHY_BASE + 0x07C)
#define DDR_PHY032		(DDR_PHY_BASE + 0x080)
#define DDR_PHY033		(DDR_PHY_BASE + 0x084)
#define DDR_PHY034		(DDR_PHY_BASE + 0x088)
#define DDR_PHY035		(DDR_PHY_BASE + 0x08C)
#define DDR_PHY036		(DDR_PHY_BASE + 0x090)
#define DDR_PHY037		(DDR_PHY_BASE + 0x094)
#define DDR_PHY038		(DDR_PHY_BASE + 0x098)
#define DDR_PHY039		(DDR_PHY_BASE + 0x09C)

#define DDR_PHY040		(DDR_PHY_BASE + 0x0A0)
#define DDR_PHY041		(DDR_PHY_BASE + 0x0A4)
#define DDR_PHY042		(DDR_PHY_BASE + 0x0A8)
#define DDR_PHY043		(DDR_PHY_BASE + 0x0AC)
#define DDR_PHY044		(DDR_PHY_BASE + 0x0B0)
#define DDR_PHY045		(DDR_PHY_BASE + 0x0B4)
#define DDR_PHY046		(DDR_PHY_BASE + 0x0B8)
#define DDR_PHY047		(DDR_PHY_BASE + 0x0BC)
#define DDR_PHY048		(DDR_PHY_BASE + 0x0C0)
#define DDR_PHY049		(DDR_PHY_BASE + 0x0C4)

#define DDR_PHY050		(DDR_PHY_BASE + 0x0C8)
#define DDR_PHY051		(DDR_PHY_BASE + 0x0CC)
#define DDR_PHY052		(DDR_PHY_BASE + 0x0D0)
#define DDR_PHY053		(DDR_PHY_BASE + 0x0D4)
#define DDR_PHY054		(DDR_PHY_BASE + 0x0D8)
#define DDR_PHY055		(DDR_PHY_BASE + 0x0DC)
#define DDR_PHY056		(DDR_PHY_BASE + 0x0E0)
#define DDR_PHY057		(DDR_PHY_BASE + 0x0E4)
#define DDR_PHY058		(DDR_PHY_BASE + 0x0E8)
#define DDR_PHY059		(DDR_PHY_BASE + 0x0EC)

#define DDR_PHY060		(DDR_PHY_BASE + 0x0F0)
#define DDR_PHY061		(DDR_PHY_BASE + 0x0F4)
#define DDR_PHY062		(DDR_PHY_BASE + 0x0F8)
#define DDR_PHY063		(DDR_PHY_BASE + 0x0FC)
#define DDR_PHY064		(DDR_PHY_BASE + 0x100)
#define DDR_PHY065		(DDR_PHY_BASE + 0x104)
#define DDR_PHY066		(DDR_PHY_BASE + 0x108)
#define DDR_PHY067		(DDR_PHY_BASE + 0x10C)
#define DDR_PHY068		(DDR_PHY_BASE + 0x110)

/* Interrupts */
#define N_IRQS			144
#define IRQ_LPTimer0		72

#define GICC_ICR		(CA5SCU_BASE_ADDR + 0x100)
#define GICC_PMR		(CA5SCU_BASE_ADDR + 0x104)
#define GICC_BPR		(CA5SCU_BASE_ADDR + 0x108)
#define GICC_IAR		(CA5SCU_BASE_ADDR + 0x10c)
#define GICC_EOIR		(CA5SCU_BASE_ADDR + 0x110)
#define GICC_RPR		(CA5SCU_BASE_ADDR + 0x114)
#define GICC_HPIR		(CA5SCU_BASE_ADDR + 0x118)
#define GICC_ABPR		(CA5SCU_BASE_ADDR + 0x11C)

#define GICD_DCR		(CA5_INTD_BASE_ADDR + 0x000)
#define GICD_ICTR		(CA5_INTD_BASE_ADDR + 0x004)
#define GICD_IIDR		(CA5_INTD_BASE_ADDR + 0x008)
#define GICD_ISR		(CA5_INTD_BASE_ADDR + 0x080) /* 5 regs */
#define GICD_ISER		(CA5_INTD_BASE_ADDR + 0x100) /* 5 regs */
#define GICD_ICER		(CA5_INTD_BASE_ADDR + 0x180) /* 5 regs */
#define GICD_ISPR		(CA5_INTD_BASE_ADDR + 0x200) /* 5 regs */
#define GICD_ICPR		(CA5_INTD_BASE_ADDR + 0x280) /* 5 regs */
#define GICD_ABR		(CA5_INTD_BASE_ADDR + 0x300) /* 5 regs */
#define GICD_IPR		(CA5_INTD_BASE_ADDR + 0x400) /* 36 regs */
#define GICD_IPTR		(CA5_INTD_BASE_ADDR + 0x800) /* 36 regs */
#define GICD_ICFR		(CA5_INTD_BASE_ADDR + 0xC00) /* 9 regs */
#define GICD_PPIS		(CA5_INTD_BASE_ADDR + 0xD00)
#define GICD_SPIS		(CA5_INTD_BASE_ADDR + 0xD04) /* 5 regs */
#define GICD_SGIR		(CA5_INTD_BASE_ADDR + 0xF00)

#define MSCM_IRSPRC		(MSCM_BASE_ADDR + 0x880)     /* 112 regs */

#define MSCM_IRSPRC_CP0E	(1 << 0)
#define MSCM_IRSPRC_CP1E	(1 << 1)
#define MSCM_IRSPRC_RO		(1 << 15)


#endif /* __ASSEMBLER__*/

#endif				/* __ASM_ARCH_VYBRID_REGS_H__ */
