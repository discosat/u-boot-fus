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
 * TCM layout (SPL)
 * ----------------
 * 0x007E_0000: --- (4KB, unused)
 * 0x007E_1000: SPL (<= ~140KB) (loaded by ROM-Loader, address defined by ATF)
 *     DRAM-FW: Training Firmware (up to 96KB, immediately behind end of SPL)
 * 0x0081_C000: DRAM Timing Data (16KB)          CONFIG_SPL_DRAM_TIMING_ADDR
 * 0x0081_FFFF: END
 *
 * The sum of SPL and DDR_FW must not exceed 236KB (0x3b000).
 *
 * OCRAM layout SPL                  U-Boot
 * ---------------------------------------------------------
 * 0x0090_0000: (Region reserved by ROM loader)(64KB)
 * 0x0091_0000: BOARD-CFG            BOARD-CFG (8KB)  CONFIG_FUS_BOARDCFG_ADDR
 * 0x0091_2000: BSS data             cfg_info  (8KB)  CONFIG_SPL_BSS_START_ADDR
 * 0x0091_4000: MALLOC_F pool        ---       (28KB) CONFIG_MALLOC_F_ADDR
 * 0x0091_B000: ---                  ---       (4KB)
 * 0x0091_C000: Stack + Global Data  ---       (16KB) CONFIG_SPL_STACK
 * 0x0092_0000: ATF                  ATF       (64KB) CONFIG_SPL_ATF_ADDR
 * 0x0093_FFFF: End
 *
 * After DRAM is available, SPL uses a MALLOC_R pool at 0x4220_0000.
 *
 * OCRAM_S layout (SPL)
 * --------------------
 * 0x0018_0000: Copy of DRAM configuration (passed to ATF)(~16KB)
 * 0x0018_4000: --- (free)
 * 0x0018_7FFF: End
 *
 * After SPL, U-Boot is loaded to DRAM* at 0x4020_0000. If a TEE program is
 * loaded, it has to go to 0xBE00_0000 and a DEK_BLOB is loaded to
 * 0x4040_0000. These addresses are defined in ATF.
 *
 * NAND flash layout
 * -------------------------------------------------------------------------
 * Planned when U-Boot can write SPL/FCB/DBBT:
 * 0x0000_0000: BCB Copy 0 (FCB+DBBT) (128KB)               \
 * 0x0002_0000: BCB Copy 1 (FCB+DBBT) (128KB)                |
 * 0x0004_0000: SPL Copy 0 (256KB)                           |
 * 0x0008_0000: SPL Copy 1 (256KB)                           | "NBoot"
 * 0x000C_0000: Reserve in case of bad blocks (256KB)        |
 * 0x0010_0000: BOARD-CFG Copy 0 (8KB)                       |
 * 0x0010_2000: FIRMWARE Copy 0 (1528KB)                     |
 * 0x0028_0000: BOARD-CFG Copy 1 (8KB)                       |
 * 0x0028_0000: FIRMWARE Copy 1 (1528KB)                    /
 * 0x0040_0000: Refresh ...
 * Actually now, when SPL/FCB/DBBT/HDMI is written by NXP tool kobs:
 * 0x0000_0000: FCB Copy 0 (128KB)                          \
 * 0x0002_0000: FCB Copy 1 (128KB)                           |
 * 0x0004_0000: FCB Copy 2 (128KB)                           |
 * 0x0006_0000: FCB Copy 3 (128KB)                           |
 * 0x0008_0000: DBBT Copy 0 (128KB)                          |
 * 0x000A_0000: DBBT Copy 1 (128KB)                          |
 * 0x000C_0000: DBBT Copy 2 (128KB)                          |
 * 0x000E_0000: DBBT Copy 3 (128KB)                          |
 * 0x0010_0000: SPL Copy 0 (256KB)         Defined by FCB    | "NBoot"
 * 0x0014_0000: HDMI-FW Copy 0 (256KB, unused but written)   |
 * 0x0018_0000: BOARD-CFG Copy 0 (8KB)     nboot-info: nboot-start[0]
 * 0x0018_2000: FIRMWARE Copy 0 (1016KB)                     |
 * 0x0028_0000: SPL Copy 1 (256KB)         Defined by FCB    |
 * 0x002C_0000: HDMI-FW Copy 1 (256KB, unused but written)   |
 * 0x0030_0000: BOARD-CFG Copy 1 (8KB)     nboot-info: nboot-start[1]
 * 0x0030_2000: FIRMWARE Copy 1 (1016KB)                    /
 * 0x0040_0000: Refresh (512KB)
 * 0x0048_0000: UBootEnv (256KB)           FDT: u-boot,nand-env-offset
 * 0x004C_0000: UBootEnvRed (256KB)        FDT: u-boot,nand-env-offset-redundant
 * 0x0050_0000: UBoot_A (3MB)              nboot-info: uboot-start[0]
 * 0x0080_0000: UBoot_B/UBootRed (3MB)     nboot-info: uboot-start[1]
 * 0x00B0_0000: UserDef (2MB)
 * 0x00D0_0000: Kernel_A (32MB)
 * 0x02D0_0000: FDT_A (1MBKB)
 * 0x02E0_0000: Kernel_B (32MB, opt)
 * 0x04E0_0000: FDT_B (1MB, opt)
 * 0x04F0_0000: TargetFS as UBI Volumes
 *
 * Remarks:
 * - nboot-start[] is initialized with CONFIG_FUS_BOARDCFG_NAND0/1.
 * - If Kernel and FDT are part of the Rootfs, these partitions are dropped.
 * - If no Update with Set A and B is used, all _B partitions are dropped;
 *   UBoot_B is replaced by UserDef. This keeps all offsets up to and
 *   including FDT_A fix and also mtd numbers in Linux. In other words: the
 *   version with Update support just inserts Kernel_B and FDT_B in front of
 *   TargetFS and renames UserDef to UBoot_B.
 * - If the size of U-Boot will increase in the future, only UBoot_B must be
 *   moved. All other hardcoded offsets stay as they are.
 *
 * eMMC Layout
 * -----------
 * Boot1/Boot2 or User HW partition:
 * 0x0000_0000: Space for GPT (32KB)
 * 0x0000_8000: Space for MBR (512B, unused)
 * 0x0000_8200: Secondary Image Table (512B)
 * 0x0000_8400: SPL Copy 0 (223KB)         Defined by i.MX8MM
 * 0x0004_0000: BOARD-CFG Copy 0 (8KB)     nboot-info: nboot-start[0]
 * 0x0004_2000: FIRMWARE Copy 0 (760KB)
 * 0x0010_0000: UBootEnv (16KB)
 * 0x0010_4000: UBootEnvRed (16KB)
 * 0x0010_8000: --- (1KB, free)
 * 0x0010_8400: SPL Copy 1 (223KB)         Defined by Secondary Image Table
 * 0x0014_0000: BOARD-CFG Copy 1 (8KB)     nboot-info: nboot-start[1]
 * 0x0014_2000: FIRMWARE Copy 1 (760KB)
 *
 * User HW partition only:
 * 0x0020_0000: UBoot_A (3MB)              nboot-info: mmc-u-boot[0]
 * 0x0050_0000: UBoot_B (3MB)              nboot-info: mmc-u-boot[1]
 * 0x0080_0000: Regular filesystem partitions (Kernel, TargetFS, etc)
 *
 * Remarks:
 * - nboot-start[] is set to CONFIG_FUS_BOARDCFG_MMC0/1 by the Makefile
 * - We can either boot from Boot1, or from Boot2 or from the User partition.
 *   So it will also work on SD cards without a Boot partition.
 * - The reserved region size stays at 8MB as with NXP.
 * - The boot partition requirement is 2 MB and will also work for small eMMC.
 *   If FIRMWARE part grows above 760K, we will need a larger Boot partition.
 * - If booting from a Boot HW partition, there is room in the User partiton
 *   from 0x00008000 to 0x00200000 to store an M4 image for example.
 * - If U-Boot grows beyond 3MB and if we do not want to increase the
 *   reserved region, we can drop the User partition boot option and use the
 *   SPL/BOARD-CFG/FIRMWARE areas for U-Boot, too. Then U-Boot can be almost
 *   4MB in size.
 */

#ifndef __FSIMX8MM_H
#define __FSIMX8MM_H

#include <linux/sizes.h>
#include <asm/arch/imx-regs.h>

#include "imx_env.h"

/* disable FAT write becaue its doesn't work
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

#define CONFIG_SYS_SERCON_NAME "ttymxc"	/* Base name for serial devices */
#define CONFIG_SYS_UART_PORT	0	/* Default UART port */
#define CONFIG_CONS_INDEX       (CONFIG_SYS_UART_PORT)

#define CONFIG_SPL_MAX_SIZE		(140 * 1024)
#define CONFIG_SYS_MONITOR_LEN		(512 * 1024)
#define CONFIG_SYS_MMCSD_RAW_MODE_U_BOOT_USE_SECTOR
#define CONFIG_SYS_MMCSD_RAW_MODE_U_BOOT_SECTOR	0x800
#define CONFIG_SYS_MMCSD_FS_BOOT_PARTITION	1

/* The final stack sizes are set up in board.c using the settings below */
#define CONFIG_SYS_STACK_SIZE	(128*1024)

/* Address in OCRAM where BOARD-CFG is loaded to; U-Boot must know this, too */
#define CONFIG_FUS_BOARDCFG_ADDR	0x910000
#define CONFIG_SPL_BSS_START_ADDR	0x912000
#define CONFIG_SPL_BSS_MAX_SIZE		0x2000	/* 8 KB */

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

/* Offsets in NAND where BOARD-CFG and FIRMWARE are stored */
#define CONFIG_FUS_BOARDCFG_NAND0 0x180000
#define CONFIG_FUS_BOARDCFG_NAND1 0x300000

/* Offsets in eMMC where BOARD-CFG and FIRMWARE are stored */
#define CONFIG_FUS_BOARDCFG_MMC0 0x00040000
#define CONFIG_FUS_BOARDCFG_MMC1 0x00140000

#define CONFIG_SYS_SPL_MALLOC_START	0x42200000
#define CONFIG_SYS_SPL_MALLOC_SIZE	0x80000	/* 512 KB */
#define CONFIG_SYS_ICACHE_OFF
#define CONFIG_SYS_DCACHE_OFF

/* These addresses are hardcoded in ATF */
#define CONFIG_SPL_USE_ATF_ENTRYPOINT
#define CONFIG_SPL_ATF_ADDR 0x920000
#define CONFIG_SPL_TEE_ADDR 0xbe000000

/* TCM Address where DRAM Timings are loaded to */
#define CONFIG_SPL_DRAM_TIMING_ADDR 0x81C000

/* malloc_f is used before GD_FLG_FULL_MALLOC_INIT set */
#define CONFIG_MALLOC_F_ADDR 0x914000

/* ### Kann das weg? Wird nirgendwo genutzt */
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
#define CONFIG_SPL_RAWNAND_BUFFERS_MALLOC

/* Fallback values if values in nboot-info are missing/damaged */
#define CONFIG_SYS_NAND_U_BOOT_OFFS 	0x00500000
#define CONFIG_SYS_NAND_U_BOOT_OFFS_B	0x00800000
#endif

#endif /* CONFIG_SPL_BUILD */

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
#define CONFIG_BOOTCOMMAND \
	"run selector; run set_bootargs; run kernel; run fdt; run failed_update_reset"
#else
#define CONFIG_BOOTCOMMAND \
	"run set_bootargs; run kernel; run fdt"
#endif

/************************************************************************
 * Environment
 ************************************************************************/
/* Define MTD partition info */
#define MTDIDS_DEFAULT  "nand0=gpmi-nand"
#define MTDPART_DEFAULT "nand0,1"
#define MTDPARTS_1	"gpmi-nand:4m(NBoot),512k(Refresh),512k(UBootEnv),"
#define MTDPARTS_2	"3m(UBoot),3m(UBootRed),2m(UserDef),"
#define MTDPARTS_2_U    "3m(UBoot_A),3m(UBoot_B),2m(UserDef),"
#define MTDPARTS_3	"32m(Kernel)ro,1024k(FDT)ro,"
#define MTDPARTS_3_A    "32m(Kernel_A)ro,1024k(FDT_A)ro,"
#define MTDPARTS_3_B    "32m(Kernel_B)ro,1024k(FDT_B)ro,"
#define MTDPARTS_4	"-(TargetFS)"

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

#ifdef CONFIG_FS_UPDATE_SUPPORT
/*
 * F&S updates are based on an A/B mechanism. All storage regions for U-Boot,
 * kernel, device tree and rootfs are doubled, there is a slot A and a slot B.
 * One slot is always active and holds the current software. The other slot is
 * passive and can be used to install new software versions. When all new
 * versions are installed, the roles of the slots are swapped. This means the
 * previously passive slot with the new software gets active and the
 * previously active slot with the old software gets passive. This
 * configuration is then started. If it proves to work, then the new roles get
 * permanent and the now passive slot is available for future versions. If the
 * system will not start successfully, the roles will be switched back and the
 * system will be working with the old software again.
 */

/* In case of NAND, load kernel and device tree from MTD partitions. */
#ifdef CONFIG_CMD_NAND
#define MTDPARTS_DEFAULT						\
	"mtdparts=" MTDPARTS_1 MTDPARTS_2_U MTDPARTS_3_A MTDPARTS_3_B MTDPARTS_4
#define BOOT_FROM_NAND							\
	".mtdparts_std=setenv mtdparts " MTDPARTS_DEFAULT "\0"		\
 	".kernel_nand_A=setenv kernel nand read ${loadaddr} Kernel_A\0" \
 	".kernel_nand_B=setenv kernel nand read ${loadaddr} Kernel_B\0" \
 	".fdt_nand_A=setenv fdt nand read ${fdtaddr} FDT_A" BOOT_WITH_FDT \
 	".fdt_nand_B=setenv fdt nand read ${fdtaddr} FDT_B" BOOT_WITH_FDT
#else
#define BOOT_FROM_NAND
#endif

/* In case of UBI, load kernel and FDT directly from UBI volumes */
#ifdef CONFIG_CMD_UBI
#define BOOT_FROM_UBI							\
	".mtdparts_ubionly=setenv mtdparts mtdparts="			\
	  MTDPARTS_1 MTDPARTS_2_U MTDPARTS_4 "\0"			\
	".ubivol_std=ubi part TargetFS;"				\
	" ubi create rootfs_A ${rootfs_size};"				\
	" ubi create rootfs_B\0"					\
	".ubivol_ubi=ubi part TargetFS;"				\
	" ubi create kernel_A ${kernel_size} s;"			\
	" ubi create kernel_B ${kernel_size} s;"			\
	" ubi create fdt_A ${fdt_site} s;"				\
	" ubi create fdt_B ${fdt_site} s;"				\
	" ubi create rootfs_A ${rootfs_size};"				\
	" ubi create rootfs_A ${rootfs_size};\0"			\
	".kernel_ubi_A=setenv kernel ubi part TargetFS\\\\;"		\
	" ubi read . kernel_A\0"					\
	".kernel_ubi_B=setenv kernel ubi part TargetFS\\\\;"		\
	" ubi read . kernel_B\0"					\
	".fdt_ubi_A=setenv fdt ubi part TargetFS\\\\;"			\
	" ubi read ${fdtaddr} fdt_A" BOOT_WITH_FDT			\
	".fdt_ubi_B=setenv fdt ubi part TargetFS\\\\;"			\
	" ubi read ${fdtaddr} fdt_B" BOOT_WITH_FDT
#else
#define BOOT_FROM_UBI
#endif

/*
 * In case of UBIFS, the rootfs is loaded from a UBI volume. If Kernel and/or
 * device tree are loaded from UBIFS, they are supposed to be part of the
 * rootfs in directory /boot.
 */
#ifdef CONFIG_CMD_UBIFS
#define BOOT_FROM_UBIFS							\
	".kernel_ubifs_A=setenv kernel ubi part TargetFS\\\\;"		\
	" ubifsmount ubi0:rootfs_A\\\\; ubifsload . /boot/${bootfile}\0" \
	".kernel_ubifs_B=setenv kernel ubi part TargetFS\\\\;"		\
	" ubifsmount ubi0:rootfs_B\\\\; ubifsload . /boot/${bootfile}\0" \
	".fdt_ubifs_A=setenv fdt ubi part TargetFS\\\\;"		\
	" ubifsmount ubi0:rootfs_A\\\\;"				\
	" ubifsload ${fdtaddr} /boot/${bootfdt}" BOOT_WITH_FDT		\
	".fdt_ubifs_B=setenv fdt ubi part TargetFS\\\\;"		\
	" ubifsmount ubi0:rootfs_B\\\\;"				\
	" ubifsload ${fdtaddr} /boot/${bootfdt}" BOOT_WITH_FDT		\
	".rootfs_ubifs_A=setenv rootfs rootfstype=squashfs"		\
	" ubi.block=0,rootfs_A ubi.mtd=TargetFS,2048"			\
	" root=/dev/ubiblock0_0 rootwait ro'\0"				\
	".rootfs_ubifs_B=setenv rootfs rootfstype=squashfs"		\
	" ubi.block=0,rootfs_B ubi.mtd=TargetFS,2048"			\
	" root=/dev/ubiblock0_1 rootwait ro'\0"
#else
#define BOOT_FROM_UBIFS
#endif

/*
 * In case of (e)MMC, the rootfs is loaded from a separate partition. Kernel
 * and device tree are loaded as files from a different partition that is
 * typically formated with FAT.
 */
#ifdef CONFIG_CMD_MMC
#define BOOT_FROM_MMC							\
	".kernel_mmc_A=setenv kernel mmc rescan\\\\;"			\
	" load mmc ${mmcdev}:5\0"					\
	".kernel_mmc_B=setenv kernel mmc rescan\\\\;"			\
	" load mmc ${mmcdev}:6\0"					\
	".fdt_mmc_A=setenv fdt mmc rescan\\\\;"				\
	" load mmc ${mmcdev}:5 ${fdtaddr} ${bootfdt}" BOOT_WITH_FDT	\
	".fdt_mmc_B=setenv fdt mmc rescan\\\\;"				\
	" load mmc ${mmcdev}:6 ${fdtaddr} ${bootfdt}" BOOT_WITH_FDT	\
	".rootfs_mmc_A=setenv rootfs root=/dev/mmcblk${mmcdev}p7"	\
	" rootfstype=squashfs rootwait\0"				\
	".rootfs_mmc_B=setenv rootfs root=/dev/mmcblk${mmcdev}p8"	\
	" rootfstype=squashfs rootwait\0"
#else
#define BOOT_FROM_MMC
#endif

/* Loading from USB is not supported for updates yet */
#define BOOT_FROM_USB

/* Loading from TFTP is not supported for updates yet */
#define BOOT_FROM_TFTP

/* Loading from NFS is not supported for updates yet */
#define BOOT_FROM_NFS

/* Generic settings for booting with updates on A/B */
#define BOOT_SYSTEM							\
	".init_fs_updater=setenv init init=/sbin/preinit.sh\0" 		\
	"BOOT_ORDER=A B\0"						\
	"BOOT_ORDER_OLD=A B\0"						\
	"BOOT_LEFT_A=3\0"						\
	"BOOT_LEFT_B=3\0"						\
	"update_reboot_state=0\0" 					\
	"update=0000\0" 						\
	"application=A\0" 						\
	"rauc_cmd=rauc.slot=A\0"					\
	"selector="							\
	"'if test \"x${BOOT_ORDER_OLD\" != \"x${BOOT_ORDER}\"; then	"														\
	"setenv rauc_cmd undef; "					\
	"for slot in \"${BOOT_ORDER}\"; do "				\
		"setenv sname BOOT_LEFT_$slot; "			\
		"if test \"${!sname}\" -gt 0; then "			\
			"echo \"Current rootfs boot_partition is $slot\"; " \
			"setexpr $sname ${!sname} - 1; "		\
			"run .kernel_${bd_kernel}_${slot}; "		\
			"run .fdt_${bd_fdt}_${slot}; "			\
			"run .rootfs_${bd_rootfs}_${slot}; "		\
			"setenv rauc_cmd rauc.slot=$slot; "		\
			"break; "					\
		"fi;"							\
	"done;"								\
	"saveenv;"							\
	"fi;'\0"

#else /* CONFIG_FS_UPDATE_SUPPORT */

/*
 * In a regular environment, all storage regions for U-Boot, kernel, device
 * tree and rootfs are only available once, no A and B. This provides more
 * free space.
 */

/* In case of NAND, load kernel and device tree from MTD partitions. */
#ifdef CONFIG_CMD_NAND
#define MTDPARTS_DEFAULT						\
	"mtdparts=" MTDPARTS_1 MTDPARTS_2 MTDPARTS_3 MTDPARTS_4
#define BOOT_FROM_NAND							\
	".mtdparts_std=setenv mtdparts " MTDPARTS_DEFAULT "\0"		\
	".kernel_nand=setenv kernel nand read ${loadaddr} Kernel\0"	\
 	".fdt_nand=setenv fdt nand read ${fdtaddr} FDT" BOOT_WITH_FDT
#else
#define BOOT_FROM_NAND
#endif

/* In case of UBI, load kernel and FDT directly from UBI volumes */
#ifdef CONFIG_CMD_UBI
#define BOOT_FROM_UBI							\
	".mtdparts_ubionly=setenv mtdparts mtdparts="			\
	  MTDPARTS_1 MTDPARTS_2 MTDPARTS_4 "\0"				\
	".ubivol_std=ubi part TargetFS; ubi create rootfs\0"		\
	".ubivol_ubi=ubi part TargetFS; ubi create kernel ${kernel_size} s;" \
	" ubi create fdt ${fdt_size} s; ubi create rootfs\0"		\
	".kernel_ubi=setenv kernel ubi part TargetFS\\\\;"		\
	" ubi read . kernel\0"						\
	".fdt_ubi=setenv fdt ubi part TargetFS\\\\;"			\
	" ubi read ${fdtaddr} fdt" BOOT_WITH_FDT
#else
#define BOOT_FROM_UBI
#endif

#ifdef CONFIG_CMD_UBIFS
#define BOOT_FROM_UBIFS							\
	".kernel_ubifs=setenv kernel ubi part TargetFS\\\\;"		\
	" ubifsmount ubi0:rootfs\\\\; ubifsload . /boot/${bootfile}\0"	\
	".fdt_ubifs=setenv fdt ubi part TargetFS\\\\;"			\
	" ubifsmount ubi0:rootfs\\\\;"					\
	" ubifsload ${fdtaddr} /boot/${bootfdt}" BOOT_WITH_FDT		\
	".rootfs_ubifs=setenv rootfs rootfstype=ubifs ubi.mtd=TargetFS" \
	" root=ubi0:rootfs\0"
#else
#define BOOT_FROM_UBIFS
#endif

/*
 * In case of (e)MMC, the rootfs is loaded from a separate partition. Kernel
 * and device tree are loaded as files from a different partition that is
 * typically formated with FAT.
 */
#ifdef CONFIG_CMD_MMC
#define BOOT_FROM_MMC							\
	".kernel_mmc=setenv kernel mmc rescan\\\\;"			\
	" load mmc ${mmcdev} . ${bootfile}\0"				\
	".fdt_mmc=setenv fdt mmc rescan\\\\;"				\
	" load mmc ${mmcdev} ${fdtaddr} ${bootfdt}" BOOT_WITH_FDT	\
	".rootfs_mmc=setenv rootfs root=/dev/mmcblk${mmcdev}p2 rootwait\0"
#else
#define BOOT_FROM_MMC
#endif

/* In case of USB, the layout is the same as on MMC. */
#define BOOT_FROM_USB							\
	".kernel_usb=setenv kernel usb start\\\\;"			\
	" load usb 0 . ${bootfile}\0"					\
	".fdt_usb=setenv fdt usb start\\\\;"				\
	" load usb 0 ${fdtaddr} ${bootfdt}" BOOT_WITH_FDT		\
	".rootfs_usb=setenv rootfs root=/dev/sda1 rootwait\0"

/* In case of TFTP, kernel and device tree are loaded from TFTP server */
#define BOOT_FROM_TFTP							\
	".kernel_tftp=setenv kernel tftpboot . ${bootfile}\0"		\
	".fdt_tftp=setenv fdt tftpboot ${fdtaddr} ${bootfdt}" BOOT_WITH_FDT

/* In case of NFS, kernel, device tree and rootfs are loaded from NFS server */
#define BOOT_FROM_NFS							\
	".kernel_nfs=setenv kernel nfs ."				\
	" ${serverip}:${rootpath}/${bootfile}\0"			\
	".fdt_nfs=setenv fdt nfs ${fdtaddr}"				\
	" ${serverip}:${rootpath}/${bootfdt}" BOOT_WITH_FDT		\
	".rootfs_nfs=setenv rootfs root=/dev/nfs"			\
	" nfsroot=${serverip}:${rootpath}\0"

/* Generic settings when not booting with updates A/B */
#define BOOT_SYSTEM

#endif /* CONFIG_FS_UPDATE_SUPPORT */

/* Generic variables */

#ifdef CONFIG_BOOTDELAY
#define FSBOOTDELAY
#else
#define FSBOOTDELAY "bootdelay=undef\0"
#endif

/* Conversion from file size to MMC block count (512 bytes per block) */
#define FILESIZE2BLOCKCOUNT \
	"filesize2blockcount=" \
		"setexpr blockcount \\${filesize} + 0x1ff; " \
		"setexpr blockcount \\${blockcount} / 0x200\0"

/* Initial environment variables */
#define CONFIG_EXTRA_ENV_SETTINGS					\
	"bd_kernel=undef\0"						\
	"bd_fdt=undef\0"						\
	"bd_rootfs=undef\0"						\
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
	"mode=undef\0"							\
	".mode_rw=setenv mode rw\0"					\
	".mode_ro=setenv mode ro\0"					\
	"init=undef\0"							\
	".init_init=setenv init\0"					\
	".init_linuxrc=setenv init init=linuxrc\0"			\
	"mtdids=undef\0"						\
	"mtdparts=undef\0"						\
	"netdev=eth0\0"							\
	"mmcdev=" __stringify(CONFIG_SYS_MMC_ENV_DEV) "\0"		\
	".network_off=setenv network\0"					\
	".network_on=setenv network ip=${ipaddr}:${serverip}"		\
	":${gatewayip}:${netmask}:${hostname}:${netdev}\0"		\
	".network_dhcp=setenv network ip=dhcp\0"			\
	"rootfs=undef\0"						\
	"kernel=undef\0"						\
	"fdt=undef\0"							\
	"fdtaddr=0x43000000\0"						\
	".fdt_none=setenv fdt booti\0"					\
	BOOT_FROM_NAND							\
	BOOT_FROM_UBI							\
	BOOT_FROM_UBIFS							\
	BOOT_FROM_MMC							\
	BOOT_FROM_USB							\
	BOOT_FROM_TFTP							\
	BOOT_FROM_NFS							\
	BOOT_SYSTEM							\
	FILESIZE2BLOCKCOUNT						\
	FSBOOTDELAY							\
	"sercon=undef\0"						\
	"installcheck=undef\0"						\
	"updatecheck=undef\0"						\
	"recovercheck=undef\0"						\
	"platform=undef\0"						\
	"arch=fsimx8mm\0"						\
	"bootfdt=undef\0"						\
	"m4_uart4=disable\0"						\
	"fdt_high=0xffffffffffffffff\0"					\
	"set_bootfdt=setenv bootfdt ${platform}.dtb\0"			\
	"set_bootargs=setenv bootargs ${console} ${login} ${mtdparts}"	\
	" ${network} ${rootfs} ${mode} ${init} ${extra}\0"

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

#define CONFIG_ENV_SIZE		0x4000		/* 16KB */

/* Fallback values if values in the device tree are missing/damaged */
#define CONFIG_ENV_MMC_OFFSET	0x100000
#define CONFIG_ENV_MMC_OFFSET_REDUND 0x104000

/* Use redundant environment, also in case without update support */
#define CONFIG_ENV_NAND_RANGE	0x00040000	/* 2 blocks = 256KB each copy */
#define CONFIG_ENV_NAND_OFFSET	0x00480000	/* Before u-boot */
#define CONFIG_ENV_NAND_OFFSET_REDUND   0x004c0000

/* Size of malloc() pool */
#define CONFIG_SYS_MALLOC_LEN	((CONFIG_ENV_SIZE + (2*1024) + (16*1024)) * 1024)

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
#define CONFIG_FSL_ESDHC
#define CONFIG_FSL_USDHC

#ifdef CONFIG_SD_BOOT
/* SPL use the CONFIG_SYS_MMC_ENV_DEV in
 * serial download mode. Otherwise use
 * board_mmc_get_env_dev function.
 * (s. mmc_get_env_dev in mmc_env.c)
 */
#define CONFIG_SYS_MMC_ENV_DEV		2 /* USDHC3 */
#define CONFIG_SYS_MMC_ENV_PART		1 /* Use mmc_get_env_part() */
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
/*####define CONFIG_CMD_NAND*/
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
