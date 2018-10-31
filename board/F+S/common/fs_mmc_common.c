/*
 * fs_mmc_common.c
 *
 * (C) Copyright 2018
 * Hartmut Keller, F&S Elektronik Systeme GmbH, keller@fs-net.de
 *
 * Common MMC code used on F&S boards
 *
 * SPDX-License-Identifier:	GPL-2.0+
 */

#include <config.h>

#ifdef CONFIG_GENERIC_MMC

#include <common.h>			/* Types, container_of(), ... */
#include <asm/gpio.h>			/* gpio_get_value(), ... */
#include <asm/io.h>			/* readl(), writel() */
#include <asm/arch/crm_regs.h>		/* struct mxc_ccm_reg */
#include <asm/arch/clock.h>		/* MXC_ESDHC_CLK, ... */
#include <mmc.h>			/* struct mmc */
#include "fs_mmc_common.h"		/* Own interface */

/* Convert from struct fsl_esdhc_cfg to struct fus_sdhc_cfg */
#define to_fs_mmc_cfg(x) container_of((x), struct fs_mmc_cfg, esdhc)

/* Return value of Card Detect pin (if present) */
int board_mmc_getcd(struct mmc *mmc)
{
	struct fsl_esdhc_cfg *fsl_cfg = mmc->priv;
	struct fs_mmc_cfg *fs_cfg = to_fs_mmc_cfg(fsl_cfg);
	u16 cd_gpio = fs_cfg->cd_gpio;

	if (cd_gpio == (u16)~0)
		return 1;		/* No CD, assume card is present */

	/* Return CD signal (active low) */
	return !gpio_get_value(cd_gpio);
}

/* Set up control pads, bus pads and card detect pad for one MMC port */
int fs_mmc_setup(bd_t *bd, u8 bus_width, struct fs_mmc_cfg *cfg,
		 const struct fs_mmc_cd *cd)
{
	struct mxc_ccm_reg *mxc_ccm = (struct mxc_ccm_reg *)CCM_BASE_ADDR;
	u32 ccgr6;

	/* Set CD pin configuration, activate GPIO for CD (if appropriate) */
	if (!cd)
		cfg->cd_gpio = ~0;
	else {
		cfg->cd_gpio = cd->gpio;
		imx_iomux_v3_setup_multiple_pads(cd->pad, 1);
		gpio_direction_input(cd->gpio);
	}

	/* Set DAT, CLK, CMD and RST pin configurations */
	cfg->esdhc.max_bus_width = bus_width;
	imx_iomux_v3_setup_multiple_pads(cfg->pads, cfg->count + bus_width);

	/* Get clock speed and ungate appropriate USDHC clock */
	ccgr6 = readl(&mxc_ccm->CCGR6);
	switch (cfg->index) {
	default:
	case 1:
		cfg->esdhc.esdhc_base = USDHC1_BASE_ADDR;
		cfg->esdhc.sdhc_clk = mxc_get_clock(MXC_ESDHC_CLK);
		ccgr6 |= (3 << 2);
		break;
	case 2:
		cfg->esdhc.esdhc_base = USDHC2_BASE_ADDR;
		cfg->esdhc.sdhc_clk = mxc_get_clock(MXC_ESDHC2_CLK);
		ccgr6 |= (3 << 4);
		break;
#ifdef USDHC3_BASE_ADDR
	case 3:
		cfg->esdhc.esdhc_base = USDHC3_BASE_ADDR;
		cfg->esdhc.sdhc_clk = mxc_get_clock(MXC_ESDHC3_CLK);
		ccgr6 |= (3 << 6);
		break;
#endif
#ifdef USDHC4_BASE_ADDR
	case 4:
		cfg->esdhc.esdhc_base = USDHC4_BASE_ADDR;
		cfg->esdhc.sdhc_clk = mxc_get_clock(MXC_ESDHC4_CLK);
		ccgr6 |= (3 << 8);
		break;
#endif
	}
	writel(ccgr6, &mxc_ccm->CCGR6);

	return fsl_esdhc_initialize(bd, &cfg->esdhc);
}
#endif /* CONFIG_GENERIC_MMC */
