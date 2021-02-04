/* SPDX-License-Identifier: GPL-2.0+ */
/*
 * Copyright (C) 2020 F&S Elektronik Systeme GmbH
 *
 * Configuration settings for all F&S boards based on i.MX8MM. This is
 * PicoCoreMX8MM.
 *
 * Activate with one of the following targets:
 *   make fsimx8mm_defconfig   Configure for i.MX8MM boards
 *   make                      Build uboot-spl.bin, u-boot.bin and
 *                             u-boot-nodtb.bin.
 *
 *
 * ====== Snip from here ====
 * Layout for i.MX8M (with only 128KB of OCRAM)
 *
 *
 * TCM layout (SPL)
 * ----------------
 * 0x007E_0000: --- (4KB, unused)
 * 0x007E_1000: SPL, address defined by ATF (up to ~120KB)
 *     DRAM_FW: Training Firmware (up to 96KB, immediately behind end of SPL)
 * 0x0081_7000: MALLOC_F pool (36KB, CONFIG_SPL_SYS_MALLOC_F_LEN)  ### Das geht so nicht!!!!!
 * 0x0081_FFFF: END
 *
 * The sum of SPL and DDR_FW must not exceed 216KB (unless the MALLOC_F pool
 * can be shrinked).
 *
 * OCRAM layout (SPL)
 * ------------------
 * 0x0090_0000: --- (64KB, reserved by ROM loader)
 * 0x0091_0000: ATF (48KB)
 * 0x0091_C000: DRAM Timing Data (16KB)
 * 0x0091_FFFF: END
 *
 * OCRAM_S layout (SPL)
 * --------------------
 * 0x0018_0000: BSS data of SPL (8KB)
 * 0x0018_2000: Board-Config (8KB)
 * 0x0018_4000: Stack + Global Data (16KB)
 * 0x0018_7FFF: End
 *
 * ====== Snip to here ====
 *
 * TCM layout (SPL)
 * ----------------
 * 0x007E_0000: --- (4KB, unused)
 * 0x007E_1000: SPL, address defined by ATF (up to ~120KB)
 *     DRAM_FW: Training Firmware (up to 96KB, immediately behind end of SPL)
 * 0x0081_E000: --- (unused) ### why?
 * 0x0081_FFFF: END
 *
 * The sum of SPL and DDR_FW must not exceed 216KB (unless the MALLOC_F pool
 * can be shrinked).
 *
 * OCRAM layout (SPL)
 * ------------------
 * 0x0090_0000: --- (64KB, reserved by ROM loader)
 * 0x0091_0000: BSS data of SPL (8KB)
 * 0x0091_2000: Board-Config (8KB)
 * 0x0091_4000: MALLOC_F pool (28KB)
 * 0x0091_B000: --- (free)
 * 0x0091_C000: Stack + Global Data (16KB)
 * 0x0092_0000: ATF (48KB)
 * 0x0092_C000: DRAM Timing Data (16KB)
 * 0x0093_FFFF: End
 *
 * OCRAM_S layout (SPL)
 * --------------------
 * 0x0018_0000: Copy of DRAM configuration (passed to ATF)(~16KB)
 * 0x0018_4000: --- (free)
 * 0x0018_7FFF: End
 *
 * After DRAM is available, SPL uses a MALLOC_R pool at 0x4220_0000.
 *
 * Later, ATF is loaded to OCRAM at 0x0092_0000 and U-Boot is loaded to DRAM
 * at 0x4020_0000. If a TEE program is loaded, it has to go to 0xBE00_0000 and
 * a DEK_BLOB is loaded to 0x4040_0000. These addresses are defined in ATF.
 *
 * NAND flash layout
 * -------------------------------------------------------------------------
---(Actually now, as written from NXP tool kobs)
 * 0x0000_0000 - 0x0001_FFFF: FCB Copy 0 (128KB)
 * 0x0002_0000 - 0x0003_FFFF: FCB Copy 1 (128KB)
 * 0x0004_0000 - 0x0005_FFFF: FCB Copy 2 (128KB)
 * 0x0006_0000 - 0x0007_FFFF: FCB Copy 3 (128KB)
 * 0x0008_0000 - 0x0009_FFFF: DBBT Copy 0 (128KB)
 * 0x000A_0000 - 0x000B_FFFF: DBBT Copy 1 (128KB)
 * 0x000C_0000 - 0x000D_FFFF: DBBT Copy 2 (128KB)
 * 0x000E_0000 - 0x000F_FFFF: DBBT Copy 3 (128KB)
 * 0x0010_0000 - 0x0013_FFFF: SPL Copy 0 (256KB)
 * 0x0014_0000 - 0x0017_FFFF: HDMI-FW Copy 0 (256KB, unused but written)
 * 0x0018_0000 - 0x0027_FFFF: --- (1024KB, unused, not written) -> BOARD-CFG
 * 0x0028_0000 - 0x002B_FFFF: SPL Copy 1 (256KB)
 * 0x002C_0000 - 0x002F_FFFF: HDMI-FW Copy 1 (256KB, unused but written)
 * 0x0030_0000 - 0x003F_FFFF: --- (1024KB, unused, not written) -> FIRMWARES
 * 0x0040_0000 - 0x007F_FFFF: UBoot + Reserve (4MB)
 * 0x0080_0000 - 0x0083_FFFF: UBootEnv (256KB)
 * 0x0084_0000 - 0x0283_FFFF: Kernel (32MB)
 * 0x0284_0000 - 0x029F_FFFF: FDT (1792KB)
 * 0x02A0_0000 -         END: TargetFS as UBI Volumes
---(Planned)
 * 0x0000_0000 - 0x0001_FFFF: BCB Copy 0 (FCB+DBBT) (128KB)         \
 * 0x0002_0000 - 0x0003_FFFF: BCB Copy 1 (FCB+DBBT) (128KB)          |
 * 0x0004_0000 - 0x0007_FFFF: SPL Copy 0 (256KB)                     |
 * 0x0008_0000 - 0x000B_FFFF: HDMI-FW Copy 0 (256KB) (unused)        |
 * 0x000C_0000 - 0x000F_FFFF: SPL Copy 1 (256KB)                     |
 * 0x0010_0000 - 0x0013_FFFF: HDMI-FW Copy 1 (256KB) (unused)        | NBoot
 * 0x0014_0000 - 0x0017_FFFF: Reserve in case of bad blocks (256KB)  |
 * 0x0018_0000 - 0x0019_FFFF: Board-Config Copy 0 (128KB)            |
 * 0x001A_0000 - 0x001B_FFFF: Board-Config Copy 1 (128KB)            |
 * 0x001C_0000 - 0x001F_FFFF: Reserve in case of bad blocks (256KB)  |
 * 0x0020_0000 - 0x002F_FFFF: ATF/DDR Copy 0 + Reserve (1024KB)      |
 * 0x0030_0000 - 0x003F_FFFF: ATF/DDR Copy 1 + Reserve (1024KB)     /
 * 0x0040_0000 - 0x0057_FFFF: UserDef (1536KB)
 * 0x0058_0000 - 0x005F_FFFF: Refresh (512KB)
 * 0x0060_0000 - 0x0087_FFFF: UBoot + Reserve (2560KB)              \
 * 0x0088_0000 - 0x008B_FFFF: UBootEnv + Reserve (256KB)             |
 * 0x008C_0000 - 0x008F_FFFF: UBootEnvRed + Reserve (256KB)          | Set A
 * 0x0090_0000 - 0x028F_FFFF: Kernel (32MB)                          |
 * 0x0290_0000 - 0x029F_FFFF: FDT (1MB)                             /
 * 0x02A0_0000 - 0x04DF_FFFF: UBoot + UBootEnv + Kernel + FDT (opt)    Set B
 * 0x04E0_0000 -         END: TargetFS as UBI Volumes                  Set A+B
---
 *
 * Remarks:
 * - If Kernel and FDT are part of the Rootfs, these partitions are dropped
 * - If no Update with Set A and B is used, all Set B partitions are dropped
 * - On i.MX8MM, no HDMI is available, so Copy 1 of SPL directly follows Copy 0
 */

#ifndef __FSIMX8MM_H
#define __FSIMX8MM_H

#include <linux/sizes.h>
#include <asm/arch/imx-regs.h>

#include "imx_env.h"

/* disable FAT write becaue its dosn't work
 *  with F&S FAT driver
 */
#undef CONFIG_FAT_WRITE

/* need for F&S bootaux */
#define M4_BOOTROM_BASE_ADDR           MCU_BOOTROM_BASE_ADDR
#define IMX_SIP_SRC_M4_START           IMX_SIP_SRC_MCU_START
#define IMX_SIP_SRC_M4_STARTED         IMX_SIP_SRC_MCU_STARTED

#ifdef CONFIG_SECURE_BOOT
#define CONFIG_CSF_SIZE			0x2000 /* 8K region */
#endif

#ifdef CONFIG_NAND_BOOT
#define CONFIG_CMD_NAND
#endif

#define CONFIG_SYS_SERCON_NAME "ttymxc"	/* Base name for serial devices */
#define CONFIG_SYS_UART_PORT	0	/* Default UART port */
#define CONFIG_CONS_INDEX       (CONFIG_SYS_UART_PORT)

/*####define CONFIG_SPL_MAX_SIZE		(148 * 1024)*/
#define CONFIG_SPL_MAX_SIZE		(132 * 1024)
#define CONFIG_SYS_MONITOR_LEN		(512 * 1024)
#define CONFIG_SYS_MMCSD_RAW_MODE_U_BOOT_USE_SECTOR
#define CONFIG_SYS_MMCSD_RAW_MODE_U_BOOT_SECTOR	0x300
#define CONFIG_SYS_MMCSD_FS_BOOT_PARTITION	1
#define CONFIG_SYS_UBOOT_BASE		(QSPI0_AMBA_BASE + CONFIG_SYS_MMCSD_RAW_MODE_U_BOOT_SECTOR * 512)

/* The final stack sizes are set up in board.c using the settings below */
#define CONFIG_SYS_STACK_SIZE	(128*1024)

#ifdef CONFIG_SPL_BUILD
/*#define CONFIG_ENABLE_DDR_TRAINING_DEBUG*/
#define CONFIG_SPL_WATCHDOG_SUPPORT
#define CONFIG_SPL_POWER_SUPPORT
#define CONFIG_SPL_DRIVERS_MISC_SUPPORT
#define CONFIG_SPL_I2C_SUPPORT
#define CONFIG_SPL_LDSCRIPT		"arch/arm/cpu/armv8/u-boot-spl.lds"
#define CONFIG_SPL_STACK		0x91fff0
#define CONFIG_SPL_LIBCOMMON_SUPPORT
#define CONFIG_SPL_LIBGENERIC_SUPPORT
#define CONFIG_SPL_SERIAL_SUPPORT
#define CONFIG_SPL_GPIO_SUPPORT
#define CONFIG_SPL_BSS_START_ADDR      0x00910000
#define CONFIG_SPL_BSS_MAX_SIZE        0x2000	/* 8 KB */
#define CONFIG_SYS_SPL_MALLOC_START    0x42200000
#define CONFIG_SYS_SPL_MALLOC_SIZE     0x80000	/* 512 KB */
#define CONFIG_SYS_ICACHE_OFF
#define CONFIG_SYS_DCACHE_OFF

#define CONFIG_SPL_USE_ATF_ENTRYPOINT
#define CONFIG_SPL_ATF_ADDR 0x920000
#define CONFIG_SPL_TEE_ADDR 0xbe000000

#define CONFIG_SPL_DRAM_TIMING_ADDR (CONFIG_SPL_ATF_ADDR + 0xc000)
#define CONFIG_SPL_BOARDCFG_ADDR \
	(CONFIG_SPL_BSS_START_ADDR + CONFIG_SPL_BSS_MAX_SIZE)

#define CONFIG_SPL_BOARDCFG_NAND_OFFSET 0x180000
#define CONFIG_SPL_FIRMWARE_NAND_OFFSET 0x300000


/* malloc_f is used before GD_FLG_FULL_MALLOC_INIT set, locate at end of TCM */
/* ###
#define CONFIG_MALLOC_F_ADDR \
	(0x820000 - CONFIG_SPL_SYS_MALLOC_F_LEN)
###*/
#define CONFIG_MALLOC_F_ADDR 0x914000

#define CONFIG_SPL_ABORT_ON_RAW_IMAGE /* For RAW image gives a error info not panic */

#define CONFIG_I2C_SUPPORT
#undef CONFIG_DM_MMC
#undef CONFIG_DM_PMIC
#undef CONFIG_DM_PMIC_PFUZE100

#define CONFIG_POWER
#define CONFIG_POWER_I2C
#define CONFIG_POWER_BD71837
/* #define CONFIG_DM_PMIC_BD71837 */

#define CONFIG_SYS_I2C
#define CONFIG_SYS_I2C_MXC_I2C1		/* enable I2C bus 0 */
#define CONFIG_SYS_I2C_MXC_I2C2		/* enable I2C bus 1 */
#define CONFIG_SYS_I2C_MXC_I2C3		/* enable I2C bus 2 */
#define CONFIG_SYS_I2C_MXC_I2C4		/* enable I2C bus 3 */

#define CONFIG_ENV_VARS_UBOOT_RUNTIME_CONFIG

#if defined(CONFIG_NAND_BOOT)
#define CONFIG_SPL_NAND_SUPPORT
#define CONFIG_SPL_DMA_SUPPORT
#define CONFIG_SPL_NAND_MXS
#define CONFIG_SYS_NAND_U_BOOT_OFFS 	0x00400000 /* Put the FIT out of first 4MB boot area */

/* Set a redundant offset in nand FIT mtdpart. The new uuu will burn full boot image (not only FIT part) to the mtdpart, so we check both two offsets */
#define CONFIG_SYS_NAND_U_BOOT_OFFS_REDUND				\
	(CONFIG_SYS_NAND_U_BOOT_OFFS + CONFIG_SYS_MMCSD_RAW_MODE_U_BOOT_SECTOR * 512 - 0x8400)

#endif

#endif

/* Add F&S update */
#define CONFIG_CMD_UPDATE
#define CONFIG_CMD_READ
#define CONFIG_SERIAL_TAG
#define CONFIG_FASTBOOT_USB_DEV 0

#define CONFIG_REMAKE_ELF

#define CONFIG_BOARD_EARLY_INIT_F
#define CONFIG_BOARD_POSTCLK_INIT
#define CONFIG_BOARD_LATE_INIT

/* Flat Device Tree Definitions */
#define CONFIG_OF_BOARD_SETUP

#undef CONFIG_CMD_EXPORTENV
#undef CONFIG_CMD_IMPORTENV
#undef CONFIG_CMD_IMLS

#undef CONFIG_CMD_CRC32
#undef CONFIG_BOOTM_NETBSD

/* ENET Config */
/* ENET1 */
#define CONFIG_SYS_DISCOVER_PHY

#if defined(CONFIG_CMD_NET)
#define CONFIG_CMD_PING
#define CONFIG_CMD_DHCP
#define CONFIG_CMD_MII
#define CONFIG_MII
#define CONFIG_ETHPRIME                 "FEC"
#define FDT_SEQ_MACADDR_FROM_ENV

#define CONFIG_FEC_MXC
#define CONFIG_FEC_XCV_TYPE             RGMII
/* #define CONFIG_FEC_MXC_PHYADDR          4 */
#define FEC_QUIRK_ENET_MAC

#define CONFIG_PHY_GIGE
#define IMX_FEC_BASE			0x30BE0000

#define CONFIG_PHY_ATHEROS
#define CONFIG_PHY_NATSEMI
#define CONFIG_SYS_FAULT_ECHO_LINK_DOWN
#define CONFIG_LIB_RAND
#define CONFIG_NET_RANDOM_ETHADDR
#define CONFIG_NETMASK		255.255.255.0
#define CONFIG_IPADDR		10.0.0.252
#define CONFIG_SERVERIP		10.0.0.122
#define CONFIG_GATEWAYIP	10.0.0.5
#define CONFIG_ROOTPATH		"/rootfs"

#endif



#define CONFIG_BOOTFILE		"Image"
#define CONFIG_PREBOOT
#ifdef CONFIG_FS_UPDATE_SUPPORT
	#define CONFIG_BOOTCOMMAND	"run selector; run set_bootargs; run kernel; run fdt; run failed_update_reset"
#else
	#define CONFIG_BOOTCOMMAND	"run set_bootargs; run kernel; run fdt"
#endif

/************************************************************************
 * Generic MTD Settings
 ************************************************************************/
#ifdef CONFIG_FS_UPDATE_SUPPORT
	#ifdef CONFIG_NAND_BOOT
		/* Define MTD partition info */
		#define MTDIDS_DEFAULT      "nand0=gpmi-nand"
		#define MTDPART_DEFAULT     "nand0,1"
		#define MTDPARTS_PART1      "gpmi-nand:4m(Spl)"
		#define MTDPARTS_PART2	    "4m(UBootA),512k(UBootEnv)"
		#define MTDPARTS_PART3	    "4m(UBootB)"
		#define MTDPARTS_PART4	    "32m(KernelA),32m(KernelB)"
		#define MTDPARTS_PART5	    "1792k(FDTA),1792k(FDTB)"
		#define MTDPARTS_PART6	    "-(TargetFS)"
		#define MTDPARTS_DEFAULT    "mtdparts=" MTDPARTS_PART1 "," MTDPARTS_PART2 "," MTDPARTS_PART3 "," MTDPARTS_PART4 "," MTDPARTS_PART5 "," MTDPARTS_PART6
		#define MTDPARTS_STD	    "setenv mtdparts " MTDPARTS_DEFAULT

	#else
		#define MTDIDS_DEFAULT		""
		#define MTDPART_DEFAULT		""
		#define MTDPARTS_STD		"setenv mtdparts "
	#endif
	#define EXTRA_UBI
#else
	#ifdef CONFIG_NAND_BOOT
		/* Define MTD partition info */
		#define MTDIDS_DEFAULT      "nand0=gpmi-nand"
		#define MTDPART_DEFAULT     "nand0,1"
		#define MTDPARTS_PART1      "gpmi-nand:4m(Spl)"
		#define MTDPARTS_PART2	    "4m(UBoot),256k(UBootEnv)"
		#define MTDPARTS_PART3	    "32m(Kernel)ro,1792k(FDT)ro"
		#define MTDPARTS_PART4	    "-(TargetFS)"
		#define MTDPARTS_DEFAULT    "mtdparts=" MTDPARTS_PART1 "," MTDPARTS_PART2 "," MTDPARTS_PART3 "," MTDPARTS_PART4
		#define MTDPARTS_STD	    "setenv mtdparts " MTDPARTS_DEFAULT
		#define MTDPARTS_UBIONLY    "setenv mtdparts mtdparts=" MTDPARTS_PART1 "," MTDPARTS_PART2 "," MTDPARTS_PART4
	#else
		#define MTDIDS_DEFAULT		""
		#define MTDPART_DEFAULT		""
		#define MTDPARTS_STD		"setenv mtdparts "
		#define MTDPARTS_UBIONLY	"setenv mtdparts "
	#endif

	#ifdef CONFIG_CMD_UBI
	#ifdef CONFIG_CMD_UBIFS
	#define EXTRA_UBIFS							\
		".kernel_ubifs=setenv kernel ubi part TargetFS\\\\; ubifsmount ubi0:rootfs\\\\; ubifsload . /boot/${bootfile}\0" \
		".fdt_ubifs=setenv fdt ubi part TargetFS\\\\; ubifsmount ubi0:rootfs\\\\; ubifsload ${fdtaddr} /boot/${bootfdt}" BOOT_WITH_FDT
	#else
	#define EXTRA_UBIFS
	#endif
	#define EXTRA_UBI EXTRA_UBIFS						\
		".mtdparts_ubionly=" MTDPARTS_UBIONLY "\0"			\
		".rootfs_ubifs=setenv rootfs rootfstype=ubifs ubi.mtd=TargetFS root=ubi0:rootfs\0" \
		".kernel_ubi=setenv kernel ubi part TargetFS\\\\; ubi read . kernel\0" \
		".fdt_ubi=setenv fdt ubi part TargetFS\\\\; ubi read ${fdtaddr} fdt" BOOT_WITH_FDT \
		".ubivol_std=ubi part TargetFS; ubi create rootfs\0"		\
		".ubivol_ubi=ubi part TargetFS; ubi create kernel 800000 s; ubi create rootfs\0"
	#else
	#define EXTRA_UBI
	#endif
#endif

/* Add some variables that are not predefined in U-Boot. For example set
   fdt_high to 0xffffffff to avoid that the device tree is relocated to the
   end of memory before booting, which is not necessary in our setup (and
   would result in problems if RAM is larger than ~1,7GB).

   All entries with content "undef" will be updated in board_late_init() with
   a board-specific value (detected at runtime).

   We use ${...} here to access variable names because this will work with the
   simple command line parser, who accepts $(...) and ${...}, and also the
   hush parser, who accepts ${...} and plain $... without any separator.

   If a variable is meant to be called with "run" and wants to set an
   environment variable that contains a ';', we can either enclose the whole
   string to set in (escaped) double quotes, or we have to escape the ';' with
   a backslash. However when U-Boot imports the environment from NAND into its
   hash table in RAM, it interprets escape sequences, which will remove a
   single backslash. So we actually need an escaped backslash, i.e. two
   backslashes. Which finally results in having to type four backslashes here,
   as each backslash must also be escaped with a backslash in C. */
#define BOOT_WITH_FDT "\\\\; booti ${loadaddr} - ${fdtaddr}\0"

#ifdef CONFIG_BOOTDELAY
#define FSBOOTDELAY
#else
#define FSBOOTDELAY "bootdelay=undef\0"
#endif

#if defined(CONFIG_ENV_IS_IN_MMC)
	#define FILSEIZE2BLOCKCOUNT "block_size=200\0" 	\
		"filesize2blockcount=" \
			"setexpr test_rest \\${filesize} % \\${block_size}; " \
			"if test \\${test_rest} = 0; then " \
				"setexpr blockcount \\${filesize} / \\${block_size}; " \
			"else " \
				"setexpr blockcount \\${filesize} / \\${block_size}; " \
				"setexpr blocckount \\${blockcount} + 1; " \
			"fi;\0"
#else
	#define FILSEIZE2BLOCKCOUNT
#endif

#if defined(CONFIG_ENV_IS_IN_NAND)
#define NAND_BOOT_VALUES "rootfs_ubi_number=0\0" \
	"fdt_partition=FDTA\0"
#else
#define NAND_BOOT_VALUES
#endif


#ifdef CONFIG_FS_UPDATE_SUPPORT

	#define FS_UPDATE_SUPPORT "BOOT_ORDER=A B\0" \
	"BOOT_ORDER_OLD=A B\0" \
	"BOOT_A_LEFT=3\0" \
	"BOOT_B_LEFT=3\0" \
	"update_reboot_state=0\0" \
	"update=0000\0" \
	"application=A\0" \
	"rauc_cmd=rauc.slot=A\0" \
	"selector=undef\0" \
	".selector_mmc=setenv selector " \
	"'if test \"x${BOOT_ORDER_OLD}\" != \"x${BOOT_ORDER}\"; then	"														\
		"setenv rauc_cmd undef; "																							\
		"for BOOT_SLOT in \"${BOOT_ORDER}\"; do "																			\
		  "if test \"x${BOOT_SLOT}\" = \"xA\" && test ${BOOT_A_LEFT} -gt 0 && test \"x${rauc_cmd}\" = \"xundef\"; then "	\
			  "echo \"Current rootfs boot_partition is A\"; "																\
			  "setexpr BOOT_A_LEFT ${BOOT_A_LEFT} - 1; "																	\
			  "setenv boot_partition 5;"																					\
			  "setenv rootfs_partition 7;"																					\
			  "setenv rauc_cmd rauc.slot=A;"																				\
		  "elif test \"x${BOOT_SLOT}\" = \"xB\" && test ${BOOT_B_LEFT} -gt 0 && test \"x${rauc_cmd}\" = \"xundef\"; then "	\
			  "echo \"Current rootfs boot_partition is B\"; "																\
			  "setexpr BOOT_B_LEFT ${BOOT_B_LEFT} - 1; "																	\
			  "setenv boot_partition 6;"																					\
			  "setenv rootfs_partition 8;"																					\
			  "setenv rauc_cmd rauc.slot=B;"																				\
		  "fi;"																												\
		"done;"																												\
		"saveenv;"																											\
	"fi;'\0"																												\
	".selector_nand=setenv selector " 																						\
	"'if test \"x${BOOT_ORDER_OLD}\" != \"x${BOOT_ORDER}\"; then	"														\
		"setenv rauc_cmd undef; "																							\
		"for BOOT_SLOT in \"${BOOT_ORDER}\"; do "																			\
		  "if test \"x${BOOT_SLOT}\" = \"xA\" && test ${BOOT_A_LEFT} -gt 0 && test \"x${rauc_cmd}\" = \"xundef\"; then "	\
			  "echo \"Current rootfs boot_partition is A\"; "																\
			  "setexpr BOOT_A_LEFT ${BOOT_A_LEFT} - 1; "																	\
			  "setenv boot_partition KernelA;"																				\
			  "setenv fdt_partition FDTA;"																					\
			  "setenv rootfs_partition rootfsA;"																			\
			  "setenv rootfs_ubi_number 0;"																					\
			  "setenv rauc_cmd rauc.slot=A;"																				\
		  "elif test \"x${BOOT_SLOT}\" = \"xB\" && test ${BOOT_B_LEFT} -gt 0 && test \"x${rauc_cmd}\" = \"xundef\"; then "	\
			  "echo \"Current rootfs boot_partition is B\"; "																\
			  "setexpr BOOT_B_LEFT ${BOOT_B_LEFT} - 1; "																	\
			  "setenv boot_partition KernelB;"																				\
			  "setenv rootfs_partition rootfsB;"																			\
			  "setenv fdt_partition FDTB;"																					\
			  "setenv rootfs_ubi_number 1;"																					\
			  "setenv rauc_cmd rauc.slot=B;"																				\
		  "fi;"																												\
		"done;"																												\
		"saveenv;"																											\
	"fi;'\0"																												\
	"failed_update_reset="																									\
		"if test \"x${BOOT_ORDER_OLD}\" != \"x${BOOT_ORDER}\"; then	"														\
			"reset; "																										\
		"fi;\0"																											\
	NAND_BOOT_VALUES \
	"boot_partition=undef\0" \
	".boot_partition_mmc= setenv boot_partition 5\0"\
	".boot_partition_nand= setenv boot_partition KernelA\0"\
	"rootfs_partition=undef\0"\
	".rootfs_partition_mmc=setenv rootfs_partition 7\0"\
	".rootfs_partition_nand=setenv rootfs_partition rootfsA\0"

	#define ROOTFS_MEM 	".rootfs_mmc=setenv rootfs root=/dev/mmcblk${mmcdev}p\\\\${boot_partition} rootwait\0"
	#define KERNEL_MEM 	".kernel_nand=setenv kernel nand read ${loadaddr} \\\\${boot_partition}\0" \
				".kernel_mmc=setenv kernel mmc rescan\\\\; fatload mmc ${mmcdev}:\\\\${boot_partition}\0" \

	#define FDT_MEM 	".fdt_nand=setenv fdt nand read ${fdtaddr} \\\\${fdt_partition}" BOOT_WITH_FDT	\
				".fdt_mmc=setenv fdt mmc rescan\\\\; fatload mmc ${mmcdev}:\\\\${boot_partition} ${fdtaddr} ${bootfdt}" BOOT_WITH_FDT

	#define BOOTARGS 	"set_rootfs=undef\0" \
				".set_rootfs_mmc=setenv set_rootfs 'setenv rootfs root=/dev/mmcblk${mmcdev}p${rootfs_partition} rootfstype=squashfs rootwait'\0" \
				".set_rootfs_nand=setenv set_rootfs 'setenv rootfs rootfstype=squashfs ubi.block=0,${rootfs_partition} ubi.mtd=TargetFS,2048 root=/dev/ubiblock0_${rootfs_ubi_number} rootwait ro'\0" \
				"set_bootargs=run set_rootfs; setenv bootargs ${console} ${login} ${mtdparts} ${network} ${rootfs} ${mode} ${init} ${extra} ${rauc_cmd}\0"
#else


	#define FS_UPDATE_SUPPORT
	#define ROOTFS_MEM 	".rootfs_mmc=setenv rootfs root=/dev/mmcblk${mmcdev}p2 rootwait\0"

	#define KERNEL_MEM	".kernel_nand=setenv kernel nand read ${loadaddr} Kernel\0" \
				".kernel_mmc=setenv kernel mmc rescan\\\\; load mmc ${mmcdev} . ${bootfile}\0"

	#define FDT_MEM 	".fdt_nand=setenv fdt nand read ${fdtaddr} FDT" BOOT_WITH_FDT	\
				".fdt_mmc=setenv fdt mmc rescan\\\\; load mmc ${mmcdev} ${fdtaddr} ${bootfdt}" BOOT_WITH_FDT

	#define BOOTARGS "set_bootargs=setenv bootargs ${console} ${login} ${mtdparts} ${network} ${rootfs} ${mode} ${init} ${extra}\0"
#endif

/* Initial environment variables */
#if defined(CONFIG_NAND_BOOT) || defined(CONFIG_SD_BOOT)
#define CONFIG_EXTRA_ENV_SETTINGS					\
	FS_UPDATE_SUPPORT 						\
	"initrd_addr=0x43800000\0"					\
	"initrd_high=0xffffffffffffffff\0"				\
	"console=undef\0"						\
	".console_none=setenv console\0"				\
	".console_serial=setenv console console=${sercon},${baudrate}\0" \
	".console_display=setenv console console=tty1\0"		\
	"login=undef\0"							\
	".login_none=setenv login login_tty=null\0"			\
	".login_serial=setenv login login_tty=${sercon},${baudrate}\0"	\
	".login_display=setenv login login_tty=tty1\0"			\
	"mtdids=undef\0"						\
	"mtdparts=undef\0"						\
	".mtdparts_std=" MTDPARTS_STD "\0"				\
	"mmcdev=" __stringify(CONFIG_SYS_MMC_ENV_DEV) "\0"		\
	".network_off=setenv network\0"					\
	".network_on=setenv network ip=${ipaddr}:${serverip}:${gatewayip}:${netmask}:${hostname}:${netdev}\0" \
	".network_dhcp=setenv network ip=dhcp\0"			\
	"rootfs=undef\0"						\
	ROOTFS_MEM							\
	".rootfs_nfs=setenv rootfs root=/dev/nfs nfsroot=${serverip}:${rootpath}\0" \
	".rootfs_usb=setenv rootfs root=/dev/sda1 rootwait\0"		\
	"kernel=undef\0"						\
	KERNEL_MEM 							\
	".kernel_tftp=setenv kernel tftpboot . ${bootfile}\0"		\
	".kernel_nfs=setenv kernel nfs . ${serverip}:${rootpath}/${bootfile}\0" \
	".kernel_usb=setenv kernel usb start\\\\; load usb 0 . ${bootfile}\0" \
	"fdt=undef\0"							\
	"fdtaddr=0x43000000\0"						\
	FDT_MEM 							\
	".fdt_none=setenv fdt booti\0"					\
	".fdt_tftp=setenv fdt tftpboot ${fdtaddr} ${bootfdt}" BOOT_WITH_FDT \
	".fdt_nfs=setenv fdt nfs ${fdtaddr} ${serverip}:${rootpath}/${bootfdt}" BOOT_WITH_FDT \
	".fdt_usb=setenv fdt usb start\\\\; load usb 0 ${fdtaddr} ${bootfdt}" BOOT_WITH_FDT \
	FILSEIZE2BLOCKCOUNT					\
	EXTRA_UBI							\
	"mode=undef\0"							\
	".mode_rw=setenv mode rw\0"					\
	".mode_ro=setenv mode ro\0"					\
	"netdev=eth0\0"							\
	"init=undef\0"							\
	".init_init=setenv init\0"					\
	".init_linuxrc=setenv init init=linuxrc\0"			\
	".init_fs_updater=setenv init init=/sbin/preinit.sh\0" \
	"sercon=undef\0"						\
	"installcheck=undef\0"						\
	"updatecheck=undef\0"						\
	"recovercheck=undef\0"						\
	"platform=undef\0"						\
	"arch=fsimx8mm\0"						\
	"bootfdt=undef\0"						\
	"m4_uart4=disable\0"						\
	FSBOOTDELAY							\
	"fdt_high=0xffffffffffffffff\0"					\
	"set_bootfdt=setenv bootfdt ${platform}.dtb\0"			\
	BOOTARGS
#endif

/* Link Definitions */
#define CONFIG_LOADADDR			0x40480000

#define CONFIG_SYS_LOAD_ADDR           CONFIG_LOADADDR

#define CONFIG_SYS_INIT_RAM_ADDR        0x40000000
#define CONFIG_SYS_INIT_RAM_SIZE        0x80000
#define CONFIG_SYS_INIT_SP_OFFSET				\
        (CONFIG_SYS_INIT_RAM_SIZE - GENERATED_GBL_DATA_SIZE)
#define CONFIG_SYS_INIT_SP_ADDR					\
        (CONFIG_SYS_INIT_RAM_ADDR + CONFIG_SYS_INIT_SP_OFFSET)

/************************************************************************
 * Environment
 ************************************************************************/

/* Environment settings for large blocks (128KB). The environment is held in
   the heap, so keep the real env size small to not waste malloc space. */
#define CONFIG_ENV_OVERWRITE			/* Allow overwriting ethaddr */

#if defined(CONFIG_ENV_IS_IN_MMC)
	#ifdef CONFIG_FS_UPDATE_SUPPORT
		#define CONFIG_ENV_SIZE			0x2000
		#define CONFIG_ENV_OFFSET_REDUND (CONFIG_ENV_OFFSET + CONFIG_ENV_SIZE)
	#else
		#define CONFIG_ENV_SIZE			0x1000
	#endif
	#define CONFIG_ENV_OFFSET               (64 * SZ_64K)
#elif defined(CONFIG_ENV_IS_IN_NAND)
	#ifdef CONFIG_FS_UPDATE_SUPPORT
		#define CONFIG_ENV_SIZE			0x00004000	/* 16KB */
		#define CONFIG_ENV_RANGE		0x00080000	/* 4 blocks = 512KB */
		#define CONFIG_ENV_OFFSET   		0x00800000 /* after u-boot */
		#define CONFIG_ENV_OFFSET_REDUND   	0x00840000
	#else
		#define CONFIG_ENV_SIZE		0x00004000	/* 16KB */
		#define CONFIG_ENV_RANGE	0x00040000	/* 2 blocks = 256KB */
		#define CONFIG_ENV_OFFSET       0x00800000 /* after u-boot */
	#endif
#endif

/* Size of malloc() pool */
#define CONFIG_SYS_MALLOC_LEN		((CONFIG_ENV_SIZE + (2*1024) + (16*1024)) * 1024)

#define CONFIG_SYS_SDRAM_BASE           0x40000000
#define CONFIG_NR_DRAM_BANKS		1
#define CONFIG_BAUDRATE			115200

#define CONFIG_MXC_UART
/* have to define for F&S serial_mxc driver */
#define UART1_BASE UART1_BASE_ADDR
#define UART2_BASE UART2_BASE_ADDR
#define UART3_BASE UART3_BASE_ADDR
#define UART4_BASE UART4_BASE_ADDR
#define UART5_BASE 0xFFFFFFFF

#define CONFIG_MXC_UART_BASE		UART1_BASE_ADDR

/* Monitor Command Prompt */

/************************************************************************
 * Command Line Editor (Shell)
 ************************************************************************/
#ifdef CONFIG_SYS_HUSH_PARSER
#define CONFIG_SYS_PROMPT_HUSH_PS2	"> "
#endif

/* Input and print buffer sizes */
#define CONFIG_SYS_CBSIZE	512	/* Console I/O Buffer Size */
#define CONFIG_SYS_PBSIZE	640	/* Print Buffer Size */
#define CONFIG_SYS_MAXARGS	16	/* max number of command args */
#define CONFIG_SYS_BARGSIZE	CONFIG_SYS_CBSIZE /* Boot Arg Buffer Size */

#define CONFIG_IMX_BOOTAUX

/* USDHC */
#define CONFIG_CMD_MMC
#define CONFIG_FSL_ESDHC
#define CONFIG_FSL_USDHC

#ifdef CONFIG_SD_BOOT
/* SPL use the CONFIG_SYS_MMC_ENV_DEV in
 * serial download mode. Otherwise use
 * board_mmc_get_env_dev function.
 * (s. mmc_get_env_dev in mmc_env.c)
 */
#define CONFIG_SYS_MMC_ENV_DEV		2 /* USDHC3 */
/* number of available  */
#define CONFIG_SYS_FSL_USDHC_NUM	2 /* use USDHC1 and USDHC3 */
#else
#define CONFIG_SYS_FSL_USDHC_NUM	1 /* use USDHC1 */
#define CONFIG_SYS_MMC_ENV_DEV		-1
#endif

#define CONFIG_SYS_FSL_ESDHC_ADDR       0

#define CONFIG_SUPPORT_EMMC_BOOT	/* eMMC specific */

#define CONFIG_MXC_GPIO

#define CONFIG_MXC_OCOTP
#define CONFIG_CMD_FUSE

#ifndef CONFIG_DM_I2C
#define CONFIG_SYS_I2C
#endif
#define CONFIG_SYS_I2C_MXC_I2C1		/* enable I2C bus 0 */
#define CONFIG_SYS_I2C_MXC_I2C2		/* enable I2C bus 1 */
#define CONFIG_SYS_I2C_MXC_I2C3		/* enable I2C bus 2 */
#define CONFIG_SYS_I2C_MXC_I2C4		/* enable I2C bus 3 */
#define CONFIG_SYS_I2C_SPEED		400000

/* USB configs */
#ifndef CONFIG_SPL_BUILD
#define CONFIG_CMD_USB
#define CONFIG_USB_STORAGE
#define CONFIG_USBD_HS

#define CONFIG_CMD_USB_MASS_STORAGE
#define CONFIG_USB_GADGET_MASS_STORAGE
#define CONFIG_USB_FUNCTION_MASS_STORAGE
#endif

#define CONFIG_USB_GADGET_DUALSPEED
#define CONFIG_USB_GADGET_VBUS_DRAW 2

#define CONFIG_CI_UDC

#define CONFIG_MXC_USB_PORTSC  (PORT_PTS_UTMI | PORT_PTS_PTW)
#define CONFIG_USB_MAX_CONTROLLER_COUNT         2

#ifdef CONFIG_NAND_BOOT
#define CONFIG_NAND_MXS
#define CONFIG_CMD_NAND
#define CONFIG_CMD_NAND_TRIMFFS

/* NAND stuff */
#define CONFIG_SYS_MAX_NAND_DEVICE	1
/* Chips per device; all chips must be the same type; if different types
   are necessary, they must be implemented as different NAND devices */
#define CONFIG_SYS_NAND_MAX_CHIPS	1
#define CONFIG_SYS_NAND_BASE		0x40000000
#define CONFIG_SYS_NAND_5_ADDR_CYCLE
#define CONFIG_SYS_NAND_ONFI_DETECTION
#define MXS_NAND_MAX_ECC_STRENGTH 62

/* DMA stuff, needed for GPMI/MXS NAND support */
#define CONFIG_APBH_DMA
#define CONFIG_APBH_DMA_BURST
#define CONFIG_APBH_DMA_BURST8

#ifdef CONFIG_CMD_UBI
#define CONFIG_MTD_DEVICE
#endif

#endif

/* Framebuffer */
#ifdef CONFIG_VIDEO
#define CONFIG_VIDEO_MXS
#define CONFIG_VIDEO_LOGO
#define CONFIG_SPLASH_SCREEN
#define CONFIG_SPLASH_SCREEN_ALIGN
#define CONFIG_CMD_BMP
#define CONFIG_BMP_16BPP
#define CONFIG_VIDEO_BMP_RLE8
#define CONFIG_VIDEO_BMP_LOGO
#define CONFIG_IMX_VIDEO_SKIP
#endif

#define CONFIG_OF_SYSTEM_SETUP

#endif
