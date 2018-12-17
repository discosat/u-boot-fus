/*
 * Copyright (C) 2018 F&S Elektronik Systeme GmbH
 *
 * Configuration settings for all F&S boards based on Freescale Vybrid. These
 * are armStoneA5, PicoCOMA5, NetDCUA5, CUBEA5, AGATEWAY and HGATEWAY.
 *
 * Activate with one of the following targets:
 *   make fsvybrid_config       Configure for Vybrid boards
 *   make fsvybrid              Configure for Vybrid boards and build
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
 * 0x0014_0000 - 0x001B_FFFF: UBoot: U-Boot image (512KB)
 * 0x001C_0000 - 0x001F_FFFF: UBootEnv: U-Boot environment (256KB)
 * 0x0020_0000 - 0x005F_FFFF: Kernel: Linux Kernel zImage (4MB)
 * 0x0060_0000 -         END: TargetFS: Root filesystem (Size - 6MB)
 *
 * NAND flash layout with UBI only, Kernel/FDT in rootfs or kernel/FDT volume
 * -------------------------------------------------------------------------
 * 0x0000_0000 - 0x0001_FFFF: NBoot: NBoot image, primary copy (128KB)
 * 0x0002_0000 - 0x0003_FFFF: NBoot: NBoot image, secondary copy (128KB)
 * 0x0004_0000 - 0x000F_FFFF: UserDef: User defined data (768KB)
 * 0x0010_0000 - 0x0013_FFFF: Refresh: Swap blocks for refreshing (256KB)
 * 0x0014_0000 - 0x001B_FFFF: UBoot: U-Boot image (512KB)
 * 0x001C_0000 - 0x001F_FFFF: UBootEnv: U-Boot environment (256KB)
 * 0x0020_0000 -         END: TargetFS: Root filesystem (Size - 2MB)
 *
 * END: 0x07FF_FFFF for 128MB, 0x0FFF_FFFF for 256MB, 0x3FFF_FFFF for 1GB
 *
 * Remark:
 * Block size is 128KB. All partition sizes have been chosen to allow for at
 * least one bad block in addition to the required size of the partition. E.g.
 * UBoot is 384KB, but the UBoot partition is 512KB to allow for one bad block
 * (128KB) in this memory region.
 *
 * RAM layout (RAM starts at 0x80000000)
 * -------------------------------------------------------------------------
 * 0x0000_0000 - 0x0000_00FF: Free RAM
 * 0x0000_0100 - 0x0000_07FF: bi_boot_params (ATAGs) (not used if FDT active)
 * 0x0000_1000 - 0x0000_105F: NBoot Args
 * 0x0000_1060 - 0x0000_7FFF: Free RAM
 * 0x0000_8000 - 0x007F_FFFF: Linux BSS (decompressed kernel)
 * 0x0100_0000 - 0x01FF_FFFF: Linux zImage
 * 0x0200_0000 - 0x07FF_FFFF: FDT + Free RAM + U-Boot (if 128MB)
 * 0x0200_0000 - 0x0FFF_FFFF: FDT + Free RAM + U-Boot (if 256MB)
 * 0x0200_0000 - 0x1FFF_FFFF: FDT + Free RAM + U-Boot (if 512MB)
 *
 * NBoot loads U-Boot to a rather low RAM address. Then U-Boot computes its
 * final size and relocates itself to the end of RAM.
 *
 * Memory layout within U-Boot (from top to bottom, starting at
 * RAM-Top = gd->ram_base + gd->ram_size)
 *
 * Addr          Size                      Comment
 * -------------------------------------------------------------------------
 * RAM-Top       CONFIG_SYS_MEM_TOP_HIDE   Hidden memory (unused)
 *               LOGBUFF_RESERVE           Linux kernel logbuffer (unused)
 *               getenv("pram") (in KB)    Protected RAM set in env (unused)
 * gd->tlb_addr  16KB (16KB aligned)       MMU page tables (TLB)
 * gd->fb_base   lcd_setmen()              LCD framebuffer (unused?)
 *               gd->monlen (4KB aligned)  U-boot code, data and bss
 *               TOTAL_MALLOC_LEN          malloc heap
 * bd            sizeof(bd_t)              Board info struct
 * gd->irq_sp    sizeof(gd_t)              Global data
 *               CONFIG_STACKSIZE_IRQ      IRQ stack
 *               CONFIG_STACKSIZE_FIQ      FIQ stack
 *               12 (8-byte aligned)       Abort-stack
 *
 * Remark: TOTAL_MALLOC_LEN depends on CONFIG_SYS_MALLOC_LEN and CONFIG_ENV_SIZE
 */

#ifndef __FSVYBRID_CONFIG_H
#define __FSVYBRID_CONFIG_H

/************************************************************************
 * High Level Configuration Options
 ************************************************************************/
#define CONFIG_IDENT_STRING " for F&S"	/* We are on an F&S board */
#define CONFIG_VYBRID			/* ### TODO: switch to CONFIG_VF610 */
#undef CONFIG_MP			/* No multi processor support */

#define CONFIG_FS_BOARD_OFFS	0	/* F&S Vybrid board types as reported
					   by NBoot start at offset 0 */
#define CONFIG_FS_BOARD_COMMON		/* Use F&S common board stuff */
#define CONFIG_FS_FDT_COMMON		/* Use F&S common FDT stuff */

#include <asm/arch/vybrid-regs.h>	/* IRAM_BASE_ADDR, IRAM_SIZE */
#define CONFIG_SYS_CACHELINE_SIZE	64

/* The ARMv7 cache code (arch/arm/lib/cache-cp15.c) now also allows to set the
   data cache mode to write-back. Unfortunately this is now the default, but
   unless this is thoroughly tested, set it back to write-through. */
#define CONFIG_SYS_ARM_CACHE_WRITETHROUGH

#undef CONFIG_ARCH_CPU_INIT
#undef CONFIG_SKIP_LOWLEVEL_INIT	/* Lowlevel init handles ARM errata */
/*###FIXME### Can we drop CONFIG_BOARD_EARLY_INIT_F?*/
#define CONFIG_BOARD_EARLY_INIT_F	/* Activate NAND flash pin mux */
#define CONFIG_BOARD_LATE_INIT		/* Init board-specific environment */
#define CONFIG_DISPLAY_CPUINFO		/* Show CPU type and speed */
#define CONFIG_DISPLAY_BOARDINFO	/* Show board information */
#define CONFIG_ZERO_BOOTDELAY_CHECK	/* Allow entering U-Boot even if boot
					   delay is zero */
#define CONFIG_USE_IRQ			/* For blinking LEDs */
#define CONFIG_SYS_LONGHELP		/* Undef to save memory */
#undef CONFIG_LOGBUFFER			/* No support for log files */
#define CONFIG_OF_BOARD_SETUP		/* Call board specific FDT fixup */

/* The load address of U-Boot is now independent from the size. Just load it
   at some rather low address in RAM. It will relocate itself to the end of
   RAM automatically when executed. */
#define CONFIG_SYS_TEXT_BASE 0x80100000	/* Where NBoot loads U-Boot */
#define CONFIG_UBOOTNB0_SIZE 0x60000	/* Size of uboot.nb0 */
#define CONFIG_SYS_THUMB_BUILD		/* Build U-Boot in THUMB mode */
#define CONFIG_BOARD_SIZE_LIMIT CONFIG_UBOOTNB0_SIZE

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
#define CONFIG_SYS_SDRAM_BASE	0x80000000

/* Vybrid has min. 256KB internal SRAM, mapped from 0x3F000000-0x3F03FFFF */
#define CONFIG_SYS_INIT_RAM_ADDR	(IRAM_BASE_ADDR)
#define CONFIG_SYS_INIT_RAM_SIZE	(IRAM_SIZE)

/* Init value for stack pointer, set at end of internal SRAM, keep room for
   global data behind stack. */
#define CONFIG_SYS_INIT_SP_OFFSET \
	(CONFIG_SYS_INIT_RAM_SIZE - GENERATED_GBL_DATA_SIZE)
#define CONFIG_SYS_INIT_SP_ADDR \
	(CONFIG_SYS_INIT_RAM_ADDR + CONFIG_SYS_INIT_SP_OFFSET)

/* Size of malloc() pool (heap). Command "ubi part" needs quite a large heap
   if the source MTD partition is large. The size should be large enough to
   also contain a copy of the environment. */
#define CONFIG_SYS_MALLOC_LEN	(2 * 1024 * 1024)

/* Allocate 2048KB protected RAM at end of RAM (Framebuffers, etc.) */
#define CONFIG_PRAM		2048

/* Alignment mask for MMU pagetable: 16kB */
#define CONFIG_SYS_TLB_ALIGN	0xFFFFC000

/* The final stack sizes are set up in board.c using the settings below */
#define CONFIG_SYS_STACK_SIZE	(128*1024)
#ifdef CONFIG_USE_IRQ
#define CONFIG_STACKSIZE_IRQ	(4*1024)
#define CONFIG_STACKSIZE_FIQ	(128)
#endif

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
#define CONFIG_SYS_VYBRID_HCLK	24000000
#define CONFIG_SYS_VYBRID_CLK32	32768

/* Timer */
#define FTM_BASE_ADDR		FTM0_BASE_ADDR
#define CONFIG_TMR_USEPIT

#define CONFIG_SYS_HZ		1000

/* ##### TODO: Only activate some devices; for now: activate all devices */
#define CONFIG_SYS_CLKCTL_CCGR0		0xFFFFFFFF
#define CONFIG_SYS_CLKCTL_CCGR1		0xFFFFFFFF
#define CONFIG_SYS_CLKCTL_CCGR2		0xFFFFFFFF
#define CONFIG_SYS_CLKCTL_CCGR3		0xFFFFFFFF
#define CONFIG_SYS_CLKCTL_CCGR4		0xFFFFFFFF
#define CONFIG_SYS_CLKCTL_CCGR5		0xFFFFFFFF
#define CONFIG_SYS_CLKCTL_CCGR6		0xFFFFFFFF
#define CONFIG_SYS_CLKCTL_CCGR7		0xFFFFFFFF
#define CONFIG_SYS_CLKCTL_CCGR8		0xFFFFFFFF
#define CONFIG_SYS_CLKCTL_CCGR9		0xFFFFFFFF
#define CONFIG_SYS_CLKCTL_CCGR10	0xFFFFFFFF
#define CONFIG_SYS_CLKCTL_CCGR11	0xFFFFFFFF


/************************************************************************
 * GPIO
 ************************************************************************/
#define CONFIG_FSVYBRID_GPIO


/************************************************************************
 * OTP Memory (Fuses)
 ************************************************************************/
/* ###TODO### */


/************************************************************************
 * Serial Console (UART)
 ************************************************************************/
#define CONFIG_VYBRID_UART		/* Use vybrid uart driver */
#define CONFIG_SYS_UART_PORT	1	/* Default UART port; however we
					   always take the port from NBoot */
#undef CONFIG_CONSOLE_MUX		/* Just one console at a time */
#define CONFIG_SYS_SERCON_NAME "ttyLP"	/* Base name for serial devices */
#define CONFIG_BAUDRATE		115200	/* Default baudrate */
#define CONFIG_SYS_BAUDRATE_TABLE	{9600, 19200, 38400, 57600, 115200}


/************************************************************************
 * I2C
 ************************************************************************/
/* No I2C used in U-Boot on F&S Vybrid boards */


/************************************************************************
 * LEDs
 ************************************************************************/
#define CONFIG_BOARD_SPECIFIC_LED
#define CONFIG_BLINK_VYBRID
#define STATUS_LED_BIT 0
#define STATUS_LED_BIT1 1


/************************************************************************
 * PMIC
 ************************************************************************/
/* No PMIC on F&S Vybrid boards */


/************************************************************************
 * Real Time Clock (RTC)
 ************************************************************************/
/* ###TODO### */


/************************************************************************
 * Ethernet
 ************************************************************************/
#define CONFIG_FEC_MXC
/* Undefine the following line if external quarts is used for ETH clock */
#define CONFIG_FS_VYBRID_PLL_ETH

/* PHY */
#define CONFIG_PHY_MICREL		/* Micrel KSZ8021RNL/KSZ8081RNA */
#define CONFIG_PHY_NATSEMI		/* TI/National Semiconductor DP83848 */
#define CONFIG_SYS_DISCOVER_PHY
#define CONFIG_SYS_FAULT_ECHO_LINK_DOWN

#undef CONFIG_ID_EEPROM			/* No EEPROM for ethernet MAC */


/************************************************************************
 * USB Host
 ************************************************************************/
/* Use USB1 as host */
#define CONFIG_USB_EHCI			/* Use EHCI driver (USB2.0) */
#define CONFIG_USB_EHCI_VYBRID		/* This is Vybrid EHCI */
#define CONFIG_EHCI_IS_TDI		/* TDI version with USBMODE register */
/* If USB0 should be used as second USB Host port, activate this entry */
/*#define CONFIG_USB_MAX_CONTROLLER_COUNT 2*/

#define CONFIG_USB_STORAGE


/************************************************************************
 * USB Device
 ************************************************************************/
/* ###TODO### */


/************************************************************************
 * Keyboard
 ************************************************************************/
#if 0
#define CONFIG_USB_KEYBOARD
#define CONFIG_SYS_DEVICE_DEREGISTER	/* Required for CONFIG_USB_KEYBOARD */
#endif


/************************************************************************
 * SD/MMC Card
 ************************************************************************/
#define CONFIG_MMC			/* SD/MMC support */
#define CONFIG_GENERIC_MMC		/* with the generic driver model */
#define CONFIG_FSL_ESDHC		/* use Freescale ESDHC driver */
#define CONFIG_SYS_FSL_ESDHC_ADDR 0	/* Not used */
/*#define CONFIG_MMC_TRACE*/

#define CONFIG_SYS_FSL_ERRATUM_ESDHC135
#define CONFIG_SYS_FSL_ERRATUM_ESDHC111
#define CONFIG_SYS_FSL_ERRATUM_ESDHC_A001


/************************************************************************
 * EMMC
 ************************************************************************/
/* No eMMC on F&S Vybrid boards */


/************************************************************************
 * NOR Flash
 ************************************************************************/
/* No NOR flash on F&S Vybrid boards */
#define CONFIG_SYS_NO_FLASH


/************************************************************************
 * SPI Flash
 ************************************************************************/
/* No QSPI flash on F&S Vybrid boards */


/************************************************************************
 * NAND Flash
 ************************************************************************/
/* Use F&S implementation of Vybrid NFC driver */
#define CONFIG_NAND_FSL_NFC_FS

/* Use our own initialization code */
#define CONFIG_SYS_NAND_SELF_INIT

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

/* Our NAND layout has a continuous set of OOB data, so we only need one
   oobfree entry (plus one empty entry to mark the end of the list). And when
   using NAND flash with 2K pages (written in a single chunk), Vybrid has at
   most 60 ECC bytes: 1 chunk, ECC32, GF15: 1*32*15 bits = 60 bytes. So we can
   also set CONFIG_SYS_NAND_MAX_ECCPOS to >=60. By setting the two values
   below, we can reduce the size of struct nand_ecclayout considerably from
   2824 bytes to 280 bytes (see include/linux/mtd/mtd.h). Please note that
   these settings have to be modified if smaller chunks or NAND flashes with
   larger pages are used. But then we have to modify the driver code anyway. */
#define CONFIG_SYS_NAND_MAX_OOBFREE	2
#define CONFIG_SYS_NAND_MAX_ECCPOS	64

/* Define if you want to support nand chips that comply to ONFI spec */
#define CONFIG_SYS_NAND_ONFI_DETECTION

/* Actually perform block refresh if pages degrade too much. Use the given
   blocks in decreasing order. */
#define CONFIG_NAND_REFRESH
#define CONFIG_SYS_NAND_BACKUP_START_BLOCK	9
#define CONFIG_SYS_NAND_BACKUP_END_BLOCK	8


/************************************************************************
 * Command Line Editor (Shell)
 ************************************************************************/
#ifdef CONFIG_SYS_HUSH_PARSER
#define CONFIG_SYS_PROMPT_HUSH_PS2	"> "
#endif

/* Allow editing (scroll between commands, etc.) */
#define CONFIG_CMDLINE_EDITING
#define CONFIG_AUTO_COMPLETE

/* Input and print buffer sizes */
#define CONFIG_SYS_CBSIZE	512	/* Console I/O Buffer Size */
#define CONFIG_SYS_PBSIZE	640	/* Print Buffer Size */
#define CONFIG_SYS_MAXARGS	16	/* max number of command args */
#define CONFIG_SYS_BARGSIZE	CONFIG_SYS_CBSIZE /* Boot Arg Buffer Size */


/************************************************************************
 * Command Definition
 ************************************************************************/
/* Only those commands are listed that are not available via Kconfig */
#undef CONFIG_CMD_BEDBUG	/* No PPC bedbug debugging support */
#define CONFIG_CMD_BLINK	/* Support for blinking LEDs */
#undef CONFIG_CMD_BMP		/* No old BMP, use new display support */
#undef CONFIG_CMD_CBFS		/* No support for coreboot filesystem */
#undef CONFIG_CMD_CDP		/* No support for CISCOs CDP network config */
#undef CONFIG_CMD_CRAMFS	/* No support for CRAMFS filesystem */
#undef CONFIG_CMD_DATE		/* No date command */
#undef CONFIG_CMD_DIAG		/* No support for board selftest */
#undef CONFIG_CMD_DTT		/* No digital thermometer and thermostat */
#undef CONFIG_CMD_EEPROM	/* No EEPROM support */
#undef CONFIG_CMD_FDC		/* No floppy disc controller */
#undef CONFIG_CMD_FITUPD	/* No update from FIT image */
#define CONFIG_CMD_GETTIME	/* Have gettime command */
#undef CONFIG_CMD_GPT		/* No support for GPT partition tables */
#undef CONFIG_CMD_HASH		/* No hash command */
#undef CONFIG_CMD_IDE		/* No IDE disk support */
#undef CONFIG_CMD_IMMAP		/* No support for PPC immap table */
#define CONFIG_CMD_INI		/* Support INI files to init environment */
#undef CONFIG_CMD_IO		/* No I/O space commands iod and iow */
#undef CONFIG_CMD_IRQ		/* No interrupt support */
#undef CONFIG_CMD_JFFS2		/* No support for JFFS2 filesystem */
#define CONFIG_CMD_LED		/* LED support */
#undef CONFIG_CMD_MD5SUM	/* No support for md5sum checksums */
#undef CONFIG_CMD_MMC_SPI	/* No access of MMC cards in SPI mode */
#define CONFIG_CMD_MTDPARTS	/* Support MTD partitions (mtdparts, chpart) */
#define CONFIG_CMD_NAND_CONVERT	/* support for NAND format conversion */
#undef CONFIG_CMD_ONENAND	/* No support for ONENAND flash memories */
#undef CONFIG_CMD_PART		/* No support for partition info */
#undef CONFIG_CMD_PCI		/* No PCI support */
#undef CONFIG_CMD_PCMCIA	/* No support for PCMCIA cards */
#undef CONFIG_CMD_PORTIO	/* No port commands (in, out) */
#undef CONFIG_CMD_PXE		/* No support for PXE files from pxelinux */
#define CONFIG_CMD_READ		/* Raw read from media without filesystem */
#undef CONFIG_CMD_REGINFO	/* No register support on ARM, only PPC */
#undef CONFIG_CMD_REISER	/* No support for reiserfs filesystem */
#undef CONFIG_CMD_SATA		/* No support for SATA disks */
#undef CONFIG_CMD_SAVES		/* No support for serial uploads (saving) */
#undef CONFIG_CMD_SCSI		/* No support for SCSI disks */
#undef CONFIG_CMD_SDRAM		/* Support SDRAM chips via I2C */
#undef CONFIG_CMD_SHA1SUM	/* No support for sha1sum checksums */
#undef CONFIG_CMD_SPL		/* No SPL support (kernel parameter images) */
#undef CONFIG_CMD_STRINGS	/* No support to show strings */
#undef CONFIG_CMD_TERMINAL	/* No terminal emulator */
#define CONFIG_CMD_UBI		/* Support for unsorted block images (UBI) */
#define CONFIG_CMD_UBIFS	/* Support for UBIFS filesystem */
#define CONFIG_CMD_UNZIP	/* Have unzip command */
#define CONFIG_CMD_UPDATE	/* Support automatic update/install */
#undef CONFIG_CMD_ZFS		/* No support for ZFS filesystem */
#undef CONFIG_CMD_ZIP		/* No support to zip memory region */


/************************************************************************
 * Display Commands (LCD)
 ************************************************************************/
#if 0					/* ### TODO */
#define CONFIG_CMD_LCD			/* Support lcd settings command */
#define CONFIG_CMD_WIN			/* Window layers, alpha blending */
#define CONFIG_CMD_CMAP			/* Support CLUT pixel formats */
#define CONFIG_CMD_DRAW			/* Support draw command */
#define CONFIG_CMD_ADRAW		/* Support alpha draw commands */
#define CONFIG_CMD_BMINFO		/* Provide bminfo command */
#define CONFIG_XLCD_PNG			/* Support for PNG bitmaps */
#define CONFIG_XLCD_BMP			/* Support for BMP bitmaps */
#define CONFIG_XLCD_JPG			/* Support for JPG bitmaps */
#define CONFIG_XLCD_EXPR		/* Allow expressions in coordinates */
#define CONFIG_XLCD_CONSOLE		/* Support console on LCD */
#define CONFIG_XLCD_CONSOLE_MULTI	/* Define a console on each window */
#define CONFIG_XLCD_FBSIZE 0x00100000	/* 1 MB default framebuffer pool */
#define CONFIG_S3C64XX_XLCD		/* Use S3C64XX lcd driver */
#define CONFIG_S3C64XX_XLCD_PWM 1	/* Use PWM1 for backlight */

/* Supported draw commands (see inlcude/cmd_xlcd.h) */
#define CONFIG_XLCD_DRAW \
	(XLCD_DRAW_PIXEL | XLCD_DRAW_LINE | XLCD_DRAW_RECT	\
	 | /*XLCD_DRAW_CIRC | XLCD_DRAW_TURTLE |*/ XLCD_DRAW_FILL	\
	 | XLCD_DRAW_TEXT | XLCD_DRAW_BITMAP | XLCD_DRAW_PROG	\
	 | XLCD_DRAW_TEST)

/* Supported test images (see include/cmd_xlcd.h) */
#define CONFIG_XLCD_TEST \
	(XLCD_TEST_GRID /*| XLCD_TEST_COLORS | XLCD_TEST_D2B | XLCD_TEST_GRAD*/)
#endif


/************************************************************************
 * Network Options
 ************************************************************************/
#define CONFIG_BOOTP_SUBNETMASK
#define CONFIG_BOOTP_GATEWAY
#define CONFIG_BOOTP_HOSTNAME
#define CONFIG_BOOTP_BOOTPATH
#define CONFIG_BOOTP_DNS		/* Configurable parts of CMD_DHCP */
#define CONFIG_BOOTP_DNS2
#define CONFIG_BOOTP_SEND_HOSTNAME
#define CONFIG_NET_RETRY_COUNT	5
#define CONFIG_ARP_TIMEOUT	2000UL
#define CONFIG_MII			/* Required in net/eth.c */


/************************************************************************
 * Filesystem Support
 ************************************************************************/
/* FAT */
#define CONFIG_FS_FAT			/* Support FAT... */
#define CONFIG_SUPPORT_VFAT		/* ...with VFAT */
#define CONFIG_DOS_PARTITION

/* EXT4 */
#define CONFIG_FS_EXT4			/* Support EXT2/3/4 */

/* JFFS2 */
#define CONFIG_JFFS2_NAND		/* Support JFFS2 in NAND */

/* YAFFS */
#undef CONFIG_YAFFS2			/* No support for YAFFS2 commands */

/* UBI/UBIFS */
#define CONFIG_RBTREE			/* Required for UBI */
#define CONFIG_LZO			/* Required for UBI */


/************************************************************************
 * Generic MTD Settings
 ************************************************************************/
#define CONFIG_MTD_DEVICE		/* Create MTD device */
#define CONFIG_MTD_PARTITIONS		/* Required for UBI */

/* Define MTD partition info */
#if CONFIG_SYS_MAX_NAND_DEVICE > 1
#define MTDIDS_DEFAULT		"nand0=fsnand0,nand1=fsnand1"
#define MTDPART_DEFAULT		"nand0,0"
#define MTDPARTS_PART1		"fsnand1:256k(NBoot)ro\\\\;fsnand0:768k@256k(UserDef)"
#else
#define MTDIDS_DEFAULT		"nand0=NAND"
#define MTDPART_DEFAULT		"nand0,1"
#define MTDPARTS_PART1		"NAND:256k(NBoot)ro,768k(UserDef)"
#endif
#define MTDPARTS_PART2		"256k(Refresh)ro,512k(UBoot)ro,256k(UBootEnv)ro"
#define MTDPARTS_PART3		"4m(Kernel)ro,1792k(FDT)ro"
#define MTDPARTS_PART4		"-(TargetFS)"
#define MTDPARTS_STD		"setenv mtdparts mtdparts=" MTDPARTS_PART1 "," MTDPARTS_PART2 "," MTDPARTS_PART3 "," MTDPARTS_PART4
#define MTDPARTS_UBIONLY	"setenv mtdparts mtdparts=" MTDPARTS_PART1 "," MTDPARTS_PART2 "," MTDPARTS_PART4


/************************************************************************
 * Environment
 ************************************************************************/
#define CONFIG_ENV_IS_IN_NAND		/* Environment is in NAND flash */
#define CONFIG_ENV_OVERWRITE		/* Allow overwriting serial/ethaddr */
#define CONFIG_SYS_CONSOLE_IS_IN_ENV	/* Console can be saved in env */

/* Environment settings for large blocks (128KB). The environment is held in
   the heap, so keep the real env size small to not waste malloc space. */
#define ENV_SIZE_DEF_LARGE   0x00004000	/* 16KB */
#define ENV_RANGE_DEF_LARGE  0x00040000 /* 2 blocks = 256KB */
#define ENV_OFFSET_DEF_LARGE 0x001C0000 /* See NAND layout above */

/* When saving the environment, we usually have a short period of time between
   erasing the NAND region and writing the new data where no valid environment
   is available. To avoid this time, we can save the environment alternatively
   to two different locations in the NAND flash. Then at least one of the
   environments is always valid. Currently we don't use this feature. */
/*#define CONFIG_SYS_ENV_OFFSET_REDUND   0x001C0000*/

#define CONFIG_ETHADDR_BASE	00:05:51:07:55:83
#define CONFIG_ETHPRIME		"FEC0"
#define CONFIG_NETMASK		255.255.255.0
#define CONFIG_IPADDR		10.0.0.252
#define CONFIG_SERVERIP		10.0.0.122
#define CONFIG_GATEWAYIP	10.0.0.5
#define CONFIG_BOOTFILE		"zImage"
#define CONFIG_ROOTPATH		"/rootfs"
#define CONFIG_MODE		"ro"
#define CONFIG_BOOTDELAY	undef
#define CONFIG_PREBOOT
#define CONFIG_BOOTARGS		"undef"
#define CONFIG_BOOTCOMMAND	"run set_bootargs; run kernel; run fdt"

/* Add some variables that are not predefined in U-Boot. All entries with
   content "undef" will be updated with a board-specific value in
   board_late_init().

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
	".ubivol_ubi=ubi part TargetFS; ubi create kernel 400000 s; ubi create rootfs\0"
#else
#define EXTRA_UBI
#endif

#define CONFIG_EXTRA_ENV_SETTINGS \
	"console=undef\0" \
	".console_none=setenv console\0" \
	".console_serial=setenv console console=${sercon},${baudrate}\0" \
	".console_display=setenv console console=tty1\0" \
	"login=undef\0" \
	".login_none=setenv login login_tty=null\0" \
	".login_serial=setenv login login_tty=${sercon},${baudrate}\0" \
	".login_display=setenv login login_tty=tty1\0" \
	"mtdparts=undef\0" \
	".mtdparts_std=" MTDPARTS_STD "\0" \
	".network_off=setenv network\0" \
	".network_on=setenv network ip=${ipaddr}:${serverip}:${gatewayip}:${netmask}:${hostname}:${netdev}\0" \
	".network_dhcp=setenv network ip=dhcp\0" \
	"rootfs=undef\0" \
	".rootfs_nfs=setenv rootfs root=/dev/nfs nfsroot=${serverip}:${rootpath}\0" \
	".rootfs_mmc=setenv rootfs root=/dev/mmcblk0p1 rootwait\0" \
	".rootfs_usb=setenv rootfs root=/dev/sda1 rootwait\0" \
	"kernel=undef\0" \
	".kernel_nand=setenv kernel nboot Kernel\0" \
	".kernel_tftp=setenv kernel tftpboot . ${bootfile}\0" \
	".kernel_nfs=setenv kernel nfs . ${serverip}:${rootpath}/${bootfile}\0" \
	".kernel_mmc=setenv kernel mmc rescan\\\\; load mmc 0 . ${bootfile}\0" \
	".kernel_usb=setenv kernel usb start\\\\; load usb 0 . ${bootfile}\0" \
	"fdt=undef\0" \
	"fdtaddr=82000000\0" \
	".fdt_none=setenv fdt bootm\0" \
	".fdt_nand=setenv fdt nand read ${fdtaddr} FDT" BOOT_WITH_FDT \
	".fdt_tftp=setenv fdt tftpboot ${fdtaddr} ${bootfdt}" BOOT_WITH_FDT \
	".fdt_nfs=setenv fdt nfs ${fdtaddr} ${serverip}:${rootpath}/${bootfdt}" BOOT_WITH_FDT \
	".fdt_mmc=setenv fdt mmc rescan\\\\; load mmc 0 ${fdtaddr} ${bootfdt}" BOOT_WITH_FDT \
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
	"arch=fsvybrid\0" \
	"bootfdt=undef\0" \
	"set_bootfdt=setenv bootfdt ${platform}.dtb\0" \
	"set_bootargs=setenv bootargs ${console} ${login} ${mtdparts} ${network} ${rootfs} ${mode} ${init} ${extra}\0"


/************************************************************************
 * DFU (USB Device Firmware Update, requires USB device support)
 ************************************************************************/
/* ###TODO### */


/************************************************************************
 * Linux Support
 ************************************************************************/
#define CONFIG_ZIMAGE_BOOT
#define CONFIG_IMAGE_BOOT

/* Try to patch serial debug port in image within first 16KB of zImage */
#define CONFIG_SYS_PATCH_TTY	0x4000

/* No ATAGs are passed to Linux when using device trees */


/************************************************************************
 * Tools
 ************************************************************************/
#define CONFIG_ADDFSHEADER


/************************************************************************
 * Libraries
 ************************************************************************/
/*#define USE_PRIVATE_LIBGCC*/
#define CONFIG_SYS_64BIT_VSPRINTF	/* Needed for nand_util.c */
#define CONFIG_USE_ARCH_MEMCPY
#define CONFIG_USE_ARCH_MEMMOVE
#define CONFIG_USE_ARCH_MEMSET
#define CONFIG_USE_ARCH_MEMSET32

#endif /* !__FSVYBRID_CONFIG_H */
