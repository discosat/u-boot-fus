#
# (C) Copyright 2018
# Hartmut Keller, F&S Elektronik Systeme GmbH, keller@fs-net.de
#
# Common code for different F&S boards
#
# SPDX-License-Identifier:	GPL-2.0+
#

MINIMAL=

ifdef CONFIG_SPL_BUILD
ifdef CONFIG_SPL_INIT_MINIMAL
MINIMAL=y
endif
endif

ifdef MINIMAL
# necessary to create built-in.o
obj- := __dummy__.o
else
obj-$(CONFIG_FS_BOARD_COMMON)	+= fs_board_common.o
obj-$(CONFIG_FS_IMAGE_COMMON)	+= fs_image_common.o
obj-$(CONFIG_FS_DRAM_COMMON)	+= fs_dram_common.o
ifndef CONFIG_SPL_BUILD
obj-$(CONFIG_FS_FDT_COMMON)	+= fs_fdt_common.o
obj-$(CONFIG_FS_MMC_COMMON)	+= fs_mmc_common.o
obj-$(CONFIG_FS_ETH_COMMON)	+= fs_eth_common.o
obj-$(CONFIG_FS_USB_COMMON)	+= fs_usb_common.o
obj-$(CONFIG_FS_DISP_COMMON)	+= fs_disp_common.o
obj-$(CONFIG_USB_TCPC)		+= tcpc.o
endif
endif
