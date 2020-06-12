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

#ifdef CONFIG_FSL_ESDHC

#include <common.h>			/* Types, container_of(), ... */
#include <asm/gpio.h>			/* gpio_get_value(), ... */
#include <asm/io.h>			/* readl(), writel() */
#include <asm/arch/crm_regs.h>		/* struct mxc_ccm_reg */
#include <asm/arch/clock.h>		/* MXC_ESDHC_CLK, ... */
#include <mmc.h>			/* struct mmc */
#include "fs_mmc_common.h"		/* Own interface */

#define USDHC_NONE -1

enum
{
	USDHC1,
	USDHC2,
	USDHC3,
	USDHC4,
	USDHCNUM,
};

static int usdhc_pos_in_init[] =
{
	USDHC_NONE,
	USDHC_NONE,
	USDHC_NONE,
	USDHC_NONE
};

static int usdhc_boot_device = -1;
static int mmc_boot_device = -1;

/* Return value of Card Detect pin (if present) */
int board_mmc_getcd(struct mmc *mmc)
{
	struct fsl_esdhc_cfg *fsl_cfg = mmc->priv;
	unsigned cd_gpio = fsl_cfg->cd_gpio;

	if (cd_gpio == ~0U)
		return 1;		/* No CD, assume card is present */

	/* Return CD signal (active low) */
	return !gpio_get_value(cd_gpio);
}

/* Set up control pads, bus pads and card detect pad for one MMC port */
int fs_mmc_setup(bd_t *bd, u8 bus_width, struct fs_mmc_cfg *cfg,
		 const struct fs_mmc_cd *cd)
{
#if !defined(CONFIG_IMX8M) && !defined(CONFIG_IMX8MM) && !defined(CONFIG_IMX8MN)
	struct mxc_ccm_reg *mxc_ccm = (struct mxc_ccm_reg *)CCM_BASE_ADDR;
	u32 ccgr6;
#endif
	static int sdhc_cnt = 0;

	/* Set CD pin configuration, activate GPIO for CD (if appropriate) */
	if (!cd)
		cfg->esdhc.cd_gpio = ~0;
	else {
		cfg->esdhc.cd_gpio = cd->gpio;
		imx_iomux_v3_setup_multiple_pads(cd->pad, 1);
		gpio_direction_input(cd->gpio);
	}

	/* Set DAT, CLK, CMD and RST pin configurations */
	cfg->esdhc.max_bus_width = bus_width;
	imx_iomux_v3_setup_multiple_pads(cfg->pads, cfg->count + bus_width);
#if !defined(CONFIG_IMX8M) && !defined(CONFIG_IMX8MM) && !defined(CONFIG_IMX8MN)
	/* Get clock speed and ungate appropriate USDHC clock */
	ccgr6 = readl(&mxc_ccm->CCGR6);
#endif
	switch (cfg->index) {
	default:
	case 1:
		cfg->esdhc.esdhc_base = USDHC1_BASE_ADDR;
		cfg->esdhc.sdhc_clk = mxc_get_clock(MXC_ESDHC_CLK);
#if !defined(CONFIG_IMX8M) && !defined(CONFIG_IMX8MM) && !defined(CONFIG_IMX8MN)		
		ccgr6 |= (3 << 2);
#endif		
		usdhc_pos_in_init[USDHC1] = sdhc_cnt;
		break;
	case 2:
		cfg->esdhc.esdhc_base = USDHC2_BASE_ADDR;
		cfg->esdhc.sdhc_clk = mxc_get_clock(MXC_ESDHC2_CLK);
#if !defined(CONFIG_IMX8M) && !defined(CONFIG_IMX8MM) && !defined(CONFIG_IMX8MN)
		ccgr6 |= (3 << 4);
#endif		
		usdhc_pos_in_init[USDHC2] = sdhc_cnt;
		break;
#ifdef USDHC3_BASE_ADDR
	case 3:
		cfg->esdhc.esdhc_base = USDHC3_BASE_ADDR;
		cfg->esdhc.sdhc_clk = mxc_get_clock(MXC_ESDHC3_CLK);
		usdhc_pos_in_init[USDHC3] = sdhc_cnt;
#if !defined(CONFIG_IMX8M) && !defined(CONFIG_IMX8MM) && !defined(CONFIG_IMX8MN)		
		ccgr6 |= (3 << 6);
#endif		
		break;
#endif
#ifdef USDHC4_BASE_ADDR
	case 4:
		cfg->esdhc.esdhc_base = USDHC4_BASE_ADDR;
		cfg->esdhc.sdhc_clk = mxc_get_clock(MXC_ESDHC4_CLK);
		usdhc_pos_in_init[USDHC4] = sdhc_cnt;
#if !defined(CONFIG_IMX8M) && !defined(CONFIG_IMX8MM) && !defined(CONFIG_IMX8MN)		
		ccgr6 |= (3 << 8);
#endif		
		break;
#endif
	}
#if !defined(CONFIG_IMX8M) && !defined(CONFIG_IMX8MM) && !defined(CONFIG_IMX8MN)	
	writel(ccgr6, &mxc_ccm->CCGR6);
#endif

	sdhc_cnt++;

	return fsl_esdhc_initialize(bd, &cfg->esdhc);
}

#ifdef CONFIG_ENV_IS_IN_MMC
/* Override board_mmc_get_env_dev to get boot dev from fuse settings */
int board_mmc_get_env_dev(int devno)
{
#if defined(CONFIG_DM_MMC) && defined(CONFIG_BLK)
	if(!find_mmc_device(devno)) {
		/* Check device tree node for usdhc[devno] 
                 */
		debug("Device %d is not available.", devno);
		return USDHC_NONE;
	} else {
		/* Use NXP aliases for mmc devices:
		 * mmc0 = &usdhc1
		 * mmc1 = &usdhc2
		 * mmc2 = &usdhc3
		 * mmc3 = &usdhc4
		 */
		usdhc_boot_device = devno;
		mmc_boot_device =   devno;
	}
#else
	usdhc_boot_device = devno;
	mmc_boot_device = usdhc_pos_in_init[devno];
#endif

	return mmc_boot_device;
}
#endif

int get_usdhc_boot_device()
{
	return usdhc_boot_device;
}

int get_mmc_boot_device()
{
	return mmc_boot_device;
}

#endif /* CONFIG_FSL_ESDHC */
