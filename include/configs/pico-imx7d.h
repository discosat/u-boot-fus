/* SPDX-License-Identifier: GPL-2.0+ */
/*
 * Copyright (C) 2017 NXP Semiconductors
 *
 * Configuration settings for the i.MX7D Pico board.
 */

#ifndef __PICO_IMX7D_CONFIG_H
#define __PICO_IMX7D_CONFIG_H

#include "mx7_common.h"

#include "imx7_spl.h"

#ifdef CONFIG_SPL_OS_BOOT
/* Falcon Mode */
#define CONFIG_SPL_FS_LOAD_ARGS_NAME	"args"
#define CONFIG_SPL_FS_LOAD_KERNEL_NAME	"uImage"
#define CONFIG_SYS_SPL_ARGS_ADDR	0x88000000

/* Falcon Mode - MMC support: args@1MB kernel@2MB */
#define CONFIG_SYS_MMCSD_RAW_MODE_ARGS_SECTOR  0x800   /* 1MB */
#define CONFIG_SYS_MMCSD_RAW_MODE_ARGS_SECTORS (CONFIG_CMD_SPL_WRITE_SIZE / 512)
#define CONFIG_SYS_MMCSD_RAW_MODE_KERNEL_SECTOR        0x1000  /* 2MB */
#endif

/* Size of malloc() pool */
#define CONFIG_SYS_MALLOC_LEN		(32 * SZ_1M)

#define CONFIG_MXC_UART_BASE		UART5_IPS_BASE_ADDR
/* Network */
#define CONFIG_FEC_MXC
#define CONFIG_FEC_XCV_TYPE		RGMII
#define CONFIG_ETHPRIME			"FEC"
#define CONFIG_FEC_MXC_PHYADDR		1

#define CONFIG_PHYLIB
#define CONFIG_PHY_BROADCOM

/* ENET1 */
#define IMX_FEC_BASE			ENET_IPS_BASE_ADDR
#define CONFIG_FEC_MXC_MDIO_BASE 	ENET2_IPS_BASE_ADDR

/* MMC Config */
#define CONFIG_SYS_FSL_ESDHC_ADDR	0
#define CONFIG_SUPPORT_EMMC_BOOT /* eMMC specific */

#define CONFIG_DFU_ENV_SETTINGS \
	"dfu_alt_info=" \
		"spl raw 0x2 0x400;" \
		"u-boot raw 0x8a 0x400;" \
		"/boot/zImage ext4 0 1;" \
		"/boot/imx7d-pico-hobbit.dtb ext4 0 1;" \
		"/boot/imx7d-pico-pi.dtb ext4 0 1;" \
		"rootfs part 0 1\0" \

#define BOOTMENU_ENV \
	"bootmenu_0=Boot using PICO-Hobbit baseboard=" \
		"setenv fdtfile imx7d-pico-hobbit.dtb\0" \
	"bootmenu_1=Boot using PICO-Pi baseboard=" \
		"setenv fdtfile imx7d-pico-pi.dtb\0" \

#define CONFIG_SYS_AUXCORE_BOOTDATA 0x60000000 /* Set to QSPI1 A flash at default */
#ifdef CONFIG_CMD_BOOTAUX

#define UPDATE_M4_ENV \
	"m4image=m4_qspi.bin\0" \
	"loadm4image=fatload mmc ${mmcdev}:${mmcpart} ${loadaddr} ${m4image}\0" \
	"update_m4_from_sd=" \
		"if sf probe 0:0; then " \
			"if run loadm4image; then " \
				"setexpr fw_sz ${filesize} + 0xffff; " \
				"setexpr fw_sz ${fw_sz} / 0x10000; "	\
				"setexpr fw_sz ${fw_sz} * 0x10000; "	\
				"sf erase 0x0 ${fw_sz}; " \
				"sf write ${loadaddr} 0x0 ${filesize}; " \
			"fi; " \
		"fi\0" \
	"m4boot=sf probe 0:0; bootaux "__stringify(CONFIG_SYS_AUXCORE_BOOTDATA)"\0"
#else
#define UPDATE_M4_ENV ""
#endif
#define CONFIG_SYS_MMC_IMG_LOAD_PART	1

#define CONFIG_MFG_ENV_SETTINGS \
	"mfgtool_args=setenv bootargs console=${console},${baudrate} " \
		"rdinit=/linuxrc " \
		"g_mass_storage.stall=0 g_mass_storage.removable=1 " \
		"g_mass_storage.idVendor=0x066F g_mass_storage.idProduct=0x37FF "\
		"g_mass_storage.iSerialNumber=\"\" "\
		"clk_ignore_unused "\
		"\0" \
	"initrd_addr=0x83800000\0" \
	"initrd_high=0xffffffff\0" \
	"bootcmd_mfg=run mfgtool_args;bootz ${loadaddr} ${initrd_addr} ${fdt_addr};\0" \

#if defined(CONFIG_NAND_BOOT)
#define CONFIG_EXTRA_ENV_SETTINGS \
	CONFIG_MFG_ENV_SETTINGS \
	"panel=EJ050NA\0" \
	"fdt_addr=0x83000000\0" \
	"fdt_high=0xffffffff\0"	  \
	"console=ttymxc4\0" \
	"bootargs=console=ttymxc4,115200 ubi.mtd=3 "  \
		"root=ubi0:rootfs rootfstype=ubifs "		     \
		"mtdparts=gpmi-nand:64m(boot),16m(kernel),16m(dtb),-(rootfs)\0"\
	"bootcmd=nand read ${loadaddr} 0x4000000 0x800000;"\
		"nand read ${fdt_addr} 0x5000000 0x100000;"\
		"bootz ${loadaddr} - ${fdt_addr}\0"

#else
#define CONFIG_EXTRA_ENV_SETTINGS \
	CONFIG_MFG_ENV_SETTINGS \
	UPDATE_M4_ENV \
	"panel=EJ050NA\0" \
	"script=boot.scr\0" \
	"image=zImage\0" \
	"console=ttymxc4\0" \
	"splashpos=m,m\0" \
	"fdt_high=0xffffffff\0" \
	"initrd_high=0xffffffff\0" \
	"fdtfile=" CONFIG_DEFAULT_FDT_FILE "\0" \
	BOOTMENU_ENV \
	"fdt_addr=0x83000000\0" \
	"fdt_addr_r=0x83000000\0" \
	"kernel_addr_r=" __stringify(CONFIG_LOADADDR) "\0" \
	"pxefile_addr_r=" __stringify(CONFIG_LOADADDR) "\0" \
	"ramdisk_addr_r=0x83000000\0" \
	"ramdiskaddr=0x83000000\0" \
	"scriptaddr=" __stringify(CONFIG_LOADADDR) "\0" \
	CONFIG_DFU_ENV_SETTINGS \
	"findfdt=" \
		"if test $fdtfile = ask ; then " \
			"bootmenu -1; fi;" \
		"if test $fdtfile != ask ; then " \
			"saveenv; fi;\0" \
	"finduuid=part uuid mmc 0:1 uuid\0" \
	"partitions=" \
		"uuid_disk=${uuid_gpt_disk};" \
		"name=rootfs,size=0,uuid=${uuid_gpt_rootfs}\0" \
	"fastboot_partition_alias_system=rootfs\0" \
	"setup_emmc=mmc dev 0; gpt write mmc 0 $partitions; reset;\0" \
	BOOTENV

#define BOOT_TARGET_DEVICES(func) \
	func(MMC, mmc, 0) \
	func(USB, usb, 0) \
	func(PXE, pxe, na) \
	func(DHCP, dhcp, na)

#include <config_distro_bootcmd.h>

#define CONFIG_SYS_MEMTEST_START	0x80000000
#define CONFIG_SYS_MEMTEST_END		(CONFIG_SYS_MEMTEST_START + 0x20000000)

#define CONFIG_SYS_LOAD_ADDR		CONFIG_LOADADDR
#define CONFIG_SYS_HZ			1000

/* Physical Memory Map */
#define PHYS_SDRAM			MMDC0_ARB_BASE_ADDR

#define CONFIG_SYS_SDRAM_BASE		PHYS_SDRAM
#define CONFIG_SYS_INIT_RAM_ADDR	IRAM_BASE_ADDR
#define CONFIG_SYS_INIT_RAM_SIZE	IRAM_SIZE

#define CONFIG_SYS_INIT_SP_OFFSET \
	(CONFIG_SYS_INIT_RAM_SIZE - GENERATED_GBL_DATA_SIZE)
#define CONFIG_SYS_INIT_SP_ADDR \
	(CONFIG_SYS_INIT_RAM_ADDR + CONFIG_SYS_INIT_SP_OFFSET)

/* I2C configs */
#define CONFIG_SYS_I2C
#define CONFIG_SYS_I2C_MXC
#define CONFIG_SYS_I2C_MXC_I2C1
#define CONFIG_SYS_I2C_MXC_I2C2
#define CONFIG_SYS_I2C_MXC_I2C3
#define CONFIG_SYS_I2C_MXC_I2C4
#define CONFIG_SYS_I2C_SPEED		100000

/* PMIC */
#define CONFIG_POWER
#define CONFIG_POWER_I2C
#define CONFIG_POWER_PFUZE3000
#define CONFIG_POWER_PFUZE3000_I2C_ADDR	0x08

/* FLASH and environment organization */
#define CONFIG_ENV_SIZE			SZ_8K

#define CONFIG_ENV_OFFSET		(13 * SZ_64K)
#define CONFIG_SYS_FSL_USDHC_NUM		2

#define CONFIG_SYS_MMC_ENV_DEV			0
#define CONFIG_SYS_MMC_ENV_PART		0
#define CONFIG_MMCROOT			"/dev/mmcblk0p2"  /* USDHC1 */

#ifdef CONFIG_VIDEO
#define	CONFIG_VIDEO_MXS
#define	CONFIG_VIDEO_LOGO
#define	CONFIG_SPLASH_SCREEN
#define CONFIG_SPLASH_SCREEN_ALIGN
#define	CONFIG_CMD_BMP
#define	CONFIG_BMP_16BPP
#define	CONFIG_VIDEO_BMP_RLE8
#define CONFIG_VIDEO_BMP_LOGO
#endif

/* #define CONFIG_SPLASH_SCREEN*/
/* #define CONFIG_MXC_EPDC*/

/*
 * SPLASH SCREEN Configs
 */
#if defined(CONFIG_SPLASH_SCREEN) && defined(CONFIG_MXC_EPDC)
/*
 * Framebuffer and LCD
 */
#define	CONFIG_CFB_CONSOLE
#define CONFIG_CMD_BMP
#define CONFIG_LCD
#define CONFIG_SYS_CONSOLE_IS_IN_ENV

#undef LCD_TEST_PATTERN
/* #define CONFIG_SPLASH_IS_IN_MMC			1 */
#define LCD_BPP					LCD_MONOCHROME
/* #define CONFIG_SPLASH_SCREEN_ALIGN		1 */

#define CONFIG_WAVEFORM_BUF_SIZE		0x400000
#endif

/* USB Configs */
#define CONFIG_USB_EHCI_HCD
#define CONFIG_USB_EHCI_MX7
#define CONFIG_EHCI_HCD_INIT_AFTER_RESET
#define CONFIG_MXC_USB_PORTSC			(PORT_PTS_UTMI | PORT_PTS_PTW)
#define CONFIG_MXC_USB_FLAGS			0
#define CONFIG_USB_MAX_CONTROLLER_COUNT	2

#define CONFIG_IMX_THERMAL

#if defined(CONFIG_MXC_EPDC) && defined(CONFIG_FSL_QSPI)
#error "EPDC Pins conflicts QSPI, Either EPDC or QSPI can be enabled!"
#endif

#if defined(CONFIG_ANDROID_THINGS_SUPPORT)
#include "pico-imx7dandroidthings.h"
#endif

#define PRODUCT_NAME "imx7d"
#define VARIANT_NAME "imx7d_pico"

#endif	/* __CONFIG_H */
