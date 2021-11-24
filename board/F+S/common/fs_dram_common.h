#ifndef FS_DRAM_COMMON_H
#define FS_DRAM_COMMON_H

#ifdef CONFIG_IMX8M
#include <asm/arch/imx8m_ddr.h>
#elif CONFIG_IMX8
#include <asm/arch/sci/sci.h>

enum DDR_CMD {
	CMD_NONE,
	CMD_INIT_DDR,
	CMD_PLL_REG,
	CMD_DELAY,
	CMD_DATA4,
	CMD_SET_BIT4,
	CMD_CLR_BIT4,
	CMD_CHECK_BITS_SET4,
	CMD_CHECK_BITS_CLR4,
	CMD_RUN_CBT,
	CMD_RDBI_BIT_DESKEW,
	CMD_DRAM_VREF_TRAINING_SW,
	CMD_DDRC_LPDDR4_DERATE_INIT,
	CMD_INITIALIZED,
};

struct dram_cfg_cmd {
	u32 cmd;
	u32 parm1;
	u32 parm2;
};

struct dram_timing_info {
	struct dram_cfg_cmd * ddrc_cfg;
	u32 ddrc_cfg_num;
};
#endif

int fs_dram_init_common(unsigned long * p);

#endif // FS_DRAM_COMMON_H
