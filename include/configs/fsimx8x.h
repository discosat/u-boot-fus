/* SPDX-License-Identifier: GPL-2.0+ */
/*
 * Copyright 2018-2019 NXP
 */

/*
#define OCRAM_BASE		0x100000
#define OCRAM_ALIAS_SIZE 0x10000

Free Space:
0x110000 - 0x140000



 * OCRAM layout SPL                 U-Boot
 * ---------------------------------------------------------
 * 0x0010_0000: SPL                 SPL       (192KB) (loaded by ROM-Loader, address defined by imximage.cfg)
 * 0x0013_0000: DRAM Timing Data              (16KB) CONFIG_SPL_DRAM_TIMING_ADDR
 * 0x0013_4000: BOARD-CFG           BOARD-CFG (8KB)  CONFIG_FUS_BOARDCFG_ADDR
 * 0x0013_6000: BSS data            cfg_info  (2KB)  CONFIG_SPL_BSS_START_ADDR
 * 0x0013_6800: MALLOC_F pool       ---       (22KB) CONFIG_MALLOC_F_ADDR
 * 0x0013_C000: Stack + Global Data ---       (16KB) CONFIG_SPL_STACK
 * 0x0013_FFFF: END (8X)
 *
 * The SPL must not exceed 192KB (0x30000).
 *
 * DRAM layout  SPL                 U-Boot
 * ---------------------------------------------------------
 * 0x8000_0000: ATF                 ATF       (128KB) CONFIG_SPL_ATF_ADDR
 * 0x8002_0000: UBoot               UBoot     (~700KB) CONFIG_SYS_TEXT_BASE
 * 0xBFFF_FFFF: END (8X)
 *
 * Remark: In the following sections the images have the following content:
 * Boot-Container: SECO, SCFW and SPL, optional: DRAM init, M4 image
 * FIRMWARE:       DRAM settings and ATF, optional: opTEE
 *
 * NAND flash layout
 * -------------------------------------------------------------------------
 * There is no variant with NAND flash. If NAND is required (e.g. for
 * SPI-NAND), see fsimx8mm/mn.h for an example.
 *
 * eMMC Layout
 * -----------
 * Scenario 1: NBoot is in Boot1/Boot2 HW-Partition (default)
 *
 * Boot1/2 HW-Partition (Boot Offset is always 0x0000):
 * 0x0000_0000: Boot-Container 0/1 (640KB)    i.MX8X (always 0)
 * 0x000A_0000: --- (free, 32KB, for compatibility with Scenario 2)
 * 0x000A_8000: UBootEnv (16KB)               in FDT, fallback in defconfig
 * 0x000A_C000: UBootEnvRed (16KB)            in FDT, fallback in defconfig
 * 0x000B_0000: BOARD-CFG Copy 0/1 (8KB)      nboot-info: nboot-start[0]
 * 0x000B_2000: FIRMWARE Copy 0/1 (824KB)     nboot-start[0] + board-cfg-size
 * 0x0018_0000: --- (free, maybe used for U-Boot in the future)
 *
 * User HW-Partition:
 * 0x0000_0000: --- (free, space for GPT, 32KB)
 * 0x0000_8000: --- (free, 1248KB, may be used for UserDef/M4 image)
 * 0x0014_0000: U-Boot A (3MB) (should be moved to 0018_0000, size 2,5MB)
 * 0x0044_0000: U-Boot B (3MB) (should be moved to 0058_0000, size 2,5MB)
 * 0x0074_0000: --- (free, 768KB)
 * 0x0080_0000: Regular filesystem partitions (Kernel, TargetFS, etc)
 *
 * The goal here is to move U-Boot to the Boot partition in the next release
 * to get the whole 8MB reserved region in User space empty for UserDef/M4
 * image and to make writing whole filesystem images easier. Currently U-Boot
 * is always destroyed when this region is not skipped when writing.
 *
 * Remarks:
 * - In this scenario, setting the fuses for the Secondary Image Offset is not
 *   necessary.
 * - spl-start of nboot-info is ignored and silently assumed to be 0.
 * - nboot-start[] of nboot-info is set to CONFIG_FUS_BOARDCFG_MMC0/1 by the
 *   Makefile, but only nboot-start[0] and uboot-start[0] are taken for both
 *   copies in the two Boot HW-Partitions.
 * - If eMMC is configured to boot from Boot1, then this is the Primary Image
 *   and Boot2 is the Secondary Image. If eMMC is configured to boot from
 *   Boot2, then this is the Primary Image and Boot1 is the Secondary Image.
 * - The U-Boot environment is always stored with the Primary Image, i.e. the
 *   partition that is configured for boot.
 * - The reserved region size at the beginning of the User HW-Partition can
 *   stay at 8MB as with NXP, for example to hold the UserDef data or an M4
 *   image. Or it can be reduced to the size of the partition table which is
 *   one simple sector when using MBR.
 * - There is an option to either increase the space for the Boot-Container or
 *   the Environment by 32KB at the cost of a lost compaitibility with
 *   scenario 2.
 *
 * Scenario 2: NBoot is in User HW-Partition or on SD-Card (optional)
 *
 * Boot1/2 HW-Partition:
 * 0x0000_0000: --- (completely empty)
 *
 * User HW-Partition (Boot Offset for the Primary Image is 0x8000):
 * 0x0000_0000: --- (space for GPT, 32KB)
 * 0x0000_8000: Boot-Container 0 (640KB)   i.MX8X; nboot-info: spl-start[0]
 * 0x000A_8000: UBootEnv (16KB)            Defined in device tree
 * 0x000A_C000: UBootEnvRed (16KB)         Defined in device tree
 * 0x000B_0000: BOARD-CFG Copy 0 (8KB)     nboot-info: nboot-start[0]
 * 0x000B_2000: FIRMWARE Copy 0 (824KB)    nboot-start[0] + board-cfg-size
 * 0x0018_0000: U-Boot A (2,5MB)           nboot-info: uboot-start[0]
 * 0x0040_0000: Boot-Container 1 (640KB)   Secondary Image Offset, spl-start[1]
 * 0x004A_0000: --- (free, 64KB)
 * 0x004B_0000: BOARD-CFG Copy 1 (8KB)     nboot-info: nboot-start[1]
 * 0x004B_2000: FIRMWARE Copy 1 (824KB)    nboot-start[1] + board-cfg-size
 * 0x0058_0000: U-Boot B (2,5MB)           nboot-info: uboot-start[1]
 * 0x0080_0000: Regular filesystem partitions (Kernel, TargetFS, etc)
 *
 * Remarks:
 * - In this scenario, the fuses for the Secondary Image Offset have to be set
 *   to 0 (=4MB). This value can basically be set to 1MB*2^n, but 1MB and 4MB
 *   are swapped, so 4MB is value 0, not 2.
 * - The reserved region size stays at 8MB as with NXP.
 * - nboot-start[] of nboot-info is set to CONFIG_FUS_BOARDCFG_MMC0/1 by the
 *   Makefile, and both entries for spl-start, nboot-start and uboot-start are
 *   actually used.
 */

#ifndef __FSIMX8X_H
#define __FSIMX8X_H

#include <linux/sizes.h>
#include <asm/arch/imx-regs.h>

#include "imx_env.h"

#define CONFIG_SPL_MAX_SIZE				(192 * 1024)
#define CONFIG_SYS_MONITOR_LEN				(512 * 1024)
#define CONFIG_SYS_MMCSD_FS_BOOT_PARTITION	1

/*
 * 0x08081000 - 0x08180FFF is for m4_0 xip image,
  * So 3rd container image may start from 0x8181000
 */
#define CONFIG_SYS_UBOOT_BASE 0x08181000
/*
 * The memory layout on stack:  DATA section save + gd + early malloc
 * the idea is re-use the early malloc (CONFIG_SYS_MALLOC_F_LEN) with
 * CONFIG_SYS_SPL_MALLOC_START
 */
#define CONFIG_FUS_BOARDCFG_ADDR	0x00134000
#define CONFIG_SPL_BSS_START_ADDR      0x00136000
#define CONFIG_SPL_BSS_MAX_SIZE		0x800	/* 2 KB */

#ifdef CONFIG_SPL_BUILD
/*#define CONFIG_ENABLE_DDR_TRAINING_DEBUG*/
#define CONFIG_SPL_LDSCRIPT		"arch/arm/cpu/armv8/u-boot-spl.lds"
#define CONFIG_SPL_STACK		0x13FFF0

/* Offsets in eMMC where BOARD-CFG and FIRMWARE are stored */
#define CONFIG_FUS_BOARDCFG_MMC0 0x00080000
#define CONFIG_FUS_BOARDCFG_MMC1 0x00740000

#define CONFIG_SYS_SPL_MALLOC_START	0x82200000
#define CONFIG_SYS_SPL_MALLOC_SIZE	0x80000	/* 512 KB */
#define CONFIG_SYS_ICACHE_OFF
#define CONFIG_SYS_DCACHE_OFF

/* These addresses are hardcoded in ATF */
#define CONFIG_SPL_USE_ATF_ENTRYPOINT
#define CONFIG_SPL_ATF_ADDR 0x80000000
#define CONFIG_SPL_TEE_ADDR 0xfe000000

/* TCM Address where DRAM Timings are loaded to */
#define CONFIG_SPL_DRAM_TIMING_ADDR 0x00130000

#define CONFIG_MALLOC_F_ADDR		0x00136800

#define CONFIG_SERIAL_LPUART_BASE	0x5a080000

#define CONFIG_SPL_RAW_IMAGE_ARM_TRUSTED_FIRMWARE

#define CONFIG_SPL_ABORT_ON_RAW_IMAGE

#define CONFIG_OF_EMBED
#endif /* CONFIG_SPL_BUILD */

/* Add F&S update */
#define CONFIG_CMD_READ
#define CONFIG_SERIAL_TAG
#define CONFIG_FASTBOOT_USB_DEV 0

#define CONFIG_REMAKE_ELF

#define CONFIG_BOARD_EARLY_INIT_F

/* Flat Device Tree Definitions */
#define CONFIG_OF_BOARD_SETUP

#undef CONFIG_CMD_EXPORTENV
#undef CONFIG_CMD_IMPORTENV
#undef CONFIG_CMD_IMLS

#undef CONFIG_CMD_CRC32

#define CONFIG_SYS_FSL_ESDHC_ADDR       0
#define USDHC1_BASE_ADDR                0x5B010000
#define USDHC2_BASE_ADDR                0x5B020000

#define CONFIG_ENV_OVERWRITE

#define CONFIG_PCIE_IMX
#define CONFIG_CMD_PCI
#define CONFIG_PCI_SCAN_SHOW

#define CONFIG_ENV_VARS_UBOOT_RUNTIME_CONFIG

#ifdef CONFIG_AHAB_BOOT
#define AHAB_ENV "sec_boot=yes\0"
#else
#define AHAB_ENV "sec_boot=no\0"
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
#define MTDIDS_DEFAULT  ""
#define MTDPART_DEFAULT ""
#define MTDPARTS_1	"gpmi-nand:4m(NBoot),512k(Refresh),512k(UBootEnv),"
#define MTDPARTS_2	"3m(UBoot),3m(UBootRed),2m(UserDef),"
#define MTDPARTS_2_U    "3m(UBoot_A),3m(UBoot_B),2m(UserDef),"
#define MTDPARTS_3	"32m(Kernel)ro,1024k(FDT)ro,"
#define MTDPARTS_3_A    "32m(Kernel_A),1024k(FDT_A),"
#define MTDPARTS_3_B    "32m(Kernel_B),1024k(FDT_B),"
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

/* Boot M4 */
#define M4_BOOT_ENV \
	"m4_0_image=m4_0.bin\0" \
	"loadm4image_0=fatload mmc ${mmcdev}:${mmcpart} ${loadaddr} ${m4_0_image}\0" \
	"m4boot_0=run loadm4image_0; dcache flush; bootaux ${loadaddr} 0\0" \

#define CONFIG_MFG_ENV_SETTINGS \
	CONFIG_MFG_ENV_SETTINGS_DEFAULT \
	"initrd_addr=0x83100000\0" \
	"initrd_high=0xffffffffffffffff\0" \
	"emmc_dev=0\0" \
	"sd_dev=1\0" \

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
	" ubi create rootfs_B ${rootfs_size};"				\
	" ubi create data\0"						\
	".ubivol_ubi=ubi part TargetFS;"				\
	" ubi create kernel_A ${kernel_size} s;"			\
	" ubi create kernel_B ${kernel_size} s;"			\
	" ubi create fdt_A ${fdt_size} s;"				\
	" ubi create fdt_B ${fdt_size} s;"				\
	" ubi create rootfs_A ${rootfs_size};"				\
	" ubi create rootfs_B ${rootfs_size};"				\
	" ubi create data\0"						\
	".kernel_ubi_A=setenv kernel ubi part TargetFS\\\\;"		\
	" ubi read ${loadaddr} kernel_A\0"					\
	".kernel_ubi_B=setenv kernel ubi part TargetFS\\\\;"		\
	" ubi read ${loadaddr} kernel_B\0"					\
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
	" ubifsmount ubi0:rootfs_A\\\\; ubifsload ${loadaddr} /boot/${bootfile}\0"\
	".kernel_ubifs_B=setenv kernel ubi part TargetFS\\\\;"		\
	" ubifsmount ubi0:rootfs_B\\\\; ubifsload ${loadaddr} /boot/${bootfile}\0"\
	".fdt_ubifs_A=setenv fdt ubi part TargetFS\\\\;"		\
	" ubifsmount ubi0:rootfs_A\\\\;"				\
	" ubifsload ${fdtaddr} /boot/${bootfdt}" BOOT_WITH_FDT		\
	".fdt_ubifs_B=setenv fdt ubi part TargetFS\\\\;"		\
	" ubifsmount ubi0:rootfs_B\\\\;"				\
	" ubifsload ${fdtaddr} /boot/${bootfdt}" BOOT_WITH_FDT		\
	".rootfs_ubifs_A=setenv rootfs 'rootfstype=squashfs"		\
	" ubi.block=0,rootfs_A ubi.mtd=TargetFS,2048"			\
	" root=/dev/ubiblock0_0 rootwait ro'\0"				\
	".rootfs_ubifs_B=setenv rootfs 'rootfstype=squashfs"		\
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
	" load mmc ${mmcdev}:5 ${fdtaddr} \\\\${bootfdt}" BOOT_WITH_FDT	\
	".fdt_mmc_B=setenv fdt mmc rescan\\\\;"				\
	" load mmc ${mmcdev}:6 ${fdtaddr} \\\\${bootfdt}" BOOT_WITH_FDT	\
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
#define BOOT_SYSTEM		\
	".init_fs_updater=setenv init init=/sbin/preinit.sh\0"		\
	"BOOT_ORDER=A B\0"						\
	"BOOT_ORDER_OLD=A B\0"						\
	"BOOT_A_LEFT=3\0"						\
	"BOOT_B_LEFT=3\0"						\
	"update_reboot_state=0\0"					\
	"update=0000\0"							\
	"application=A\0"						\
	"rauc_cmd=rauc.slot=A\0"					\
	"selector="							\
	"if test \"x${BOOT_ORDER_OLD}\" != \"x${BOOT_ORDER}\"; then "			\
		"setenv rauc_cmd undef; "						\
		"for slot in \"${BOOT_ORDER}\"; do "					\
			"setenv sname \"BOOT_\"\"$slot\"\"_LEFT\"; "			\
			"if test \"${!sname}\" -gt 0; then "				\
				"echo \"Current rootfs boot_partition is $slot\"; "	\
				"setexpr $sname ${!sname} - 1; "			\
				"run .kernel_${bd_kernel}_${slot}; "			\
				"run .fdt_${bd_fdt}_${slot}; "				\
				"run .rootfs_${bd_rootfs}_${slot}; "			\
				"setenv rauc_cmd rauc.slot=${slot}; "			\
				"setenv sname ; "					\
				"saveenv;"						\
				"exit;"							\
			"else "								\
				"for slot_a in \"${BOOT_ORDER_OLD}\"; do "		\
					"run .kernel_${bd_kernel}_${slot_a}; "		\
					"run .fdt_${bd_fdt}_${slot_a}; "		\
					"run .rootfs_${bd_rootfs}_${slot_a}; "		\
					"setenv rauc_cmd rauc.slot=${slot_a}; "		\
					"setenv sname ;"				\
					"saveenv;"					\
					"exit;"						\
				"done;"							\
			"fi;"								\
		"done;"									\
	"fi;\0"

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
	" ubi read ${loadaddr} kernel\0"						\
	".fdt_ubi=setenv fdt ubi part TargetFS\\\\;"			\
	" ubi read ${fdtaddr} fdt" BOOT_WITH_FDT
#else
#define BOOT_FROM_UBI
#endif

#ifdef CONFIG_CMD_UBIFS
#define BOOT_FROM_UBIFS							\
	".kernel_ubifs=setenv kernel ubi part TargetFS\\\\;"		\
	" ubifsmount ubi0:rootfs\\\\; ubifsload ${loadaddr} /boot/${bootfile}\0"	\
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
	" load mmc ${mmcdev} ${loadaddr} ${bootfile}\0"				\
	".fdt_mmc=setenv fdt mmc rescan\\\\;"				\
	" load mmc ${mmcdev} ${fdtaddr} \\\\${bootfdt}" BOOT_WITH_FDT	\
	".rootfs_mmc=setenv rootfs root=/dev/mmcblk${mmcdev}p2 rootwait\0"
#else
#define BOOT_FROM_MMC
#endif

/* In case of USB, the layout is the same as on MMC. */
#define BOOT_FROM_USB							\
	".kernel_usb=setenv kernel usb start\\\\;"			\
	" load usb 0 ${loadaddr} ${bootfile}\0"					\
	".fdt_usb=setenv fdt usb start\\\\;"				\
	" load usb 0 ${fdtaddr} ${bootfdt}" BOOT_WITH_FDT		\
	".rootfs_usb=setenv rootfs root=/dev/sda1 rootwait\0"

/* In case of TFTP, kernel and device tree are loaded from TFTP server */
#define BOOT_FROM_TFTP							\
	".kernel_tftp=setenv kernel tftpboot ${loadaddr} ${bootfile}\0"		\
	".fdt_tftp=setenv fdt tftpboot ${fdtaddr} ${bootfdt}" BOOT_WITH_FDT

/* In case of NFS, kernel, device tree and rootfs are loaded from NFS server */
#define BOOT_FROM_NFS							\
	".kernel_nfs=setenv kernel nfs ${loadaddr}"				\
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
/* Reset update process if not catched error occurs that result into u-boot shell drop */
#define FAILED_UPDATE_RESET \
	"failed_update_reset=" \
		"if test \"x${BOOT_ORDER_OLD}\" != \"x${BOOT_ORDER}\"; then	" \
			"reset; "\
		"fi;\0"

/* Initial environment variables */
#define CONFIG_EXTRA_ENV_SETTINGS					\
	CONFIG_MFG_ENV_SETTINGS \
	M4_BOOT_ENV \
	AHAB_ENV \
	"bd_kernel=undef\0"						\
	"bd_fdt=undef\0"						\
	"bd_rootfs=undef\0"						\
	"initrd_addr=0x83000000\0"					\
	"initrd_high=0xffffffffffffffff\0"				\
	"console=undef\0"						\
	".console_none=setenv console\0"				\
	".console_serial=setenv console console=${sercon},${baudrate}\0"\
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
	"fdtaddr=0x83000000\0"						\
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
	FAILED_UPDATE_RESET						\
	"sercon=undef\0"						\
	"installcheck=undef\0"						\
	"updatecheck=undef\0"						\
	"recovercheck=undef\0"						\
	"platform=undef\0"						\
	"arch=fsimx8x\0"						\
	"bootfdt=undef\0"						\
	"usb_pgood_delay=500\0"						\
	"m4_uart4=disable\0"						\
	"fdt_high=0xffffffffffffffff\0"					\
	"set_bootfdt=setenv bootfdt ${platform}.dtb\0"			\
	"set_bootargs=setenv bootargs ${console} ${login} ${mtdparts}"	\
	" ${network} ${rootfs} ${mode} ${init} ${extra} ${rauc_cmd}\0"
#if 0
/* Initial environment variables */
#define CONFIG_EXTRA_ENV_SETTINGS		\
	CONFIG_MFG_ENV_SETTINGS \
	M4_BOOT_ENV \
	AHAB_ENV \
	"script=boot.scr\0" \
	"image=Image\0" \
	"panel=NULL\0" \
	"console=ttyLP2\0" \
	"fdt_addr=0x83000000\0"			\
	"fdt_high=0xffffffffffffffff\0"		\
	"cntr_addr=0x98000000\0"			\
	"cntr_file=os_cntr_signed.bin\0" \
	"boot_fdt=try\0" \
	"fdt_file=undefined\0" \
	"mmcdev="__stringify(CONFIG_SYS_MMC_ENV_DEV)"\0" \
	"mmcpart=" __stringify(CONFIG_SYS_MMC_IMG_LOAD_PART) "\0" \
	"mmcroot=" CONFIG_MMCROOT " rootwait rw\0" \
	"mmcautodetect=yes\0" \
	"mmcargs=setenv bootargs console=${console},${baudrate} earlycon root=${mmcroot}\0 " \
	"loadbootscript=fatload mmc ${mmcdev}:${mmcpart} ${loadaddr} ${script};\0" \
	"bootscript=echo Running bootscript from mmc ...; " \
		"source\0" \
	"loadimage=fatload mmc ${mmcdev}:${mmcpart} ${loadaddr} ${image}\0" \
	"loadfdt=fatload mmc ${mmcdev}:${mmcpart} ${fdt_addr} ${fdt_file}\0" \
	"loadcntr=fatload mmc ${mmcdev}:${mmcpart} ${cntr_addr} ${cntr_file}\0" \
	"auth_os=auth_cntr ${cntr_addr}\0" \
	"boot_os=booti ${loadaddr} - ${fdt_addr};\0" \
	"mmcboot=echo Booting from mmc ...; " \
		"run mmcargs; " \
		"if test ${sec_boot} = yes; then " \
			"if run auth_os; then " \
				"run boot_os; " \
			"else " \
				"echo ERR: failed to authenticate; " \
			"fi; " \
		"else " \
			"if test ${boot_fdt} = yes || test ${boot_fdt} = try; then " \
				"if run loadfdt; then " \
					"run boot_os; " \
				"else " \
					"echo WARN: Cannot load the DT; " \
				"fi; " \
			"else " \
				"echo wait for boot; " \
			"fi;" \
		"fi;\0" \
	"netargs=setenv bootargs console=${console},${baudrate} earlycon " \
		"root=/dev/nfs " \
		"ip=dhcp nfsroot=${serverip}:${nfsroot},v3,tcp\0" \
	"netboot=echo Booting from net ...; " \
		"run netargs;  " \
		"if test ${ip_dyn} = yes; then " \
			"setenv get_cmd dhcp; " \
		"else " \
			"setenv get_cmd tftp; " \
		"fi; " \
		"if test ${sec_boot} = yes; then " \
			"${get_cmd} ${cntr_addr} ${cntr_file}; " \
			"if run auth_os; then " \
				"run boot_os; " \
			"else " \
				"echo ERR: failed to authenticate; " \
			"fi; " \
		"else " \
			"${get_cmd} ${loadaddr} ${image}; " \
			"if test ${boot_fdt} = yes || test ${boot_fdt} = try; then " \
				"if ${get_cmd} ${fdt_addr} ${fdt_file}; then " \
					"run boot_os; " \
				"else " \
					"echo WARN: Cannot load the DT; " \
				"fi; " \
			"else " \
				"booti; " \
			"fi;" \
		"fi;\0"

#define CONFIG_BOOTCOMMAND \
	   "mmc dev ${mmcdev}; if mmc rescan; then " \
		   "if run loadbootscript; then " \
			   "run bootscript; " \
		   "else " \
			   "if test ${sec_boot} = yes; then " \
				   "if run loadcntr; then " \
					   "run mmcboot; " \
				   "else run netboot; " \
				   "fi; " \
			    "else " \
				   "if run loadimage; then " \
					   "run mmcboot; " \
				   "else run netboot; " \
				   "fi; " \
			 "fi; " \
		   "fi; " \
	   "else booti ${loadaddr} - ${fdt_addr}; fi"
#endif
/* Link Definitions */
#define CONFIG_LOADADDR			0x80280000

#define CONFIG_SYS_LOAD_ADDR           CONFIG_LOADADDR

#define CONFIG_SYS_INIT_SP_ADDR         0x80200000

/* Default environment is in SD */
#if 0 //###def CONFIG_QSPI_BOOT
#define CONFIG_ENV_SECT_SIZE	(128 * 1024)
#define CONFIG_ENV_SPI_BUS	CONFIG_SF_DEFAULT_BUS
#define CONFIG_ENV_SPI_CS	CONFIG_SF_DEFAULT_CS
#define CONFIG_ENV_SPI_MODE	CONFIG_SF_DEFAULT_MODE
#define CONFIG_ENV_SPI_MAX_HZ	CONFIG_SF_DEFAULT_SPEED
#endif

#define CONFIG_SYS_MMC_IMG_LOAD_PART	1

#define CONFIG_MMCROOT			"/dev/mmcblk0p2"  /* USDHC2 */
#define CONFIG_SYS_FSL_USDHC_NUM	2

/* Size of malloc() pool */
#define CONFIG_SYS_MALLOC_LEN		((CONFIG_ENV_SIZE + (32 * 1024)) * 1024)

#define CONFIG_SYS_SDRAM_BASE		0x80000000
#define PHYS_SDRAM_1			0x80000000
#define PHYS_SDRAM_2			0x880000000

#define PHYS_SDRAM_1_SIZE		0x40000000	/* 1 GB */
#define PHYS_SDRAM_2_SIZE		0x00000000	/* 0 GB */

/* Serial */
#define CONFIG_BAUDRATE			115200

/* Monitor Command Prompt */
#define CONFIG_SYS_PROMPT_HUSH_PS2     "> "
#define CONFIG_SYS_CBSIZE              2048
#define CONFIG_SYS_MAXARGS             64
#define CONFIG_SYS_BARGSIZE CONFIG_SYS_CBSIZE
#define CONFIG_SYS_PBSIZE		(CONFIG_SYS_CBSIZE + \
					sizeof(CONFIG_SYS_PROMPT) + 16)

/* Generic Timer Definitions */
#define COUNTER_FREQUENCY		8000000	/* 8MHz */

#ifndef CONFIG_DM_PCA953X
#define CONFIG_PCA953X
#define CONFIG_CMD_PCA953X
#define CONFIG_CMD_PCA953X_INFO
#endif

/* MT35XU512ABA1G12 has only one Die, so QSPI0 B won't work */
#ifdef CONFIG_FSL_FSPI
#define FSL_FSPI_FLASH_SIZE		SZ_64M
#define FSL_FSPI_FLASH_NUM		1
#define FSPI0_BASE_ADDR			0x5d120000
#define FSPI0_AMBA_BASE			0
#define CONFIG_SYS_FSL_FSPI_AHB
#endif

/* USB configs */
#define CONFIG_USBD_HS

#define CONFIG_USB_MAX_CONTROLLER_COUNT 2

/* USB OTG controller configs */
#ifdef CONFIG_USB_EHCI_HCD
#define CONFIG_USB_HOST_ETHER
#define CONFIG_USB_ETHER_ASIX
#define CONFIG_MXC_USB_PORTSC		(PORT_PTS_UTMI | PORT_PTS_PTW)
#endif

/* Networking */
#define CONFIG_FEC_XCV_TYPE		RGMII
#define FEC_QUIRK_ENET_MAC

#define CONFIG_NETMASK		255.255.255.0
#define CONFIG_IPADDR		10.0.0.252
#define CONFIG_SERVERIP		10.0.0.122
#define CONFIG_GATEWAYIP	10.0.0.5
#define CONFIG_ROOTPATH		"/rootfs"

/* Framebuffer */
#ifdef CONFIG_VIDEO
#define CONFIG_VIDEO_IMXDPUV1
#define CONFIG_VIDEO_BMP_RLE8
#define CONFIG_SPLASH_SCREEN
#define CONFIG_SPLASH_SCREEN_ALIGN
#define CONFIG_BMP_16BPP
#define CONFIG_VIDEO_LOGO
#define CONFIG_VIDEO_BMP_LOGO
#define CONFIG_IMX_VIDEO_SKIP
#endif

#define CONFIG_OF_SYSTEM_SETUP

#endif /* __FSIMX8X_H */
