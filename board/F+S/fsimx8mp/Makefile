#
# Copyright 2021 F&S Elektronik Systeme GmbH
#
# SPDX-License-Identifier:      GPL-2.0+
#

obj-y += fsimx8mp.o

ifdef CONFIG_SPL_BUILD
obj-y += spl.o
obj-$(CONFIG_IMX8M_LPDDR4) += lpddr4_timing_k4f6e3s4hm_mgcj.o
obj-$(CONFIG_IMX8M_LPDDR4) += lpddr4_timing_k4f8e3s4hd_mgcj.o
obj-$(CONFIG_IMX8M_LPDDR4) += fs_lpddr4_common.o
endif

ifndef CONFIG_SPL_BUILD
obj-$(CONFIG_USB_TCPC) += ../../freescale/common/tcpc.o
endif
