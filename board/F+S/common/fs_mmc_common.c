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

#ifdef CONFIG_FSL_ESDHC_IMX

#include <common.h>			/* Types, container_of(), ... */
#include <asm/gpio.h>			/* gpio_get_value(), ... */
#include <asm/io.h>			/* readl(), writel() */
#include <asm/mach-imx/boot_mode.h>
#include "fs_board_common.h"		/* Own interface */
#if !defined(CONFIG_IMX8M) && !defined(CONFIG_IMX8MM) && !defined(CONFIG_IMX8MN) && !defined(CONFIG_ARCH_MX7ULP)
#include <asm/arch/crm_regs.h>		/* struct mxc_ccm_reg */
#endif
#include <asm/arch/clock.h>		/* MXC_ESDHC_CLK, ... */
#include <mmc.h>			/* struct mmc */
#include "fs_mmc_common.h"		/* Own interface */
#ifdef CONFIG_FS_BOARD_CFG
#include "fs_image_common.h"		/* fs_image_*() */
#endif

#define USDHC_NONE -1

enum
{
	USDHC1,
	USDHC2,
	USDHC3,
	USDHC4,
	USDHCNUM,
};

static int usdhc_boot_device = USDHC_NONE;
static int mmc_boot_device = USDHC_NONE;

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

#if !defined(CONFIG_DM_MMC) || !defined(CONFIG_BLK)
static int usdhc_pos_in_init[] =
{
	USDHC_NONE,
	USDHC_NONE,
	USDHC_NONE,
	USDHC_NONE
};

/* Set up control pads, bus pads and card detect pad for one MMC port */
int fs_mmc_setup(struct bd_info *bd, u8 bus_width, struct fs_mmc_cfg *cfg,
		 const struct fs_mmc_cd *cd)
{
	struct mxc_ccm_reg *mxc_ccm = (struct mxc_ccm_reg *)CCM_BASE_ADDR;
	u32 ccgr6;
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
	/* Get clock speed and ungate appropriate USDHC clock */
	ccgr6 = readl(&mxc_ccm->CCGR6);

	switch (cfg->index) {
	default:
	case 1:
		cfg->esdhc.esdhc_base = USDHC1_BASE_ADDR;
		cfg->esdhc.sdhc_clk = mxc_get_clock(MXC_ESDHC_CLK);
		ccgr6 |= (3 << 2);
		usdhc_pos_in_init[USDHC1] = sdhc_cnt;
		break;
	case 2:
		cfg->esdhc.esdhc_base = USDHC2_BASE_ADDR;
		cfg->esdhc.sdhc_clk = mxc_get_clock(MXC_ESDHC2_CLK);
		ccgr6 |= (3 << 4);
		usdhc_pos_in_init[USDHC2] = sdhc_cnt;
		break;
#ifdef USDHC3_BASE_ADDR
	case 3:
		cfg->esdhc.esdhc_base = USDHC3_BASE_ADDR;
		cfg->esdhc.sdhc_clk = mxc_get_clock(MXC_ESDHC3_CLK);
		usdhc_pos_in_init[USDHC3] = sdhc_cnt;
		ccgr6 |= (3 << 6);
		break;
#endif
#ifdef USDHC4_BASE_ADDR
	case 4:
		cfg->esdhc.esdhc_base = USDHC4_BASE_ADDR;
		cfg->esdhc.sdhc_clk = mxc_get_clock(MXC_ESDHC4_CLK);
		usdhc_pos_in_init[USDHC4] = sdhc_cnt;
		ccgr6 |= (3 << 8);
		break;
#endif
	}
	writel(ccgr6, &mxc_ccm->CCGR6);

	sdhc_cnt++;

	return fsl_esdhc_initialize(bd, &cfg->esdhc);
}
#endif

#ifdef CONFIG_ENV_IS_IN_MMC
/* Override board_mmc_get_env_dev to get boot dev from fuse settings */
int board_mmc_get_env_dev(int devno)
{
	enum boot_device boot_dev = fs_board_get_boot_dev();

	switch (boot_dev) {
	case SD1_BOOT:
	case SD2_BOOT:
	case SD3_BOOT:
	case SD4_BOOT:
		devno = boot_dev - SD1_BOOT;
		break;
	case MMC1_BOOT:
	case MMC2_BOOT:
	case MMC3_BOOT:
	case MMC4_BOOT:
		devno = boot_dev - MMC1_BOOT;
		break;
	default:
		devno = USDHC_NONE;
		break;
	}

	/* Set Linux device number */
	usdhc_boot_device = devno;
#if !defined(CONFIG_DM_MMC) || !defined(CONFIG_BLK)
	/*
	 * If using device trees, the U-Boot device number is the same as in
	 * Linux. But if not using device trees, the U-Boot device number
	 * depends on the initialization sequence.
	 */
	if (devno != USDHC_NONE)
		devno = usdhc_pos_in_init[devno];
#endif
	mmc_boot_device = devno;

	return devno;
}

#ifdef CONFIG_FS_BOARD_CFG

static void fs_mmc_get_env_info(struct mmc *mmc, struct cfg_info *cfg)
{
	void *fdt;
	int offs;
	unsigned int align;
	int err;

	if (cfg->flags & CI_FLAGS_HAVE_ENV)
		return;

	/*
	 * To find the environment location, we must access the BOARD-CFG in
	 * OCRAM. However this is not a safe resource, because it can be
	 * modified at any time. This is why we copy all relevant info to
	 * struct cfg_info right early in fs_setup_cfg_info() in the board,
	 * where we checked that the BOARD-CFG is valid. But at this early
	 * stage, the flash environment is not running yet and we cannot
	 * determine from which hwpart we are booting. So it is not possible
	 * to get the environment addresses there.
	 *
	 * Therefore we use a cache method. The first time we are called here,
	 * which is still in board_init_f(), we can assume that the BOARD-CFG
	 * is still valid. So access the BOARD-CFG, determine the environment
	 * location and store the start addresses for both copies in the
	 * cfg_info. From then on, we can use the cfg_info when we are called.
	 *
	 * If the environment location is not contained in nboot-info, it was
	 * located in the device tree of the previous U-Boot and NBoot didn't
	 * know anything about it. We have a list of known places where the
	 * environment was located in the past, so we take the first one
	 * (=newest) of these. This is not necessarily the right one, if a
	 * very old NBoot is still active right now. Then the environment will
	 * not be found. Theoretically we could try all entries and check
	 * CRC32 to find the correct position, but as the old environment may
	 * even had a different size, current U-Boot will not be able to
	 * handle this anyway.
	 *
	 * If this U-Boot is running in combination with the old NBoot just to
	 * be able to update NBoot, then do not save the environment in
	 * U-Boot and try option -e with fsimage save. This may find the
	 * correct environment and after a restart the old environment is
	 * available again.
	 */
	fdt = fs_image_get_cfg_fdt();
	offs = fs_image_get_nboot_info_offs(fdt);
	align = mmc_get_blk_desc(mmc)->blksz;

	err = fs_image_get_fdt_val(fdt, offs, "env-start", align,
				   2, cfg->env_start);
	if (err == -ENOENT) {
		/* This is an old version, use the old known position */
		err = fs_image_get_known_env_mmc(0, cfg->env_start, NULL);
	}
	if (err) {
		cfg->env_start[0] = CONFIG_ENV_MMC_OFFSET;
		cfg->env_start[1] = CONFIG_ENV_MMC_OFFSET_REDUND;
	}

	cfg->flags |= CI_FLAGS_HAVE_ENV;
}

/* Take env address and hwpart from nboot-info */
int board_mmc_get_env_addr(struct mmc *mmc, int copy, u32 *env_addr)
{
	struct cfg_info *cfg = fs_board_get_cfg_info();

	fs_mmc_get_env_info(mmc, cfg);

	*env_addr = cfg->env_start[copy];

	return 0;
}

uint board_mmc_get_env_part(struct mmc *mmc, int copy)
{
	struct cfg_info *cfg = fs_board_get_cfg_info();
	uint hwpart;

	fs_mmc_get_env_info(mmc, cfg);

	hwpart = (mmc->part_config >>3) & PART_ACCESS_MASK;
	if (hwpart > 2)
		hwpart = 0;

	/* If both addresses are equal, the copies are on different hwparts */
	if (copy && hwpart && (cfg->env_start[0] == cfg->env_start[1]))
		hwpart = 3 - hwpart;

	return hwpart;
}
#endif
#endif

__weak int get_usdhc_boot_device()
{
	return usdhc_boot_device;
}

__weak int get_mmc_boot_device()
{
	return mmc_boot_device;
}

#endif /* CONFIG_FSL_ESDHC_IMX */
