#
#
# SPDX-License-Identifier:      GPL-2.0+
#

obj-y += fsimx8m.o

ifdef CONFIG_SPL_BUILD
obj-y += spl.o
ifdef CONFIG_IMX8M_LPDDR4
obj-$(CONFIG_IMX8M_LPDDR4) += lpddr4_timing_k4f6e2s4hm_mgcj.o lpddr4_timing_k4f8e3s4hd_mgcl.o
else
obj-y += ddr/ddr_init.o ddr/ddrphy_train.o ddr/helper.o
endif
endif

ifndef CONFIG_SPL_BUILD
obj-$(CONFIG_USB_TCPC) += ../../freescale/common/tcpc.o
endif
