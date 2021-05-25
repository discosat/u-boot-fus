/*
 * Copyright (C) 2017 F&S Elektronik Systeme GmbH
 * Copyright (C) 2018-2019 F&S Elektronik Systeme GmbH
 * Copyright (C) 2021 F&S Elektronik Systeme GmbH
 *
 * Configuration settings for all F&S boards based on i.MX8MP. This is
 * PicoCoreMX8MP.
 *
 * Activate with one of the following targets:
 *   make fsimx8mp_defconfig   Configure for i.MX8MP boards
 *   make                     Build uboot-spl.bin, u-boot.bin and u-boot-nodtb.bin.
 *
 * SPDX-License-Identifier:	GPL-2.0+
 */

#ifndef __FSIMX8MP_H
#define __FSIMX8MP_H

#include <linux/sizes.h>
#include <asm/arch/imx-regs.h>

#include "imx_env.h"

/* disable FAT write becaue its doesn't work
 *  with F&S FAT driver
 */
#undef CONFIG_FAT_WRITE

#define CONFIG_SYS_UART_PORT	1	/* Default UART port */
#define CONFIG_CONS_INDEX       (CONFIG_SYS_UART_PORT)

#define DEV_NAME_SIZE 20
#define CONFIG_SPL_MAX_SIZE		(152 * 1024)
#define CONFIG_SYS_MONITOR_LEN		(512 * 1024)
#define CONFIG_SYS_MMCSD_RAW_MODE_U_BOOT_USE_SECTOR
#define CONFIG_SYS_MMCSD_RAW_MODE_U_BOOT_SECTOR	0x300
#define CONFIG_SYS_MMCSD_FS_BOOT_PARTITION	1
#define CONFIG_SYS_UBOOT_BASE	(QSPI0_AMBA_BASE + CONFIG_SYS_MMCSD_RAW_MODE_U_BOOT_SECTOR * 512)

#ifdef CONFIG_SPL_BUILD
#define CONFIG_SPL_STACK		0x187FF0
#define CONFIG_SPL_BSS_START_ADDR      0x0095e000
#define CONFIG_SPL_BSS_MAX_SIZE        0x2000	/* 8 KB */
#define CONFIG_SYS_SPL_MALLOC_START    0x42200000
#define CONFIG_SYS_SPL_MALLOC_SIZE     SZ_512K	/* 512 KB */

#define CONFIG_MALLOC_F_ADDR		0x184000 /* malloc f used before GD_FLG_FULL_MALLOC_INIT set */

#define CONFIG_SPL_ABORT_ON_RAW_IMAGE

#define CONFIG_POWER
#define CONFIG_POWER_I2C
#define CONFIG_POWER_PCA9450

#define CONFIG_SYS_I2C

#endif

/* Add F&S update */
#define CONFIG_CMD_UPDATE
#define CONFIG_CMD_READ
#define CONFIG_SERIAL_TAG

#define CONFIG_REMAKE_ELF
/* ENET Config */
/* ENET1 */
#define CONFIG_SYS_DISCOVER_PHY

#if defined(CONFIG_CMD_NET)
#define CONFIG_ETHPRIME                 "eth1" /* Set qos to primary since we use its MDIO */
#define FDT_SEQ_MACADDR_FROM_ENV

#define CONFIG_FEC_XCV_TYPE             RGMII
//#define CONFIG_FEC_MXC_PHYADDR          5
#define FEC_QUIRK_ENET_MAC

//#define DWC_NET_PHYADDR					4
#ifdef CONFIG_DWC_ETH_QOS
#define CONFIG_SYS_NONCACHED_MEMORY     (1 * SZ_1M)     /* 1M */
#endif

#define PHY_ANEG_TIMEOUT 20000

#define CONFIG_PHY_ATHEROS
#define CONFIG_NETMASK		255.255.255.0
#define CONFIG_IPADDR		10.0.0.252
#define CONFIG_SERVERIP		10.0.0.122
#define CONFIG_GATEWAYIP	10.0.0.5
#define CONFIG_ROOTPATH		"/rootfs"

#endif



#define CONFIG_BOOTFILE		"Image"
#define CONFIG_PREBOOT
#ifdef CONFIG_FS_UPDATE_SUPPORT
	#define CONFIG_BOOTCOMMAND	"run selector; run set_bootargs; run kernel; run fdt"
#else
	#define CONFIG_BOOTCOMMAND	"run set_bootargs; run kernel; run fdt"
#endif

#define MTDIDS_DEFAULT		""
#define MTDPART_DEFAULT		""
#define MTDPARTS_STD		"setenv mtdparts "
#define MTDPARTS_UBIONLY	"setenv mtdparts "

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

#ifdef CONFIG_FS_UPDATE_SUPPORT

	#define FS_UPDATE_SUPPORT "BOOT_ORDER=A B\0" \
	"BOOT_ORDER_ALT=undef\0" \
	"BOOT_A_LEFT=3\0" \
	"BOOT_B_LEFT=3\0" \
	"rauc_cmd=rauc.slot=A\0" \
	"selector=undef\0" \
	".selector_mmc=setenv selector " \
	"'if test \"x${BOOT_ORDER_ALT}\" != \"x${BOOT_ORDER}\"; then	"														\
	"setenv rauc_cmd undef; "																								\
		"for BOOT_SLOT in \"${BOOT_ORDER}\"; do "																			\
		  "if test \"x${BOOT_SLOT}\" = \"xA\" && test ${BOOT_A_LEFT} -gt 0 && test \"x${rauc_cmd}\" = \"xundef\"; then "	\
			  "echo \"Current rootfs boot_partition is A\"; "																\
			  "setexpr BOOT_A_LEFT ${BOOT_A_LEFT} - 1; "																	\
			  "setenv boot_partition 1;"																					\
			  "setenv rootfs_partition 5;"																					\
			  "setenv rauc_cmd rauc.slot=A;"																				\
		  "elif test \"x${BOOT_SLOT}\" = \"xB\" && test ${BOOT_B_LEFT} -gt 0 && test \"x${rauc_cmd}\" = \"xundef\"; then "	\
			  "echo \"Current rootfs boot_partition is B\"; "																\
			  "setexpr BOOT_B_LEFT ${BOOT_B_LEFT} - 1; "																	\
			  "setenv boot_partition 2;"																					\
			  "setenv rootfs_partition 6;"																					\
			  "setenv rauc_cmd rauc.slot=B;"																				\
		  "fi;"																												\
		"done;"																												\
	"saveenv;"																												\
	"fi;'\0"																												\
	"'if test \"x${BOOT_ORDER_ALT}\" != \"x${BOOT_ORDER}\"; then	"														\
	"setenv rauc_cmd undef; "																								\
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
	"saveenv;"																												\
	"fi;'\0"																												\
	"boot_partition=undef\0" \
	".boot_partition_mmc= setenv boot_partition 1\0"\
	"rootfs_partition=undef\0"\
	".rootfs_partition_mmc=setenv rootfs_partition 4\0"\

	#define ROOTFS_MEM 	".rootfs_mmc=setenv rootfs root=/dev/mmcblk${mmcdev}p\\\\${boot_partition} rootwait\0"
	#define KERNEL_MEM 	".kernel_mmc=setenv kernel mmc rescan\\\\; load mmc ${mmcdev}:\\\\${boot_partition}\0" \

	#define FDT_MEM 	".fdt_mmc=setenv fdt mmc rescan\\\\; load mmc ${mmcdev}:\\\\${boot_partition} ${fdtaddr} ${bootfdt}" BOOT_WITH_FDT

	#define BOOTARGS 	"set_rootfs=undef\0" \
				".set_rootfs_mmc=setenv set_rootfs 'setenv rootfs root=/dev/mmcblk${mmcdev}p${rootfs_partition} rootfstype=squashfs rootwait'\0" \
				"set_bootargs=run set_rootfs; setenv bootargs ${console} ${login} ${mtdparts} ${network} ${rootfs} ${mode} ${init} ${extra} ${rauc_cmd}\0"
#else


	#define FS_UPDATE_SUPPORT
	#define ROOTFS_MEM 	".rootfs_mmc=setenv rootfs root=/dev/mmcblk${mmcdev}p2 rootwait\0"

	#define KERNEL_MEM	".kernel_mmc=setenv kernel mmc rescan\\\\; load mmc ${mmcdev} . ${bootfile}\0"

	#define FDT_MEM 	".fdt_mmc=setenv fdt mmc rescan\\\\; load mmc ${mmcdev} ${fdtaddr} ${bootfdt}" BOOT_WITH_FDT

	#define BOOTARGS "set_bootargs=setenv bootargs ${console} ${login} ${mtdparts} ${network} ${rootfs} ${mode} ${init} ${extra}\0"
#endif

/* Initial environment variables */
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
	"mode=undef\0"							\
	".mode_rw=setenv mode rw\0"					\
	".mode_ro=setenv mode ro\0"					\
	"netdev=eth0\0"							\
	"init=undef\0"							\
	".init_init=setenv init\0"					\
	".init_linuxrc=setenv init init=linuxrc\0"			\
	"sercon=undef\0"						\
	"installcheck=undef\0"						\
	"updatecheck=undef\0"						\
	"recovercheck=undef\0"						\
	"platform=undef\0"						\
	"arch=fsimx8mp\0"						\
	"bootfdt=undef\0"						\
	"m4_uart4=disable\0"						\
	FSBOOTDELAY							\
	"fdt_high=0xffffffffffffffff\0"					\
	"set_bootfdt=setenv bootfdt ${platform}.dtb\0"			\
	BOOTARGS

/* Link Definitions */
#define CONFIG_LOADADDR			0x40480000

#define CONFIG_SYS_LOAD_ADDR		CONFIG_LOADADDR

#define CONFIG_SYS_INIT_RAM_ADDR	0x40000000
#define CONFIG_SYS_INIT_RAM_SIZE	0x80000
#define CONFIG_SYS_INIT_SP_OFFSET \
	(CONFIG_SYS_INIT_RAM_SIZE - GENERATED_GBL_DATA_SIZE)
#define CONFIG_SYS_INIT_SP_ADDR \
	(CONFIG_SYS_INIT_RAM_ADDR + CONFIG_SYS_INIT_SP_OFFSET)

#define CONFIG_ENV_OVERWRITE
#define CONFIG_ENV_SPI_BUS		CONFIG_SF_DEFAULT_BUS
#define CONFIG_ENV_SPI_CS		CONFIG_SF_DEFAULT_CS
#define CONFIG_ENV_SPI_MODE		CONFIG_SF_DEFAULT_MODE
#define CONFIG_ENV_SPI_MAX_HZ		CONFIG_SF_DEFAULT_SPEED

/* Size of malloc() pool */
#define CONFIG_SYS_MALLOC_LEN		SZ_32M

/* Totally 1GB LPDDR4 */
/* Link Definitions */
#define CONFIG_LOADADDR			0x40480000

#define CONFIG_SYS_LOAD_ADDR           CONFIG_LOADADDR

#define CONFIG_SYS_SDRAM_BASE		0x40000000
#define PHYS_SDRAM			0x40000000
#define PHYS_SDRAM_SIZE			0x80000000	/* 2 GB */

#define CONFIG_SYS_MEMTEST_START	0x40000000
#define CONFIG_SYS_MEMTEST_END		(CONFIG_SYS_MEMTEST_START + \
					(PHYS_SDRAM_SIZE >> 1))

#define CONFIG_MXC_UART_BASE		UART2_BASE_ADDR

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

#define CONFIG_FSL_USDHC
/* SPL use the CONFIG_SYS_MMC_ENV_DEV in
 * serial download mode. Otherwise use
 * board_mmc_get_env_dev function.
 * (s. mmc_get_env_dev in mmc_env.c)
 */
#define CONFIG_SYS_MMC_ENV_DEV		2 /* USDHC3 */
/* number of available  */
#define CONFIG_SYS_FSL_USDHC_NUM	2 /* use USDHC1 and USDHC3 */


#define CONFIG_SYS_FSL_ESDHC_ADDR	0

#define CONFIG_SYS_MMC_IMG_LOAD_PART	1

#ifdef CONFIG_FSL_FSPI
#define FSL_FSPI_FLASH_SIZE		SZ_32M
#define FSL_FSPI_FLASH_NUM		1
#define FSPI0_BASE_ADDR			0x30bb0000
#define FSPI0_AMBA_BASE			0x0
#define CONFIG_FSPI_QUAD_SUPPORT

#define CONFIG_SYS_FSL_FSPI_AHB
#endif
#define CONFIG_SYS_I2C_SPEED		100000

/* USB configs */
#ifndef CONFIG_SPL_BUILD
#define CONFIG_CMD_USB
#define CONFIG_USB_STORAGE

#define CONFIG_CMD_USB_MASS_STORAGE
#define CONFIG_USB_GADGET_MASS_STORAGE
#define CONFIG_USB_FUNCTION_MASS_STORAGE
#endif

#define CONFIG_USB_MAX_CONTROLLER_COUNT         2
#define CONFIG_USBD_HS
#define CONFIG_USB_GADGET_VBUS_DRAW 2

#ifdef CONFIG_DM_VIDEO
#define CONFIG_VIDEO_LOGO
#define CONFIG_SPLASH_SCREEN
#define CONFIG_SPLASH_SCREEN_ALIGN
#define CONFIG_CMD_BMP
#define CONFIG_BMP_16BPP
#define CONFIG_BMP_24BPP
#define CONFIG_BMP_32BPP
#define CONFIG_VIDEO_BMP_RLE8
#define CONFIG_VIDEO_BMP_LOGO
#endif

#endif
