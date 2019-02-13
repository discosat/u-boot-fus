/*
 * Copyright (C) 2018 F&S Elektronik Systeme GmbH
 *
 * SPDX-License-Identifier:	GPL-2.0+
 *
 * Clock handling for mxsfb display driver on i.MX6SX/UL/ULL
 */

#include <common.h>
#include <asm/io.h>
#include <asm/arch/crm_regs.h>
#include <asm/arch/clock.h>

extern struct mxc_ccm_reg *imx_ccm;

extern u32 decode_pll(enum pll_clocks pll);
extern u32 mxc_get_pll_pfd(enum pll_clocks pll, int pfd_num);
extern int set_lvds_clk(void *addr, unsigned int di, unsigned int ldb_di,
			unsigned int freq_khz, int split,
			unsigned int pfd, unsigned int pfd_mux);

unsigned int mxs_get_ldb_clock(int channel)
{
	u32 clk_src;
	unsigned int freq;

#ifdef CONFIG_MX6SX
	if (channel == 1) {
		clk_src = __raw_readl(&imx_ccm->cscmr1);
		clk_src &= MXC_CCM_CSCMR1_QSPI1_CLK_SEL_MASK;
		clk_src >>= MXC_CCM_CSCMR1_QSPI1_CLK_SEL_OFFSET;

		switch (clk_src) {
		case 0:
			freq = decode_pll(PLL_USBOTG);
			break;
		case 1:
			freq = mxc_get_pll_pfd(PLL_BUS, 0);
			break;
		case 2:
			freq = mxc_get_pll_pfd(PLL_BUS, 2);
			break;
		case 3:
			freq = decode_pll(PLL_BUS);
			break;
		case 4:
			freq = mxc_get_pll_pfd(PLL_USBOTG, 3);
			break;
		case 5:
			freq = mxc_get_pll_pfd(PLL_USBOTG, 2);
			break;
		default:
			freq = 0;
			break;
		}
	} else
#endif
	{
		clk_src = __raw_readl(&imx_ccm->cs2cdr);
		clk_src &= MXC_CCM_CS2CDR_LDB_DI0_CLK_SEL_MASK;
		clk_src >>= MXC_CCM_CS2CDR_LDB_DI0_CLK_SEL_OFFSET;

		switch (clk_src) {
		case 0:
			freq = decode_pll(PLL_VIDEO);
			break;
		case 1:
			freq = mxc_get_pll_pfd(PLL_BUS, 0);
			break;
		case 2:
			freq = mxc_get_pll_pfd(PLL_BUS, 2);
			break;
		case 3:
			freq = mxc_get_pll_pfd(PLL_BUS, 3);
			break;
		case 4:
			freq = mxc_get_pll_pfd(PLL_BUS, 1);
			break;
		case 5:
			freq = mxc_get_pll_pfd(PLL_USBOTG, 3);
			break;
		default:
			freq = 0;
			break;
		}
	}

	return freq;
}

unsigned int mxs_get_lcdif_clock(int lcdif)
{
	u32 reg;
	unsigned int freq, pre_sel, pre_div, post_div, clk_sel;

	reg = readl(&imx_ccm->cscdr2);
#ifdef CONFIG_MX6SX
	if (lcdif == 2) {
		pre_sel = (reg >> 6) & 7;
		pre_div = (reg >> 3) & 7;
		clk_sel = (reg >> 0) & 7;
		post_div = readl(&imx_ccm->cscmr1);
		post_div &= MXC_CCM_CSCMR1_LCDIF2_PODF_MASK;
		post_div >>= MXC_CCM_CSCMR1_LCDIF2_PODF_OFFSET;
	} else
#endif
	{
		pre_sel = (reg >> 15) & 7;
		pre_div = (reg >> 12) & 7;
		clk_sel = (reg >> 9) & 7;
		post_div = readl(&imx_ccm->cbcmr);
		post_div &= MXC_CCM_CBCMR_LCDIF1_PODF_MASK;
		post_div >>= MXC_CCM_CBCMR_LCDIF1_PODF_OFFSET;
	}

	switch (clk_sel) {
	case 0:
		switch (pre_sel) {
		case 0:
			freq = decode_pll(PLL_BUS);
			break;
		case 1:
			freq = mxc_get_pll_pfd(PLL_USBOTG, 3);
			break;
		case 2:
			freq = decode_pll(PLL_VIDEO);
			break;
		case 3:
			freq = mxc_get_pll_pfd(PLL_BUS, 0);
			break;
		case 4:
#ifdef CONFIG_MX6SX
			if (lcdif == 2)
				freq = mxc_get_pll_pfd(PLL_BUS, 3);
			else
#endif
				freq = mxc_get_pll_pfd(PLL_BUS, 1);
			break;
		case 5:
			freq = mxc_get_pll_pfd(PLL_USBOTG, 1);
			break;
		default:
			freq = 0;
			break;
		}
		freq /= (pre_div + 1) * (post_div + 1);
		break;
	case 3:
		freq = mxs_get_ldb_clock(0);
		reg = __raw_readl(&imx_ccm->cscmr2);
		if (!(reg & MXC_CCM_CSCMR2_LDB_DI0_IPU_DIV))
			freq *= 2;
		freq /= 7;
		break;
	case 4:
		freq = mxs_get_ldb_clock(1);
		reg = __raw_readl(&imx_ccm->cscmr2);
		if (!(reg & MXC_CCM_CSCMR2_LDB_DI1_IPU_DIV))
			freq *= 2;
		freq /= 7;
		break;
	default:
		freq = 0;
		break;
	}

	return freq;
}

void switch_ldb_di_clk_src(unsigned new_ldb_di_clk_src, unsigned ldb_di)
{
	u32 cs2cdr;
	unsigned old_ldb_di_clk_src;
	int shift = 9;

	/*
	 * ldb_di0_clk_src:
	 *   0 (000b): PLL5
	 *   1 (001b): PLL2_PFD0
	 *   2 (010b): PLL2_PFD2
	 *   3 (011b): PLL2_PFD3
	 *   4 (100b): PLL2_PFD1
	 *   5 (101b): PLL3_PFD3
	 *   6 (110b): reserved (always off)
	 *   7 (111b): reserved (always off)
	 */
	cs2cdr = readl(&imx_ccm->cs2cdr);
	old_ldb_di_clk_src = (cs2cdr >> shift) & 0x07;

	if (old_ldb_di_clk_src == new_ldb_di_clk_src)
		return;

	//### TODO: only disable old and new clock; enable later again but
	//### only if it was active before
	disable_video_pll();

	/* Switch MUX to new clock source */
	cs2cdr &= ~(7 << shift);
	cs2cdr |= new_ldb_di_clk_src << shift;
	writel(cs2cdr, &imx_ccm->cs2cdr);
}

void mxs_enable_lcdif_clk(unsigned int base_addr)
{
	u32 reg = 0;

	/* Enable the LCDIF pix clock, axi clock, disp axi clock */
	reg = readl(&imx_ccm->CCGR3);
#ifdef CONFIG_MX6SX
	reg |= MXC_CCM_CCGR3_DISP_AXI_MASK;
	if (base_addr == LCDIF2_BASE_ADDR)
		reg |= MXC_CCM_CCGR3_LCDIF2_PIX_MASK;
	else
#endif
		reg |= MXC_CCM_CCGR3_LCDIF1_PIX_MASK;
	writel(reg, &imx_ccm->CCGR3);

	reg = readl(&imx_ccm->CCGR2);
	reg |= (MXC_CCM_CCGR2_LCD_MASK);
	writel(reg, &imx_ccm->CCGR2);
}

/* Set lcdif_pre_clk_sel to PLL2_PFDn/PLL5, lcdif_clk_sel to lcdif_pre_clk */
int mxs_config_lcdif_clk(unsigned int base_addr, unsigned int freq_khz)
{
	unsigned int pll_base_clock;
	unsigned int pfd_khz, best_pfd_khz = 0;
	unsigned int delta, best_delta = ~0;
	unsigned int div, divider = 0;
	unsigned int pred, lcdif_pred = 0;
	unsigned int podf, lcdif_podf = 0;
	unsigned int pfd_mux, cscdr2_shift;
	u32 reg;

	/*
	 * Try to use a PLL2_PFD first. Each LCDIF has a dedicated PFD that is
	 * not used by other devices, so it is possible to change the rate
	 * without causing trouble elsewhere.
	 *
	 *   LCDIF1: PLL2_PFD1
	 *   LCDIF2: PLL2_PFD3 (MX6SX only)
	 *
	 * The PFD clock rate is: rate = pll_rate * 18 / div, where div is a
	 * value between 12 and 35. For the 528 MHz clock rate of PLL2, this
	 * results in a rate between 271.5 MHz and 792 MHz.
	 *
	 * This clock rate can further be divided by lcdif_pred and lcdif_podf,
	 * where each is a value between 1 and 8. This results in a divider of
	 * 1 to 64, but not all values are possible. For example 19 is not
	 * possible because it can not be built by two factors between 1 and 8.
	 *
	 * So try all possible combinations and chose the rate with the
	 * smallest error. The two inner loops are symmetric. Avoid duplicate
	 * combinations by starting the innermost loop at the position of the
	 * second loop. This reduces the loop count of the two inner loops
	 * from 64 to 36. Actually there are only 30 unique combinations, but
	 * the effort to reduce the count further, e.g. by using a table, is
	 * not worth it and would result in much longer code.
	 *
	 * If the target clock rate that is possible by PLL2_PFD is not
	 * accurate enough, use PLL5 instead. Please note that PLL5 does not
	 * provide spread spectrum.
	 *
	 * ### TODO: PLL5 may be required by LVDS; so before using PLL5 we
	 * could check as an intermediate step if one of PLL3_PFD1 and
	 * PLL3_PFD3, that are also available as clock sources, happens to
	 * match. However PLL3 also has no spread spectrum.
	 */
	pll_base_clock = (decode_pll(PLL_BUS) + 500) / 1000;
	for (div = 12; div <= 35; div++) {
		unsigned int tmp_khz = pll_base_clock * 18 / div;

		for (pred = 1; pred <= 8; pred++) {
			for (podf = pred; podf <= 8; podf++) {
				unsigned int mul = pred * podf;

				pfd_khz = (tmp_khz + mul / 2) / mul;
				if (pfd_khz < freq_khz)
					delta = freq_khz - pfd_khz;
				else
					delta = pfd_khz - freq_khz;
				if (delta < best_delta) {
					best_pfd_khz = pfd_khz;
					best_delta = delta;
					divider = div;
					lcdif_pred = pred - 1;
					lcdif_podf = podf - 1;
				}
			}
		}
	}

	if (!freq_is_accurate(best_pfd_khz, freq_khz))
		divider = 0;

	if (divider) {
		unsigned int pfd = 1;
#ifdef CONFIG_MX6SX
		if (base_addr == LCDIF2_BASE_ADDR)
			pfd = 3;
#endif
		reg = readl(&imx_ccm->analog_pfd_528);
		reg &= ~(0xff << (pfd * 8));
		reg |= (divider << (pfd * 8));
		writel(reg, &imx_ccm->analog_pfd_528);
		pfd_mux = 4;
	} else {
		int ret;
		unsigned int pll5_min_khz = MXC_HCLK * 27 / 1000;

		/*
		 * PLL2_PFD<n> is not accurate enough, try to use PLL5. Double
		 * freq_khz until it is the range of this PLL; if it can not
		 * be done by lcdif_pred and lcdif_podf alone, PLL5 itself can
		 * also divide up to 16.
		 */
		lcdif_podf = 1;
		lcdif_podf = 1;
		while ((freq_khz < pll5_min_khz) && (lcdif_pred < 7)) {
			lcdif_pred += 2;
			freq_khz <<= 1;
		}
		while ((freq_khz < pll5_min_khz) && (lcdif_podf < 7)) {
			lcdif_podf += 2;
			freq_khz <<= 1;
		}
		ret = setup_video_pll(freq_khz);
		if (ret) {
			printf("Can not set display freq %ukHz\n", freq_khz);
			return ret;	/* Not possible with PLL5, too */
		}
		enable_video_pll();
		pfd_mux = 2;
	}

	/* Set lcdif_podf */
#ifdef CONFIG_MX6SX
	if (base_addr == LCDIF2_BASE_ADDR) {
		reg = readl(&imx_ccm->cscmr1);
		reg &= ~MXC_CCM_CSCMR1_LCDIF2_PODF_MASK;
		reg |= (lcdif_podf << MXC_CCM_CSCMR1_LCDIF2_PODF_OFFSET);
		writel(reg, &imx_ccm->cscmr1);
		cscdr2_shift = 0;
	} else
#endif
	{
		reg = readl(&imx_ccm->cbcmr);
		reg &= ~MXC_CCM_CBCMR_LCDIF1_PODF_MASK;
		reg |= (lcdif_podf << MXC_CCM_CBCMR_LCDIF1_PODF_OFFSET);
		writel(reg, &imx_ccm->cbcmr);
		cscdr2_shift = 9;
	}

	/* Set lcdif_pre_clk_mux, lcdif_pred and lcdif_clk_sel */
	reg = readl(&imx_ccm->cscdr2);
	reg &= ~(0x1ff << cscdr2_shift);
	reg |= ((pfd_mux << 6) | (lcdif_pred << 3) | (0 << 0)) << cscdr2_shift;
	writel(reg, &imx_ccm->cscdr2);

	return 0;
}

/* Set ldb_di_clk_src to PLL2_PFDn/PLL5, set display clock source to ldb_di */
int mxs_config_lvds_clk(uint32_t base_addr, unsigned int freq_khz)
{
	unsigned int pfd, pfd_mux, di;

#ifdef CONFIG_MX6SX
	if (base_addr == LCDIF2_BASE_ADDR) {
		pfd = 3;		/* Use PLL2_PFD3 if possible ... */
		pfd_mux = 3;		/* ... which is MUX 3 in CS2CDR */
		di = 0;
	} else
#endif
	{
		pfd = 1;		/* Use PLL2_PFD1 if possible ... */
		pfd_mux = 4;		/* ... which is MUX 4 in CS2CDR */
		di = 1;
	}

	/* Always use ldb_di0, ldb_di1 is used for QSPI1 */
	return set_lvds_clk(&imx_ccm->cscdr2, di, 0, freq_khz, 0, pfd, pfd_mux);
}
