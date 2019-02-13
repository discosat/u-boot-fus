/*
 * fs_mmc_common.h
 *
 * (C) Copyright 2018
 * Hartmut Keller, F&S Elektronik Systeme GmbH, keller@fs-net.de
 *
 * Common MMC code used on F&S boards
 *
 * SPDX-License-Identifier:	GPL-2.0+
 */

#ifndef __FS_MMC_COMMON_H__
#define __FS_MMC_COMMON_H__

#include <asm/mach-imx/iomux-v3.h>	/* iomuc_v3_cfg_t */
#include <fsl_esdhc.h>			/* struct fsl_esdhc_cfg */

/*
 * Usage of fs_mmc_setup()
 *
 * 1. Prepare an array of iomux_v3_cfg_t pads that include all control signals
 *    (CLK, CMD, RESET, VSELECT, etc.).
 * 2. Add all possible data signals that are available on this port (up to 8)
 *    to the end of the above array. Sort data signals in increasing order,
 *    i.e. from DAT0 to DAT7. The real number of data bits will be given as
 *    bus_width argument to fs_mmc_setup(), so you can use the same pads array
 *    for 1-bit, 4-bit and 8-bit bus width, only the used data bits will be
 *    activated.
 * 3. Prepare an instance of struct fs_mmc_cfg and set the following entries:
 *      pads:  A pointer to the above pads array
 *      count: The number of control signals (excluding data signals)
 *      index: The ID of the USDHC port
 *    The remaining entries of this struct will be filled in by fs_mmc_setup().
 * 4. If you want to use a CD pin (Card Detect), prepare an array of
 *    iomux_v3_cfg_t pads with exactly one pad.
 * 5. Prepare an instance of struct fs_cd_cfg and set the two entries:
 *      pad:  A Pointer to the CD pad from step 4
 *      gpio: The GPIO number of the pad used as CD
 * 6. Call fs_mmc_setup(). Pass the bus width to use, the fs_mmc_cfg struct from
 *    step 3 and the fs_mmc_cd struct from step 5 (or NULL if no CD is used).
 *    fs_mmc_setup() will init the CD pad, and count+bus_width entries from the
 *    pads array.
 * 7. Repeat this sequence for all MMC ports.
 *
 * Remark:
 * The struct fs_mmc_cfg prepared in step 3 has to stay active as long as
 * U-Boot runs. So this must not be a local variable on the stack.
 */

struct fs_mmc_cfg {
	const iomux_v3_cfg_t *const pads; /* Pads to init */
	const u8 count;			/* Number of control pads (w/o data) */
	const u8 index;			/* USDHC index (1..4) */
	struct fsl_esdhc_cfg esdhc;	/* Data needed for MMC driver */
};

struct fs_mmc_cd {
	const iomux_v3_cfg_t *const pad;/* Pad to init (just one) */
	unsigned int gpio;		/* GPIO number for this pad */
};

int fs_mmc_setup(bd_t *bd, u8 bus_width, struct fs_mmc_cfg *cfg,
		 const struct fs_mmc_cd *cd);

#endif /* !__FS_MMC_COMMON_H__ */
