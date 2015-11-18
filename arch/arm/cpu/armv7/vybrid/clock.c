/*
 * (C) Copyright 2007
 * Sascha Hauer, Pengutronix
 *
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

#include <common.h>
#include <asm/io.h>
#include <asm/errno.h>
#include <asm/arch/vybrid-regs.h>
#include <asm/arch/clock.h>
#include <div64.h>

enum pll_clocks {
	PLL1_CLOCK = 0,
	PLL2_CLOCK,
	PLL3_CLOCK,
	PLL4_CLOCK,
	PLL5_CLOCK,
	PLL6_CLOCK,
	PLL7_CLOCK,
	PLL_CLOCKS,
};

enum pfd_outputs {
	PFD1_OUTPUT = 0,
	PFD2_OUTPUT,
	PFD3_OUTPUT,
	PFD4_OUTPUT,
};

static u32 vybrid_get_pll_clk(enum pll_clocks pll)
{
	u32 freq, mfi, mfn, mfd, ctrl;

	switch (pll) {
	default:
	case PLL1_CLOCK:
		/* This is a 528MHz PLL, mainly for CPU core */
		ctrl = __raw_readl(ANATOP_BASE_ADDR + ANADIG_PLL_SYS_CTRL);
		mfi = (ctrl & ANADIG_PLL_CTRL_DIV_SELECT_BIT) ? 22 : 20;
		mfn = __raw_readl(ANATOP_BASE_ADDR + ANADIG_PLL_SYS_NUM);
		mfd = __raw_readl(ANATOP_BASE_ADDR + ANADIG_PLL_SYS_DENOM);
		break;

	case PLL2_CLOCK:
		/* This is a 528MHz PLL, mainly for RAM access */
		ctrl = __raw_readl(ANATOP_BASE_ADDR + ANADIG_PLL_528_CTRL);
		mfi = (ctrl & ANADIG_PLL_CTRL_DIV_SELECT_BIT) ? 22 : 20;
		mfn = __raw_readl(ANATOP_BASE_ADDR + ANADIG_PLL_528_NUM);
		mfd = __raw_readl(ANATOP_BASE_ADDR + ANADIG_PLL_528_DENOM);
		break;

	case PLL3_CLOCK:
		/* This is a 480MHz PLL, mainly for USB0 access */
		ctrl = __raw_readl(ANATOP_BASE_ADDR + ANADIG_USB1_PLL_CTRL);
		mfi = 20;
		mfn = 0;
		mfd = 18;
		break;

	case PLL4_CLOCK:
		/* This PLL can create 600 to 1300MHz, mainly for Audio */
		ctrl = __raw_readl(ANATOP_BASE_ADDR + ANADIG_PLL_AUD_CTRL);
		mfi = ctrl & ANADIG_PLL_CTRL_DIV_SELECT_MASK;
		mfn = __raw_readl(ANATOP_BASE_ADDR + ANADIG_PLL_AUD_NUM);
		mfd = __raw_readl(ANATOP_BASE_ADDR + ANADIG_PLL_AUD_DENOM);
		break;

	case PLL5_CLOCK:
		/* This PLL outputs 1000MHz, internally divided by 2; the
		   DIV_SELECTION selects final speed, which is 50MHz when
		   setting 1; mainly for Ethernet */
		ctrl = __raw_readl(ANATOP_BASE_ADDR + ANADIG_PLL_ENET_CTRL);
		mfi = 0x29;
		mfn = 0x0AAAAD44;
		mfd = 0x100003E6;
		break;

	case PLL6_CLOCK:
		/* This PLL can create 600 to 1300MHz, mainly for Video */
		ctrl = __raw_readl(ANATOP_BASE_ADDR + ANADIG_PLL_VID_CTRL);
		mfi = ctrl & ANADIG_PLL_CTRL_DIV_SELECT_MASK;
		mfn = __raw_readl(ANATOP_BASE_ADDR + ANADIG_PLL_VID_NUM);
		mfd = __raw_readl(ANATOP_BASE_ADDR + ANADIG_PLL_VID_DENOM);
		break;

	case PLL7_CLOCK:
		/* This is a 480MHz PLL, mainly for USB1 access */
		ctrl = __raw_readl(ANATOP_BASE_ADDR + ANADIG_USB2_PLL_CTRL);
		mfi = 20;
		mfn = 0;
		mfd = 18;
		break;
	}

	freq = (ctrl & ANADIG_PLL_CTRL_ENABLE) ? CONFIG_SYS_VYBRID_HCLK : 0;
	if (!(ctrl & ANADIG_PLL_CTRL_BYPASS)) {
		uint64_t part;

		part = (uint64_t)freq * mfn;
		do_div(part, mfd);

		freq = freq*mfi + (u32)part;
	}

	return freq;
}

static u32 vybrid_get_pll_pfd_clk(enum pll_clocks pll, enum pfd_outputs pfd_num)
{
	u32 freq;
	u32 pfd;

	freq = vybrid_get_pll_clk(pll);

	switch (pll) {
	default:
	case PLL1_CLOCK:
		pfd = __raw_readl(ANATOP_BASE_ADDR + ANADIG_PFD_528_SYS);
		break;
	case PLL2_CLOCK:
		pfd = __raw_readl(ANATOP_BASE_ADDR + ANADIG_PLL_PFD_528);
		break;
	case PLL3_CLOCK:
		pfd = __raw_readl(ANATOP_BASE_ADDR + ANADIG_PLL_PFD_480_USB1);
		break;

	case PLL4_CLOCK:
	case PLL5_CLOCK:
	case PLL6_CLOCK:
	case PLL7_CLOCK:
		return freq;		/* These PLLs don't have PFDs */
	}

	pfd >>= pfd_num * 8;

	if (pfd & ANADIG_PLL_PFD_CLKGATE)
		freq = 0;
	else {
		uint64_t part;

		part = (uint64_t)freq * 18;
		do_div(part, pfd & ANADIG_PLL_PFD_FRAC_MASK);
		freq = (u32)part;
	}

	return freq;
}

static u32 vybrid_get_arm_clk(void)
{
	struct clkctl *ccm = (struct clkctl *)CCM_BASE_ADDR;
	u32 ccsr = __raw_readl(&ccm->ccsr);
	u32 cacrr;
	u32 freq;
	u32 s;
	u32 div;

	s = (ccsr & CCSR_SYS_CLK_SEL_MASK) >> CCSR_SYS_CLK_SEL_SHIFT;
	switch (s) {
	case 0:				/* Fast clock */
		if (ccsr & CCSR_FAST_CLK_SEL)
			freq = CONFIG_SYS_VYBRID_HCLK;
		else
			freq = 24000000;
		break;
	case 1:				/* Slow clock */
		if (ccsr & CCSR_SLOW_CLK_SEL)
			freq = CONFIG_SYS_VYBRID_CLK32;
		else
			freq = 32768;
		break;
	case 2:				/* PLL2 PFD */
		s = (ccsr & CCSR_PLL2_CLK_SEL_MASK) >> CCSR_PLL2_CLK_SEL_SHIFT;
		switch (s) {
		case 0:
			freq = vybrid_get_pll_clk(PLL2_CLOCK);
			break;
		case 1:
			freq = vybrid_get_pll_pfd_clk(PLL2_CLOCK, PFD1_OUTPUT);
			break;
		case 2:
			freq = vybrid_get_pll_pfd_clk(PLL2_CLOCK, PFD2_OUTPUT);
			break;
		case 3:
			freq = vybrid_get_pll_pfd_clk(PLL2_CLOCK, PFD3_OUTPUT);
			break;
		case 4:
			freq = vybrid_get_pll_pfd_clk(PLL2_CLOCK, PFD4_OUTPUT);
			break;
		default:
			freq = 0;
			break;
		}
		break;
	case 3:				/* PLL2 main clock */
		freq = vybrid_get_pll_clk(PLL2_CLOCK);
		break;
	case 4:				/* PLL1 PFD */
		s = (ccsr & CCSR_PLL1_CLK_SEL_MASK) >> CCSR_PLL1_CLK_SEL_SHIFT;
		switch (s) {
		case 0:
			freq = vybrid_get_pll_clk(PLL1_CLOCK);
			break;
		case 1:
			freq = vybrid_get_pll_pfd_clk(PLL1_CLOCK, PFD1_OUTPUT);
			break;
		case 2:
			freq = vybrid_get_pll_pfd_clk(PLL1_CLOCK, PFD2_OUTPUT);
			break;
		case 3:
			freq = vybrid_get_pll_pfd_clk(PLL1_CLOCK, PFD3_OUTPUT);
			break;
		case 4:
			freq = vybrid_get_pll_pfd_clk(PLL1_CLOCK, PFD4_OUTPUT);
			break;
		default:
			freq = 0;
			break;
		}
		break;
	case 5:				/* PLL3 main clock */
		freq = vybrid_get_pll_clk(PLL3_CLOCK);
		break;
	default:
		freq = 0;
		break;
	}

	cacrr = __raw_readl(&ccm->cacrr);
	div = (cacrr & CACRR_ARM_CLK_DIV_MASK) >> CACRR_ARM_CLK_DIV_SHIFT;

	return freq / (div + 1);
}

static u32 vybrid_get_ddr_clk(void)
{
	struct clkctl *ccm = (struct clkctl *)CCM_BASE_ADDR;
	u32 ccsr = __raw_readl(&ccm->ccsr);
	u32 freq;

	if (ccsr & CCSR_DDRC_CLK_SEL)
		freq = vybrid_get_arm_clk();
	else
		freq = vybrid_get_pll_pfd_clk(PLL2_CLOCK, PFD2_OUTPUT);

	return freq;
}

static u32 vybrid_get_bus_clk(void)
{
	struct clkctl *ccm = (struct clkctl *)CCM_BASE_ADDR;
	u32 freq = vybrid_get_arm_clk();
	u32 cacrr = __raw_readl(&ccm->cacrr);
	u32 div = (cacrr & CACRR_BUS_CLK_DIV_MASK) >> CACRR_BUS_CLK_DIV_SHIFT;

	return freq / (div + 1);
}

static u32 vybrid_get_ipg_clk(void)
{
	struct clkctl *ccm = (struct clkctl *)CCM_BASE_ADDR;
	u32 freq = vybrid_get_bus_clk();
	u32 cacrr = __raw_readl(&ccm->cacrr);
	u32 div = (cacrr & CACRR_IPG_CLK_DIV_MASK) >> CACRR_IPG_CLK_DIV_SHIFT;

	return freq / (div + 1);
}

static u32 vybrid_get_fec_clk(void)
{
	struct clkctl *ccm = (struct clkctl *)CCM_BASE_ADDR;
        u32 cscmr2, rmii_clk_sel;
        u32 freq = 0;

        cscmr2 = __raw_readl(&ccm->cscmr2);
        rmii_clk_sel = cscmr2 & CSCMR2_RMII_CLK_SEL_MASK;
        rmii_clk_sel >>= CSCMR2_RMII_CLK_SEL_SHIFT;

        switch (rmii_clk_sel) {
        case 0:
                freq = ENET_EXTERNAL_CLK;
                break;
        case 1:
                freq = AUDIO_EXTERNAL_CLK;
                break;
        case 2:
                freq = vybrid_get_pll_clk(PLL5_CLOCK);
                break;
        case 3:
                freq = vybrid_get_pll_clk(PLL5_CLOCK) / 2;
                break;
        }

        return freq;
}

/* The API of get vybrid clocks. */
unsigned int vybrid_get_clock(enum vybrid_clock clk)
{
	switch (clk) {
	case VYBRID_ARM_CLK:
		return vybrid_get_arm_clk();
	case VYBRID_DDR_CLK:
		return vybrid_get_ddr_clk();
	case VYBRID_IPG_CLK:
		return vybrid_get_ipg_clk();
	case VYBRID_FEC_CLK:
		return vybrid_get_fec_clk();

	default:
		break;
	}

	return 0;
}

unsigned int vybrid_get_esdhc_clk(int esdhc_num)
{
	u32 esdhc_clk_sel;
	u32 esdhc_en;
	u32 esdhc_div;
	struct clkctl *ccm = (struct clkctl *)CCM_BASE_ADDR;
	u32 cscdr2 = __raw_readl(&ccm->cscdr2);
	u32 cscmr1 = __raw_readl(&ccm->cscmr1);
	u32 freq;

	if (esdhc_num == 0) {
		esdhc_clk_sel = (cscmr1 & CSCMR1_ESDHC0_CLK_SEL_MASK)
					>> CSCMR1_ESDHC0_CLK_SEL_SHIFT;
		esdhc_en = (cscdr2 & CSCDR2_ESDHC0_EN);
		esdhc_div = (cscdr2 & CSCDR2_ESDHC0_DIV_MASK)
					>> CSCDR2_ESDHC0_DIV_SHIFT;
	} else {
		esdhc_clk_sel = (cscmr1 & CSCMR1_ESDHC1_CLK_SEL_MASK)
					>> CSCMR1_ESDHC1_CLK_SEL_SHIFT;
		esdhc_en = (cscdr2 & CSCDR2_ESDHC1_EN);
		esdhc_div = (cscdr2 & CSCDR2_ESDHC1_DIV_MASK)
					>> CSCDR2_ESDHC1_DIV_SHIFT;
	}

	if (esdhc_en) {
		if (esdhc_clk_sel == 0)
			freq = vybrid_get_pll_clk(PLL3_CLOCK);
		else if (esdhc_clk_sel == 1)
			freq = vybrid_get_pll_pfd_clk(PLL3_CLOCK, PFD3_OUTPUT);
		else if (esdhc_clk_sel == 2)
			freq = vybrid_get_pll_pfd_clk(PLL1_CLOCK, PFD3_OUTPUT);
		else
			freq = vybrid_get_bus_clk();
	} else
		freq = 0;

	return freq / (esdhc_div + 1);
}

/* Dump some core clockes. */
int do_vybrid_showclocks(cmd_tbl_t *cmdtp, int flag, int argc,
			 char * const argv[])
{
	u32 freq;
	static const char const *disabled = " (disabled in CCM)";
	struct clkctl *ccm = (struct clkctl *)CCM_BASE_ADDR;
	u32 ccsr = __raw_readl(&ccm->ccsr);

	freq = vybrid_get_pll_clk(PLL1_CLOCK);
	printf("PLL1 Output %u.%06u MHz\n", freq / 1000000, freq % 1000000);
	freq = vybrid_get_pll_pfd_clk(PLL1_CLOCK, PFD1_OUTPUT);
	printf("  PLL1 PFD1 %u.%06u MHz%s\n", freq / 1000000, freq % 1000000,
	       (ccsr & CCSR_PLL1_PFD1_EN) ? "" : disabled);
	freq = vybrid_get_pll_pfd_clk(PLL1_CLOCK, PFD2_OUTPUT);
	printf("  PLL1 PFD2 %u.%06u MHz%s\n", freq / 1000000, freq % 1000000,
	       (ccsr & CCSR_PLL1_PFD2_EN) ? "" : disabled);
	freq = vybrid_get_pll_pfd_clk(PLL1_CLOCK, PFD3_OUTPUT);
	printf("  PLL1 PFD3 %u.%06u MHz%s\n", freq / 1000000, freq % 1000000,
	       (ccsr & CCSR_PLL1_PFD3_EN) ? "" : disabled);
	freq = vybrid_get_pll_pfd_clk(PLL1_CLOCK, PFD4_OUTPUT);
	printf("  PLL1 PFD4 %u.%06u MHz%s\n", freq / 1000000, freq % 1000000,
	       (ccsr & CCSR_PLL1_PFD4_EN) ? "" : disabled);
	freq = vybrid_get_pll_clk(PLL2_CLOCK);
	printf("PLL2 Output %u.%06u MHz\n", freq / 1000000, freq % 1000000);
	freq = vybrid_get_pll_pfd_clk(PLL2_CLOCK, PFD1_OUTPUT);
	printf("  PLL2 PFD1 %u.%06u MHz%s\n", freq / 1000000, freq % 1000000,
	       (ccsr & CCSR_PLL2_PFD1_EN) ? "" : disabled);
	freq = vybrid_get_pll_pfd_clk(PLL2_CLOCK, PFD2_OUTPUT);
	printf("  PLL2 PFD2 %u.%06u MHz%s\n", freq / 1000000, freq % 1000000,
	       (ccsr & CCSR_PLL2_PFD2_EN) ? "" : disabled);
	freq = vybrid_get_pll_pfd_clk(PLL2_CLOCK, PFD3_OUTPUT);
	printf("  PLL2 PFD3 %u.%06u MHz%s\n", freq / 1000000, freq % 1000000,
	       (ccsr & CCSR_PLL2_PFD3_EN) ? "" : disabled);
	freq = vybrid_get_pll_pfd_clk(PLL2_CLOCK, PFD4_OUTPUT);
	printf("  PLL2 PFD4 %u.%06u MHz%s\n", freq / 1000000, freq % 1000000,
	       (ccsr & CCSR_PLL2_PFD4_EN) ? "" : disabled);
	freq = vybrid_get_pll_clk(PLL3_CLOCK);
	printf("PLL3 Output %u.%06u MHz\n", freq / 1000000, freq % 1000000);
	freq = vybrid_get_pll_pfd_clk(PLL3_CLOCK, PFD1_OUTPUT);
	printf("  PLL3 PFD1 %u.%06u MHz%s\n", freq / 1000000, freq % 1000000,
	       (ccsr & CCSR_PLL3_PFD1_EN) ? "" : disabled);
	freq = vybrid_get_pll_pfd_clk(PLL3_CLOCK, PFD2_OUTPUT);
	printf("  PLL3 PFD2 %u.%06u MHz%s\n", freq / 1000000, freq % 1000000,
	       (ccsr & CCSR_PLL3_PFD2_EN) ? "" : disabled);
	freq = vybrid_get_pll_pfd_clk(PLL3_CLOCK, PFD3_OUTPUT);
	printf("  PLL3 PFD3 %u.%06u MHz%s\n", freq / 1000000, freq % 1000000,
	       (ccsr & CCSR_PLL3_PFD3_EN) ? "" : disabled);
	freq = vybrid_get_pll_pfd_clk(PLL3_CLOCK, PFD4_OUTPUT);
	printf("  PLL3 PFD4 %u.%06u MHz%s\n", freq / 1000000, freq % 1000000,
	       (ccsr & CCSR_PLL3_PFD4_EN) ? "" : disabled);
	freq = vybrid_get_pll_clk(PLL4_CLOCK);
	printf("PLL4 Output %u.%06u MHz\n", freq / 1000000, freq % 1000000);
	freq = vybrid_get_pll_clk(PLL5_CLOCK);
	printf("PLL5 Output %u.%06u MHz\n", freq / 1000000, freq % 1000000);
	freq = vybrid_get_pll_clk(PLL6_CLOCK);
	printf("PLL6 Output %u.%06u MHz\n", freq / 1000000, freq % 1000000);
	freq = vybrid_get_pll_clk(PLL7_CLOCK);
	printf("PLL7 Output %u.%06u MHz\n", freq / 1000000, freq % 1000000);

	freq = vybrid_get_arm_clk();
	printf("\nARM Clock: %u.%06u MHz\n", freq / 1000000, freq % 1000000);
	freq = vybrid_get_bus_clk();
	printf("BUS Clock: %u.%06u MHz\n", freq / 1000000, freq % 1000000);
	freq = vybrid_get_ipg_clk();
	printf("IPG Clock: %u.%06u MHz\n", freq / 1000000, freq % 1000000);
	freq = vybrid_get_ddr_clk();
	printf("DDR Clock: %u.%06u MHz\n", freq / 1000000, freq % 1000000);

	return 0;
}

/***************************************************/

U_BOOT_CMD(
	clocks,	CONFIG_SYS_MAXARGS, 1, do_vybrid_showclocks,
	"display clocks",
	""
);
