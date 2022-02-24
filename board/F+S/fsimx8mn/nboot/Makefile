#
# Copyright 2020 F&S Elektronik Systeme GmbH
#
# SPDX-License-Identifier:      GPL-2.0+
#

include spl/include/autoconf.mk

# NBoot version with year and month. Please update for every release. If there
# needs to be more than one release within a month, add .1, .2, .3, ..., e.g.
# 2021.03.2 for the third release in March 2021.
NBOOT_VERSION = 2022.02

# List of files to add to BOARD-CONFIGS image. The file name depends on the
# names given in the Selectsoft WaWi for the board and the PCB revision. The
# format is:
#
#   <board>-FERT<n>.<rev>.fs
#
# <board>: Name of the board
# <n>:     Subtype of board (i.e. mounting variant)
# <rev>:   Revision number, typically the PCB revision 100, 110, 120, etc.
#
# Group by <board> and <n>, then within each group sort by *descending* <rev>
# number, i.e. add new revisions in front of the list. The first configuration
# where <board> and <n> match and where <rev> is less than or equal to the
# <rev> of the BOARD-ID is selected. This is because not every new board
# revision needs a new software configuration. And a board with a new board
# revision that does not have a its own configuration yet can at least be
# started with an older configuration.
#
# For example assume for a certain board type, there are BOARD-CFGs with
# revisions .130, .120 and .100. Boards with revision .121 or .120 in their
# BOARD-ID would both select .120, a board with .110 would select .100.
BOARD_CONFIGS = \
	PCoreMX8MN-FERT1.120.fs \
	PCoreMX8MN-FERT1.110.fs \
	PCoreMX8MN-FERT3.130.fs \
	PCoreMX8MN-FERT3.120.fs \
	PCoreMX8MN-FERT4.130.fs \
	PCoreMX8MN-FERT4.120.fs \
	PCoreMX8MN-FERT5.120.fs \
	PCoreMX8MN-FERT5.110.fs \
	PCoreMX8MN-FERT6.120.fs \
	PCoreMX8MN-FERT6.110.fs

# List of DRAM Timings for each DRAM-TYPE
RT_DDR3L := \
	ddr3l_k4b2g1646fbym_timing.fs \
	ddr3l_2x_k4b2g1646fbym_timing.fs \
	ddr3l_2x_mt41k128m16tw_timing.fs
RT_LPDDR4 := \
	lpddr4_k4f8e3s4hd_mgcl_timing.fs
RT_DDR4 :=

# List of DRAM-TYPES for DRAM-SETTINGS image; only add those that have timings
DRAM_TYPES :=
ifneq ($(RT_LPDDR4),)
DRAM_TYPES += lpddr4_type.fs
endif
ifneq ($(RT_DDR3L),)
DRAM_TYPES += ddr3l_type.fs
endif
ifneq ($(RT_DDR4),)
DRAM_TYPES += ddr4_type.fs
endif

# Prepend the DDR Training Firmware as an additional file before the timings
RT_DDR3L := ddr3l_fw.fs $(RT_DDR3L)
RT_LPDDR4 := lpddr4_fw.fs $(RT_LPDDR4)
RT_DDR4 := ddr4_fw.fs $(RT_DDR4)

# List of files to add to FIRMWARE image
FIRMWARE_FILES = $(BOARD)_dram_settings.fs $(BOARD)_atf.fs

# List of files to add to EXTRA image
EXTRA_FILES = addfsheader.sh.fs fsimage.sh.fs

# List of files to add to NBOOT image
NBOOT_FILES = $(BOARD)_spl.fs $(BOARD)_board_configs.fs \
	$(BOARD)_firmware.fs $(BOARD)_extra.fs

# Final NBOOT image
NBOOT = $(BOARD)_nboot.fs

ifeq ($(BOARD),fsimx8mn)
NANO = _201810
endif

# Add source tree path for board configurations
NBOOT_PATH = $(srctree)/board/$(BOARDDIR)/nboot
DTS_BOARD_CONFIGS = $(addprefix $(NBOOT_PATH)/, $(BOARD_CONFIGS:.fs,.dts))

# Add object path for F&S images
OBJ_BOARD_CONFIGS = $(addprefix $(obj)/, $(BOARD_CONFIGS))
OBJ_RT_DDR3L = $(addprefix $(obj)/, $(RT_DDR3L))
OBJ_RT_LPDDR4 = $(addprefix $(obj)/, $(RT_LPDDR4))
OBJ_RT_DDR4 = $(addprefix $(obj)/, $(RT_DDR4))
OBJ_DRAM_TYPES = $(addprefix $(obj)/, $(DRAM_TYPES))
OBJ_FIRMWARE_FILES = $(addprefix $(obj)/, $(FIRMWARE_FILES))
OBJ_EXTRA_FILES = $(addprefix $(obj)/, $(EXTRA_FILES))
OBJ_NBOOT_FILES = $(addprefix $(obj)/, $(NBOOT_FILES))
OBJ_NBOOT = $(addprefix $(obj)/, $(NBOOT))

# List of provided firmware files from NXP
NXP_DDR3L = ddr3_imem_1d.bin ddr3_dmem_1d.bin
NXP_LPDDR4 = lpddr4_pmu_train_1d_imem.bin lpddr4_pmu_train_1d_dmem.bin \
	lpddr4_pmu_train_2d_imem.bin lpddr4_pmu_train_2d_dmem.bin
NXP_DDR4 = ddr4_imem_1d$(NANO).bin ddr4_dmem_1d$(NANO).bin \
	ddr4_imem_2d$(NANO).bin ddr4_dmem_2d$(NANO).bin
NXP_ATF = bl31-imx8mn.bin

# Add path for NXP firmware images
NXP_PATH = $(srctree)/board/F+S/NXP-Firmware
NXP_PATH_DDR3L = $(addprefix $(NXP_PATH)/, $(NXP_DDR3L))
NXP_PATH_LPDDR4 = $(addprefix $(NXP_PATH)/, $(NXP_LPDDR4))
NXP_PATH_DDR4 = $(addprefix $(NXP_PATH)/, $(NXP_DDR4))
NXP_PATH_ATF = $(addprefix $(NXP_PATH)/, $(NXP_ATF))

# Concatenate two single files with padding: 32KB + x
quiet_cmd_dd2 = GROUP   $@
cmd_dd2 = dd if=$(word 1,$^) of=$@ bs=16k status=none; \
	dd if=$(word 2,$^) of=$@ bs=16k seek=2 conv=notrunc status=none

# Concatenate four single files with padding: 32KB + 16KB + 32KB + x
quiet_cmd_dd4 = GROUP   $@
cmd_dd4 = dd if=$(word 1,$^) of=$@ bs=16k status=none; \
	dd if=$(word 2,$^) of=$@ bs=16k seek=2 conv=notrunc status=none; \
	dd if=$(word 3,$^) of=$@ bs=16k seek=3 conv=notrunc status=none; \
	dd if=$(word 4,$^) of=$@ bs=16k seek=5 conv=notrunc status=none

.INTERMEDIATE: $(obj)/.ddr3l_fw.tmp $(obj)/.lpddr4_fw.tmp $(obj)/.ddr4_fw.tmp

$(obj)/.ddr3l_fw.tmp: $(NXP_PATH_DDR3L)
	$(call cmd,dd2)

$(obj)/.lpddr4_fw.tmp: $(NXP_PATH_LPDDR4)
	$(call cmd,dd4)

$(obj)/.ddr4_fw.tmp: $(NXP_PATH_DDR4)
	$(call cmd,dd4)

quiet_cmd_sed = SED     $@
cmd_sed = sed $2 $< > $@

#quiet_cmd_lds = LDS     $@
#cmd_lds = $(CPP) $(cpp_flags) -P -x c -o $@ $<
#cmd_lds = sed -e "s/###DRAM_TIMING_ADDR###/$(CONFIG_SPL_DRAM_TIMING_ADDR)/g" \
	$< > $@

DRAM_TIMINGS_LDS = $(obj)/.dram-timings.lds.tmp
DRAM_TIMINGS_REPLACE_SED_CMD = \
	-e "s/\#\#\#DRAM_TIMING_ADDR\#\#\#/$(CONFIG_SPL_DRAM_TIMING_ADDR)/g"

$(DRAM_TIMINGS_LDS): $(NBOOT_PATH)/dram-timings.lds ${srctree}/include/configs/$(BOARD).h
	$(call cmd,sed,$(DRAM_TIMINGS_REPLACE_SED_CMD))

quiet_cmd_dram_elf = LINK    $@
cmd_dram_elf = $(LD) -T $(DRAM_TIMINGS_LDS) -o $@ $<
#cmd_dram_elf = $(LD) -T $(DRAM_TIMINGS_LDS) -Map $*.map -o $@ $<

$(obj)/.%.elf.tmp: $(obj)/%.o $(DRAM_TIMINGS_LDS)
	$(call cmd,dram_elf)

quiet_cmd_dram_bin = OBJCOPY $@
cmd_dram_bin = $(OBJCOPY) --gap-fill=0xff -O binary $< $@

$(obj)/.%.bin.tmp: $(obj)/.%.elf.tmp
	$(call cmd,dram_bin)

quiet_cmd_fsimg = FSIMG   $@
cmd_fsimg = ${srctree}/scripts/addfsheader.sh -q -p 16 $2 $^ > $@

$(obj)/%_timing.fs: $(obj)/.%_timing.bin.tmp
	$(call cmd,fsimg,-t DRAM-TIMING -d $(*F))

$(obj)/%_fw.fs: $(obj)/.%_fw.tmp
	$(call cmd,fsimg,-t DRAM-FW -d $(*F))

$(obj)/ddr3l_type.fs: $(OBJ_RT_DDR3L)
$(obj)/lpddr4_type.fs: $(OBJ_RT_LPDDR4)
$(obj)/ddr4_type.fs: $(OBJ_RT_DDR4)

%_type.fs:
	$(call cmd,fsimg,-t DRAM-TYPE -d $(*F))

%_dram_settings.fs: $(OBJ_DRAM_TYPES)
	$(call cmd,fsimg,-t DRAM-SETTINGS -d $(*F))

%_atf.fs: $(NXP_PATH_ATF)
	$(call cmd,fsimg,-t ATF -d $(*F))

%_firmware.fs: $(OBJ_FIRMWARE_FILES)
	$(call cmd,fsimg,-t FIRMWARE -d $(*F))

%_spl.fs: SPL
	$(call cmd,fsimg,-t SPL -d $(*F))

REPLACE_NAND_START = $(CONFIG_FUS_BOARDCFG_NAND0) $(CONFIG_FUS_BOARDCFG_NAND1)
REPLACE_MMC_START = $(CONFIG_FUS_BOARDCFG_MMC0) $(CONFIG_FUS_BOARDCFG_MMC1)
NBOOT_INFO_REPLACE_SED_CMD = \
	-e "s/\#\#\#NAND_START\#\#\#/$(REPLACE_NAND_START)/g" \
	-e "s/\#\#\#MMC_START\#\#\#/$(REPLACE_MMC_START)/g" \
	-e "s/\#\#\#NBOOT_VERSION\#\#\#/$(NBOOT_VERSION)/g"

$(obj)/.nboot-info-%.dtsi.tmp: $(NBOOT_PATH)/nboot-info-%.dtsi
	$(call cmd,sed,$(NBOOT_INFO_REPLACE_SED_CMD))
#	cat $@

quiet_cmd_dtc = DTC     $@
cmd_dtc = $(DTC) -I dts -O dtb -a 16 -o $@ $<

NBOOT_INFO = $(obj)/.nboot-info-nand.dtsi.tmp $(obj)/.nboot-info-mmc.dtsi.tmp
$(obj)/.%.dtb.tmp: $(NBOOT_PATH)/%.dts $(NBOOT_INFO)
	$(call cmd,dtc)

$(obj)/%.fs: $(obj)/.%.dtb.tmp
	$(call cmd,fsimg,-t BOARD-CFG -d $(*F))

%_board_configs.fs: $(OBJ_BOARD_CONFIGS)
	$(call cmd,fsimg,-t BOARD-CONFIGS -d $(*F))

#./scripts/dtc/dtc -O dtb -o arch/arm/dts/picocoremx8mn.dtb -b 0 -i
#	arch/arm/dts/  -Wno-unit_address_vs_reg -Wno-simple_bus_reg
#	-Wno-unit_address_format -Wno-pci_bridge
#	-Wno-pci_device_bus_num -Wno-pci_device_reg  -d
#	arch/arm/dts/.picocoremx8mn.dtb.d.dtc.tmp
#	arch/arm/dts/.picocoremx8mn.dtb.dts.tmp
#	$(call cmd,fsimg)

$(obj)/addfsheader.sh.fs: $(srctree)/scripts/addfsheader.sh
$(obj)/fsimage.sh.fs: $(srctree)/scripts/fsimage.sh
%.sh.fs:
	$(call cmd,fsimg,-t BASH-SCRIPT -d $(*F))

%_extra.fs: $(OBJ_EXTRA_FILES)
	$(call cmd,fsimg,-t EXTRA -d $(*F))

nboot-$(BOARD)-$(NBOOT_VERSION).fs: $(OBJ_NBOOT_FILES)
	$(call cmd,fsimg,-t NBOOT -d $(BOARD))

PHONY += nboot
nboot: nboot-$(BOARD)-$(NBOOT_VERSION).fs
	@echo "Finished doing nboot"
