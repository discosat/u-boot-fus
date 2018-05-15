/*
 * Copyright (C) 2010-2015 Freescale Semiconductor, Inc.
 *
 * SPDX-License-Identifier:	GPL-2.0+
 */

#include <common.h>
#include <div64.h>
#include <asm/io.h>
#include <asm/errno.h>
#include <asm/arch/imx-regs.h>
#include <asm/arch/crm_regs.h>
#include <asm/arch/clock.h>
#include <asm/arch/sys_proto.h>

enum pll_clocks {
	PLL_SYS,	/* System PLL */
	PLL_BUS,	/* System Bus PLL*/
	PLL_USBOTG,	/* OTG USB PLL */
	PLL_ENET,	/* ENET PLL */
	PLL_AUDIO,	/* AUDIO PLL */
	PLL_VIDEO,	/* AUDIO PLL */
	PLL_USB2,	/* USB Host PLL */
};

struct mxc_ccm_reg *imx_ccm = (struct mxc_ccm_reg *)CCM_BASE_ADDR;

#ifdef CONFIG_MXC_OCOTP
void enable_ocotp_clk(unsigned char enable)
{
	u32 reg;

	reg = __raw_readl(&imx_ccm->CCGR2);
	if (enable)
		reg |= MXC_CCM_CCGR2_OCOTP_CTRL_MASK;
	else
		reg &= ~MXC_CCM_CCGR2_OCOTP_CTRL_MASK;
	__raw_writel(reg, &imx_ccm->CCGR2);
}
#endif

#ifdef CONFIG_NAND_MXS
void setup_gpmi_io_clk(u32 cfg)
{
	/* Disable clocks per ERR007177 from MX6 errata */
	clrbits_le32(&imx_ccm->CCGR4,
		     MXC_CCM_CCGR4_RAWNAND_U_BCH_INPUT_APB_MASK |
		     MXC_CCM_CCGR4_RAWNAND_U_GPMI_BCH_INPUT_BCH_MASK |
		     MXC_CCM_CCGR4_RAWNAND_U_GPMI_BCH_INPUT_GPMI_IO_MASK |
		     MXC_CCM_CCGR4_RAWNAND_U_GPMI_INPUT_APB_MASK |
		     MXC_CCM_CCGR4_PL301_MX6QPER1_BCH_MASK);

#if defined(CONFIG_MX6SX)
	clrbits_le32(&imx_ccm->CCGR4, MXC_CCM_CCGR4_QSPI2_ENFC_MASK);

	clrsetbits_le32(&imx_ccm->cs2cdr,
			MXC_CCM_CS2CDR_QSPI2_CLK_PODF_MASK |
			MXC_CCM_CS2CDR_QSPI2_CLK_PRED_MASK |
			MXC_CCM_CS2CDR_QSPI2_CLK_SEL_MASK,
			cfg);

	setbits_le32(&imx_ccm->CCGR4, MXC_CCM_CCGR4_QSPI2_ENFC_MASK);
#elif defined(CONFIG_MX6UL)
	/*
	 * config gpmi and bch clock to 100 MHz
	 * bch/gpmi select PLL2 PFD2 400M
	 * 100M = 400M / 4
	 */
	clrbits_le32(&imx_ccm->cscmr1,
		     MXC_CCM_CSCMR1_BCH_CLK_SEL |
		     MXC_CCM_CSCMR1_GPMI_CLK_SEL);
	clrsetbits_le32(&imx_ccm->cscdr1,
			MXC_CCM_CSCDR1_BCH_PODF_MASK |
			MXC_CCM_CSCDR1_GPMI_PODF_MASK,
			cfg);
#else
	clrbits_le32(&imx_ccm->CCGR2, MXC_CCM_CCGR2_IOMUX_IPT_CLK_IO_MASK);

	clrsetbits_le32(&imx_ccm->cs2cdr,
			MXC_CCM_CS2CDR_ENFC_CLK_PODF_MASK |
			MXC_CCM_CS2CDR_ENFC_CLK_PRED_MASK |
			MXC_CCM_CS2CDR_ENFC_CLK_SEL_MASK,
			cfg);

	setbits_le32(&imx_ccm->CCGR2, MXC_CCM_CCGR2_IOMUX_IPT_CLK_IO_MASK);
#endif
	setbits_le32(&imx_ccm->CCGR4,
		     MXC_CCM_CCGR4_RAWNAND_U_BCH_INPUT_APB_MASK |
		     MXC_CCM_CCGR4_RAWNAND_U_GPMI_BCH_INPUT_BCH_MASK |
		     MXC_CCM_CCGR4_RAWNAND_U_GPMI_BCH_INPUT_GPMI_IO_MASK |
		     MXC_CCM_CCGR4_RAWNAND_U_GPMI_INPUT_APB_MASK |
		     MXC_CCM_CCGR4_PL301_MX6QPER1_BCH_MASK);
}
#endif

void enable_usboh3_clk(unsigned char enable)
{
	u32 reg;

	reg = __raw_readl(&imx_ccm->CCGR6);
	if (enable)
		reg |= MXC_CCM_CCGR6_USBOH3_MASK;
	else
		reg &= ~(MXC_CCM_CCGR6_USBOH3_MASK);
	__raw_writel(reg, &imx_ccm->CCGR6);

}

#if defined(CONFIG_FEC_MXC) && !defined(CONFIG_MX6SX)
void enable_enet_clk(unsigned char enable)
{
	u32 mask, *addr;

	if (is_cpu_type(MXC_CPU_MX6ULL)) {
		mask = MXC_CCM_CCGR0_ENET_CLK_ENABLE_MASK;
		addr = &imx_ccm->CCGR0;
	} else if (is_cpu_type(MXC_CPU_MX6UL)) {
		mask = MXC_CCM_CCGR3_ENET_CLK_ENABLE_MASK;
		addr = &imx_ccm->CCGR3;
	} else {
		mask = MXC_CCM_CCGR1_ENET_CLK_ENABLE_MASK;
		addr = &imx_ccm->CCGR1;
	}

	if (enable)
		setbits_le32(addr, mask);
	else
		clrbits_le32(addr, mask);
}
#endif

#ifdef CONFIG_MXC_UART
void enable_uart_clk(unsigned char enable)
{
	u32 mask;

	if (is_cpu_type(MXC_CPU_MX6UL) || is_cpu_type(MXC_CPU_MX6ULL))
		mask = MXC_CCM_CCGR5_UART_MASK;
	else
		mask = MXC_CCM_CCGR5_UART_MASK | MXC_CCM_CCGR5_UART_SERIAL_MASK;

	if (enable)
		setbits_le32(&imx_ccm->CCGR5, mask);
	else
		clrbits_le32(&imx_ccm->CCGR5, mask);
}
#endif

#ifdef CONFIG_MMC
int enable_usdhc_clk(unsigned char enable, unsigned bus_num)
{
	u32 mask;

	if (bus_num > 3)
		return -EINVAL;

	mask = MXC_CCM_CCGR_CG_MASK << (bus_num * 2 + 2);
	if (enable)
		setbits_le32(&imx_ccm->CCGR6, mask);
	else
		clrbits_le32(&imx_ccm->CCGR6, mask);

	return 0;
}
#endif

#ifdef CONFIG_SYS_I2C_MXC
/* i2c_num can be from 0 - 3 */
int enable_i2c_clk(unsigned char enable, unsigned i2c_num)
{
	u32 reg;
	u32 mask;
	u32 *addr;

	if (i2c_num > 3)
		return -EINVAL;
	if (i2c_num < 3) {
		mask = MXC_CCM_CCGR_CG_MASK
			<< (MXC_CCM_CCGR2_I2C1_SERIAL_OFFSET
			+ (i2c_num << 1));
		reg = __raw_readl(&imx_ccm->CCGR2);
		if (enable)
			reg |= mask;
		else
			reg &= ~mask;
		__raw_writel(reg, &imx_ccm->CCGR2);
	} else {
		if (is_cpu_type(MXC_CPU_MX6SX) || is_cpu_type(MXC_CPU_MX6UL) ||
		    is_cpu_type(MXC_CPU_MX6ULL)) {
			mask = MXC_CCM_CCGR6_I2C4_MASK;
			addr = &imx_ccm->CCGR6;
		} else {
			mask = MXC_CCM_CCGR1_I2C4_SERIAL_MASK;
			addr = &imx_ccm->CCGR1;
		}
		reg = __raw_readl(addr);
		if (enable)
			reg |= mask;
		else
			reg &= ~mask;
		__raw_writel(reg, addr);
	}
	return 0;
}
#endif

/* spi_num can be from 0 - SPI_MAX_NUM */
int enable_spi_clk(unsigned char enable, unsigned spi_num)
{
	u32 reg;
	u32 mask;

	if (spi_num > SPI_MAX_NUM)
		return -EINVAL;

	mask = MXC_CCM_CCGR_CG_MASK << (spi_num << 1);
	reg = __raw_readl(&imx_ccm->CCGR1);
	if (enable)
		reg |= mask;
	else
		reg &= ~mask;
	__raw_writel(reg, &imx_ccm->CCGR1);
	return 0;
}

/* Post dividers for PLL_ENET */
const u32 enet_post_div[] = {
	20, 				/* 500 MHz/20 = 25 MHz */
	10,				/* 500 MHz/10 = 50 MHz */
	5,				/* 500 MHz/5 = 100 MHz */
	4				/* 500 MHz/4 = 125 MHz */
};

/* Post dividers for PLL_AUDIO/PLL_VIDEO */
const u32 av_post_div[] = { 4, 2, 1, 1 };

static u32 decode_pll(enum pll_clocks pll)
{
	u32 val, div;
	u32 post_div = 1;
	u32 num = 0;
	u32 denom = 1;
	u32 infreq = MXC_HCLK;
	u64 temp64;

	switch (pll) {
	case PLL_SYS:
		val = __raw_readl(&imx_ccm->analog_pll_sys);
		div = val & BM_ANADIG_PLL_SYS_DIV_SELECT;
		post_div = 2;
		break;

	case PLL_BUS:
		val = __raw_readl(&imx_ccm->analog_pll_528);
		div = (val & BM_ANADIG_PLL_528_DIV_SELECT) * 2 + 20;
		num = __raw_readl(&imx_ccm->analog_pll_528_num);
		denom = __raw_readl(&imx_ccm->analog_pll_528_denom);
		break;

	case PLL_USBOTG:
		val = __raw_readl(&imx_ccm->analog_usb1_pll_480_ctrl);
		div = (val & BM_ANADIG_USB1_PLL_480_CTRL_DIV_SELECT) * 2 + 20;
		break;

	case PLL_USB2:
		val = __raw_readl(&imx_ccm->analog_usb2_pll_480_ctrl);
		div = (val & BM_ANADIG_USB1_PLL_480_CTRL_DIV_SELECT) * 2 + 20;
		break;

	case PLL_ENET:
		/* 24 MHz * (20 + 5/6) = 500 MHz as base clock */
		val = __raw_readl(&imx_ccm->analog_pll_enet);
		div = 20;
		num = 5;
		denom = 6;
		post_div = enet_post_div[val & BM_ANADIG_PLL_ENET_DIV_SELECT];
		break;

	case PLL_AUDIO:
		val = __raw_readl(&imx_ccm->analog_pll_audio);
		div = val & BM_ANADIG_PLL_AUDIO_DIV_SELECT;
		num = __raw_readl(&imx_ccm->analog_pll_audio_num);
		denom = __raw_readl(&imx_ccm->analog_pll_audio_denom);
		post_div = (val & BM_ANADIG_PLL_AUDIO_POST_DIV_SELECT) >>
			BP_ANADIG_PLL_AUDIO_POST_DIV_SELECT;
		post_div = av_post_div[post_div];
		break;

	case PLL_VIDEO:
		val = __raw_readl(&imx_ccm->analog_pll_video);
		div = val & BM_ANADIG_PLL_VIDEO_DIV_SELECT;
		num = __raw_readl(&imx_ccm->analog_pll_video_num);
		denom = __raw_readl(&imx_ccm->analog_pll_video_denom);
		post_div = (val & BM_ANADIG_PLL_VIDEO_POST_DIV_SELECT) >>
			BP_ANADIG_PLL_VIDEO_POST_DIV_SELECT;
		post_div = av_post_div[post_div];
		break;

	default:
		return 0;
	}

	/* Check if PLL is enabled */
	if (!(val & (1 << 13)))
		return 0;

	/* Check if PLL is bypassed */
	if (val & (1 << 16))
		return infreq;

	temp64 = (u64)infreq * num;
	do_div(temp64, denom);

	return (infreq * div + (u32)temp64) / post_div;
}

static u32 mxc_get_pll_pfd(enum pll_clocks pll, int pfd_num)
{
	u32 div;
	u64 freq;

	switch (pll) {
	case PLL_BUS:
		if (!is_cpu_type(MXC_CPU_MX6UL) &&
		    !is_cpu_type(MXC_CPU_MX6SX)) {
			if (pfd_num == 3) {
				/* No PFD3 on PLL2 */
				return 0;
			}
		}
		div = __raw_readl(&imx_ccm->analog_pfd_528);
		freq = (u64)decode_pll(PLL_BUS);
		break;
	case PLL_USBOTG:
		div = __raw_readl(&imx_ccm->analog_pfd_480);
		freq = (u64)decode_pll(PLL_USBOTG);
		break;
	default:
		/* No PFD on other PLL					     */
		return 0;
	}

	return lldiv(freq * 18, (div & ANATOP_PFD_FRAC_MASK(pfd_num)) >>
			      ANATOP_PFD_FRAC_SHIFT(pfd_num));
}

static u32 get_mcu_main_clk(void)
{
	u32 reg, freq;

	reg = __raw_readl(&imx_ccm->cacrr);
	reg &= MXC_CCM_CACRR_ARM_PODF_MASK;
	reg >>= MXC_CCM_CACRR_ARM_PODF_OFFSET;
	freq = decode_pll(PLL_SYS);

	return freq / (reg + 1);
}

u32 get_periph_clk(void)
{
	u32 reg, div = 0, freq = 0;

	reg = __raw_readl(&imx_ccm->cbcdr);
	if (reg & MXC_CCM_CBCDR_PERIPH_CLK_SEL) {
		div = (reg & MXC_CCM_CBCDR_PERIPH_CLK2_PODF_MASK) >>
		       MXC_CCM_CBCDR_PERIPH_CLK2_PODF_OFFSET;
		reg = __raw_readl(&imx_ccm->cbcmr);
		reg &= MXC_CCM_CBCMR_PERIPH_CLK2_SEL_MASK;
		reg >>= MXC_CCM_CBCMR_PERIPH_CLK2_SEL_OFFSET;

		switch (reg) {
		case 0:
			freq = decode_pll(PLL_USBOTG);
			break;
		case 1:
		case 2:
			freq = MXC_HCLK;
			break;
		default:
			break;
		}
	} else {
		reg = __raw_readl(&imx_ccm->cbcmr);
		reg &= MXC_CCM_CBCMR_PRE_PERIPH_CLK_SEL_MASK;
		reg >>= MXC_CCM_CBCMR_PRE_PERIPH_CLK_SEL_OFFSET;

		switch (reg) {
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
		default:
			break;
		}
	}

	return freq / (div + 1);
}

static u32 get_ipg_clk(void)
{
	u32 reg, ipg_podf;

	reg = __raw_readl(&imx_ccm->cbcdr);
	reg &= MXC_CCM_CBCDR_IPG_PODF_MASK;
	ipg_podf = reg >> MXC_CCM_CBCDR_IPG_PODF_OFFSET;

	return get_ahb_clk() / (ipg_podf + 1);
}

static u32 get_ipg_per_clk(void)
{
	u32 reg, perclk_podf;

	reg = __raw_readl(&imx_ccm->cscmr1);
	if (is_cpu_type(MXC_CPU_MX6SL) || is_cpu_type(MXC_CPU_MX6SX) ||
	    is_mx6dqp() || is_cpu_type(MXC_CPU_MX6UL) ||
	    is_cpu_type(MXC_CPU_MX6ULL)) {
		if (reg & MXC_CCM_CSCMR1_PER_CLK_SEL_MASK)
			return MXC_HCLK; /* OSC 24Mhz */
	}

	perclk_podf = reg & MXC_CCM_CSCMR1_PERCLK_PODF_MASK;

	return get_ipg_clk() / (perclk_podf + 1);
}

static u32 get_uart_clk(void)
{
	u32 reg, uart_podf;
	u32 freq = decode_pll(PLL_USBOTG) / 6; /* static divider */
	reg = __raw_readl(&imx_ccm->cscdr1);

	if (is_cpu_type(MXC_CPU_MX6SL) || is_cpu_type(MXC_CPU_MX6SX) ||
	    is_mx6dqp() || is_cpu_type(MXC_CPU_MX6UL) ||
	    is_cpu_type(MXC_CPU_MX6ULL)) {
		if (reg & MXC_CCM_CSCDR1_UART_CLK_SEL)
			freq = MXC_HCLK;
	}

	reg &= MXC_CCM_CSCDR1_UART_CLK_PODF_MASK;
	uart_podf = reg >> MXC_CCM_CSCDR1_UART_CLK_PODF_OFFSET;

	return freq / (uart_podf + 1);
}

static u32 get_cspi_clk(void)
{
	u32 reg, cspi_podf;

	reg = __raw_readl(&imx_ccm->cscdr2);
	cspi_podf = (reg & MXC_CCM_CSCDR2_ECSPI_CLK_PODF_MASK) >>
		     MXC_CCM_CSCDR2_ECSPI_CLK_PODF_OFFSET;

	if (is_mx6dqp() || is_cpu_type(MXC_CPU_MX6SL) ||
	    is_cpu_type(MXC_CPU_MX6SX) || is_cpu_type(MXC_CPU_MX6UL) ||
	    is_cpu_type(MXC_CPU_MX6ULL)) {
		if (reg & MXC_CCM_CSCDR2_ECSPI_CLK_SEL_MASK)
			return MXC_HCLK / (cspi_podf + 1);
	}

	return	decode_pll(PLL_USBOTG) / (8 * (cspi_podf + 1));
}

static u32 get_axi_clk(void)
{
	u32 root_freq, axi_podf;
	u32 cbcdr =  __raw_readl(&imx_ccm->cbcdr);

	axi_podf = cbcdr & MXC_CCM_CBCDR_AXI_PODF_MASK;
	axi_podf >>= MXC_CCM_CBCDR_AXI_PODF_OFFSET;

	if (cbcdr & MXC_CCM_CBCDR_AXI_SEL) {
		if (cbcdr & MXC_CCM_CBCDR_AXI_ALT_SEL)
			root_freq = mxc_get_pll_pfd(PLL_BUS, 2);
		else
			root_freq = mxc_get_pll_pfd(PLL_USBOTG, 1);
	} else
		root_freq = get_periph_clk();

	return  root_freq / (axi_podf + 1);
}

static u32 get_emi_slow_clk(void)
{
	u32 emi_clk_sel, emi_slow_podf, cscmr1, root_freq = 0;

	cscmr1 =  __raw_readl(&imx_ccm->cscmr1);
	emi_clk_sel = cscmr1 & MXC_CCM_CSCMR1_ACLK_EMI_SLOW_MASK;
	emi_clk_sel >>= MXC_CCM_CSCMR1_ACLK_EMI_SLOW_OFFSET;
	emi_slow_podf = cscmr1 & MXC_CCM_CSCMR1_ACLK_EMI_SLOW_PODF_MASK;
	emi_slow_podf >>= MXC_CCM_CSCMR1_ACLK_EMI_SLOW_PODF_OFFSET;

	switch (emi_clk_sel) {
	case 0:
		root_freq = get_axi_clk();
		break;
	case 1:
		root_freq = decode_pll(PLL_USBOTG);
		break;
	case 2:
		root_freq =  mxc_get_pll_pfd(PLL_BUS, 2);
		break;
	case 3:
		root_freq =  mxc_get_pll_pfd(PLL_BUS, 0);
		break;
	}

	return root_freq / (emi_slow_podf + 1);
}

static u32 get_mmdc_ch0_clk(void)
{
	u32 cbcmr = __raw_readl(&imx_ccm->cbcmr);
	u32 cbcdr = __raw_readl(&imx_ccm->cbcdr);

	u32 freq, podf, per2_clk2_podf, pmu_misc2_audio_div;

	if (is_cpu_type(MXC_CPU_MX6SX) || is_cpu_type(MXC_CPU_MX6UL) ||
	    is_cpu_type(MXC_CPU_MX6SL) || is_cpu_type(MXC_CPU_MX6ULL)) {
		podf = (cbcdr & MXC_CCM_CBCDR_MMDC_CH1_PODF_MASK) >>
			MXC_CCM_CBCDR_MMDC_CH1_PODF_OFFSET;
		if (cbcdr & MXC_CCM_CBCDR_PERIPH2_CLK_SEL) {
			per2_clk2_podf = (cbcdr & MXC_CCM_CBCDR_PERIPH2_CLK2_PODF_MASK) >>
				MXC_CCM_CBCDR_PERIPH2_CLK2_PODF_OFFSET;
			if (is_cpu_type(MXC_CPU_MX6SL)) {
				if (cbcmr & MXC_CCM_CBCMR_PERIPH2_CLK2_SEL)
					freq = MXC_HCLK;
				else
					freq = decode_pll(PLL_USBOTG);
			} else {
				if (cbcmr & MXC_CCM_CBCMR_PERIPH2_CLK2_SEL)
					freq = decode_pll(PLL_BUS);
				else
					freq = decode_pll(PLL_USBOTG);
			}
		} else {
			per2_clk2_podf = 0;
			switch ((cbcmr &
				MXC_CCM_CBCMR_PRE_PERIPH2_CLK_SEL_MASK) >>
				MXC_CCM_CBCMR_PRE_PERIPH2_CLK_SEL_OFFSET) {
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
				pmu_misc2_audio_div = PMU_MISC2_AUDIO_DIV(__raw_readl(&imx_ccm->ana_misc2));
				switch (pmu_misc2_audio_div) {
				case 0:
				case 2:
					pmu_misc2_audio_div = 1;
					break;
				case 1:
					pmu_misc2_audio_div = 2;
					break;
				case 3:
					pmu_misc2_audio_div = 4;
					break;
				}
				freq = decode_pll(PLL_AUDIO) / pmu_misc2_audio_div;
				break;
			}
		}
		return freq / (podf + 1) / (per2_clk2_podf + 1);
	} else {
		podf = (cbcdr & MXC_CCM_CBCDR_MMDC_CH0_PODF_MASK) >>
			MXC_CCM_CBCDR_MMDC_CH0_PODF_OFFSET;
		return get_periph_clk() / (podf + 1);
	}
}

#ifdef CONFIG_VIDEO_IPUV3

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

unsigned int mxc_get_ldb_clock(int channel)
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

unsigned int mxc_get_ipu_di_clock(int ipu, int di)
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
		freq = mxc_get_ldb_clock(0);
		reg = __raw_readl(&imx_ccm->cscmr2);
		if (!(reg & MXC_CCM_CSCMR2_LDB_DI0_IPU_DIV))
			freq *= 2;
		freq /= 7;
		break;
	case 4:
		freq = mxc_get_ldb_clock(1);
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

unsigned int mxc_get_ipu_clock(int ipu)
{
	u32 reg;
	unsigned int podf;
	unsigned int freq;

	reg = __raw_readl(&imx_ccm->cscdr3);
	if (ipu == 2) {
		if (is_cpu_type(MXC_CPU_MX6DL))
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

#define PLL5_FREQ_MIN	650000000
#define PLL5_FREQ_MAX	1300000000

static void disable_pll5(void)
{
	u32 reg;

	/* Disable the PLL */
	reg = __raw_readl(&imx_ccm->analog_pll_video);
	reg |= BM_ANADIG_PLL_VIDEO_BYPASS;
	reg &= ~BM_ANADIG_PLL_VIDEO_ENABLE;
	__raw_writel(reg, &imx_ccm->analog_pll_video);
}

static void setup_pll5(u32 freq)
{
	unsigned int reg;
	u32 divider;
	u32 pre_div_rate;
	u32 post_div_sel = 2;
	u32 control3 = 0;
	u64 temp64;
	u32 mfn, mfd = 1000000;

	/* Set PLL5 Clock */
	pre_div_rate = freq;
	while (pre_div_rate < PLL5_FREQ_MIN) {
		pre_div_rate *= 2;
		/*
		 * test_div_sel field values:
		 * 2 -> Divide by 1
		 * 1 -> Divide by 2
		 * 0 -> Divide by 4
		 *
		 * control3 field values:
		 * 0 -> Divide by 1
		 * 1 -> Divide by 2
		 * 3 -> Divide by 4
		 */
		if (post_div_sel != 0)
			post_div_sel --;
		else {
			control3 ++;
			if (control3 == 2)
				control3 ++;
		}
	}
	divider = pre_div_rate / MXC_HCLK;
	temp64 = (u64) (pre_div_rate - (divider * MXC_HCLK));
	temp64 *= mfd;
	do_div(temp64, MXC_HCLK);
	mfn = temp64;

	__raw_writel(mfn, &imx_ccm->analog_pll_video_num);
	__raw_writel(mfd, &imx_ccm->analog_pll_video_denom);
	reg = __raw_readl(&imx_ccm->analog_pll_video);
	reg &= ~BM_ANADIG_PLL_VIDEO_DIV_SELECT;
	reg |= (divider << BP_ANADIG_PLL_VIDEO_DIV_SELECT);
	reg &= ~BM_ANADIG_PLL_VIDEO_POST_DIV_SELECT;
	reg |= (post_div_sel << BP_ANADIG_PLL_VIDEO_POST_DIV_SELECT);
	__raw_writel(reg, &imx_ccm->analog_pll_video);

	reg = __raw_readl(&imx_ccm->ana_misc2);
	reg &= ~BM_ANADIG_ANA_MISC2_CONTROL3;
	reg |= control3 << BP_ANADIG_ANA_MISC2_CONTROL3;
	__raw_writel(reg, &imx_ccm->ana_misc2);

	/* Enable the PLL power */
	reg = __raw_readl(&imx_ccm->analog_pll_video);
	reg &= ~BM_ANADIG_PLL_VIDEO_POWERDOWN;
	__raw_writel(reg, &imx_ccm->analog_pll_video);

	/* Wait for PLL to lock */
	while ((__raw_readl(&imx_ccm->analog_pll_video)
		& BM_ANADIG_PLL_VIDEO_LOCK) == 0) {
		;
	}

	/* Enable the PLL output */
	reg = __raw_readl(&imx_ccm->analog_pll_video);
	reg &= ~BM_ANADIG_PLL_VIDEO_BYPASS;
	reg |= BM_ANADIG_PLL_VIDEO_ENABLE;
	__raw_writel(reg, &imx_ccm->analog_pll_video);
}

void enable_ldb_di_clk(int channel)
{
	u32 ccgr3;

	ccgr3 = readl(&imx_ccm->CCGR3);
	if (channel == 1)
		ccgr3 |= MXC_CCM_CCGR3_LDB_DI1_MASK;
	else
		ccgr3 |= MXC_CCM_CCGR3_LDB_DI0_MASK;
	writel(ccgr3, &imx_ccm->CCGR3);
}

/*
 * Set IPU clock source and enable IPU clock.
 * 
 * CPU       Max. IPU clock    Use clock source   Divide by   Result
 * ------------------------------------------------------------------
 * MX6Q      264 MHz           MMDC_CH0           2           264 MHz
 * MX6DL     270 MHz           PLL3_PFD1          2           270 MHz
 */
void enable_ipu_clock(int ipu)
{
	u32 ccgr3, cscdr3;
	u32 clk_src =  1;

	if (is_cpu_type(MXC_CPU_MX6DL)) {
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

int config_lvds_clk(unsigned int ipu, unsigned int di, unsigned int freq,
		    unsigned int split)
{
	u32 divider;
	u32 reg;
	u32 mask;
	int use_pll5;
	void *addr;
	int shift = (di == 1) ? 12 : 9;

#ifdef CONFIG_MX6Q
	/* ### Why? */
	__raw_writel(BM_ANADIG_PFD_528_PFD2_CLKGATE,
		     &imx_ccm->analog_pfd_528_set);
#endif

	disable_pll5();

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
	 * it topll3_sw_clk and then to disable pll3_sw_clk
	 * Make sure periph2_clk2_sel is set to pll3_sw_clk
	 */
	reg = __raw_readl(&imx_ccm->cbcmr);
	reg &= ~(1 << 20);
	__raw_writel(reg, &imx_ccm->cbcmr);
	
	/*
	 * Set MMDC_CH1 mask bit.
	 */
	reg = __raw_readl(&imx_ccm->ccdr);
	reg |= 1 << 16;
	__raw_writel(reg, &imx_ccm->ccdr);

	/*
	 * Set the periph2_clk_sel to the top mux so that
	 * mmdc_ch1 is from pll3_sw_clk.
	 */
	reg = __raw_readl(&imx_ccm->cbcdr);
	reg |= 1 << 26;
	__raw_writel(reg, &imx_ccm->cbcdr);

	/*
	 * Wait for the clock switch.
	 */
	while (__raw_readl(&imx_ccm->cdhipr))
		;
	
	/*
	 * Disable pll3_sw_clk by selecting the bypass clock source.
	 */
	reg = __raw_readl(&imx_ccm->ccsr);
	reg |= 1 << 0;
	__raw_writel(reg, &imx_ccm->ccsr);

	/*
	 * Set the ldb_di<n>_clk to 111b.
	 */
	reg = __raw_readl(&imx_ccm->cs2cdr);
	reg |= (7 << shift);
	__raw_writel(reg, &imx_ccm->cs2cdr);

	/*
	 * Set the ldb_di<n>_clk from 111b to 100b.
	 */
	reg &= ~(0x3 << shift);
	__raw_writel(reg, &imx_ccm->cs2cdr);

	/* Check if PLL2_PFD0 is sufficient; if not, use PLL5. PLL2 main
	   clock is 528 MHz. To avoid a computation overflow, drop the two
	   LSBs (i.e. divide by 4) */
	divider = ((decode_pll(PLL_BUS)/4) * 18 + (freq/8))/ (freq/4);
	use_pll5 = (divider < 12) || (divider > 35);
	/* Set ldb_di<n>_clk to PLL5 (000b) or PLL2 PFD0 (001b) */
	reg &= ~(0x7 << shift);
	if (!use_pll5)
		reg |= (0x1 << shift);
	__raw_writel(reg, &imx_ccm->cs2cdr);

	/*
	 * Unbypass pll3_sw_clk.
	 */
	reg = __raw_readl(&imx_ccm->ccsr);
	reg &= ~(1 << 0);
	__raw_writel(reg, &imx_ccm->ccsr);

	/*
	 * Set the periph2_clk_sel back to the bottom mux so that
	 * mmdc_ch1 is from its original parent.
	 */
	reg = __raw_readl(&imx_ccm->cbcdr);
	reg &= ~(1 << 26);
	__raw_writel(reg, &imx_ccm->cbcdr);

	/*
	 * Wait for the clock switch.
	 */
	while (__raw_readl(&imx_ccm->cdhipr))
		;

	/*
	 * Clear MMDC_CH1 mask bit.
	 */
	reg = __raw_readl(&imx_ccm->ccdr);
	reg &= ~(1 << 16);
	__raw_writel(reg, &imx_ccm->ccdr);

	if (use_pll5) {
		setup_pll5(freq);
	} else {
		/* Set PLL2 PFD0 Clock */
		reg = __raw_readl(&imx_ccm->analog_pfd_528);
		reg &= ~BM_ANADIG_PFD_528_PFD0_FRAC;
		reg |= (divider << BP_ANADIG_PFD_528_PFD0_FRAC);
		__raw_writel(reg, &imx_ccm->analog_pfd_528);
	}

#ifdef CONFIG_MX6Q
	/* Why ??? */
	__raw_writel(BM_ANADIG_PFD_528_PFD2_CLKGATE,
		     &imx_ccm->analog_pfd_528_clr);
#endif

	reg = __raw_readl(&imx_ccm->cscmr2);
	if (di == 1)
		mask = MXC_CCM_CSCMR2_LDB_DI1_IPU_DIV;
	else
		mask = MXC_CCM_CSCMR2_LDB_DI0_IPU_DIV;
	if (split)
		reg &= ~mask;		/* ipu_di_clk = ldb_di_clk/3.5 */
	else
		reg |= mask;		/* ipu_di_clk = ldb_di_clk/7 */
	__raw_writel(reg, &imx_ccm->cscmr2);

	if (use_pll5) {
		/* Set ipu_di_clk clock source to ldb_di_clk, and root clock pre-multiplexer from PLL5, ipu_di_podf divide by 1 */
		if (di == 1) {
			mask = 0x1ff << 9;
			reg = ((4 << 9) | (2 << 15));
		} else {
			mask = 0x1ff << 0;
			reg = ((3 << 0) | (2 << 6));
		}
	} else {
		/* Set ipu_di_clk clock source to ldb_di_clk, and root clock pre-multiplexer from PLL2 PFD0, ipu_di_podf divide by 1 */
		if (di == 1) {
			mask = 0x1ff << 9;
			reg = ((4 << 9) | (3 << 15));
		} else {
			mask = 0x1ff << 0;
			reg = ((3 << 0) | (3 << 6));
		}
	}

	if (ipu == 2)
		addr = &imx_ccm->cscdr2;
	else
		addr = &imx_ccm->chsccdr;
	__raw_writel((__raw_readl(addr) & ~mask) | reg, addr);

	return use_pll5;
}

int config_lcd_di_clk(u32 ipu, u32 di)
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
#endif /* CONFIG_VIDEO_IPUV3 */

#ifdef CONFIG_FEC_MXC
int enable_fec_anatop_clock(int fec_id, enum enet_freq freq)
{
	u32 reg = 0;
	s32 timeout = 100000;

	struct anatop_regs __iomem *anatop =
		(struct anatop_regs __iomem *)ANATOP_BASE_ADDR;

	if (freq < ENET_25MHZ || freq > ENET_125MHZ)
		return -EINVAL;

	reg = readl(&anatop->pll_enet);

	if (fec_id == 0) {
		reg &= ~BM_ANADIG_PLL_ENET_DIV_SELECT;
		reg |= BF_ANADIG_PLL_ENET_DIV_SELECT(freq);
	} else if (fec_id == 1) {
		/* Only i.MX6SX/UL support ENET2 */
		if (!(is_cpu_type(MXC_CPU_MX6SX) ||
		      is_cpu_type(MXC_CPU_MX6UL) ||
		      is_cpu_type(MXC_CPU_MX6ULL)))
			return -EINVAL;
		reg &= ~BM_ANADIG_PLL_ENET2_DIV_SELECT;
		reg |= BF_ANADIG_PLL_ENET2_DIV_SELECT(freq);
	} else {
		return -EINVAL;
	}

	if ((reg & BM_ANADIG_PLL_ENET_POWERDOWN) ||
	    (!(reg & BM_ANADIG_PLL_ENET_LOCK))) {
		reg &= ~BM_ANADIG_PLL_ENET_POWERDOWN;
		writel(reg, &anatop->pll_enet);
		while (timeout--) {
			if (readl(&anatop->pll_enet) & BM_ANADIG_PLL_ENET_LOCK)
				break;
		}
		if (timeout < 0)
			return -ETIMEDOUT;
	}

	/* Enable FEC clock */
	if (fec_id == 0)
		reg |= BM_ANADIG_PLL_ENET_ENABLE;
	else
		reg |= BM_ANADIG_PLL_ENET2_ENABLE;
	reg &= ~BM_ANADIG_PLL_ENET_BYPASS;
#ifdef CONFIG_FEC_MXC_25M_REF_CLK
	reg |= BM_ANADIG_PLL_ENET_REF_25M_ENABLE;
#endif
	writel(reg, &anatop->pll_enet);

#ifdef CONFIG_MX6SX
	/* Disable enet system clcok before switching clock parent */
	reg = readl(&imx_ccm->CCGR3);
	reg &= ~MXC_CCM_CCGR3_ENET_CLK_ENABLE_MASK;
	writel(reg, &imx_ccm->CCGR3);

	/*
	 * Set enet ahb clock to 200MHz
	 * pll2_pfd2_396m-> ENET_PODF-> ENET_AHB
	 */
	reg = readl(&imx_ccm->chsccdr);
	reg &= ~(MXC_CCM_CHSCCDR_ENET_PRE_CLK_SEL_MASK
		 | MXC_CCM_CHSCCDR_ENET_PODF_MASK
		 | MXC_CCM_CHSCCDR_ENET_CLK_SEL_MASK);
	/* PLL2 PFD2 */
	reg |= (4 << MXC_CCM_CHSCCDR_ENET_PRE_CLK_SEL_OFFSET);
	/* Div = 2*/
	reg |= (1 << MXC_CCM_CHSCCDR_ENET_PODF_OFFSET);
	reg |= (0 << MXC_CCM_CHSCCDR_ENET_CLK_SEL_OFFSET);
	writel(reg, &imx_ccm->chsccdr);

	/* Enable enet system clock */
	reg = readl(&imx_ccm->CCGR3);
	reg |= MXC_CCM_CCGR3_ENET_CLK_ENABLE_MASK;
	writel(reg, &imx_ccm->CCGR3);
#endif
	return 0;
}
#endif

static u32 get_usdhc_clk(u32 port)
{
	u32 root_freq = 0, usdhc_podf = 0, clk_sel = 0;
	u32 cscmr1 = __raw_readl(&imx_ccm->cscmr1);
	u32 cscdr1 = __raw_readl(&imx_ccm->cscdr1);

	switch (port) {
	case 0:
		usdhc_podf = (cscdr1 & MXC_CCM_CSCDR1_USDHC1_PODF_MASK) >>
					MXC_CCM_CSCDR1_USDHC1_PODF_OFFSET;
		clk_sel = cscmr1 & MXC_CCM_CSCMR1_USDHC1_CLK_SEL;

		break;
	case 1:
		usdhc_podf = (cscdr1 & MXC_CCM_CSCDR1_USDHC2_PODF_MASK) >>
					MXC_CCM_CSCDR1_USDHC2_PODF_OFFSET;
		clk_sel = cscmr1 & MXC_CCM_CSCMR1_USDHC2_CLK_SEL;

		break;
	case 2:
		usdhc_podf = (cscdr1 & MXC_CCM_CSCDR1_USDHC3_PODF_MASK) >>
					MXC_CCM_CSCDR1_USDHC3_PODF_OFFSET;
		clk_sel = cscmr1 & MXC_CCM_CSCMR1_USDHC3_CLK_SEL;

		break;
	case 3:
		usdhc_podf = (cscdr1 & MXC_CCM_CSCDR1_USDHC4_PODF_MASK) >>
					MXC_CCM_CSCDR1_USDHC4_PODF_OFFSET;
		clk_sel = cscmr1 & MXC_CCM_CSCMR1_USDHC4_CLK_SEL;

		break;
	default:
		break;
	}

	if (clk_sel)
		root_freq = mxc_get_pll_pfd(PLL_BUS, 0);
	else
		root_freq = mxc_get_pll_pfd(PLL_BUS, 2);

	return root_freq / (usdhc_podf + 1);
}

u32 imx_get_uartclk(void)
{
	return get_uart_clk();
}

u32 imx_get_fecclk(void)
{
	return mxc_get_clock(MXC_IPG_CLK);
}

#if defined(CONFIG_CMD_SATA) || defined(CONFIG_PCIE_IMX)
static int enable_enet_pll(uint32_t en)
{
	struct mxc_ccm_reg *const imx_ccm
		= (struct mxc_ccm_reg *) CCM_BASE_ADDR;
	s32 timeout = 100000;
	u32 reg = 0;

	/* Enable PLLs */
	reg = readl(&imx_ccm->analog_pll_enet);
	reg &= ~BM_ANADIG_PLL_SYS_POWERDOWN;
	writel(reg, &imx_ccm->analog_pll_enet);
	reg |= BM_ANADIG_PLL_SYS_ENABLE;
	while (timeout--) {
		if (readl(&imx_ccm->analog_pll_enet) & BM_ANADIG_PLL_SYS_LOCK)
			break;
	}
	if (timeout <= 0)
		return -EIO;
	reg &= ~BM_ANADIG_PLL_SYS_BYPASS;
	writel(reg, &imx_ccm->analog_pll_enet);
	reg |= en;
	writel(reg, &imx_ccm->analog_pll_enet);
	return 0;
}
#endif

#ifdef CONFIG_CMD_SATA
static void ungate_sata_clock(void)
{
	struct mxc_ccm_reg *const imx_ccm =
		(struct mxc_ccm_reg *)CCM_BASE_ADDR;

	/* Enable SATA clock. */
	setbits_le32(&imx_ccm->CCGR5, MXC_CCM_CCGR5_SATA_MASK);
}

int enable_sata_clock(void)
{
	ungate_sata_clock();
	return enable_enet_pll(BM_ANADIG_PLL_ENET_ENABLE_SATA);
}

void disable_sata_clock(void)
{
	struct mxc_ccm_reg *const imx_ccm =
		(struct mxc_ccm_reg *)CCM_BASE_ADDR;

	clrbits_le32(&imx_ccm->CCGR5, MXC_CCM_CCGR5_SATA_MASK);
}
#endif

#ifdef CONFIG_PCIE_IMX
static void ungate_disp_axi_clock(void)
{
	struct mxc_ccm_reg *const imx_ccm =
		(struct mxc_ccm_reg *)CCM_BASE_ADDR;

	/* Enable display axi clock. */
	setbits_le32(&imx_ccm->CCGR3, MXC_CCM_CCGR3_DISP_AXI_MASK);
}

static void ungate_pcie_clock(void)
{
	struct mxc_ccm_reg *const imx_ccm =
		(struct mxc_ccm_reg *)CCM_BASE_ADDR;

	/* Enable PCIe clock. */
	setbits_le32(&imx_ccm->CCGR4, MXC_CCM_CCGR4_PCIE_MASK);
}

int enable_pcie_clock(void)
{
	struct anatop_regs *anatop_regs =
		(struct anatop_regs *)ANATOP_BASE_ADDR;
	struct mxc_ccm_reg *ccm_regs = (struct mxc_ccm_reg *)CCM_BASE_ADDR;
	u32 lvds1_clk_sel;

	/*
	 * Here be dragons!
	 *
	 * The register ANATOP_MISC1 is not documented in the Freescale
	 * MX6RM. The register that is mapped in the ANATOP space and
	 * marked as ANATOP_MISC1 is actually documented in the PMU section
	 * of the datasheet as PMU_MISC1.
	 *
	 * Switch LVDS clock source to SATA (0xb) on mx6q/dl or PCI (0xa) on
	 * mx6sx, disable clock INPUT and enable clock OUTPUT. This is important
	 * for PCI express link that is clocked from the i.MX6.
	 */
#define ANADIG_ANA_MISC1_LVDSCLK1_IBEN		(1 << 12)
#define ANADIG_ANA_MISC1_LVDSCLK1_OBEN		(1 << 10)
#define ANADIG_ANA_MISC1_LVDS1_CLK_SEL_MASK	0x0000001F
#define ANADIG_ANA_MISC1_LVDS1_CLK_SEL_PCIE_REF	0xa
#define ANADIG_ANA_MISC1_LVDS1_CLK_SEL_SATA_REF	0xb

	if (is_cpu_type(MXC_CPU_MX6SX))
		lvds1_clk_sel = ANADIG_ANA_MISC1_LVDS1_CLK_SEL_PCIE_REF;
	else
		lvds1_clk_sel = ANADIG_ANA_MISC1_LVDS1_CLK_SEL_SATA_REF;

	clrsetbits_le32(&anatop_regs->ana_misc1,
			ANADIG_ANA_MISC1_LVDSCLK1_IBEN |
			ANADIG_ANA_MISC1_LVDS1_CLK_SEL_MASK,
			ANADIG_ANA_MISC1_LVDSCLK1_OBEN | lvds1_clk_sel);

	/* PCIe reference clock sourced from AXI. */
	clrbits_le32(&ccm_regs->cbcmr, MXC_CCM_CBCMR_PCIE_AXI_CLK_SEL);

	if (!is_cpu_type(MXC_CPU_MX6SX)) {
		/* Party time! Ungate the clock to the PCIe. */
#ifdef CONFIG_CMD_SATA
		ungate_sata_clock();
#endif
		ungate_pcie_clock();

		return enable_enet_pll(BM_ANADIG_PLL_ENET_ENABLE_SATA |
				       BM_ANADIG_PLL_ENET_ENABLE_PCIE);
	} else {
		/* Party time! Ungate the clock to the PCIe. */
		ungate_disp_axi_clock();
		ungate_pcie_clock();

		return enable_enet_pll(BM_ANADIG_PLL_ENET_ENABLE_PCIE);
	}
}
#endif

#ifdef CONFIG_SECURE_BOOT
void hab_caam_clock_enable(unsigned char enable)
{
	u32 reg;

	if (is_cpu_type(MXC_CPU_MX6ULL)) {
		/* CG5, DCP clock */
		reg = __raw_readl(&imx_ccm->CCGR0);
		if (enable)
			reg |= MXC_CCM_CCGR0_DCP_CLK_MASK;
		else
			reg &= ~MXC_CCM_CCGR0_DCP_CLK_MASK;
		__raw_writel(reg, &imx_ccm->CCGR0);
	} else {
		/* CG4 ~ CG6, CAAM clocks */
		reg = __raw_readl(&imx_ccm->CCGR0);
		if (enable)
			reg |= (MXC_CCM_CCGR0_CAAM_WRAPPER_IPG_MASK |
				MXC_CCM_CCGR0_CAAM_WRAPPER_ACLK_MASK |
				MXC_CCM_CCGR0_CAAM_SECURE_MEM_MASK);
		else
			reg &= ~(MXC_CCM_CCGR0_CAAM_WRAPPER_IPG_MASK |
				MXC_CCM_CCGR0_CAAM_WRAPPER_ACLK_MASK |
				MXC_CCM_CCGR0_CAAM_SECURE_MEM_MASK);
		__raw_writel(reg, &imx_ccm->CCGR0);
	}

	/* EMI slow clk */
	reg = __raw_readl(&imx_ccm->CCGR6);
	if (enable)
		reg |= MXC_CCM_CCGR6_EMI_SLOW_MASK;
	else
		reg &= ~MXC_CCM_CCGR6_EMI_SLOW_MASK;
	__raw_writel(reg, &imx_ccm->CCGR6);
}
#endif

unsigned int mxc_get_clock(enum mxc_clock clk)
{
	switch (clk) {
	case MXC_ARM_CLK:
		return get_mcu_main_clk();
	case MXC_PER_CLK:
		return get_periph_clk();
	case MXC_AHB_CLK:
		return get_ahb_clk();
	case MXC_IPG_CLK:
		return get_ipg_clk();
	case MXC_IPG_PERCLK:
	case MXC_I2C_CLK:
		return get_ipg_per_clk();
	case MXC_UART_CLK:
		return get_uart_clk();
	case MXC_CSPI_CLK:
		return get_cspi_clk();
	case MXC_AXI_CLK:
		return get_axi_clk();
	case MXC_EMI_SLOW_CLK:
		return get_emi_slow_clk();
	case MXC_DDR_CLK:
		return get_mmdc_ch0_clk();
	case MXC_ESDHC_CLK:
		return get_usdhc_clk(0);
	case MXC_ESDHC2_CLK:
		return get_usdhc_clk(1);
	case MXC_ESDHC3_CLK:
		return get_usdhc_clk(2);
	case MXC_ESDHC4_CLK:
		return get_usdhc_clk(3);
	case MXC_SATA_CLK:
		return get_ahb_clk();
	default:
		printf("Unsupported MXC CLK: %d\n", clk);
		break;
	}

	return 0;
}

static void show_freq(const char *name, u32 freq)
{
	freq = (freq + 50000) / 100000;
	printf("%-13s%4d.%01d MHz\n", name, freq / 10, freq % 10);
}

/*
 * Dump some core clockes.
 */
int do_mx6_showclocks(cmd_tbl_t *cmdtp, int flag, int argc, char * const argv[])
{
	show_freq("PLL1 (ARM)", decode_pll(PLL_SYS));
	show_freq("PLL2 (BUS)", decode_pll(PLL_BUS));
	show_freq("  PLL2_PFD0", mxc_get_pll_pfd(PLL_BUS, 0));
	show_freq("  PLL2_PFD1", mxc_get_pll_pfd(PLL_BUS, 1));
	show_freq("  PLL2_PFD2", mxc_get_pll_pfd(PLL_BUS, 2));
	if (is_cpu_type(MXC_CPU_MX6UL))
		show_freq("  PLL2_PFD3", mxc_get_pll_pfd(PLL_BUS, 3));
	show_freq("PLL3 (USBOTG)", decode_pll(PLL_USBOTG));
	show_freq("  PLL3_PFD0", mxc_get_pll_pfd(PLL_USBOTG, 0));
	show_freq("  PLL3_PFD1", mxc_get_pll_pfd(PLL_USBOTG, 1));
	show_freq("  PLL3_PFD2", mxc_get_pll_pfd(PLL_USBOTG, 2));
	show_freq("  PLL3_PFD3", mxc_get_pll_pfd(PLL_USBOTG, 3));
	show_freq("PLL4 (AUDIO)", decode_pll(PLL_AUDIO));
	show_freq("PLL5 (VIDEO)", decode_pll(PLL_VIDEO));
	show_freq("PLL6 (ENET)", decode_pll(PLL_ENET));
	show_freq("PLL7 (USB2)", decode_pll(PLL_USB2));

	puts("\n");
	show_freq("IPG", mxc_get_clock(MXC_IPG_CLK));
	show_freq("UART", mxc_get_clock(MXC_UART_CLK));
#ifdef CONFIG_MXC_SPI
	show_freq("CSPI", mxc_get_clock(MXC_CSPI_CLK));
#endif
	show_freq("AHB", mxc_get_clock(MXC_AHB_CLK));
	show_freq("AXI", mxc_get_clock(MXC_AXI_CLK));
	show_freq("DDR", mxc_get_clock(MXC_DDR_CLK));
	show_freq("USDHC1", mxc_get_clock(MXC_ESDHC_CLK));
	show_freq("USDHC2", mxc_get_clock(MXC_ESDHC2_CLK));
	show_freq("USDHC3", mxc_get_clock(MXC_ESDHC3_CLK));
	show_freq("USDHC4", mxc_get_clock(MXC_ESDHC4_CLK));
	show_freq("EMI SLOW", mxc_get_clock(MXC_EMI_SLOW_CLK));
	show_freq("IPG PERCLK", mxc_get_clock(MXC_IPG_PERCLK));

#ifdef CONFIG_VIDEO_IPUV3
	puts("\n");
	show_freq("IPU1", mxc_get_ipu_clock(1));
	show_freq("IPU1_DI0", mxc_get_ipu_di_clock(1, 0));
	show_freq("IPU1_DI1", mxc_get_ipu_di_clock(1, 1));
	if (!is_cpu_type(MXC_CPU_MX6DL)) {
		show_freq("IPU2", mxc_get_ipu_clock(2));
		show_freq("IPU2_DI0", mxc_get_ipu_di_clock(2, 0));
		show_freq("IPU2_DI1", mxc_get_ipu_di_clock(2, 1));
	}
	show_freq("LDB_DI0", mxc_get_ldb_clock(0));
	show_freq("LDB_DI1", mxc_get_ldb_clock(1));
#endif
	return 0;
}

/***************************************************/

U_BOOT_CMD(
	clocks,	CONFIG_SYS_MAXARGS, 1, do_mx6_showclocks,
	"display clocks",
	""
);
