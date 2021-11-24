#include "fs_dram_common.h"

#if defined(CONFIG_IMX8)
/* Helper function for ioctl with SCU */
int parse_ddr_cmd(u32 cmd, u32 parm1, u32 parm2)
{
	return sc_misc_board_ioctl(-1, &cmd, &parm1, &parm2);
}

/* Helper function to apply dram_timing */
int parse_dram_timing(struct dram_timing_info * pdram)
{
	int ret = 0;

	for (int i = 0; i < pdram->ddrc_cfg_num; i++)
		 ret |= parse_ddr_cmd(pdram->ddrc_cfg[i].cmd, pdram->ddrc_cfg[i].parm1, pdram->ddrc_cfg[i].parm2);

	return ret;
}
#endif

int fs_dram_init_common(unsigned long * p) {
	struct dram_timing_info *dti = (struct dram_timing_info *)*p;
	//dti = &dram_timing;
#if defined(CONFIG_IMX8M)
	return ddr_init(dti);
#elif defined(CONFIG_IMX8)
	return parse_dram_timing(dti);
#else
	return -1;
#endif
}
