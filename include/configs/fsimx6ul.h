/*
 * Copyright (C) 2018 F&S Elektronik Systeme GmbH
 *
 * Configuration settings for all F&S boards based on i.MX6 UltraLite and ULL.
 * These are efusA7UL, PicoCOM1.2, PicoCOMA7 and PicoCoreMX6UL. Differences
 * between UL and ULL can be handled at runtime, so one config is sufficient.
 *
 * Activate with one of the following targets:
 *   make fsimx6ul_config     Configure for i.MX6 UltraLite/ULL boards
 *   make                     Build uboot.nb0
 *
 * SPDX-License-Identifier:	GPL-2.0+
 *
 * The following addresses are given as offsets of the device.
 *
 * NAND flash layout with separate Kernel/FDT MTD partition 
 * -------------------------------------------------------------------------
 * 0x0000_0000 - 0x0001_FFFF: NBoot: NBoot image, primary copy (128KB)
 * 0x0002_0000 - 0x0003_FFFF: NBoot: NBoot image, secondary copy (128KB)
 * 0x0004_0000 - 0x000F_FFFF: UserDef: User defined data (768KB)
 * 0x0010_0000 - 0x0013_FFFF: Refresh: Swap blocks for refreshing (256KB)
 * 0x0014_0000 - 0x001F_FFFF: UBoot: U-Boot image (768KB)
 * 0x0020_0000 - 0x0023_FFFF: UBootEnv: U-Boot environment (256KB)
 * 0x0024_0000 - 0x00A3_FFFF: Kernel: Linux Kernel zImage (8MB)
 * 0x00A4_0000 - 0x00BF_FFFF: FDT: Flat Device Tree(s) (1792KB)
 * 0x00C0_0000 -         END: TargetFS: Root filesystem (Size - 12MB)
 *
 * NAND flash layout with UBI only, Kernel/FDT in rootfs or kernel/FDT volume
 * -------------------------------------------------------------------------
 * 0x0000_0000 - 0x0001_FFFF: NBoot: NBoot image, primary copy (128KB)
 * 0x0002_0000 - 0x0003_FFFF: NBoot: NBoot image, secondary copy (128KB)
 * 0x0004_0000 - 0x000F_FFFF: UserDef: User defined data (768KB)
 * 0x0010_0000 - 0x0013_FFFF: Refresh: Swap blocks for refreshing (256KB)
 * 0x0014_0000 - 0x001F_FFFF: UBoot: U-Boot image (768KB)
 * 0x0020_0000 - 0x0023_FFFF: UBootEnv: U-Boot environment (256KB)
 * 0x0024_0000 -         END: TargetFS: Root filesystem (Size - 2.25MB)
 *
 * END: 0x07FF_FFFF for 128MB, 0x0FFF_FFFF for 256MB, 0x1FFF_FFFF for 512MB,
 *      0x3FFF_FFFF for 1GB.
 *
 * Remark:
 * Block size is 128KB. All partition sizes have been chosen to allow for at
 * least one bad block in addition to the required size of the partition. E.g.
 * UBoot is 512KB, but the UBoot partition is 768KB to allow for two bad blocks
 * (256KB) in this memory region.
 *
 * eMMC flash layout with separate Kernel/FDT MTD partition
 * -------------------------------------------------------------------------
 * BOOTPARTITION1
 * 0x0000_0000 - 0x0000_FFFF: NBoot: NBoot image, primary copy (64KB)
 * 0x0001_0000 - 0x0001_FFFF: NBoot: NBoot image, secondary copy (64KB)
 * 0x0002_0000 - 0x0002_1FFF: NBoot: NBoot configuration (4KB)
 * 0x0002_2000 - 0x0005_FFFF: M4: M4 image (248KB)
 * 0x0010_0000 - 0x0010_3FFF: UBootEnv (16KB)
 * 0x0010_4000 - 0x0010_7FFF: UBootEnvRed (16KB)
 *
 * User HW partition only:
 * 0x0020_0000: UBoot_A (3MB)              nboot-info: mmc-u-boot[0]
 * 0x0050_0000: UBoot_B (3MB)              nboot-info: mmc-u-boot[1]
 * 0x0080_0000: Regular filesystem partitions (Kernel, TargetFS, etc)
 */

#ifndef __FSIMX6UL_CONFIG_H
#define __FSIMX6UL_CONFIG_H

/************************************************************************
 * High Level Configuration Options
 ************************************************************************/
#undef CONFIG_MP			/* No multi processor support */

/*####define CONFIG_IMX_THERMAL*/	/* Read CPU temperature */

#ifndef CONFIG_SYS_L2CACHE_OFF
#define CONFIG_SYS_L2_PL310
#define CONFIG_SYS_PL310_BASE	L2_PL310_BASE
#endif

/* i.MX6ULL has some IOMUXC registers moved to SNVS, indicated by LPSR bit */ 
#define CONFIG_IOMUX_LPSR

/* The ARMv7 cache code (arch/arm/lib/cache-cp15.c) and the i.MX6 init code
   (arch/arm/cpu/armv7/mx6/soc.c) now also allow to set the data cache mode to
   write-back. Unfortunately this is now the default, but (at least) i.MX6
   SDHC does not work with write-back yet. So set back to write-through. */
#define CONFIG_SYS_ARM_CACHE_WRITETHROUGH

#include <asm/arch/imx-regs.h>		/* IRAM_BASE_ADDR, IRAM_SIZE */

#undef CONFIG_SKIP_LOWLEVEL_INIT	/* Lowlevel init handles ARM errata */

/* The load address of U-Boot is now independent from the size. Just load it
   at some rather low address in RAM. It will relocate itself to the end of
   RAM automatically when executed. */
#define CONFIG_SYS_TEXT_BASE 0x80100000	/* Where NBoot loads U-Boot */
#define CONFIG_BOARD_SIZE_LIMIT 0x80000	/* Size of uboot.nb0 */

/* For the default load address, use an offset of 16MB. The final kernel (after
   decompressing the zImage) must be at offset 0x8000. But if we load the
   zImage there, the loader code will move it away to make room for the
   uncompressed image at this position. So we'll load it directly to a higher
   address to avoid this additional copying. */
#define CONFIG_SYS_LOAD_OFFS 0x01000000


/************************************************************************
 * Memory Layout
 ************************************************************************/
/* Physical addresses of DDR and CPU-internal SRAM */
#define CONFIG_NR_DRAM_BANKS	1
#define CONFIG_SYS_SDRAM_BASE	MMDC0_ARB_BASE_ADDR

/* MX6UL has 128KB SRAM, mapped from 0x00900000-0x0091FFFF */
#define CONFIG_SYS_INIT_RAM_ADDR	(IRAM_BASE_ADDR)
#define CONFIG_SYS_INIT_RAM_SIZE	(0x20000/*###IRAM_SIZE###*/)

/* Init value for stack pointer, set at end of internal SRAM, keep room for
   global data behind stack. */
#define CONFIG_SYS_INIT_SP_OFFSET \
	(CONFIG_SYS_INIT_RAM_SIZE - GENERATED_GBL_DATA_SIZE)
#define CONFIG_SYS_INIT_SP_ADDR \
	(CONFIG_SYS_INIT_RAM_ADDR + CONFIG_SYS_INIT_SP_OFFSET)

/* Size of malloc() pool (heap). Command "ubi part" needs quite a large heap
   if the source MTD partition is large. The size should be large enough to
   also contain a copy of the environment. */
#define CONFIG_SYS_MALLOC_LEN	(10 * 1024 * 1024)

/* Allocate 2048KB protected RAM at end of RAM (device tree, etc.) */
#define CONFIG_PRAM		2048

/* If environment variable fdt_high is not set, then the device tree is
   relocated to the end of RAM before booting Linux. In this case do not go
   beyond RAM offset 0x6f800000. Otherwise it will not fit into Linux' lowmem
   region anymore and the kernel will hang when trying to access the device
   tree after it has set up its final page table. */
#define CONFIG_SYS_BOOTMAPSZ	0x6f800000

/* The final stack sizes are set up in board.c using the settings below */
#define CONFIG_SYS_STACK_SIZE	(128*1024)

/* Memory test checks all RAM before U-Boot (i.e. leaves last MB with U-Boot
   untested) ### If not set, test from beginning of RAM to before stack. */
#if 0
#define CONFIG_SYS_MEMTEST_START CONFIG_SYS_SDRAM_BASE
#define CONFIG_SYS_MEMTEST_END	(CONFIG_SYS_SDRAM_BASE + OUR_UBOOT_OFFS)
#endif

/************************************************************************
 * Clock Settings and Timers
 ************************************************************************/
/* Basic input clocks */
#define CONFIG_SYS_MX6_HCLK	24000000

#define COUNTER_FREQUENCY	8000000 /* 8Mhz */


/************************************************************************
 * GPIO
 ************************************************************************/


/************************************************************************
 * OTP Memory (Fuses)
 ************************************************************************/


/************************************************************************
 * Serial Console (UART)
 ************************************************************************/
#define CONFIG_SYS_UART_PORT	1	/* Default UART port; however we
					   always take the port from NBoot */
#define CONFIG_CONS_INDEX       (CONFIG_SYS_UART_PORT)
#define CONFIG_SYS_SERCON_NAME "ttymxc"	/* Base name for serial devices */
#define CONFIG_SYS_BAUDRATE_TABLE	{9600, 19200, 38400, 57600, 115200}


/************************************************************************
 * I2C
 ************************************************************************/
#define CONFIG_SYS_I2C
#define CONFIG_I2C_MULTI_BUS

/* Bus 0: I2C1 (mxc0) to access RGB display adapter - efusA7UL */
/* Bus 1: I2C4 (mxc3) to access RGB display adapter - PicoCoreMX6UL */
#define CONFIG_SYS_I2C_MXC_I2C1
#define CONFIG_SYS_I2C_MXC_I2C4
#define CONFIG_SYS_I2C_SPEED	100000

/* Bus 2: Soft-I2C (soft00) to access GPIO expander */
#define CONFIG_SYS_I2C_SOFT
#define CONFIG_SOFT_I2C_GPIO_SCL	IMX_GPIO_NR(5, 9)
#define CONFIG_SOFT_I2C_GPIO_SDA	IMX_GPIO_NR(5, 8)
#define CONFIG_SYS_I2C_SOFT_SPEED	50000
#define CONFIG_SYS_I2C_SOFT_SLAVE       0
#define CONFIG_SOFT_I2C_READ_REPEATED_START
#define CONFIG_SYS_SPD_BUS_NUM		2


/************************************************************************
 * LEDs
 ************************************************************************/


/************************************************************************
 * PMIC
 ************************************************************************/
/* No PMIC on F&S i.MX6 UL boards */


/************************************************************************
 * Real Time Clock (RTC)
 ************************************************************************/
/* ###TODO### */


/************************************************************************
 * Ethernet
 ************************************************************************/

/* PHY */
#define CONFIG_SYS_DISCOVER_PHY
#define CONFIG_SYS_FAULT_ECHO_LINK_DOWN

#undef CONFIG_ID_EEPROM			/* No EEPROM for ethernet MAC */


/************************************************************************
 * USB Host
 ************************************************************************/
/* Use USB1 as host */
#define CONFIG_MXC_USB_PORTSC (PORT_PTS_UTMI | PORT_PTS_PTW)
#define CONFIG_USB_MAX_CONTROLLER_COUNT 2


/************************************************************************
 * USB Device
 ************************************************************************/
/* ###TODO### */


/************************************************************************
 * Keyboard
 ************************************************************************/


/************************************************************************
 * SD/MMC Card, eMMC
 ************************************************************************/
#define CONFIG_FSL_ESDHC		  /* use Freescale ESDHC driver */
#define CONFIG_FSL_USDHC		  /* with USDHC modifications */
#define CONFIG_SYS_FSL_ESDHC_ADDR 0	  /* Not used */
#define CONFIG_SYS_FSL_USDHC_NUM       1

#ifdef CONFIG_ENV_IS_IN_MMC
#define CONFIG_SYS_MMC_ENV_DEV 0
#define CONFIG_SYS_MMC_ENV_PART 1 /* NBoot, UBoot and UbootEnv in BootPart1 */
#endif


/************************************************************************
 * NOR Flash
 ************************************************************************/
/* No NOR flash on F&S i.MX6 boards */


/************************************************************************
 * SPI Flash
 ************************************************************************/
/* No QSPI flash on F&S i.MX6 boards */


/************************************************************************
 * NAND Flash
 ************************************************************************/
/* Use F&S implementation of GPMI NFC NAND Flash Driver (MXS) */
#define CONFIG_SYS_NAND_BASE	0x40000000

/* To avoid that NBoot is erased inadvertently, we define a skip region in the
   first NAND device that can not be written and always reads as 0xFF. However
   if value CONFIG_SYS_MAX_NAND_DEVICE is set to 2, the NBoot region is shown
   as a second NAND device with just that size. This makes it easier to have a
   different ECC strategy and software write protection for NBoot. */
#if 1
#define CONFIG_SYS_MAX_NAND_DEVICE	1
#else
#define CONFIG_SYS_MAX_NAND_DEVICE	2
#endif

/* Chips per device; all chips must be the same type; if different types
   are necessary, they must be implemented as different NAND devices */
#define CONFIG_SYS_NAND_MAX_CHIPS	1

/* Define if you want to support nand chips that comply to ONFI spec */
#define CONFIG_SYS_NAND_ONFI_DETECTION


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


/************************************************************************
 * Display (LCD)
 ************************************************************************/
#define CONFIG_VIDEO_LOGO		/* Allow a logo on the console... */
#define CONFIG_VIDEO_BMP_LOGO		/* ...as BMP image... */
#define CONFIG_BMP_16BPP		/* ...with 16 bits per pixel */
#define CONFIG_SPLASH_SCREEN		/* Support splash screen */


/************************************************************************
 * Network Options
 ************************************************************************/
#define CONFIG_BOOTP_DNS2
#define CONFIG_BOOTP_SEND_HOSTNAME
#define CONFIG_NET_RETRY_COUNT	5
#define CONFIG_ARP_TIMEOUT	2000UL


/************************************************************************
 * Filesystem Support
 ************************************************************************/


/************************************************************************
 * Generic MTD Settings
 ************************************************************************/
#define CONFIG_MTD_DEVICE		/* Create MTD device */

/* Define MTD partition info */
#if CONFIG_SYS_MAX_NAND_DEVICE > 1
#define MTDIDS_DEFAULT		"nand0=gpmi-nand0,nand1=gpmi-nand1"
#define MTDPART_DEFAULT		"nand0,0"
#define MTDPARTS_PART1		"fsnand1:256k(NBoot)ro\\\\;fsnand0:768k@256k(UserDef)"
#else
#define MTDIDS_DEFAULT		"nand0=gpmi-nand"
#define MTDPART_DEFAULT		"nand0,1"
#define MTDPARTS_PART1		"gpmi-nand:256k(NBoot)ro,768k(UserDef)"
#endif
#define MTDPARTS_PART2		"256k(Refresh)ro,768k(UBoot)ro,256k(UBootEnv)ro"
#define MTDPARTS_PART3		"8m(Kernel)ro,1792k(FDT)ro"
#define MTDPARTS_PART4		"-(TargetFS)"
#define MTDPARTS_DEFAULT	"mtdparts=" MTDPARTS_PART1 "," MTDPARTS_PART2 "," MTDPARTS_PART3 "," MTDPARTS_PART4
#define MTDPARTS_STD		"setenv mtdparts " MTDPARTS_DEFAULT
#define MTDPARTS_UBIONLY	"setenv mtdparts mtdparts=" MTDPARTS_PART1 "," MTDPARTS_PART2 "," MTDPARTS_PART4


/************************************************************************
 * Environment
 ************************************************************************/
/* Environment settings for large blocks (128KB). The environment is held in
   the heap, so keep the real env size small to not waste malloc space. */
#define CONFIG_ENV_SIZE		0x00004000	/* 16KB */
#define CONFIG_ENV_OVERWRITE			/* Allow overwriting ethaddr */

#define CONFIG_ENV_MMC_OFFSET	0x000100000	/* See MMC layout above */

#define CONFIG_ENV_NAND_RANGE	0x00040000	/* 2 blocks = 256KB */
#define CONFIG_ENV_NAND_OFFSET	0x00200000	/* See NAND layout above */

/* When saving the environment, we usually have a short period of time between
   erasing the NAND region and writing the new data where no valid environment
   is available. To avoid this time, we can save the environment alternatively
   to two different locations in the NAND flash. Then at least one of the
   environments is always valid. Currently we don't use this feature. */
/*#define CONFIG_SYS_ENV_OFFSET_REDUND   0x001C0000*/

#define CONFIG_ETHPRIME		"FEC0"
#define CONFIG_NETMASK		255.255.255.0
#define CONFIG_IPADDR		10.0.0.252
#define CONFIG_SERVERIP		10.0.0.122
#define CONFIG_GATEWAYIP	10.0.0.5
#define CONFIG_BOOTFILE		"zImage"
#define CONFIG_ROOTPATH		"/rootfs"
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
#define BOOT_WITH_FDT "\\\\; bootm ${loadaddr} - ${fdtaddr}\0"

#ifdef CONFIG_CMD_UBI
#ifdef CONFIG_CMD_UBIFS
#define EXTRA_UBIFS \
	".kernel_ubifs=setenv kernel ubi part TargetFS\\\\; ubifsmount ubi0:rootfs\\\\; ubifsload . /boot/${bootfile}\0" \
	".fdt_ubifs=setenv fdt ubi part TargetFS\\\\; ubifsmount ubi0:rootfs\\\\; ubifsload ${fdtaddr} /boot/${bootfdt}" BOOT_WITH_FDT
#else
#define EXTRA_UBIFS
#endif
#define EXTRA_UBI EXTRA_UBIFS \
	".mtdparts_ubionly=" MTDPARTS_UBIONLY "\0" \
	".rootfs_ubifs=setenv rootfs rootfstype=ubifs ubi.mtd=TargetFS root=ubi0:rootfs\0" \
	".kernel_ubi=setenv kernel ubi part TargetFS\\\\; ubi read . kernel\0" \
	".fdt_ubi=setenv fdt ubi part TargetFS\\\\; ubi read ${fdtaddr} fdt" BOOT_WITH_FDT \
	".ubivol_std=ubi part TargetFS; ubi create rootfs\0" \
	".ubivol_ubi=ubi part TargetFS; ubi create kernel 800000 s; ubi create rootfs\0"
#else
#define EXTRA_UBI
#endif

#ifdef CONFIG_BOOTDELAY
#define FSBOOTDELAY
#else
#define FSBOOTDELAY "bootdelay=undef\0"
#endif

#define CONFIG_EXTRA_ENV_SETTINGS \
	"bd_kernel=undef\0" \
	"bd_fdt=undef\0" \
	"bd_rootfs=undef\0" \
	"console=undef\0" \
	".console_none=setenv console\0" \
	".console_serial=setenv console console=${sercon},${baudrate}\0" \
	".console_display=setenv console console=tty1\0" \
	"login=undef\0" \
	".login_none=setenv login login_tty=null\0" \
	".login_serial=setenv login login_tty=${sercon},${baudrate}\0" \
	".login_display=setenv login login_tty=tty1\0" \
	"mtdids=undef\0" \
	"mtdparts=undef\0" \
	".mtdparts_std=" MTDPARTS_STD "\0" \
	"mmcdev=undef\0" \
	"usdhcdev=undef\0" \
	".network_off=setenv network\0" \
	".network_on=setenv network ip=${ipaddr}:${serverip}:${gatewayip}:${netmask}:${hostname}:${netdev}\0" \
	".network_dhcp=setenv network ip=dhcp\0" \
	"rootfs=undef\0" \
	".rootfs_nfs=setenv rootfs root=/dev/nfs nfsroot=${serverip}:${rootpath}\0" \
	".rootfs_mmc=setenv rootfs root=/dev/mmcblk${usdhcdev}p2 rootwait\0" \
	".rootfs_usb=setenv rootfs root=/dev/sda1 rootwait\0" \
	"kernel=undef\0" \
	".kernel_nand=setenv kernel nboot Kernel\0" \
	".kernel_tftp=setenv kernel tftpboot . ${bootfile}\0" \
	".kernel_nfs=setenv kernel nfs . ${serverip}:${rootpath}/${bootfile}\0" \
	".kernel_mmc=setenv kernel mmc rescan\\\\; load mmc ${mmcdev} . ${bootfile}\0" \
	".kernel_usb=setenv kernel usb start\\\\; load usb 0 . ${bootfile}\0" \
	"fdt=undef\0" \
	"fdtaddr=82000000\0" \
	".fdt_none=setenv fdt bootm\0" \
	".fdt_nand=setenv fdt nand read ${fdtaddr} FDT" BOOT_WITH_FDT \
	".fdt_tftp=setenv fdt tftpboot ${fdtaddr} ${bootfdt}" BOOT_WITH_FDT \
	".fdt_nfs=setenv fdt nfs ${fdtaddr} ${serverip}:${rootpath}/${bootfdt}" BOOT_WITH_FDT \
	".fdt_mmc=setenv fdt mmc rescan\\\\; load mmc ${mmcdev} ${fdtaddr} ${bootfdt}" BOOT_WITH_FDT \
	".fdt_usb=setenv fdt usb start\\\\; load usb 0 ${fdtaddr} ${bootfdt}" BOOT_WITH_FDT \
	EXTRA_UBI \
	"mode=undef\0" \
	".mode_rw=setenv mode rw\0" \
	".mode_ro=setenv mode ro\0" \
	"netdev=eth0\0" \
	"init=undef\0" \
	".init_init=setenv init\0" \
	".init_linuxrc=setenv init init=linuxrc\0" \
	"sercon=undef\0" \
	"installcheck=undef\0" \
	"updatecheck=undef\0" \
	"recovercheck=undef\0" \
	"platform=undef\0" \
	"arch=fsimx6ul\0" \
	"bootfdt=undef\0" \
	FSBOOTDELAY \
	"fdt_high=ffffffff\0" \
	"set_bootfdt=setenv bootfdt ${platform}.dtb\0" \
	"set_bootargs=setenv bootargs ${console} ${login} ${mtdparts} ${network} ${rootfs} ${mode} ${init} ${extra}\0"


/************************************************************************
 * DFU (USB Device Firmware Update, requires USB device support)
 ************************************************************************/
/* ###TODO### */


/************************************************************************
 * Tools
 ************************************************************************/


/************************************************************************
 * Libraries
 ************************************************************************/


/************************************************************************
 * Secure Boot
 ************************************************************************/
#define SECURE_PARTITIONS	"UBoot", "Kernel", "FDT", "Images"


#endif /* !__FSIMX6UL_CONFIG_H */
