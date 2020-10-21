#
# Copyright 2018 NXP
#
# SPDX-License-Identifier:      GPL-2.0+
#

obj-y += fsimx8mx.o

ifdef CONFIG_SPL_BUILD
obj-y += spl.o
obj-$(CONFIG_IMX8M_DDR3L) +=ddr3l_timing_2x_mt41k128m16tw.o
obj-$(CONFIG_IMX8M_DDR3L) +=ddr3l_timing_2x_im4g16d3fdbg107i.o
endif
