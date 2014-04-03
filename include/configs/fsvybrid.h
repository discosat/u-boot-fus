/*
 * (C) Copyright 2014
 * F&S Elektronik Systeme GmbH
 *
 * Configuration settings for all F&S boards based on Vybrid. This is
 * armStoneA5, PicoCOMA5 and NetDCUA5.
 * Activate with one of the following targets:
 *   make fsvybrid_config       Configure for Vybrid boards
 *   make fsvybrid              Configure for Vybrid boards and build
 *
 * See file CREDITS for list of people who contributed to this
 * project.
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
 * NAND flash layout (Block size 128KB) (128MB, 1GB)
 * -------------------------------------------------------------------------
 * 0x0000_0000 - 0x0001_FFFF: NBoot: NBoot image, primary copy (128KB)
 * 0x0002_0000 - 0x0003_FFFF: NBoot: NBoot image, secondary copy (128KB)
 * 0x0004_0000 - 0x000F_FFFF: Cortex-M4 image (768KB)
 * 0x0010_0000 - 0x0017_FFFF: UBoot: U-Boot image (512KB)
 * 0x0018_0000 - 0x001B_FFFF: UBootEnv: U-Boot environment (256KB)
 * 0x001C_0000 - 0x003F_FFFF: UserDef: User defined data (2.25MB = 2304KB)
 * 0x0040_0000 - 0x007F_FFFF: Kernel: Linux Kernel uImage (4MB)
 * 0x0080_0000 - 0x07FF_FFFF: TargetFS: Root filesystem (120MB if 128MB)
 * 0x0080_0000 - 0x0FFF_FFFF: TargetFS: Root filesystem (248MB if 256MB)
 * 0x0080_0000 - 0x3FFF_FFFF: TargetFS: Root filesystem (1016MB if 1GB)
 *
 * Remark:
 * All partition sizes have been chosen to allow for at least one bad block in
 * addition to the required size of the partition. E.g. UBoot is 384KB, but
 * the UBoot partition is 512KB to allow for one bad block (128KB) in this
 * memory region.
 *
 * RAM layout (RAM starts at 0x80000000)
 * -------------------------------------------------------------------------
 * 0x0000_0000 - 0x0000_00FF: Free RAM
 * 0x0000_0100 - 0x0000_07FF: bi_boot_params (ATAGs)
 * 0x0000_1000 - 0x0000_105F: NBoot Args
 * 0x0000_1060 - 0x0000_7FFF: Free RAM
 * 0x0000_8000 - 0x007F_FFFF: Linux zImage
 * 0x0080_0000 - 0x00FF_FFFF: Linux BSS (decompressed kernel)
 * 0x0100_0000 - 0x07FF_FFFF: Free RAM + U-Boot (if 128MB)
 * 0x0100_0000 - 0x0FFF_FFFF: Free RAM + U-Boot (if 256MB)
 * 0x0100_0000 - 0x1FFF_FFFF: Free RAM + U-Boot (if 512MB)
 *
 * NBoot loads U-Boot to a rather low RAM address. Then U-Boot computes its
 * final size and relocates itself to the end of RAM. This new ARM specific
 * loader scheme was introduced in u-boot-2010.12. It only requires to set
 * gd->ram_base correctly and to privide a board-specific function dram_init()
 * that sets gd->ram_size to the actually available RAM. For more details see
 * arch/arm/lib/board.c.
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
 *
 */

#ifndef __CONFIG_H
#define __CONFIG_H

/* High Level Configuration Options */

/************************************************************************
 * High Level Configuration Options
 ************************************************************************/
#define CONFIG_IDENT_STRING " for F&S"	  /* We are on an F&S board */

/* CPU, family and board defines */
#define CONFIG_VYBRID			  /* Freescale Vybrid */
#define CONFIG_FSVYBRID			  /* on an F&S Vybrid Board */

/* Basic input clocks */
#define CONFIG_SYS_VYBRID_HCLK		24000000
#define CONFIG_SYS_VYBRID_CLK32		32768

/* Timer */
#define FTM_BASE_ADDR			FTM0_BASE_ADDR
#define CONFIG_TMR_USEPIT

#define CONFIG_BOARD_EARLY_INIT_F //####kann evtl. wieder weg


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
//### #undef CONFIG_CMDLINE_EDITING !??!?!
#undef CONFIG_AUTO_COMPLETE

/************************************************************************
 * Miscellaneous configurable options
 ************************************************************************/
#define CONFIG_SYS_LONGHELP		   /* undef to save memory */
#define CONFIG_SYS_CBSIZE	512	   /* Console I/O Buffer Size */
#define CONFIG_SYS_PBSIZE	640	   /* Print Buffer Size */
#define CONFIG_SYS_MAXARGS	16	   /* max number of command args */
#define CONFIG_SYS_BARGSIZE	CONFIG_SYS_CBSIZE /* Boot Arg Buffer Size */
#define CONFIG_UBOOTNB0_SIZE    384	   /* size of uboot.nb0 (in kB) */

/* We use special board_late_init() function to set board specific
   environment variables that can't be set with a fix value here */
#define CONFIG_BOARD_LATE_INIT

/* Allow stopping of automatic booting even if boot delay is zero */
#define CONFIG_ZERO_BOOTDELAY_CHECK

#define CONFIG_DISPLAY_CPUINFO		  /* Show CPU type and speed */
#define CONFIG_DISPLAY_BOARDINFO	  /* Show board information */

/* No need for IRQ/FIQ stuff; this also obsoletes the IRQ/FIQ stacks */
#undef CONFIG_USE_IRQ

/* ### Still to check */
#define CONFIG_SYS_HZ		1000


/************************************************************************
 * Memory layout
 ************************************************************************/
/* Use MMU */
//#define CONFIG_ENABLE_MMU //#### MMU 1:1 Test
//#define CONFIG_SOFT_MMUTABLE

/* Physical addresses of DDR and GPU RAM */
#define CONFIG_NR_DRAM_BANKS		1
#define PHYS_SDRAM_0			0x80000000 /* DDR */
#define PHYS_SDRAM_0_SIZE		(128 * 1024 * 1024) /* varying */

#define PHYS_GPURAM			0x3F400000 /* GPURAM */
#define PHYS_GPURAM_SIZE		(512 * 1024)

/* The load address of U-Boot is now independend from the size. Just load it
   at some rather low address in RAM. It will relocate itself to the end of
   RAM automatically when executed. */
#define CONFIG_SYS_PHY_UBOOT_BASE	0x80f00000
#define CONFIG_SYS_UBOOT_BASE		CONFIG_SYS_PHY_UBOOT_BASE

/* We have at least 256KB internal SRAM, mapped from 0x3F000000-0x3F03FFFF */
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

/* Alignment mask for MMU pagetable: 16kB */
#define CONFIG_SYS_TLB_ALIGN	0xFFFFC000

//####define CONFIG_SYS_ICACHE_OFF
#define CONFIG_SYS_CACHELINE_SIZE	64

#include <asm/arch/vybrid-regs.h> //####notwendig?

/* Stack */

/* The stack sizes are set up in start.S using the settings below */
#define CONFIG_SYS_STACK_SIZE	(128*1024)  /* 128KB */
#ifdef CONFIG_USE_IRQ
#define CONFIG_STACKSIZE_IRQ	(4*1024)  /* IRQ stack */
#define CONFIG_STACKSIZE_FIQ	(4*1024)  /* FIQ stack */
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
#if 0
#define CONFIG_CMD_LCD			  /* Support lcd settings command */
//#define CONFIG_CMD_WIN			  /* Window layers, alpha blending */
//#define CONFIG_CMD_CMAP			  /* Support CLUT pixel formats */
#define CONFIG_CMD_DRAW			  /* Support draw command */
//#define CONFIG_CMD_ADRAW		  /* Support alpha draw commands */
//#define CONFIG_CMD_BMINFO		  /* Provide bminfo command */
//#define CONFIG_XLCD_PNG			  /* Support for PNG bitmaps */
#define CONFIG_XLCD_BMP			  /* Support for BMP bitmaps */
//#define CONFIG_XLCD_JPG			  /* Support for JPG bitmaps */
#define CONFIG_XLCD_EXPR		  /* Allow expressions in coordinates */
#define CONFIG_XLCD_CONSOLE		  /* Support console on LCD */
#define CONFIG_XLCD_CONSOLE_MULTI	  /* Define a console on each window */
#define CONFIG_XLCD_FBSIZE 0x00100000	  /* 1 MB default framebuffer pool */
#define CONFIG_S3C64XX_XLCD		  /* Use S3C64XX lcd driver */
#define CONFIG_S3C64XX_XLCD_PWM 1	  /* Use PWM1 for backlight */

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
 * Serial console (UART)
 ************************************************************************/
#define CONFIG_VYBRID_UART		  /* Use vybrid uart driver */
#define CONFIG_SYS_UART_PORT	1	  /* Default UART port; however we
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
#define CONFIG_REVISION_TAG		  /* Board revision & features */


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
#define CONFIG_CMD_DHCP		/* support TFTP boot after DHCP request */
#undef CONFIG_CMD_DIAG		/* no support for board selftest */
#undef CONFIG_CMD_DNS		/* no lookup of IP via a DNS name server */
#undef CONFIG_CMD_DTT		/* no digital thermometer and thermostat */
#define CONFIG_CMD_ECHO		/* echo arguments */
#define CONFIG_CMD_EDITENV	/* allow editing of environment variables */
#undef CONFIG_CMD_EEPROM	/* no EEPROM support */
#undef CONFIG_CMD_ELF		/* no support to boot ELF images */
#define CONFIG_CMD_EXT2		/* support for EXT2 filesystem */
#define CONFIG_CMD_FAT		/* support for FAT/VFAT filesystem */
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
#undef CONFIG_CMD_IRQ		/* no interrupt support */
#undef CONFIG_CMD_ITEST		/* no integer (and string) test */
#undef CONFIG_CMD_JFFS2		/* no support for JFFS2 filesystem */
#undef CONFIG_CMD_LDRINFO	/* no ldr support for blackfin */
#define CONFIG_CMD_LED		/* LED support */
#undef CONFIG_CMD_LOADB		/* no serial load of binaries (loadb) */
#undef CONFIG_CMD_LOADS		/* no serial load of s-records (loads) */
#undef CONFIG_CMD_LICENSE	/* no support to show GPL license */
#undef CONFIG_CMD_MD5SUM	/* no support for md5sum checksums */
#define CONFIG_CMD_MEMORY	/* md mm nm mw cp cmp crc base loop mtest */
#undef CONFIG_CMD_MFSL		/* no support for Microblaze FSL */
#define CONFIG_CMD_MII		/* support for listing MDIO busses */
#define CONFIG_CMD_MISC		/* miscellaneous commands (sleep) */
#define CONFIG_CMD_MMC		/* support for SD/MMC cards */
#undef CONFIG_CMD_MMC_SPI	/* no access of MMC cards in SPI mode */
#undef CONFIG_CMD_MOVI		/* no support for MOVI NAND flash memories */
#undef CONIFG_CMD_MP		/* no multi processor support */
#define CONFIG_CMD_MTDPARTS	/* support MTD partitions (mtdparts, chpart) */
#define	CONFIG_CMD_NAND		/* support for common NAND flash memories */
#define CONFIG_CMD_NET		/* support BOOTP and TFTP (bootp, tftpboot) */
#define CONFIG_CMD_NFS		/* support download via NFS */
#undef CONFIG_CMD_ONENAND	/* no support for ONENAND flash memories */
#undef CONFIG_CMD_OTP		/* no support for one-time-programmable mem */
#undef CONFIG_CMD_PCI		/* no PCI support */
#undef CONFIG_CMD_PCMCIA	/* no support for PCMCIA cards */
#define CONFIG_CMD_PING		/* support ping command */
#undef CONFIG_CMD_PORTIO	/* no port commands (in, out) */
#undef CONFIG_CMD_PXE		/* no support for PXE files from pxelinux */
#undef CONFIG_CMD_RARP		/* no support for booting via RARP */
#undef CONFIG_CMD_REGINFO	/* no register support on ARM, only PPC */
#undef CONFIG_CMD_REISER	/* no support for reiserfs filesystem */
#define CONFIG_CMD_RUN		/* run command in env variable	*/
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
#undef CONFIG_CMD_TPM		/* no support for TPM */
#undef CONFIG_CMD_TSI148	/* no support for Turndra Tsi148 */
#define CONFIG_CMD_UBI		/* support for unsorted block images (UBI) */
#undef CONFIG_CMD_UBIFS		/* no support for UBIFS filesystem */
#undef CONFIG_CMD_UNIVERSE	/* no support for Turndra Universe */
#define CONFIG_CMD_UNZIP	/* have unzip command */
#define CONFIG_CMD_UPDATE	/* support automatic update/install */
//#####define CONFIG_CMD_USB		/* USB host support */
#undef CONFIG_CMD_XIMG		/* no support to load part of Multi Image */

//####define CONFIG_OF_LIBFDT	/* device tree support (fdt) */
#undef CONFIG_LOGBUFFER		/* no support for log files */
#undef CONFIG_ID_EEPROM		/* no EEPROM for ethernet MAC */
#undef CONFIG_DATAFLASH_MMC_SELECT /* no dataflash support */
#undef CONFIG_S3C_USBD		/* no USB device support */
#undef CONFIG_YAFFS2		/* no support for YAFFS2 filesystem commands */


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
#define CONFIG_MCFFEC
#define CONFIG_MII		1
#define CONFIG_MII_INIT		1
#define CONFIG_FS_VYBRID_PLL_ETH // ### undefine if external quartz is used for ETH clock
#define CONFIG_SYS_DISCOVER_PHY
#define CONFIG_SYS_RX_ETH_BUFFER	8
#define CONFIG_SYS_FAULT_ECHO_LINK_DOWN

#define CONFIG_SYS_FEC0_PINMUX	0
#define CONFIG_SYS_FEC1_PINMUX	0
#define CONFIG_SYS_FEC0_IOBASE	MACNET0_BASE_ADDR
#define CONFIG_SYS_FEC1_IOBASE	MACNET1_BASE_ADDR
#define CONFIG_SYS_FEC0_MIIBASE	MACNET0_BASE_ADDR
#define CONFIG_SYS_FEC1_MIIBASE	MACNET1_BASE_ADDR
#define MCFFEC_TOUT_LOOP 50000
#define CONFIG_HAS_ETH1

#define CONFIG_OVERWRITE_ETHADDR_ONCE


/* If CONFIG_SYS_DISCOVER_PHY is not defined - hardcoded */
#ifndef CONFIG_SYS_DISCOVER_PHY
#define FECDUPLEX	FULL
#define FECSPEED	_100BASET
#else
#ifndef CONFIG_SYS_FAULT_ECHO_LINK_DOWN
#define CONFIG_SYS_FAULT_ECHO_LINK_DOWN
#endif
#endif	/* CONFIG_SYS_DISCOVER_PHY */

#define CONFIG_ARP_TIMEOUT		200UL


/************************************************************************
 * CPU (PLL) timings
 ************************************************************************/
/* Already done in NBoot */
/* ##### clock/PLL configuration */
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
 * RAM timing
 ************************************************************************/
/* Already done in NBoot */


/************************************************************************
 * USB host
 ************************************************************************/
//####TODO


/************************************************************************
 * USB device
 ************************************************************************/
/* No support for special USB device download on this board */
#undef CONFIG_S3C_USBD
//#define USBD_DOWN_ADDR		0xc0000000


/************************************************************************
 * Keyboard
 ************************************************************************/
//#define CONFIG_USB_KEYBOARD
//#define CONFIG_SYS_DEVICE_DEREGISTER	/* Required for CONFIG_USB_KEYBOARD */


/************************************************************************
 * QUAD_SPI
 ************************************************************************/
//#define CONFIG_QUAD_SPI


/************************************************************************
 * NOR Flash
 ************************************************************************/
/* No support for NOR flash */
#define CONFIG_SYS_NO_FLASH	1	  /* no NOR flash */


/************************************************************************
 * SD/MMC card support
 ************************************************************************/
#define CONFIG_MMC			  /* SD/MMC support */
#define CONFIG_GENERIC_MMC		  /* with the generic driver model */
  //#define CONFIG_SDHCI			  /* use SDHCI driver */
#define CONFIG_FSL_ESDHC		  /* use Freescale ESDHC */

#define CONFIG_SYS_FSL_ESDHC_ADDR	0
#define CONFIG_ESDHC_NO_SNOOP		1
/*#define CONFIG_MMC_TRACE*/

/*#define CONFIG_ESDHC_DETECT_USE_EXTERN_IRQ1*/
#define CONFIG_SYS_FSL_ERRATUM_ESDHC135
#define CONFIG_SYS_FSL_ERRATUM_ESDHC111
#define CONFIG_SYS_FSL_ERRATUM_ESDHC_A001




#undef CONFIG_OF_LIBFDT

/* Hardware drivers */
#define CONFIG_VYBRID_GPIO


/************************************************************************
 * NAND flash organization (incl. JFFS2 and UBIFS)
 ************************************************************************/
/* VYBRID only has one NAND flash controller, so we can only have one
   physical NAND device; however as NBOOT needs a different ECC as everything
   else, we split the NAND up into two virtual devices to allow these two
   different ECC strategies and OOB layouts. ### TODO */
#if 1
#define CONFIG_MTD_NAND_FSL_NFC_SWECC	1
#define CONFIG_NAND_FSL_NFC
#else
#define CONFIG_NAND_MXC
#endif

//#define CONFIG_SYS_MAX_NAND_DEVICE	2
#define CONFIG_SYS_MAX_NAND_DEVICE	1

/* Chips per device; all chips must be the same type; if different types
   are necessary, they must be implemented as different NAND devices */
#define CONFIG_SYS_NAND_MAX_CHIPS	1

/* Define if you want to support nand chips that comply to ONFI spec */
#define CONFIG_SYS_NAND_ONFI_DETECTION

/* Address of the DATA register for reading and writing data; we need an
   entry for each device. As we only virtually split our flash into two
   devices, they both have the same address. ### TODO */
#define CONFIG_SYS_NAND_BASE		0x400E0000
//#define CONFIG_SYS_NAND_BASE_LIST {0x400E0000, 0x400E0000}

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

/* We have two virtual NAND chips, give them names ### TODO */
#define MTDIDS_DEFAULT		"nand0=NAND"
//#define MTDIDS_DEFAULT		"nand0=fsnand0,nand1=fsnand1"
//#define MTDIDS_DEFAULT "nand0=NAND"

/* We don't define settings for mtdparts default. Instead in fsvybrid.c
   board_late_init() we set variables mtdids, mtdparts and partition; We have
   mtdparts settings for 128K block size. ### TODO */
#define MTDPARTS_DEF_LARGE	"mtdparts=NAND:256k(Nboot)ro,768k(M4img)ro,512k(Uboot)ro,256k(UbootEnv)ro,2304k(UserDef),4m(Kernel)ro,-(TargetFS)"
//#define MTDPARTS_DEF_LARGE	"mtdparts=fsnand1:256k(NBoot)ro;fsnand0:768k@256k(M4img)ro,512k(UBoot)ro,256k(UBootEnv)ro,2304k(UserDef),4m(Kernel)ro,-(TargetFS)"

/* Set UserDef as default partition */
//#define MTDPART_DEFAULT "nand0,3"
#define MTDPART_DEFAULT "nand0,4"

#define CONFIG_MTD_DEVICE		  /* Create MTD device */
#define CONFIG_MTD_PARTITIONS		  /* Required for UBI */
#define CONFIG_RBTREE			  /* Required for UBI */
#define CONFIG_LZO			  /* Required for UBI */

/* Use board_nand_select_device() to switch to a device */
#define CONFIG_SYS_NAND_SELECT_DEVICE








/************************************************************************
 * Environment
 ************************************************************************/
#define CONFIG_ENV_IS_IN_NAND		  /* Environment is in NAND flash */

/* Environment settings for large blocks (128KB); we keep the size as more
   just wastes malloc space (the environment is held in the heap) */
//####define ENV_SIZE_DEF_LARGE   0x00020000	  /* 1 block = 128KB */
#define ENV_SIZE_DEF_LARGE   0x00004000	  /* Also 16KB */
#define ENV_RANGE_DEF_LARGE  0x00040000   /* 2 blocks = 256KB */
#define ENV_OFFSET_DEF_LARGE 0x00180000   /* See NAND layout above */

/* When saving the environment, we usually have a short period of time between
   erasing the NAND region and writing the new data where no valid environment
   is available. To avoid this time, we can save the environment alternatively
   to two different locations in the NAND flash. Then at least one of the
   environments is always valid. Currently we don't use this feature. */
//#define CONFIG_SYS_ENV_OFFSET_REDUND   0x001C0000

#define CONFIG_BOOTDELAY	3
#define CONFIG_BOOTCOMMAND      "nand read $loadaddr Kernel ; bootm $loadaddr"
#define CONFIG_EXTRA_ENV_SETTINGS \
	"installcheck=default\0" \
	"updatecheck=default\0" \
	"bootubi=setenv bootargs console=$sercon,115200 fec_mac=$ethaddr $mtdparts rootfstype=ubifs ubi.mtd=TargetFS root=ubi0:rootfs ro init=linuxrc\0" \
	"bootubidhcp=setenv bootargs console=$sercon,115200 ip=dhcp fec_mac=$ethaddr $mtdparts rootfstype=ubifs ubi.mtd=TargetFS root=ubi0:rootfs ro init=linuxrc\0" \
	"bootnfs=setenv bootargs console=$sercon,115200 $mtdparts ip=$ipaddr:$serverip:$gatewayip:$netmask::eth0 fec_mac=$ethaddr root=/dev/nfs nfsroot=/rootfs ro init=linuxrc\0" \
	"bootnfsdhcp=setenv bootargs console=$sercon,115200 $mtdparts ip=dhcp fec_mac=$ethaddr root=/dev/nfs nfsroot=$serverip:/rootfs ro init=linuxrc\0"
#define CONFIG_ETHADDR_BASE	00:05:51:07:55:83
#define CONFIG_ETHPRIME		"FEC0"
#define CONFIG_NETMASK          255.255.255.0
#define CONFIG_IPADDR		10.0.0.252
#define CONFIG_SERVERIP		10.0.0.122
#define CONFIG_GATEWAYIP	10.0.0.5
#define CONFIG_BOOTFILE         "uImage"

/* allow to overwrite serial and ethaddr */
#define CONFIG_ENV_OVERWRITE

/************************************************************************
 * LEDs
 ************************************************************************/
#define CONFIG_BOARD_SPECIFIC_LED
#define STATUS_LED_BIT 0
#define STATUS_LED_BIT1 1


/************************************************************************
 * Tools
 ************************************************************************/
#define CONFIG_ADDFSHEADER      1

/************************************************************************
 * Libraries
 ************************************************************************/
//#define USE_PRIVATE_LIBGCC
#define CONFIG_SYS_64BIT_VSPRINTF	  /* needed for nand_util.c */
#define CONFIG_USE_ARCH_MEMCPY
#define CONFIG_USE_ARCH_MEMMOVE
#define CONFIG_USE_ARCH_MEMSET
#define CONFIG_USE_ARCH_MEMSET32

#endif /*!__CONFIG_H */
