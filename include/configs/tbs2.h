/*
 * Copyright (C) 2020 F&S Elektronik Systeme GmbH
 *
 * Configuration settings for custom board tbs2 based on i.MX 8M Mini.
 *
 * Activate with one of the following targets:
 *   make tbs2_defconfig      Configure for custom board tbs2
 *   make                     Build u-boot-spl.bin u-boot-nodtb.bin tbs2.dtb
 *
 * SPDX-License-Identifier:	GPL-2.0+
 *
 * The following addresses are given as offsets of the device.
 *
 * eMMC flash layout with separate Kernel/FDT MTD partition
 * -------------------------------------------------------------------------
 * BOOTPARTITION1
 * 0x0000_8400 - 0x0040_0000: Flash.bin: SPL, ATF, U-Boot image (~800KB)
 * USER AREA
 * 0x0040_0000 - 0x0040_1000: UBootEnv: U-Boot environment (4KB)
 * 0x0000_0000 - 0x0000_43FF: GPT: Linux Kernel zImage (34 * 512B)
 * 0x0010_0000 - 0x01FF_FFFF: System: System Partition (Kernel + FDT) (31MB)
 * 0x0200_0000 - (End of RootFS): TargetFS: Root filesystem (Size)
 * (end of rootfs) -         END: Data: Data filesystem for user
 */

#ifndef __TBS2_H
#define __TBS2_H

#include <linux/sizes.h>
#include <asm/arch/imx-regs.h>

#include "imx_env.h"

/************************************************************************
 * High Level Configuration Options
 ************************************************************************/


/************************************************************************
 * Memory Layout
 ************************************************************************/
/* Physical addresses of DDR and CPU-internal SRAM */
#define CONFIG_SYS_SDRAM_BASE		0x40000000
#define CONFIG_NR_DRAM_BANKS		1

/* i.MX 8M Mini has 256KB of internal SRAM,
   mapped from 0x00900000-0x0091FFFF/0x0093FFFF */
#define CONFIG_SYS_INIT_RAM_ADDR        0x40000000
#define CONFIG_SYS_INIT_RAM_SIZE        0x80000
#define CONFIG_SYS_INIT_SP_OFFSET				\
        (CONFIG_SYS_INIT_RAM_SIZE - GENERATED_GBL_DATA_SIZE)
#define CONFIG_SYS_INIT_SP_ADDR					\
        (CONFIG_SYS_INIT_RAM_ADDR + CONFIG_SYS_INIT_SP_OFFSET)

/* Size of malloc() pool (heap). Command "ubi part" needs quite a large heap
   if the source MTD partition is large. The size should be large enough to
   also contain a copy of the environment. */
#define CONFIG_SYS_MALLOC_LEN		((CONFIG_ENV_SIZE + (2*1024) + (16*1024)) * 1024)

/* The final stack sizes are set up in board.c using the settings below */
#define CONFIG_SYS_STACK_SIZE	(128*1024)

/************************************************************************
 * Clock Settings and Timers
 ************************************************************************/


/************************************************************************
 * GPIO
 ************************************************************************/
#define CONFIG_MXC_GPIO


/************************************************************************
 * OTP Memory (Fuses)
 ************************************************************************/
#define CONFIG_MXC_OCOTP
#define CONFIG_CMD_FUSE


/************************************************************************
 * Serial Console (UART)
 ************************************************************************/
#define CONFIG_SYS_SERCON_NAME "ttymxc"	/* Base name for serial devices */
#define CONFIG_SYS_UART_PORT	0	/* Default UART port */
#define CONFIG_CONS_INDEX       (CONFIG_SYS_UART_PORT)

#define CONFIG_BAUDRATE			115200

#define CONFIG_MXC_UART
/* have to define for F&S serial_mxc driver */
#define UART1_BASE UART1_BASE_ADDR
#define UART2_BASE UART2_BASE_ADDR
#define UART3_BASE UART3_BASE_ADDR
#define UART4_BASE UART4_BASE_ADDR
#define UART5_BASE 0xFFFFFFFF

#define CONFIG_MXC_UART_BASE		UART1_BASE_ADDR
#define CONFIG_SERIAL_TAG


/************************************************************************
 * I2C
 ************************************************************************/
#define CONFIG_SYS_I2C_SPEED		100000


/************************************************************************
 * LEDs
 ************************************************************************/


/************************************************************************
 * PMIC
 ************************************************************************/


/************************************************************************
 * SPL
 ************************************************************************/
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

#define CONFIG_MALLOC_F_ADDR		0x912000 /* malloc f used before GD_FLG_FULL_MALLOC_INIT set */

#define CONFIG_SPL_ABORT_ON_RAW_IMAGE /* For RAW image gives a error info not panic */


#define CONFIG_I2C_SUPPORT
#undef CONFIG_DM_MMC
#undef CONFIG_DM_PMIC
#undef CONFIG_DM_PMIC_PFUZE100

#define CONFIG_POWER
#define CONFIG_POWER_I2C
#define CONFIG_POWER_BD71837

#define CONFIG_SYS_I2C
#define CONFIG_SYS_I2C_MXC_I2C1		/* enable I2C bus 0 */
#define CONFIG_SYS_I2C_MXC_I2C2		/* enable I2C bus 1 */
#define CONFIG_SYS_I2C_MXC_I2C3		/* enable I2C bus 2 */
#define CONFIG_SYS_I2C_MXC_I2C4		/* enable I2C bus 3 */

#define CONFIG_ENV_VARS_UBOOT_RUNTIME_CONFIG

#endif

#define CONFIG_SPL_MAX_SIZE		(148 * 1024)
#define CONFIG_SYS_MONITOR_LEN		(512 * 1024)
#define CONFIG_SYS_MMCSD_RAW_MODE_U_BOOT_USE_SECTOR
#define CONFIG_SYS_MMCSD_RAW_MODE_U_BOOT_SECTOR	0x300
#define CONFIG_SYS_MMCSD_FS_BOOT_PARTITION	1
#define CONFIG_SYS_UBOOT_BASE		(QSPI0_AMBA_BASE + CONFIG_SYS_MMCSD_RAW_MODE_U_BOOT_SECTOR * 512)


/************************************************************************
 * Ethernet
 ************************************************************************/

/* PHY */
#define CONFIG_SYS_DISCOVER_PHY
#define CONFIG_SYS_FAULT_ECHO_LINK_DOWN


/************************************************************************
 * USB Host
 ************************************************************************/


/************************************************************************
 * USB Device
 ************************************************************************/
#define CONFIG_FASTBOOT_USB_DEV 0
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

#define CONFIG_OF_SYSTEM_SETUP


/************************************************************************
 * Keyboard
 ************************************************************************/


/************************************************************************
 * SD/MMC Card, eMMC
 ************************************************************************/
#define CONFIG_CMD_MMC
#define CONFIG_FSL_ESDHC
#define CONFIG_FSL_USDHC
#define CONFIG_SYS_FSL_ESDHC_ADDR       0
#define CONFIG_SUPPORT_EMMC_BOOT	/* eMMC specific */

#ifdef CONFIG_SD_BOOT
#define MTDIDS_DEFAULT		""
#define MTDPART_DEFAULT		""
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


/************************************************************************
 * NOR Flash
 ************************************************************************/


/************************************************************************
 * SPI Flash
 ************************************************************************/


/************************************************************************
 * NAND Flash
 ************************************************************************/


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


/************************************************************************
 * Command Definition
 ************************************************************************/
#define CONFIG_CMD_UPDATE
#define CONFIG_CMD_READ
#undef CONFIG_CMD_EXPORTENV
#undef CONFIG_CMD_IMPORTENV
#undef CONFIG_CMD_IMLS
#undef CONFIG_CMD_CRC32


/************************************************************************
 * Display (LCD)
 ************************************************************************/


/************************************************************************
 * Network Options
 ************************************************************************/
#if defined(CONFIG_CMD_NET)
#define CONFIG_CMD_PING
#define CONFIG_CMD_DHCP
#define CONFIG_CMD_MII
#define CONFIG_MII

#define CONFIG_FEC_MXC
#define FEC_QUIRK_ENET_MAC

#define CONFIG_PHY_GIGE
#define IMX_FEC_BASE			0x30BE0000

#define CONFIG_PHY_MICREL_KSZ8XXX
#define CONFIG_LIB_RAND
#define CONFIG_NET_RANDOM_ETHADDR

#endif


/************************************************************************
 * Filesystem Support
 ************************************************************************/
/* disable FAT write because its dosn't work
 *  with F&S FAT driver
 */
#undef CONFIG_FAT_WRITE


/************************************************************************
 * Generic MTD Settings
 ************************************************************************/


/************************************************************************
 * M4 specific configuration
 ************************************************************************/
/* need for F&S bootaux */
#define M4_BOOTROM_BASE_ADDR           MCU_BOOTROM_BASE_ADDR
#define IMX_SIP_SRC_M4_START           IMX_SIP_SRC_MCU_START
#define IMX_SIP_SRC_M4_STARTED         IMX_SIP_SRC_MCU_STARTED
#define CONFIG_IMX_BOOTAUX


/************************************************************************
 * Environment
 ************************************************************************/
 /* Environment settings for large blocks (128KB). The environment is held in
   the heap, so keep the real env size small to not waste malloc space. */
#if defined(CONFIG_ENV_IS_IN_MMC)
#define CONFIG_ENV_SIZE			0x1000
#define CONFIG_ENV_OFFSET               (64 * SZ_64K)
#define CONFIG_ENV_OVERWRITE			/* Allow overwriting ethaddr */
#endif

/* When saving the environment, we usually have a short period of time between
   erasing the eMMC region and writing the new data where no valid environment
   is available. To avoid this time, we can save the environment alternatively
   to two different locations in the eMMC flash. Then at least one of the
   environments is always valid. Currently we don't use this feature. */
/*#define CONFIG_SYS_ENV_OFFSET_REDUND   0xFFFFFFFF */

#define CONFIG_ETHPRIME                 "FEC"
#define FDT_SEQ_MACADDR_FROM_ENV
#define CONFIG_NETMASK		255.255.255.0
#define CONFIG_IPADDR		10.0.0.252
#define CONFIG_SERVERIP		10.0.0.122
#define CONFIG_GATEWAYIP	10.0.0.5
#define CONFIG_ROOTPATH		"/rootfs"
#define CONFIG_BOOTFILE		"Image"
#define CONFIG_PREBOOT
#define CONFIG_BOOTCOMMAND	"run set_bootargs; run kernel; run fdt"

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

#define CONFIG_EXTRA_ENV_SETTINGS					\
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
	"mmcdev=" __stringify(CONFIG_SYS_MMC_ENV_DEV) "\0"		\
	".network_off=setenv network\0"					\
	".network_on=setenv network ip=${ipaddr}:${serverip}:${gatewayip}:${netmask}:${hostname}:${netdev}\0" \
	".network_dhcp=setenv network ip=dhcp\0"			\
	"rootfs=undef\0"						\
	".rootfs_nfs=setenv rootfs root=/dev/nfs nfsroot=${serverip}:${rootpath},tcp,v3\0" \
	".rootfs_mmc=setenv rootfs root=/dev/mmcblk${mmcdev}p2 rootwait\0" \
	".rootfs_usb=setenv rootfs root=/dev/sda1 rootwait\0"		\
	"kernel=undef\0"						\
	".kernel_tftp=setenv kernel tftpboot . ${bootfile}\0"		\
	".kernel_nfs=setenv kernel nfs . ${serverip}:${rootpath}/${bootfile}\0" \
	".kernel_mmc=setenv kernel mmc rescan\\\\; load mmc ${mmcdev} . ${bootfile}\0" \
	".kernel_usb=setenv kernel usb start\\\\; load usb 0 . ${bootfile}\0" \
	"fdt=undef\0"							\
	"fdtaddr=0x43000000\0"						\
	".fdt_none=setenv fdt booti\0"					\
	".fdt_tftp=setenv fdt tftpboot ${fdtaddr} ${bootfdt}" BOOT_WITH_FDT \
	".fdt_nfs=setenv fdt nfs ${fdtaddr} ${serverip}:${rootpath}/${bootfdt}" BOOT_WITH_FDT \
	".fdt_mmc=setenv fdt mmc rescan\\\\; load mmc ${mmcdev} ${fdtaddr} ${bootfdt}" BOOT_WITH_FDT \
	".fdt_usb=setenv fdt usb start\\\\; load usb 0 ${fdtaddr} ${bootfdt}" BOOT_WITH_FDT \
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
	"arch=fsimx8mm\0"						\
	"bootfdt=undef\0"						\
	"m4_uart4=disable\0"						\
	FSBOOTDELAY							\
	"fdt_high=0xffffffffffffffff\0"					\
	"set_bootfdt=setenv bootfdt ${platform}.dtb\0"			\
	"set_bootargs=setenv bootargs ${console} ${login} ${mtdparts} ${network} ${rootfs} ${mode} ${init} ${extra}\0"


#define CONFIG_LOADADDR			0x40480000
#define CONFIG_SYS_LOAD_ADDR           CONFIG_LOADADDR


/************************************************************************
 * DFU (USB Device Firmware Update, requires USB device support)
 ************************************************************************/
/* ###TODO### */


/************************************************************************
 * Tools
 ************************************************************************/
#define CONFIG_REMAKE_ELF


/************************************************************************
 * Libraries
 ************************************************************************/


/************************************************************************
 * Common
 ************************************************************************/
#define CONFIG_BOARD_EARLY_INIT_F
#define CONFIG_BOARD_POSTCLK_INIT
#define CONFIG_BOARD_LATE_INIT

/* Flat Device Tree Definitions */
#define CONFIG_OF_BOARD_SETUP

#undef CONFIG_BOOTM_NETBSD


/************************************************************************
 * Secure Boot
 ************************************************************************/
#ifdef CONFIG_SECURE_BOOT
#define CONFIG_CSF_SIZE					0x2000 /* 8K region */
#endif


#endif /* !__TBS2_H */
