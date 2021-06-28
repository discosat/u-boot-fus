/*
 * fs_lpddr4_common.h
 *
 * (C) Copyright 2021
 * Anatol Derksen, F&S Elektronik Systeme GmbH, derksen@fs-net.de
 *
 * Common lpddr4 ram functions
 *
 * SPDX-License-Identifier:	GPL-2.0+
 */

#ifndef __FS_LPDDR4_COMMON_H__
#define __FS_LPDDR4_COMMON_H__

#include <asm/arch/ddr.h>

struct lpddr4_info;

/* description of lpddr4 sdram chip */
struct lpddr4_info {
	/* manufacturer name */
	char name[20];
	/* manufacturer id [MR5..M8]
	 * 32-bit from high to low.
	 *  */
	unsigned int id;
	/* total density of one chip in byte */
	unsigned int total_density;
	/* memory number */
	unsigned int memory_number;
	/* dram timings - pointer to array with dram timings */
	struct dram_timing_info *dram_timings;
};

unsigned int fs_init_ram(void);
const struct lpddr4_info* get_dram_info(unsigned int config);

#endif /* !__FS_USB_COMMON_H__ */
