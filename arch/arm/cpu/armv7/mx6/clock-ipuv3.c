/*
 * Copyright (C) 2018 F&S Elektronik Systeme GmbH
 *
 * SPDX-License-Identifier:	GPL-2.0+
 *
 * Clock handling for IPUv3 display driver on i.MX6QDL
 */

#include <common.h>
#include <asm/io.h>
#include <asm/arch/crm_regs.h>
#include <asm/arch/clock.h>
#include <asm/arch/sys_proto.h>

extern struct mxc_ccm_reg *imx_ccm;

extern u32 decode_pll(enum pll_clocks pll);
extern u32 mxc_get_pll_pfd(enum pll_clocks pll, int pfd_num);
extern u32 get_mmdc_ch0_clk(void);
extern int set_lvds_clk(void *addr, unsigned int di, unsigned int ldb_di,
			unsigned int freq_khz, int split,
			unsigned int pfd, unsigned int pfd_mux);

/* MMDC_CH1_CLK is not really IPUV3 specific, but it is only needed here */
static unsigned int mxc_get_mmdc_ch1_clk(void)
{
	u32 cbcmr = __raw_readl(&imx_ccm->cbcmr);
	u32 cbcdr = __raw_readl(&imx_ccm->cbcdr);
	unsigned int freq;

	if (cbcdr & MXC_CCM_CBCDR_PERIPH2_CLK_SEL) {
		if (cbcmr & MXC_CCM_CBCMR_PERIPH2_CLK2_SEL)
			freq = decode_pll(PLL_BUS);
		else
			freq = decode_pll(PLL_USBOTG);
	} else {
		switch ((cbcmr & MXC_CCM_CBCMR_PRE_PERIPH2_CLK_SEL_MASK)
			>> MXC_CCM_CBCMR_PRE_PERIPH2_CLK_SEL_OFFSET) {
		case 0:
			freq = decode_pll(PLL_BUS);
			break;
		case 1:
			freq = mxc_get_pll_pfd(PLL_BUS, 2);
			break;
		case 2:
			freq = mxc_get_pll_pfd(PLL_BUS, 0);
			break;
		case 3:
			/* static / 2 divider */
			freq = mxc_get_pll_pfd(PLL_BUS, 2) / 2;
			break;
		}
	}

	return freq;
}

unsigned int ipuv3_get_ldb_clock(int channel)
{
	u32 cs2cdr;
	unsigned int freq;

	cs2cdr = __raw_readl(&imx_ccm->cs2cdr);
	if (channel == 1)
		cs2cdr = (cs2cdr & MXC_CCM_CS2CDR_LDB_DI1_CLK_SEL_MASK)
			>> MXC_CCM_CS2CDR_LDB_DI1_CLK_SEL_OFFSET;
	else
		cs2cdr = (cs2cdr & MXC_CCM_CS2CDR_LDB_DI0_CLK_SEL_MASK)
			>> MXC_CCM_CS2CDR_LDB_DI0_CLK_SEL_OFFSET;
	switch (cs2cdr) {
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
		freq = mxc_get_mmdc_ch1_clk();
		break;
	case 4:
		freq = decode_pll(PLL_USBOTG);
		break;
	default:
		freq = 0;
		break;
	}

	return freq;
}

unsigned int ipuv3_get_ipu_di_clock(int ipu, int di)
{
	u32 reg;
	unsigned int freq;
	unsigned int di_pre_sel;
	unsigned int di_pre_div;
	unsigned int di_clk_sel;

	reg = __raw_readl((ipu == 2) ? &imx_ccm->cscdr2 : &imx_ccm->chsccdr);
	if (di == 1) {
		di_pre_sel = (reg >> 15) & 7;
		di_pre_div = (reg >> 12) & 7;
		di_clk_sel = (reg >> 9) & 7;
	} else {
		di_pre_sel = (reg >> 6) & 7;
		di_pre_div = (reg >> 3) & 7;
		di_clk_sel = (reg >> 0) & 7;
	}

	switch (di_clk_sel) {
	case 0:
		switch (di_pre_sel) {
		case 0:
			freq = get_mmdc_ch0_clk();
			break;
		case 1:
			freq = decode_pll(PLL_USBOTG);
			break;
		case 2:
			freq = decode_pll(PLL_VIDEO);
			break;
		case 3:
			freq = mxc_get_pll_pfd(PLL_BUS, 0);
			break;
		case 4:
			freq = mxc_get_pll_pfd(PLL_BUS, 2);
			break;
		case 5:
			freq = mxc_get_pll_pfd(PLL_USBOTG, 1);
			break;
		default:
			freq = 0;
			break;
		}
		freq /= di_pre_div + 1;
		break;
	case 3:
		freq = ipuv3_get_ldb_clock(0);
		reg = __raw_readl(&imx_ccm->cscmr2);
		if (!(reg & MXC_CCM_CSCMR2_LDB_DI0_IPU_DIV))
			freq *= 2;
		freq /= 7;
		break;
	case 4:
		freq = ipuv3_get_ldb_clock(1);
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

unsigned int ipuv3_get_ipu_clock(int ipu)
{
	u32 reg;
	unsigned int podf;
	unsigned int freq;

	reg = __raw_readl(&imx_ccm->cscdr3);
	if (ipu == 2) {
		if (is_mx6sdl())
			return 0;
		podf = (reg & MXC_CCM_CSCDR3_IPU2_HSP_PODF_MASK)
			>> MXC_CCM_CSCDR3_IPU2_HSP_PODF_OFFSET;
		reg = (reg & MXC_CCM_CSCDR3_IPU2_HSP_CLK_SEL_MASK)
			>>  MXC_CCM_CSCDR3_IPU2_HSP_CLK_SEL_OFFSET;
	} else {
		podf = (reg & MXC_CCM_CSCDR3_IPU1_HSP_PODF_MASK)
			>> MXC_CCM_CSCDR3_IPU1_HSP_PODF_OFFSET;
		reg = (reg & MXC_CCM_CSCDR3_IPU1_HSP_CLK_SEL_MASK)
			>>  MXC_CCM_CSCDR3_IPU1_HSP_CLK_SEL_OFFSET;
	}

	switch (reg) {
	case 0:
		freq = get_mmdc_ch0_clk();
		break;
	case 1:
		freq = mxc_get_pll_pfd(PLL_BUS, 2);
		break;
	case 2:
		freq = 120000000;
		break;
	case 3:
		freq = mxc_get_pll_pfd(PLL_USBOTG, 1);
		break;
	}

	return freq/(podf + 1);
}

void switch_ldb_di_clk_src(unsigned new_ldb_di_clk_src, unsigned ldb_di)
{
	u32 reg;
	u32 cs2cdr;
	unsigned old_ldb_di_clk_src;
	int shift = (ldb_di == 1) ? 12 : 9;

	/*
	 * ldb_di<n>_clk_src:
	 *   0 (000b): PLL5
	 *   1 (001b): PLL2_PFD0
	 *   2 (010b): PLL2_PFD2
	 *   3 (011b): mmdc_ch1_clk
	 *   4 (100b): pll3_sw_clk
	 *   5 (101b): reserved (always off)
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

#ifdef CONFIG_MX6Q
	/* ### Why only MX6Q? */
	writel(BM_ANADIG_PFD_528_PFD2_CLKGATE, &imx_ccm->analog_pfd_528_set);
#endif

	/*
	 * Need to follow a strict procedure when changing the LDB
	 * clock, else we can introduce a glitch. Things to keep in
	 * mind:
	 * 1. The current and new parent clocks must be disabled.
	 * 2. The default clock for ldb_dio_clk is mmdc_ch1 which has
	 * no CG bit.
	 * 3. In the RTL implementation of the LDB_DI_CLK_SEL mux
	 * the top four options are in one mux and the PLL3 option along
	 * with another option is in the second mux. There is third mux
	 * used to decide between the first and second mux.
	 * The code below switches the parent to the bottom mux first
	 * and then manipulates the top mux. This ensures that no glitch
	 * will enter the divider.
	 *
	 * Need to disable MMDC_CH1 clock manually as there is no CG bit
	 * for this clock. The only way to disable this clock is to move
	 * it to pll3_sw_clk and then to disable pll3_sw_clk.
	 * Make sure periph2_clk2_sel is set to pll3_sw_clk
	 *
	 * Remark:
	 * pll3_sw_clk is also parent of the UARTs. So if an UART
	 * transmission is going on in parallel (e.g. by sending from
	 * the UART FIFO), then the bit timing is disturbed and some
	 * characters may arrive damaged.
	 */
	reg = readl(&imx_ccm->cbcmr);
	reg &= ~(1 << 20);
	writel(reg, &imx_ccm->cbcmr);
	
	/* Set MMDC_CH1 mask bit. */
	reg = readl(&imx_ccm->ccdr);
	reg |= 1 << 16;
	writel(reg, &imx_ccm->ccdr);

	/*
	 * Set the periph2_clk_sel to the top mux so that
	 * mmdc_ch1 is from pll3_sw_clk.
	 */
	reg = readl(&imx_ccm->cbcdr);
	reg |= 1 << 26;
	writel(reg, &imx_ccm->cbcdr);

	/* Wait for the clock switch */
	while (readl(&imx_ccm->cdhipr))
		;
	
	/* Disable pll3_sw_clk by selecting the bypass clock source */
	reg = readl(&imx_ccm->ccsr);
	reg |= 1 << 0;
	writel(reg, &imx_ccm->ccsr);

	/* Switch to bottom MUX by setting bit 2 */
	cs2cdr |= (4 << shift);
	writel(cs2cdr, &imx_ccm->cs2cdr);

	/* Switch lower bits while on bottom MUX */
	cs2cdr &= ~(3 << shift);
	cs2cdr |= (new_ldb_di_clk_src | 4) << shift;
	writel(cs2cdr, &imx_ccm->cs2cdr);

	/* Switch back to top MUX by clearing bit 2 */
	cs2cdr &= ~(7 << shift);
	cs2cdr |= new_ldb_di_clk_src << shift;
	writel(cs2cdr, &imx_ccm->cs2cdr);

	/* Unbypass pll3_sw_clk */
	reg = readl(&imx_ccm->ccsr);
	reg &= ~(1 << 0);
	writel(reg, &imx_ccm->ccsr);

	/*
	 * Set the periph2_clk_sel back to the bottom mux so that
	 * mmdc_ch1 is from its original parent.
	 */
	reg = __raw_readl(&imx_ccm->cbcdr);
	reg &= ~(1 << 26);
	__raw_writel(reg, &imx_ccm->cbcdr);

	/* Wait for the clock switch. */
	while (readl(&imx_ccm->cdhipr))
		;

	/* Clear MMDC_CH1 mask bit. */
	reg = readl(&imx_ccm->ccdr);
	reg &= ~(1 << 16);
	writel(reg, &imx_ccm->ccdr);

#ifdef CONFIG_MX6Q
	/* Why only MX6Q??? */
	writel(BM_ANADIG_PFD_528_PFD2_CLKGATE, &imx_ccm->analog_pfd_528_clr);
#endif
}

/*
 * Set IPU clock source and enable IPU clock.
 * 
 * CPU       Max. IPU clock    Use clock source   Divide by   Result
 * ------------------------------------------------------------------
 * MX6Q      264 MHz           MMDC_CH0           2           264 MHz
 * MX6DL     270 MHz           PLL3_PFD1          2           270 MHz
 *
 * Remark: For internal (HSP) IPU clock, do not use a clock source with spread
 * spectrum, this seems to cause trouble (lost frames from time to time).
 * However the display clock may have spread spectrum. Actually in case of
 * LCD, the display clock *should* have spread spectrum (i.e. be derived from
 * PLL2) for EMI reasons. For LVDS and HDMI this does not matter because they
 * use low voltage differential signals anyway.
 */
void ipuv3_enable_ipu_clk(int ipu)
{
	u32 ccgr3, cscdr3;
	u32 clk_src = 1;

	if (is_mx6sdl()) {
		if (ipu != 1)
			return;
		clk_src = 3;
	}

	/* Set IPU1 clock source to PLL3_PFD1 (=540MHz), divide by 2 */
	cscdr3 = readl(&imx_ccm->cscdr3);
	ccgr3 = readl(&imx_ccm->CCGR3);
	if (ipu == 2) {
		cscdr3 &= ~MXC_CCM_CSCDR3_IPU2_HSP_CLK_SEL_MASK;
		cscdr3 |= (clk_src << MXC_CCM_CSCDR3_IPU2_HSP_CLK_SEL_OFFSET);
		cscdr3 &= ~MXC_CCM_CSCDR3_IPU2_HSP_PODF_MASK;
		cscdr3 |= (1 << MXC_CCM_CSCDR3_IPU2_HSP_PODF_OFFSET);
		ccgr3 |= MXC_CCM_CCGR3_IPU2_IPU_MASK;
	} else {
		cscdr3 &= ~MXC_CCM_CSCDR3_IPU1_HSP_CLK_SEL_MASK;
		cscdr3 |= (clk_src << MXC_CCM_CSCDR3_IPU1_HSP_CLK_SEL_OFFSET);
		cscdr3 &= ~MXC_CCM_CSCDR3_IPU1_HSP_PODF_MASK;
		cscdr3 |= (1 << MXC_CCM_CSCDR3_IPU1_HSP_PODF_OFFSET);
		ccgr3 |= MXC_CCM_CCGR3_IPU1_IPU_MASK;
	}
	writel(cscdr3, &imx_ccm->cscdr3);
	writel(ccgr3, &imx_ccm->CCGR3);
}

/* Set ipu_pre_clk_sel to PLL2_PFD2, ipu_clk_sel to ipu_pre_clk */
int ipuv3_config_lcd_di_clk(u32 ipu, u32 di)
{

	u32 reg;
	void *addr;

	/*
	 * Set ipu_di_clk clock source to pre-muxed ipu di clock, divided by 2,
	 * and root clock pre-muxed from PLL2_PFD2 (396MHz). This results in
	 * 198MHz. Because IPU can add an additional divider from 1.0 to
	 * 255.9375 (256-1/16), this should be OK for all LCDs.
	 */
	addr = (ipu == 2) ? &imx_ccm->cscdr2 : &imx_ccm->chsccdr;
	reg = __raw_readl(addr);
	if (di == 1) {
		reg &= ~(0x1ff << 9);
		reg |= (4 << 15) | (1 << 12) | (0 << 9);
	} else {
		reg &= ~(0x1ff << 0);
		reg |= (4 << 6) | (1 << 3) | (0 << 0);
	}
	__raw_writel(reg, addr);

	return 0;
}

/* Set ldb_di_clk_src to PLL2_PFDn/PLL5, set display clock source to ldb_di */
int ipuv3_config_lvds_clk(unsigned int ipu, unsigned int di,
			  unsigned int freq_khz, unsigned int split)
{
	void *addr;

	if (ipu == 2)
		addr = &imx_ccm->cscdr2;
	else
		addr = &imx_ccm->chsccdr;

	/* Use PFD2_PFD0 if possible, which is MUX 1 in CSC2CDR (!) */
	return set_lvds_clk(addr, di, di, freq_khz, split, 0, 1);
}
