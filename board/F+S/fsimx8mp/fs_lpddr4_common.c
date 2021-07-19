/*
 * fs_lpddr4_common.c
 *
 * (C) Copyright 2021
 * Anatol Derksen, F&S Elektronik Systeme GmbH, derksen@fs-net.de
 *
 * Common lpddr4 ram functions
 *
 * SPDX-License-Identifier:	GPL-2.0+
 */


#include <common.h>
#include <errno.h>
#include <asm/io.h>
#include <asm/arch/ddr.h>
#include <asm/arch/clock.h>
#include <asm/arch/ddr.h>
#include <asm/arch/lpddr4_define.h>
#include <asm/arch/sys_proto.h>
#include "fs_lpddr4_common.h"

/* use from ddrphy_utils.c */
extern unsigned int lpddr4_mr_read(unsigned int mr_rank, unsigned int mr_addr);

/* generated dram timings */
extern struct dram_timing_info dram_timing_k4f6e3s4hm_mgcj;
extern struct dram_timing_info dram_timing_k4f8e3s4hd_mgcj;
extern struct dram_timing_info dram_timing_fl4c2001g_d9;

/* Description table of different lpddr4 sdram chips */
static const struct lpddr4_info lpddr4_table[] = {
	{
			.name = "Samsung",
			.id = 0x01061010,
			.total_density = 0x80000000,
			.memory_number = 1,
			.dram_timings = &dram_timing_k4f6e3s4hm_mgcj
	},
	{
			.name = "Samsung",
			.id = 0x01060008,
			.total_density = 0x40000000,
			.memory_number = 1,
			.dram_timings = &dram_timing_k4f8e3s4hd_mgcj
	},
	{
			.name = "Longsys",
			.id = 0x01051108,
			.total_density = 0x40000000,
			.memory_number = 1,
			.dram_timings = &dram_timing_fl4c2001g_d9
	},
};

/* Get pointer to a descriptor for given mr basic configuration value.
 * configuration format -> MR5..MR8
 */
const struct lpddr4_info* get_dram_info(unsigned int config)
{
	int i = ARRAY_SIZE(lpddr4_table);

	do {
		i--;
		if(config == lpddr4_table[i].id)
			return (const struct lpddr4_info*)&lpddr4_table[i];
	}while (i);

	return 0;
}

/* get lpddr4 basic configuration registers MR5-MR8*/
static unsigned int get_lpddr4_mr_bc(void)
{
	unsigned int value = 0;
	unsigned int config = 0;

	value = lpddr4_mr_read(0x3, 5);
	config |= (value & 0xFF);

	value = lpddr4_mr_read(0x3, 6);
	config <<= 8;
	config |= (value & 0xFF);

	value = lpddr4_mr_read(0x3, 7);
	config <<= 8;
	config |= (value & 0xFF);

	value = lpddr4_mr_read(0x3, 8);
	config <<= 8;
	config |= (value & 0xFF);

	debug("MR basic configuration: 0x%x \n", config);

	return config;
}

/* Initialize LPDDR4 SDRAM with the suitable descriptor */
unsigned int fs_init_ram(void)
{
	unsigned int i, mr_bc=0;
	unsigned int lpddr4_info_size =  ARRAY_SIZE(lpddr4_table);

	for(i=0; i < lpddr4_info_size; i++)
	{
		if(ddr_init(lpddr4_table[i].dram_timings))
			continue;

		mr_bc = get_lpddr4_mr_bc();
		if(mr_bc == lpddr4_table[i].id)
			break;
	}

	if(i >= lpddr4_info_size)
		return 0;

	return mr_bc;
}
