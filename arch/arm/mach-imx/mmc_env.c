// SPDX-License-Identifier: GPL-2.0+
/*
 * Copyright (C) 2017 NXP
 */

#include <common.h>
#include <asm/arch/imx-regs.h>
#include <asm/arch/sys_proto.h>
#include <asm/io.h>
#include <asm/mach-imx/boot_mode.h>

__weak int board_mmc_get_env_dev(int devno)
{
	return devno;
}

int mmc_get_env_dev(void)
{
	enum boot_device boot_dev = get_boot_device();
	int devno;

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
		devno = CONFIG_SYS_MMC_ENV_DEV;
		break;
	}

	return board_mmc_get_env_dev(devno);
}
