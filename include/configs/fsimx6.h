/*
 * Copyright (C) 2014 F&S Elektronik Systeme GmbH
 *
 * Configuration settings for all F&S boards based on i.MX6. This is
 * armStoneA9, QBlissA9, PicoMODA9 and efusA9.
 * Activate with one of the following targets:
 *   make fsimx6_config       Configure for i.MX6 boards
 *   make uboot-fsimx6.nb0    Build uboot-fsimx6.nb0
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of
 * the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston,
 * MA 02111-1307 USA
 *
 * The following addresses are given as offsets of the device.
 *
 * NAND flash layout with separate Kernel/FTD MTD partition 
 * -------------------------------------------------------------------------
 * 0x0000_0000 - 0x0001_FFFF: NBoot: NBoot image, primary copy (128KB)
 * 0x0002_0000 - 0x0003_FFFF: NBoot: NBoot image, secondary copy (128KB)
 * 0x0004_0000 - 0x000F_FFFF: UserDef: User defined data (768KB)
 * 0x0010_0000 - 0x0013_FFFF: Refresh: Swap blocks for refreshing (256KB)
 * 0x0014_0000 - 0x001F_FFFF: UBoot: U-Boot image (768KB)
 * 0x0020_0000 - 0x0023_FFFF: UBootEnv: U-Boot environment (256KB)
 * 0x0024_0000 - 0x007F_FFFF: Kernel: Linux Kernel uImage (5888KB)
 * 0x0080_0000 -         END: TargetFS: Root filesystem (Size - 8MB)
 *
 * NAND flash layout with UBI only, Kernel/FDT in rootfs or kernel volume
 * -------------------------------------------------------------------------
 * 0x0000_0000 - 0x0001_FFFF: NBoot: NBoot image, primary copy (128KB)
 * 0x0002_0000 - 0x0003_FFFF: NBoot: NBoot image, secondary copy (128KB)
 * 0x0004_0000 - 0x000F_FFFF: UserDef: User defined data (768KB)
 * 0x0010_0000 - 0x0013_FFFF: Refresh: Swap blocks for refreshing (256KB)
 * 0x0014_0000 - 0x001F_FFFF: UBoot: U-Boot image (768KB)
 * 0x0020_0000 - 0x0023_FFFF: UBootEnv: U-Boot environment (256KB)
 * 0x0024_0000 -         END: TargetFS: Root filesystem (Size - 2.25MB)
 *
 * END: 0x07FF_FFFF for 128MB, 0x0FFF_FFFF for 256MB, 0x3FFF_FFFF for 1GB
 *
 * Remark:
 * Block size is 128KB. All partition sizes have been chosen to allow for at
 * least one bad block in addition to the required size of the partition. E.g.
 * UBoot is 512KB, but the UBoot partition is 768KB to allow for two bad blocks
 * (256KB) in this memory region.
 */

#ifndef __FSIMX6_CONFIG_H
#define __FSIMX6_CONFIG_H

/************************************************************************
 * High Level Configuration Options
 ************************************************************************/
#define CONFIG_IDENT_STRING " for F&S"	/* We are on an F&S board */

/* CPU, family and board defines */
#define CONFIG_MX6			/* Freescale i.MX6 */
#define CONFIG_FSIMX6			/* on an F&S i.MX6 board */

#define CONFIG_SYS_L2CACHE_OFF
#ifndef CONFIG_SYS_L2CACHE_OFF
#define CONFIG_SYS_L2_PL310
#define CONFIG_SYS_PL310_BASE	L2_PL310_BASE
#endif

#include <asm/arch/imx-regs.h>		/* IRAM_BASE_ADDR, IRAM_SIZE */
//#include "mx6_common.h"			/* Errata, cache handling */

/* Basic input clocks */
#define CONFIG_SYS_MX6_HCLK		24000000

/* Debug options */
 //#define CONFIG_DEBUG_EARLY_SERIAL
 //#define DEBUG
 //#define CONFIG_DEBUG_DUMP
 //#define CONFIG_DEBUG_DUMP_SYMS

#define CONFIG_MXC_GPIO

#define CONFIG_CMD_FUSE
#ifdef CONFIG_CMD_FUSE
#define CONFIG_MXC_OCOTP
#endif

/************************************************************************
 * Command line editor (shell)
 ************************************************************************/
/* Use hush command line parser */
#define CONFIG_SYS_HUSH_PARSER		  /* use "hush" command parser */
#ifdef CONFIG_SYS_HUSH_PARSER
#define CONFIG_SYS_PROMPT_HUSH_PS2	"> "
#endif

/* Allow editing (scroll between commands, etc.) */
#define CONFIG_CMDLINE_EDITING
#define CONFIG_AUTO_COMPLETE

/************************************************************************
 * Miscellaneous configurable options
 ************************************************************************/
#define CONFIG_SYS_LONGHELP		   /* undef to save memory */
#define CONFIG_SYS_CBSIZE	512	   /* Console I/O Buffer Size */
#define CONFIG_SYS_PBSIZE	640	   /* Print Buffer Size */
#define CONFIG_SYS_MAXARGS	16	   /* max number of command args */
#define CONFIG_SYS_BARGSIZE	CONFIG_SYS_CBSIZE /* Boot Arg Buffer Size */
#define CONFIG_UBOOTNB0_SIZE    384	   /* size of uboot.nb0 (in kB) */

#define CONFIG_BOARD_EARLY_INIT_F

/* We use special board_late_init() function to set board specific
   environment variables that can't be set with a fix value here */
#define CONFIG_BOARD_LATE_INIT

/* Allow stopping of automatic booting even if boot delay is zero */
#define CONFIG_ZERO_BOOTDELAY_CHECK

#define CONFIG_DISPLAY_CPUINFO		  /* Show CPU type and speed */
#define CONFIG_DISPLAY_BOARDINFO	  /* Show board information */

/* We need IRQ for blinking LEDs */
//####define CONFIG_USE_IRQ

/* ### Still to check */
#define CONFIG_SYS_HZ		1000

//####define CONFIG_SYS_LOAD_ADDR		CONFIG_LOADADDR

/************************************************************************
 * Memory layout
 ************************************************************************/
/* Physical addresses of DDR and GPU RAM */
#define CONFIG_NR_DRAM_BANKS		1
#define PHYS_SDRAM			MMDC0_ARB_BASE_ADDR
#define PHYS_SDRAM_SIZE			(1u * CONFIG_DDR_MB * 1024 * 1024)

//####define CONFIG_SYS_SDRAM_BASE       PHYS_SDRAM

/* The load address of U-Boot is now independent from the size. Just load it
   at some rather low address in RAM. It will relocate itself to the end of
   RAM automatically when executed. */
#define CONFIG_SYS_PHY_UBOOT_BASE	0x80f00000
#define CONFIG_SYS_UBOOT_BASE		CONFIG_SYS_PHY_UBOOT_BASE

/* MX6 has 128KB (Solo/DualLite) or 256KB (Dual/Quad) of internal SRAM,
   mapped from 0x00900000-0x00901FFFFF/0x00903FFFF */
#define CONFIG_SYS_INIT_RAM_ADDR	(IRAM_BASE_ADDR)
#define CONFIG_SYS_INIT_RAM_SIZE	(IRAM_SIZE)

/* Init value for stack pointer, set at end of internal SRAM, keep room for
   globale data behind stack. */
#define CONFIG_SYS_INIT_SP_OFFSET \
	(CONFIG_SYS_INIT_RAM_SIZE - GENERATED_GBL_DATA_SIZE)
#define CONFIG_SYS_INIT_SP_ADDR \
	(CONFIG_SYS_INIT_RAM_ADDR + CONFIG_SYS_INIT_SP_OFFSET)

/* Size of malloc() pool (heap). Command "ubi part" needs quite a large heap
   if the source MTD partition is large. The size should be large enough to
   also contain a copy of the environment. */
#define CONFIG_SYS_MALLOC_LEN		(2 * 1024 * 1024)
//####define CONFIG_SYS_MALLOC_LEN		(10 * 1024 * 1024)

/* Alignment mask for MMU pagetable: 16kB */
#define CONFIG_SYS_TLB_ALIGN	0xFFFFC000

//####define CONFIG_SYS_ICACHE_OFF

/* Stack */

/* The stack sizes are set up in start.S using the settings below */
#define CONFIG_SYS_STACK_SIZE	(128*1024)  /* 128KB */
#ifdef CONFIG_USE_IRQ
#define CONFIG_STACKSIZE_IRQ	(4*1024)    /* IRQ stack */
#define CONFIG_STACKSIZE_FIQ	(128)       /* No FIQ required */
#endif

/* Allocate 2048KB protected RAM at end of RAM */
#define CONFIG_PRAM		2048

/* Memory test checks all RAM before U-Boot (i.e. leaves last MB with U-Boot
   untested) ### If not set, test from beginning of RAM to before stack. */
//####define CONFIG_SYS_MEMTEST_START CONFIG_SYS_SDRAM_BASE
//####define CONFIG_SYS_MEMTEST_END	(CONFIG_SYS_SDRAM_BASE + OUR_UBOOT_OFFS)

/* For the default load address, use an offset of 8MB. The final kernel (after
   decompressing the zImage) must be at offset 0x8000. But if we load the
   zImage there, the loader code will move it away to make room for the
   uncompressed image at this position. So we'll load it directly to a higher
   address to avoid this additional copying. */
#define CONFIG_SYS_LOAD_OFFS 0x00800000


/************************************************************************
 * Display (LCD)
 ************************************************************************/


/************************************************************************
 * Serial console (UART)
 ************************************************************************/
#define CONFIG_MXC_UART
#define CONFIG_SYS_UART_PORT	3	  /* Default UART port; however we
					     always take the port from NBoot */
#define CONFIG_SERIAL_MULTI		  /* Support several serial lines */
//#define CONFIG_CONSOLE_MUX		  /* Allow several consoles at once */
#define CONFIG_SYS_SERCON_NAME "ttymxc"	  /* Base name for serial devices */
#define CONFIG_SYS_CONSOLE_IS_IN_ENV	  /* Console can be saved in env */
//#define CONFIG_BAUDRATE	38400	  /* Default baudrate */
#define CONFIG_BAUDRATE		115200	  /* Default baudrate */
#define CONFIG_SYS_BAUDRATE_TABLE	{9600, 19200, 38400, 57600, 115200}


/************************************************************************
 * FAT support
 ************************************************************************/
#define CONFIG_DOS_PARTITION
#define CONFIG_SUPPORT_VFAT


/************************************************************************
 * Linux support
 ************************************************************************/
#define CONFIG_ZIMAGE_BOOT
#define CONFIG_IMAGE_BOOT

/* Try to patch serial debug port in image within first 16KB of zImage */
#define CONFIG_SYS_PATCH_TTY 0x4000

/* ATAGs passed to Linux */
#define CONFIG_SETUP_MEMORY_TAGS	  /* Memory setup */
#define CONFIG_CMDLINE_TAG		  /* Command line */
#undef CONFIG_INITRD_TAG		  /* No initrd */
#define CONFIG_REVISION_TAG		  /* Board revision */
#define CONFIG_FSHWCONFIG_TAG		  /* Hardware config (NBoot-Args) */
#define CONFIG_FSM4CONFIG_TAG		  /* M4 image and config (M4-Args) */


/************************************************************************
 * Real Time Clock (RTC)
 ************************************************************************/
//###TODO

/************************************************************************
 * Command definition
 ************************************************************************/
/* We don't include
     #include <config_cmd_default.h>
   but instead we define all the commands that we want ourselves. However
   <config_cmd_defaults.h> (note the s) is always included nonetheless. This
   always sets the following defines: CONFIG_CMD_BOOTM, CONFIG_CMD_CRC32,
   CONFIG_CMD_GO, CONFIG_CMD_EXPORTENV, CONFIG_CMD_IMPORTENV. */
#undef CONFIG_CMD_AMBAPP	/* no support to show AMBA plug&play devices */
#define CONFIG_CMD_ASKENV	/* ask user for variable */
#define CONFIG_CMD_BDI		/* board information (bdinfo) */
#undef CONFIG_CMD_BEDBUG	/* no PPC bedbug debugging support */
#undef CONFIG_CMD_BLINK		/* no support for blinking LEDs */
#undef CONFIG_CMD_BMP		/* no old BMP, use new display support */
#define CONFIG_CMD_BOOTD	/* boot default target */
#undef CONFIG_CMD_BOOTLDR	/* no ldr support for blackfin */
#define CONFIG_CMD_BOOTZ	/* boot zImage */
#define CONFIG_CMD_CACHE	/* switch cache on and off */
#undef CONFIG_CMD_CDP		/* no support for CISCOs CDP network config */
#define CONFIG_CMD_CONSOLE	/* console information (coninfo) */
#undef CONFIG_CMD_CPLBINFO	/* no display of PPC CPLB tables */
#undef CONFIG_CMD_CRAMFS	/* no support for CRAMFS filesystem */
//####define CONFIG_CMD_DATE		/* no Date command */
#undef CONFIG_CMD_DFU		/* no support for device firmware update */ 
#define CONFIG_CMD_DHCP		/* support TFTP boot after DHCP request */
#undef CONFIG_CMD_DIAG		/* no support for board selftest */
#undef CONFIG_CMD_DNS		/* no lookup of IP via a DNS name server */
#undef CONFIG_CMD_DTT		/* no digital thermometer and thermostat */
#define CONFIG_CMD_ECHO		/* echo arguments */
#define CONFIG_CMD_EDITENV	/* allow editing of environment variables */
#undef CONFIG_CMD_EEPROM	/* no EEPROM support */
#undef CONFIG_CMD_ELF		/* no support to boot ELF images */
#define CONFIG_CMD_EXT2		/* support for EXT2 commands */
#define CONFIG_CMD_EXT4		/* support for EXT4 commands */
#define CONFIG_CMD_FAT		/* support for FAT commands */
#undef CONFIG_CMD_FDC		/* no floppy disc controller */
#undef CONFIG_CMD_FDOS		/* no support for DOS from floppy disc */
#undef CONFIG_CMD_FITUPD	/* no update from FIT image */
#undef CONFIG_CMD_FLASH		/* no NOR flash (flinfo, erase, protect) */
#undef CONFIG_CMD_FPGA		/* no FPGA configuration support */
#undef CONFIG_CMD_GPIO		/* no support to set GPIO pins */
#undef CONFIG_CMD_GREPENV	/* no support to search in environment */
#undef CONFIG_CMD_HWFLOW	/* no switching of serial flow control */
#undef CONFIG_CMD_I2C		/* no I2C support */
#undef CONFIG_CMD_IDE		/* no IDE disk support */
#define CONFIG_CMD_IMI		/* image information (iminfo) */
#undef CONFIG_CMD_IMLS		/* no support to list all found images */
#undef CONFIG_CMD_IMMAP		/* no support for PPC immap table */
#define CONFIG_CMD_INI		/* support INI files to init environment */
#undef CONFIG_CMD_IRQ		/* no interrupt support */
#define CONFIG_CMD_ITEST	/* Integer (and string) test */
#undef CONFIG_CMD_JFFS2		/* no support for JFFS2 filesystem */
#undef CONFIG_CMD_LDRINFO	/* no ldr support for blackfin */
#undef CONFIG_CMD_LED		/* no LED support */
#undef CONFIG_CMD_LICENSE	/* no support to show GPL license */
#undef CONFIG_CMD_LOADB		/* no serial load of binaries (loadb) */
#undef CONFIG_CMD_LOADS		/* no serial load of s-records (loads) */
#undef CONFIG_CMD_MD5SUM	/* no support for md5sum checksums */
#define CONFIG_CMD_MEMORY	/* md mm nm mw cp cmp crc base loop mtest */
#undef CONFIG_CMD_MFSL		/* no support for Microblaze FSL */
#define CONFIG_CMD_MII		/* support for listing MDIO busses */
#define CONFIG_CMD_MISC		/* miscellaneous commands (sleep) */
#define CONFIG_CMD_MMC		/* support for SD/MMC cards */
#undef CONFIG_CMD_MMC_SPI	/* no access of MMC cards in SPI mode */
#undef CONFIG_CMD_MOVI		/* no support for MOVI NAND flash memories */
#define CONFIG_CMD_MTDPARTS	/* support MTD partitions (mtdparts, chpart) */
#define CONFIG_CMD_NAND		/* support for common NAND flash memories */
#define CONFIG_CMD_NET		/* support BOOTP and TFTP (bootp, tftpboot) */
#define CONFIG_CMD_NFS		/* support download via NFS */
#undef CONFIG_CMD_ONENAND	/* no support for ONENAND flash memories */
#undef CONFIG_CMD_OTP		/* no support for one-time-programmable mem */
#undef CONFIG_CMD_PART		/* no support for partition info */
#undef CONFIG_CMD_PCI		/* no PCI support */
#undef CONFIG_CMD_PCMCIA	/* no support for PCMCIA cards */
#define CONFIG_CMD_PING		/* support ping command */
#undef CONFIG_CMD_PORTIO	/* no port commands (in, out) */
#undef CONFIG_CMD_PXE		/* no support for PXE files from pxelinux */
#undef CONFIG_CMD_RARP		/* no support for booting via RARP */
#undef CONFIG_CMD_REGINFO	/* no register support on ARM, only PPC */
#undef CONFIG_CMD_REISER	/* no support for reiserfs filesystem */
#define CONFIG_CMD_RUN		/* run command in env variable */
#undef CONFIG_CMD_SATA		/* no support for SATA disks */
#define CONFIG_CMD_SAVEENV	/* allow saving environment to NAND */
#undef CONFIG_CMD_SAVES		/* no support for serial uploads (saving) */
#undef CONFIG_CMD_SCSI		/* no support for SCSI disks */
#undef CONFIG_CMD_SDRAM		/* support SDRAM chips via I2C */
#define CONFIG_CMD_SETEXPR	/* set variable by evaluating an expression */
#undef CONFIG_CMD_SETGETDCR	/* no support for PPC DCR register */
#undef CONFIG_CMD_SF		/* no support for serial SPI flashs */
#undef CONFIG_CMD_SHA1SUM	/* no support for sha1sum checksums */
//####define CONFIG_CMD_SNTP		/* allow synchronizing RTC via network */
#define CONFIG_CMD_SOURCE	/* source support (was autoscr)	*/
#undef CONFIG_CMD_SPI		/* no SPI support */
#undef CONFIG_CMD_SPIBOOTLDR	/* no ldr support over SPI for blackfin */
#undef CONFIG_CMD_SPL		/* no SPL support (kernel parameter images) */
#undef CONFIG_CMD_STRINGS	/* no support to show strings */
#undef CONFIG_CMD_TERMINAL	/* no terminal emulator */
#undef CONFIG_CMD_TFTPPUT	/* no sending of TFTP files (tftpput) */
#undef CONFIG_CMD_TFTPSRV	/* no acting as TFTP server (tftpsrv) */
#undef CONFIG_CMD_TIME		/* no support to time command execution */
#define CONFIG_CMD_TIMER	/* support system timer access */
#undef CONFIG_CMD_TPM		/* no support for TPM */
#undef CONFIG_CMD_TSI148	/* no support for Turndra Tsi148 */
#define CONFIG_CMD_UBI		/* support for unsorted block images (UBI) */
#define CONFIG_CMD_UBIFS	/* support for UBIFS filesystem */
#undef CONFIG_CMD_UNIVERSE	/* no support for Turndra Universe */
#define CONFIG_CMD_UNZIP	/* have unzip command */
#define CONFIG_CMD_UPDATE	/* support automatic update/install */
#define CONFIG_CMD_USB		/* USB host support */
#define CONFIG_CMD_XIMG		/* Load part of Multi Image */
#undef CONFIG_CMD_ZFS		/* no support for ZFS filesystem */
#undef CONFIG_CMD_ZIP		/* no support to zip memory region */

//####define CONFIG_OF_LIBFDT	/* device tree support (fdt) */
#undef CONFIG_LOGBUFFER		/* no support for log files */
#undef CONFIG_MP		/* no multi processor support */
#undef CONFIG_ID_EEPROM		/* no EEPROM for ethernet MAC */
#undef CONFIG_DATAFLASH_MMC_SELECT /* no dataflash support */
#undef CONFIG_S3C_USBD		/* no USB device support */
#undef CONFIG_YAFFS2		/* no support for YAFFS2 filesystem commands */

/* Supported Filesystems; this is independent from the supported commands */
#define CONFIG_FS_FAT		/* support for FAT/VFAT filesystem */
#define CONFIG_FS_EXT4		/* support for EXT2/3/4 filesystem */


/************************************************************************
 * BOOTP options
 ************************************************************************/
#define CONFIG_BOOTP_SUBNETMASK
#define CONFIG_BOOTP_GATEWAY
#define CONFIG_BOOTP_HOSTNAME
#define CONFIG_BOOTP_BOOTPATH


/************************************************************************
 * Ethernet
 ************************************************************************/
#define CONFIG_FEC_MXC
#define CONFIG_MII		1
#define CONFIG_ARP_TIMEOUT	200UL
#define CONFIG_SYS_DISCOVER_PHY

#define IMX_FEC_BASE		ENET_BASE_ADDR
#define CONFIG_FEC_XCV_TYPE	RGMII
#define CONFIG_FEC_MXC_PHYADDR	4

#define CONFIG_PHYLIB
#define CONFIG_PHY_ATHEROS


/************************************************************************
 * CPU (PLL) timings
 ************************************************************************/
/* Already done in NBoot */


/************************************************************************
 * RAM timing
 ************************************************************************/
/* Already done in NBoot */


/************************************************************************
 * USB host
 ************************************************************************/
#define CONFIG_USB_EHCI			/* Use EHCI driver (USB2.0) */
#define CONFIG_USB_EHCI_MX6		/* This is MX6 EHCI */
#define CONFIG_MXC_USB_PORT 1		/* Use USB port 1 */
#define CONFIG_MXC_USB_PORTSC (PORT_PTS_UTMI | PORT_PTS_PTW)
#define CONFIG_MXC_USB_FLAGS 0
#define CONFIG_USB_STORAGE

/************************************************************************
 * USB device
 ************************************************************************/


/************************************************************************
 * Keyboard
 ************************************************************************/


/************************************************************************
 * QUAD_SPI
 ************************************************************************/


/************************************************************************
 * NOR Flash
 ************************************************************************/
/* No support for NOR flash */
#define CONFIG_SYS_NO_FLASH	1	  /* no NOR flash */


/************************************************************************
 * SD/MMC card support
 ************************************************************************/
#define CONFIG_MMC			  /* SD/MMC support */
#define CONFIG_GENERIC_MMC		  /* with the generic driver model, */
#define CONFIG_FSL_ESDHC		  /* use Freescale ESDHC driver */
#define CONFIG_FSL_USDHC		  /* with USDHC modifications */
#define CONFIG_SYS_FSL_ESDHC_ADDR 0	  /* Not used */
#define CONFIG_SYS_FSL_USDHC_NUM       1

//#define CONFIG_ESDHC_NO_SNOOP		1
//#define CONFIG_SYS_FSL_ERRATUM_ESDHC135
//#define CONFIG_SYS_FSL_ERRATUM_ESDHC111
//#define CONFIG_SYS_FSL_ERRATUM_ESDHC_A001


/************************************************************************
 * NAND flash organization (incl. JFFS2 and UBIFS)
 ************************************************************************/

#ifdef CONFIG_CMD_NAND

/* NAND stuff */
//#####define CONFIG_NAND_MXS
#define CONFIG_SYS_NAND_BASE		0x40000000

/* DMA stuff, needed for GPMI/MXS NAND support */
//####define CONFIG_APBH_DMA
//####define CONFIG_APBH_DMA_BURST
//####define CONFIG_APBH_DMA_BURST8

#define CONFIG_SYS_USE_NANDFLASH

#endif

#ifdef CONFIG_SYS_USE_NANDFLASH

/* To avoid that NBoot is erased inadvertently, we define a skip region in the
   first NAND device that can not be written and always reads as 0xFF. However
   if value CONFIG_SYS_MAX_NAND_DEVICE is set to 2, the NBoot region is shown
   as a second NAND device with just that size. This makes it easier to have a
   different ECC strategy and software write protection for NBoot. */
#define CONFIG_SYS_MAX_NAND_DEVICE	1
//#define CONFIG_SYS_MAX_NAND_DEVICE	2

/* Chips per device; all chips must be the same type; if different types
   are necessary, they must be implemented as different NAND devices */
#define CONFIG_SYS_NAND_MAX_CHIPS	2

/* Define if you want to support nand chips that comply to ONFI spec */
#define CONFIG_SYS_NAND_ONFI_DETECTION

/* Actually perform block refresh if pages degrade too much. Use the given
   blocks in decreasing order. ### TODO ### */
#undef CONFIG_NAND_REFRESH
#define CONFIG_SYS_NAND_BACKUP_START_BLOCK	9
#define CONFIG_SYS_NAND_BACKUP_END_BLOCK	8

/* Support JFFS2 in NAND (commands: fsload, ls, fsinfo) */
#define CONFIG_JFFS2_NAND

/* Don't support YAFFS in NAND (nand write.yaffs) */
#undef CONFIG_CMD_NAND_YAFFS

/* No support for nand write.trimffs to suppress writing of pages at the end
   of the image that only contain 0xFF bytes */
#undef CONFIG_CMD_NAND_TRIMFFS

/* No support hardware lock/unlock of NAND flashs */
#undef CONFIG_CMD_NAND_LOCK_UNLOCK

/* Don't show MTD net sizes (without bad blocks) just the nominal size */
#undef CONFIG_CMD_MTDPARTS_SHOW_NET_SIZES

/* Don't increase partition sizes to compensate for bad blocks */
#undef CONFIG_CMD_MTDPARTS_SPREAD

#define CONFIG_MTD_DEVICE		  /* Create MTD device */
#define CONFIG_MTD_PARTITIONS		  /* Required for UBI */
#define CONFIG_RBTREE			  /* Required for UBI */
#define CONFIG_LZO			  /* Required for UBI */
#endif

//#define CONFIG_FLASH_HEADER
//#define CONFIG_FLASH_HEADER_OFFSET 0x400


/************************************************************************
 * Environment
 ************************************************************************/
#define CONFIG_ENV_IS_IN_NAND		  /* Environment is in NAND flash */

/* Environment settings for large blocks (128KB). The environment is held in
   the heap, so keep the real env size small to not waste malloc space. */
#define ENV_SIZE_DEF_LARGE   0x00004000	  /* 16KB */
#define ENV_RANGE_DEF_LARGE  0x00040000   /* 2 blocks = 256KB */
#define ENV_OFFSET_DEF_LARGE 0x00200000   /* See NAND layout above */

/* When saving the environment, we usually have a short period of time between
   erasing the NAND region and writing the new data where no valid environment
   is available. To avoid this time, we can save the environment alternatively
   to two different locations in the NAND flash. Then at least one of the
   environments is always valid. Currently we don't use this feature. */
//#define CONFIG_SYS_ENV_OFFSET_REDUND   0x001C0000

//#define CONFIG_ETHADDR_BASE	00:05:51:07:55:83
#define CONFIG_ETHADDR		00:05:51:07:55:83
#define CONFIG_ETHPRIME		"FEC"
#define CONFIG_NETMASK		255.255.255.0
#define CONFIG_IPADDR		10.0.0.252
#define CONFIG_SERVERIP		10.0.0.122
#define CONFIG_GATEWAYIP	10.0.0.5
#define CONFIG_BOOTFILE		"uImage"
#define CONFIG_ROOTPATH		"/rootfs"
#define CONFIG_MODE		"ro"
#define CONFIG_BOOTDELAY	undef
#define CONFIG_PREBOOT
#define CONFIG_BOOTARGS		"undef"
#define CONFIG_BOOTCOMMAND	"run set_bootargs; run kernel; bootm"

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
#define MTDPARTS_PART3		"5888K(Kernel)ro"
#define MTDPARTS_PART4		"-(TargetFS)"
#define MTDPARTS_STD		"setenv mtdparts mtdparts=" MTDPARTS_PART1 "," MTDPARTS_PART2 "," MTDPARTS_PART3 "," MTDPARTS_PART4
#define MTDPARTS_UBIONLY	"setenv mtdparts mtdparts=" MTDPARTS_PART1 "," MTDPARTS_PART2 "," MTDPARTS_PART4

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
#ifdef CONFIG_CMD_UBI
#ifdef CONFIG_CMD_UBIFS
#define EXTRA_UBIFS \
	"_kernel_ubifs=setenv kernel ubi part TargetFS\\\\; ubifsmount rootfs\\\\; ubifsload . /boot/${bootfile}\0"
#else
#define EXTRA_UBIFS
#endif
#define EXTRA_UBI EXTRA_UBIFS \
	"_mtdparts_ubionly=" MTDPARTS_UBIONLY "\0" \
	"_rootfs_ubifs=setenv rootfs rootfstype=ubifs ubi.mtd=TargetFS root=ubi0:rootfs\0" \
	"_kernel_ubi=setenv kernel ubi part TargetFS\\\\; ubi read . kernel\0" \
	"_ubivol_std=ubi part TargetFS; ubi create rootfs\0" \
	"_ubivol_ubi=ubi part TargetFS; ubi create kernel 400000 s; ubi create rootfs\0"
#else
#define EXTRA_UBI
#endif

#define CONFIG_EXTRA_ENV_SETTINGS \
	"console=undef\0" \
	"_console_none=setenv console\0" \
	"_console_serial=setenv console console=${sercon},${baudrate}\0" \
	"_console_display=setenv console console=/dev/tty1\0" \
	"login=undef\0" \
	"_login_none=setenv login login_tty=null\0" \
	"_login_serial=setenv login login_tty=${sercon},${baudrate}\0" \
	"_login_display=setenv login login_tty=/dev/tty1\0" \
	"mtdparts=undef\0" \
	"_mtdparts_std=" MTDPARTS_STD "\0" \
	"_network_off=setenv network\0"					\
	"_network_on=setenv network ip=${ipaddr}:${serverip}:${gatewayip}:${netmask}:${hostname}:${netdev}\0" \
	"_network_dhcp=setenv network ip=dhcp\0" \
	"rootfs=undef\0" \
	"_rootfs_nfs=setenv rootfs root=/dev/nfs nfsroot=${rootpath}\0" \
	"_rootfs_mmc=setenv rootfs root=/dev/mmcblk0p1\0" \
	"_rootfs_usb=setenv rootfs root=/dev/sda1\0" \
	"fsload=undef\0" \
	"_fsload_fat=setenv fsload fatload\0" \
	"_fsload_ext2=setenv fsload ext2load\0" \
	"kernel=undef\0" \
	"_kernel_nand=setenv kernel nboot Kernel\0" \
	"_kernel_tftp=setenv kernel tftpboot . ${bootfile}\0" \
	"_kernel_nfs=setenv kernel nfs . ${serverip}:${rootpath}/${bootfile}\0" \
	"_kernel_mmc=setenv kernel mmc rescan\\\\; ${fsload} mmc 0 . ${bootfile}\0" \
	"_kernel_usb=setenv kernel usb start\\\\; ${fsload} usb 0 . ${bootfile}\0" \
	EXTRA_UBI \
	"mode=undef\0" \
	"_mode_rw=setenv mode rw\0" \
	"_mode_ro=setenv mode ro\0" \
	"netdev=eth0\0" \
	"init=undef\0" \
	"_init_init=setenv init\0" \
	"_init_linuxrc=setenv init init=linuxrc\0" \
	"sercon=undef\0" \
	"installcheck=undef\0" \
	"updatecheck=undef\0" \
	"recovercheck=undef\0" \
	"platform=undef\0" \
	"arch=fsimx6\0" \
	"set_bootargs=setenv bootargs ${console} ${login} ${mtdparts} ${network} ${rootfs} ${mode} ${init} ${extra}\0"

/* allow to overwrite serial and ethaddr */
#define CONFIG_ENV_OVERWRITE
#define CONFIG_CONS_INDEX              1

/************************************************************************
 * LEDs
 ************************************************************************/


/************************************************************************
 * Tools
 ************************************************************************/
#define CONFIG_ADDFSHEADER	1


/************************************************************************
 * Libraries
 ************************************************************************/
//#define USE_PRIVATE_LIBGCC
#define CONFIG_SYS_64BIT_VSPRINTF	  /* needed for nand_util.c */
#define CONFIG_USE_ARCH_MEMCPY
#define CONFIG_USE_ARCH_MEMMOVE
#define CONFIG_USE_ARCH_MEMSET
#define CONFIG_USE_ARCH_MEMSET32

#endif /* !__FSIMX6_CONFIG_H */
