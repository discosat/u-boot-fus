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
obj-$(CONFIG_FS_FDT_COMMON)	+= fs_fdt_common.o
endif
