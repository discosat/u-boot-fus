#
# Copyright 2018 NXP
#
# SPDX-License-Identifier:      GPL-2.0+
#

obj-y += fsimx8mm.o

ifdef CONFIG_SPL_BUILD
obj-y += spl.o
obj-$(CONFIG_IMX8M_LPDDR4) += lpddr4_timing_k4f8e3s4hd_mgcl.o lpddr4_timing_k4f8e304hb_mgcj.o
obj-$(CONFIG_IMX8M_DDR3L) += ddr3l_timing_2x_mt41k128m16tw.o
obj-$(CONFIG_IMX8M_DDR3L) += ddr3l_timing_2x_im4g16d3fdbg107i.o
endif
