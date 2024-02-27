/* SPDX-License-Identifier: GPL-2.0+ */
/*
 * (C) Copyright 2023
 * F&S Elektronik Systeme GmbH, keller@fs-net.de.
 */

#ifndef	__FS_CFG_INFO_H
#define __FS_CFG_INFO_H

#include <asm/mach-imx/boot_mode.h>	/* enum boot_device */

/*
 * This is a binary version of that part of the BOARD-CFG data that is needed
 * throughout of U-Boot. It is stored in the global_data, so keep it as small
 * as possible.
 */
#define CI_FLAGS_SECONDARY       BIT(0)	/* Running from secondary SPL */
#define CI_FLAGS_HAVE_ENV        BIT(1)	/* env_start[] is valid */
#define CI_FLAGS_SECONDARY_UBOOT BIT(2)
struct cfg_info {
	enum boot_device boot_dev;
	u8 board_type;
	u32 board_rev;
	u32 features;
	u32 dram_size;
	u32 dram_chips;
	u32 flags;			/* See CI_FLAGS_* above */
	u32 env_start[2];
};

#endif /* !__FS_CFG_INFO_H */
