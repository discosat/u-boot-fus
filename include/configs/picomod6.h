/*
 * (C) Copyright 2010
 * F&S Elektronik Systeme GmbH
 *
 * Configuation settings for the F&S PicoMOD6 board.
 * Activate with "make picomod6_config"
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
 *
 * RAM layout of PicoMOD6 (RAM starts at 0x50000000) (128MB)
 * ---------------------------------------------------------
 * Offset 0x0000_0000 - 0x0000_00FF:
 * Offset 0x0000_0100 - 0x0000_7FFF: bi_boot_params
 * Offset 0x0000_8000 - 0x007F_FFFF: Linux zImage
 * Offset 0x0080_0000 - 0x00FF_FFFF: Linux BSS (decompressed kernel)
 * Offset 0x0100_0000 - 0x03EF_FFFF: (unused, e.g. INITRD)
 * Offset 0x07F0_0000 - 0x07FF_FFFF: U-Boot (inkl. malloc area)
 *
 * NAND flash layout of PicoMOD6 (64MB) (Block size 16KB)
 * --------------------------------------------------------
 * Offset 0x0000_0000 - 0x0000_7FFF: NBoot (32KB)
 * Offset 0x0000_8000 - 0x0007_7FFF: U-Boot (448KB)
 * Offset 0x0007_8000 - 0x0007_FFFF: U-Boot environment (32KB)
 * Offset 0x0008_0000 - 0x000F_FFFF: Space for user defined data (512KB)
 * Offset 0x0010_0000 - 0x003F_FFFF: Linux Kernel zImage (3MB)
 * Offset 0x0040_0000 - 0x03FF_FFFF: Linux Target System (60MB)
 *
 * NAND flash layout of PicoMOD6 (1GB) (Block size 128KB)
 * --------------------------------------------------------
 * Offset 0x0000_0000 - 0x0003_FFFF: NBoot (256KB)
 * Offset 0x0004_0000 - 0x000B_FFFF: U-Boot (512KB)
 * Offset 0x000C_0000 - 0x000F_FFFF: U-Boot environment (256KB)
 * Offset 0x0010_0000 - 0x004F_FFFF: Space for user defined data (4MB)
 * Offset 0x0050_0000 - 0x007F_FFFF: Linux Kernel zImage (3MB)
 * Offset 0x0080_0000 - 0x3FFF_FFFF: Linux Target System (1016MB)
 *
 * Memory layout within U-Boot (Base address CFG_PHY_UBOOT_BASE)
 * ----------------------------------------------------------------
 * (Code size):           U-Boot code    
 * CONFIG_STACKSIZE_FIQ:  FIQ stack (if CONFIG_USE_IRQ is defined)
 * CONFIG_STACKSIZE_IRQ:  IRQ stack (if CONFIG_USE_IRQ is defined)
 * CFG_GBL_DATA_SIZE:     Gobal data
 * CFG_MALLOC_LEN:        Heap
 * CFG_STACK_SIZE:        (128K) Stack
 * ----------------------------------------------------------------
 * CFG_UBOOT_SIZE:        Sum is size of U-Boot 
 *
 */

#ifndef __CONFIG_H
#define __CONFIG_H

/* ### TODO: Files/Switches, die noch zu testen und ggf. einzubinden sind:
   Define              File           Kommentar
   -------------------------------------------------------------------- 
   CONFIG_CMD_BMP      cmd_bmp.c      wenn LCD eingebaut
   CONFIG_CMD_DIAG     cmd_diag.c     Selbsttests
   CONFIG_CMD_FAT      cmd_fat.c      fatls, fatinfo, fatload
   CONFIG_CMD_SAVES    cmd_load.c     Upload (save)
   CONFIG_CMD_HWFLOW   cmd_load.c     HW-Handshake (RTS/CTS)
   CONFIG_LOGBUFFER    cmd_log.c      Log-File-Funktionen
   CONFIG_CMD_MMC      cmd_mmc.c      MMC Card
   CONFIG_CMD_ASKENV   cmd_nvedit.c   Abfragen einer Environmentvariablen über
                                      stdin (z.B. in einem autoskript)
   CONFIG_CMD_PORTIO   cmd_portio.c   in, out (evtl. nützlich für Kunden, die
                                      früh I/Os setzen müssen)
   CONFIG_CMD_REISER   cmd_reiser.c   reiserload, reiserls (Support für reiserfs)
   CONFIG_CMD_SETEXPR  cmd_setexpr.c  Environmentvariable als Ergebnis einer
                                      Berechnung setzen 
   CONFIG_CMD_SF       cmd_sf.c       SPI-Flash-Support
   CONFIG_CMD_SPI      cmd_spi.c      SPI-Daten senden
   CONFIG_CMD_STRINGS  cmd_strings.c  Strings im Speicher anzeigen
   CONFIG_CMD_TERMINAL cmd_terminal.c In einen Terminalmodus schalten
   CONFIG_CMD_USB      cmd_usbd.c     Download als USB-Device; vielleicht kann
                                      man diesen Modus so ändern, dass
				      Downloads mit NetDCU-USBLoader möglich 
				      werden
   CONFIG_LCD          lcd.c          Display-Unterstützung
   CONFIG_USB_KEYBOARD usb_kbd.c      Unterstützung für USB-Tastaturen
 */


/************************************************************************
 * High Level Configuration Options
 ************************************************************************/
/* Define extactly one of the two following lines */
#define __NAND_64MB__			  /* Compile U-Boot for 64MB flash */
#undef __NAND_1GB__			  /* Compile U-Boot for 1GB flash */

/* We are on a PicoMOD6 */
#define CONFIG_IDENT_STRING	" for PicoMOD6"

/* CPU, family and board defines */
#define CONFIG_S3C6410		1	  /* SAMSUNG S3C6410 SoC */
#define CONFIG_S3C64XX		1	  /* SAMSUNG S3C64XX Family */
#define CONFIG_PICOMOD6		1	  /* F&S PicoMOD6 Board */

/* Architecture magic and machine type */
#define MACH_TYPE		0x9BE	  /* PicoMOD6/Linux */
#define UBOOT_MAGIC		(0x43090000 | MACH_TYPE)

/* Input clock of PLL */
#define CONFIG_SYS_CLK_FREQ	12000000  /* 12MHz */


/************************************************************************
 * Command line editor (shell)
 ************************************************************************/
/* Use standard command line parser */
#undef CFG_HUSH_PARSER			  /* use "hush" command parser */
#ifdef CFG_HUSH_PARSER
#define CFG_PROMPT_HUSH_PS2	"> "
#endif

#define CFG_PROMPT		"PicoMOD6 # " /* Monitor Command Prompt */

/* Allow editing (scroll between commands, etc.) */
#define CONFIG_CMDLINE_EDITING


/************************************************************************
 * Miscellaneous configurable options
 ************************************************************************/
#define CFG_LONGHELP			   /* undef to save memory */
#define CFG_CBSIZE		256	   /* Console I/O Buffer Size */
#define CFG_PBSIZE		384	   /* Print Buffer Size */
#define CFG_MAXARGS		16	   /* max number of command args */
#define CFG_BARGSIZE		CFG_CBSIZE /* Boot Argument Buffer Size	*/

/* Stack is above U-Boot code, at the end of CFG_UBOOT_SIZE */
#define CONFIG_MEMORY_UPPER_CODE

/* No need to call special board_late_init() function */
#undef BOARD_LATE_INIT

/* Power Management is enabled */
#define CONFIG_PM

/* Allow stopping of automatic booting even if boot delay is zero */
#define CONFIG_ZERO_BOOTDELAY_CHECK

#define CONFIG_DISPLAY_CPUINFO		  /* Show CPU type and speed */
#define CONFIG_DISPLAY_BOARDINFO	  /* Show board information */

/* Skip relocation, we are already loaded at the correct position by NBoot */
//#undef CONFIG_SKIP_RELOCATE_UBOOT /* no big difference */
#define CONFIG_SKIP_RELOCATE_UBOOT

/* No need for IRQ/FIQ stuff; this also obsoletes the IRQ/FIQ stacks */
#undef CONFIG_USE_IRQ

/* Strange -- not used anywhere, but everybody and its dog undefines it */
#undef CFG_CLKS_IN_HZ		/* everything, incl board info, in Hz */

/* The PWM Timer 4 uses a divider by 1 and a prescaler of 16. This results in
   the following counter value for 1 sec. */
//####define CFG_HZ			1562500		/* at PCLK 50MHz */
#define CFG_HZ			4156250		/* at PCLK 66.5MHz */


/************************************************************************
 * Memory layout
 ************************************************************************/
/* Don't use MMU on PicoMOD6 */
#define CONFIG_ENABLE_MMU //#### MMU 1:1 Test
#if 0  //#ifdef CONFIG_ENABLE_MMU
#define virt_to_phys(x)	virt_to_phy_picomod6(x)
#else
#define virt_to_phys(x)	(x)
#endif

#define MEMORY_BASE_ADDRESS	0x50000000      /* Physical RAM address */

#define CONFIG_NR_DRAM_BANKS	1	        /* we have 1 bank of DRAM */
#define PHYS_SDRAM_1		MEMORY_BASE_ADDRESS /* SDRAM Bank #1 */
#define PHYS_SDRAM_1_SIZE	0x08000000      /* 128 MB */

/* Total memory required by uboot: 1MB */
#define CFG_UBOOT_SIZE		(1*1024*1024)

/* Locate U-Boot at 1MB below end of memory */
#define PM6_UBOOT_OFFS          (PHYS_SDRAM_1_SIZE - CFG_UBOOT_SIZE)

/* This results in the following base address for uboot */
#define CFG_PHY_UBOOT_BASE	(MEMORY_BASE_ADDRESS + PM6_UBOOT_OFFS)

#if 0 //###def CONFIG_ENABLE_MMU
#define CFG_UBOOT_BASE		(0xc0000000 + PM6_UBOOT_OFFS)
#else
#define CFG_UBOOT_BASE		CFG_PHY_UBOOT_BASE
#endif

/* Size of malloc() pool (heap) */
#define CFG_MALLOC_LEN		(CFG_ENV_SIZE + 512*1024)

/* Size in bytes reserved for initial data */
#define CFG_GBL_DATA_SIZE	128

/* Stack */

/* The stack sizes are set up in start.S using the settings below */
#define CFG_STACK_SIZE		128*1024  /* 128KB */
//#define CONFIG_STACKSIZE	0x20000	  /* (unused on S3C6410) */
#ifdef CONFIG_USE_IRQ
#define CONFIG_STACKSIZE_IRQ	(4*1024)  /* IRQ stack */
#define CONFIG_STACKSIZE_FIQ	(4*1024)  /* FIQ stack */
#endif

/* Memory test checks all RAM before U-Boot (i.e. leaves last MB with U-Boot
   untested) */
#define CFG_MEMTEST_START	MEMORY_BASE_ADDRESS
#define CFG_MEMTEST_END		MEMORY_BASE_ADDRESS + PM6_UBOOT_OFFS

/* Default load address */
#define CFG_LOAD_ADDR		MEMORY_BASE_ADDRESS+0x8000


/************************************************************************
 * Serial console (UART)
 ************************************************************************/
#define CONFIG_SERIAL3          1	  /* SERIAL 3 (UART2) on PicoMOD6 */

#define CONFIG_BAUDRATE		38400

/* valid baudrates */
#define CFG_BAUDRATE_TABLE	{9600, 19200, 38400, 57600, 115200}

#if defined(CONFIG_CMD_KGDB)
#define CONFIG_KGDB_BAUDRATE	115200	  /* speed to run kgdb serial port */
#define CONFIG_KGDB_SER_INDEX	1	  /* which serial port to use */
#endif

#define CFG_CONSOLE_IS_IN_ENV		  /* Console can be saved in env */
//#define CONFIG_UART_66		  /* default clock value of CLK_UART */


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

#define CONFIG_SETUP_MEMORY_TAGS
#define CONFIG_CMDLINE_TAG
//#define CONFIG_INITRD_TAG


/************************************************************************
 * I2C
 ************************************************************************/
/* No need to access I2C from U-Boot on PicoMOD6 */
#undef CONFIG_S3C64XX_I2C
#ifdef CONFIG_S3C64XX_I2C
#define CONFIG_HARD_I2C		1
#define CFG_I2C_SPEED		50000
#define CFG_I2C_SLAVE		0xFE
#endif


/************************************************************************
 * Real Time Clock (RTC)
 ************************************************************************/
#define CONFIG_RTC_S3C64XX	1


/************************************************************************
 * Command definition
 ************************************************************************/
//#include <config_cmd_default.h>
#define CONFIG_CMD_AUTOSCRIPT	/* Autoscript Support		*/
#define CONFIG_CMD_BDI		/* bdinfo			*/
#define CONFIG_CMD_BOOTD	/* bootd			*/
#define CONFIG_CMD_CONSOLE	/* coninfo			*/
#define CONFIG_CMD_ECHO		/* echo arguments		*/
#define CONFIG_CMD_ENV		/* saveenv			*/
#define CONFIG_CMD_FLASH	/* flinfo, erase, protect	*/
#undef CONFIG_CMD_FPGA		/* FPGA configuration Support	*/
#define CONFIG_CMD_IMI		/* iminfo			*/
#undef CONFIG_CMD_IMLS		/* List all found images	*/
#define CONFIG_CMD_ITEST	/* Integer (and string) test	*/
#undef CONFIG_CMD_LOADB		/* loadb			*/
#undef CONFIG_CMD_LOADS		/* loads			*/
#define CONFIG_CMD_MEMORY	/* md mm nm mw cp cmp crc base loop mtest */
#define CONFIG_CMD_MISC		/* Misc functions like sleep etc*/
#define CONFIG_CMD_NET		/* bootp, tftpboot, rarpboot	*/
#define CONFIG_CMD_NFS		/* NFS support			*/
#define CONFIG_CMD_RUN		/* run command in env variable	*/
#undef CONFIG_CMD_SETGETDCR	/* DCR support on 4xx		*/
#undef CONFIG_CMD_XIMG		/* Load part of Multi Image	*/


#undef CONFIG_CMD_FLASH
#undef CONFIG_CMD_FPGA

#define CONFIG_CMD_CACHE
#define CONFIG_CMD_USB
#define CONFIG_CMD_MMC
#define CONFIG_CMD_FAT
#undef CONFIG_CMD_REGINFO		  /* No register info on ARM */
#define	CONFIG_CMD_NAND
#undef CONFIG_CMD_ONENAND
#undef CONFIG_CMD_MOVINAND
#undef CONFIG_CMD_FDC
#define CONFIG_CMD_DHCP
#define CONFIG_CMD_PING
#define CONFIG_CMD_SNTP
#define CONFIG_CMD_DATE
#define CONFIG_CMD_EXT2
#define CONFIG_CMD_JFFS2
#define CONFIG_CMD_NET

#define CONFIG_CMD_ELF
//#define CONFIG_CMD_I2C


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
/* The PicoMOD6 has a NE2000 compatible AX88769B ethernet chip */
//#define CONFIG_NET_MULTI
#define CONFIG_DRIVER_NE2000
#define CONFIG_DRIVER_NE2000_BASE	0x18000000
#define CONFIG_DRIVER_NE2000_SOFTMAC
#define CONFIG_DRIVER_AX88796L


#ifndef CONFIG_PICOMOD6 /* Already done in NBoot */
/************************************************************************
 * CPU (PLL) timings
 ************************************************************************/

//#define CONFIG_CLK_800_133_66
//#define CONFIG_CLK_666_133_66
#define CONFIG_CLK_532_133_66
//#define CONFIG_CLK_400_133_66
//#define CONFIG_CLK_400_100_50
//#define CONFIG_CLK_OTHERS

#define CONFIG_CLKSRC_CLKUART

#define set_pll(mdiv, pdiv, sdiv)	(1<<31 | mdiv<<16 | pdiv<<8 | sdiv)

#if defined(CONFIG_CLK_666_133_66) /* FIN 12MHz, Fout 666MHz */
#define APLL_MDIV	333
#define APLL_PDIV	3
#define APLL_SDIV	1
#undef  CONFIG_SYNC_MODE /* ASYNC MODE */

#elif defined(CONFIG_CLK_532_133_66) /* FIN 12MHz, Fout 532MHz */
#define APLL_MDIV	266
#define APLL_PDIV	3
#define APLL_SDIV	1
#define CONFIG_SYNC_MODE

#elif defined(CONFIG_CLK_400_133_66) || defined(CONFIG_CLK_800_133_66) /* FIN 12MHz, Fout 800MHz */
#define APLL_MDIV	400
#define APLL_PDIV	3
#define APLL_SDIV	1
#define CONFIG_SYNC_MODE

#elif defined(CONFIG_CLK_400_100_50) /* FIN 12MHz, Fout 400MHz */
#define APLL_MDIV	400
#define APLL_PDIV	3
#define APLL_SDIV	2
#define CONFIG_SYNC_MODE

#elif defined(CONFIG_CLK_OTHERS)
/*If you have to use another value, please define pll value here*/
/* FIN 12MHz, Fout 532MHz */
#define APLL_MDIV	266
#define APLL_PDIV	3
#define APLL_SDIV	1
#define CONFIG_SYNC_MODE

#else
#error "Not Support Fequency or Mode!! you have to setup right configuration."
#endif

#define APLL_VAL	set_pll(APLL_MDIV, APLL_PDIV, APLL_SDIV)
/* prevent overflow */
#define Startup_APLL	(CONFIG_SYS_CLK_FREQ/(APLL_PDIV<<APLL_SDIV)*APLL_MDIV)

/* fixed MPLL 533MHz */
#define MPLL_MDIV	266
#define MPLL_PDIV	3
#define MPLL_SDIV	1

#define MPLL_VAL	set_pll(MPLL_MDIV, MPLL_PDIV, MPLL_SDIV)
/* prevent overflow */
#define Startup_MPLL	((CONFIG_SYS_CLK_FREQ)/(MPLL_PDIV<<MPLL_SDIV)*MPLL_MDIV)

#if defined(CONFIG_CLK_800_133_66)
#define Startup_APLLdiv		0
#define Startup_HCLKx2div	2
#elif defined(CONFIG_CLK_400_133_66)
#define Startup_APLLdiv		1
#define Startup_HCLKx2div	2
#else
#define Startup_APLLdiv		0
#define Startup_HCLKx2div	1
#endif

#define	Startup_PCLKdiv		3
#define Startup_HCLKdiv		1
#define Startup_MPLLdiv		1

#define CLK_DIV_VAL	((Startup_PCLKdiv<<12)|(Startup_HCLKx2div<<9)|(Startup_HCLKdiv<<8)|(Startup_MPLLdiv<<4)|Startup_APLLdiv)

#if defined(CONFIG_SYNC_MODE)
#define Startup_HCLK	(Startup_APLL/(Startup_HCLKx2div+1)/(Startup_HCLKdiv+1))
#else
#define Startup_HCLK	(Startup_MPLL/(Startup_HCLKx2div+1)/(Startup_HCLKdiv+1))
#endif
#endif /* !CONFIG_PICOMOD6 */


#ifndef CONFIG_PICOMOD6 /* Already done in NBoot */
/************************************************************************
 * RAM timing
 ************************************************************************/
#define DMC1_MEM_CFG		0x00010012	/* Supports one CKE control,
						   Chip1, Burst4, Row/Column
						   bit */
#define DMC1_MEM_CFG2		0xB45
#define DMC1_CHIP0_CFG		0x150F8
#define DMC_DDR_32_CFG		0x0 		/* 32bit, DDR */

/* Memory Parameters */
/* DDR Parameters */
#define DDR_tREFRESH		7800		/* ns */
#define DDR_tRAS		45		/* ns (min: 45ns)*/
#define DDR_tRC 		68		/* ns (min: 67.5ns)*/
#define DDR_tRCD		23		/* ns (min: 22.5ns)*/
#define DDR_tRFC		80		/* ns (min: 80ns)*/
#define DDR_tRP 		23		/* ns (min: 22.5ns)*/
#define DDR_tRRD		15		/* ns (min: 15ns)*/
#define DDR_tWR 		15		/* ns (min: 15ns)*/
#define DDR_tXSR		120		/* ns (min: 120ns)*/
#define DDR_CASL		3		/* CAS Latency 3 */

/* mDDR memory configuration */
#define DMC_DDR_BA_EMRS 	2
#define DMC_DDR_MEM_CASLAT	3
#define DMC_DDR_CAS_LATENCY	(DDR_CASL<<1)	/* 6   Set Cas Latency to 3 */
#define DMC_DDR_t_DQSS		1		/* Min 0.75 ~ 1.25 */
#define DMC_DDR_t_MRD		2		/* Min 2 tck */
#define DMC_DDR_t_RAS		(((Startup_HCLK / 1000 * DDR_tRAS) - 1) / 1000000 + 1)	//7, Min 45ns
#define DMC_DDR_t_RC		(((Startup_HCLK / 1000 * DDR_tRC) - 1) / 1000000 + 1) 	//10, Min 67.5ns
#define DMC_DDR_t_RCD		(((Startup_HCLK / 1000 * DDR_tRCD) - 1) / 1000000 + 1) 	//4,5(TRM), Min 22.5ns
#define DMC_DDR_schedule_RCD	((DMC_DDR_t_RCD - 3) << 3)
#define DMC_DDR_t_RFC		(((Startup_HCLK / 1000 * DDR_tRFC) - 1) / 1000000 + 1) 	//11,18(TRM) Min 80ns
#define DMC_DDR_schedule_RFC	((DMC_DDR_t_RFC - 3) << 5)
#define DMC_DDR_t_RP		(((Startup_HCLK / 1000 * DDR_tRP) - 1) / 1000000 + 1) 	//4, 5(TRM) Min 22.5ns
#define DMC_DDR_schedule_RP	((DMC_DDR_t_RP - 3) << 3)
#define DMC_DDR_t_RRD		(((Startup_HCLK / 1000 * DDR_tRRD) - 1) / 1000000 + 1)	//3, Min 15ns
#define DMC_DDR_t_WR		(((Startup_HCLK / 1000 * DDR_tWR) - 1) / 1000000 + 1)	//Min 15ns
#define DMC_DDR_t_WTR		2
#define DMC_DDR_t_XP		2							//1tck + tIS(1.5ns)
#define DMC_DDR_t_XSR		(((Startup_HCLK / 1000 * DDR_tXSR) - 1) / 1000000 + 1)	//17, Min 120ns
#define DMC_DDR_t_ESR		DMC_DDR_t_XSR
#define DMC_DDR_REFRESH_PRD	(((Startup_HCLK / 1000 * DDR_tREFRESH) - 1) / 1000000) 	// TRM 2656
#define DMC_DDR_USER_CONFIG	1							// 2b01 : mDDR
#endif /* !CONFIG_PICOMOD6 */


/************************************************************************
 * USB host
 ************************************************************************/
#define CONFIG_USB_STORAGE
//#define CONFIG_USB_OHCI
#define CONFIG_USB_OHCI_NEW
#define CFG_USB_OHCI_REGS_BASE 0x74300000
#define CFG_USB_OHCI_BOARD_INIT
#define CFG_USB_OHCI_SLOT_NAME "PM6 USB Host"
#define CFG_USB_OHCI_MAX_ROOT_PORTS 2
//#define CONFIG_USB_KEYBOARD
//#define CFG_DEVICE_DEREGISTER		/* Required for CONFIG_USB_KEYBOARD */


/************************************************************************
 * USB device
 ************************************************************************/
/* No support for special USB device download on PicoMOD6 */
#undef CONFIG_S3C_USBD
//#define USBD_DOWN_ADDR		0xc0000000


/************************************************************************
 * OneNAND Flash
 ************************************************************************/
/* No support for OneNAND */
#undef CONFIG_ONENAND
#define CFG_MAX_ONENAND_DEVICE	0
#define CFG_ONENAND_BASE 	(0x70100000)


/************************************************************************
 * MoviNAND Flash
 ************************************************************************/
/* No support for MoviNAND */
#undef CONFIG_MOVINAND


/************************************************************************
 * NOR Flash
 ************************************************************************/
/* No support for NOR flash */
#define CFG_MAX_FLASH_BANKS	0	/* max number of memory banks */
//#define CFG_FLASH_BASE		0x00000000
#define CFG_MAX_FLASH_SECT	1024
//#define CONFIG_AMD_LV800
//#define PHYS_FLASH_SIZE		0x100000

/* timeout values are in ticks */
#define CFG_FLASH_ERASE_TOUT	(5*CFG_HZ) /* Timeout for Flash Erase */
#define CFG_FLASH_WRITE_TOUT	(5*CFG_HZ) /* Timeout for Flash Write */


/************************************************************************
 * SD/MMC card support
 ************************************************************************/
/* We have support for SD/MMC card */
#define CONFIG_MMC


/************************************************************************
 * NAND flash organization
 ************************************************************************/
/* We have one NAND device */
#define CFG_MAX_NAND_DEVICE     1

/* One chip per device */
#define NAND_MAX_CHIPS          1

/* Address of the DATA register for reading and writing data */
#define CFG_NAND_BASE           (0x70200010)

/* .i read skips bad blocks */
#define CFG_NAND_SKIP_BAD_DOT_I	1

/* Support YAFFS access */
#define CFG_NAND_YAFFS_WRITE	1

/* Support hardware ECC */
#define CFG_NAND_HWECC

/* Don't use bad block table */
#undef CFG_NAND_FLASH_BBT

/* The first two blocks in NAND are using 8-bit ECC (instead of 1-bit ECC) */
#define CONFIG_NAND_BL1_8BIT_ECC

/* Use NAND write protection (unused, only used if CFG_NAND_LEGACY is set) */
#define	CFG_NAND_WP		1

/* Commands to (de)select NAND flash */
#define NAND_DISABLE_CE()	(NFCONT_REG |= (1 << 1))
#define NAND_ENABLE_CE()	(NFCONT_REG &= ~(1 << 1))
#define NF_TRANSRnB()		do { while(!(NFSTAT_REG & (1 << 0))); } while(0)

/* Support JFFS2 in NAND (commands: fsload, ls, fsinfo) */
#define CONFIG_JFFS2_NAND

/* Support mtd partitions (commands: mtdparts, chpart) */
#define CONFIG_JFFS2_CMDLINE


/************************************************************************
 * Environment
 ************************************************************************/
/* Use this if the environment should be in the NAND flash */
#define CFG_ENV_IS_IN_NAND

/* Use this if the code should decide itself if the system was booted from
   NAND, MoviNAND (=MMC-Card) or OneNAND. The environment ist assumed to be
   in the same device. */
//#define CFG_ENV_IS_IN_AUTO

#define CFG_ENV_ADDR		0	  /* Only needed for NOR flash */

#ifdef __NAND_64MB__
/* The environment is 16KB and can use a region of 32KB, allowing for one bad
   block. */
#define CFG_ENV_SIZE		0x00004000   /* 16KB actual environment */
#define CFG_ENV_RANGE           0x00008000   /* 32KB region for environment */
#define CFG_ENV_OFFSET		0x00078000
#else
/* The environment is 128KB and can use a region of 256KB, allowing for one
   bad block. */
#define CFG_ENV_SIZE		0x00020000   /* 128KB actual environment */
#define CFG_ENV_RANGE           0x00040000   /* 256KB region for environment */
#define CFG_ENV_OFFSET		0x000C0000
#endif

/* When saving the environment, we usually have a short period of time between
   erasing the NAND region and writing the new data where no valid environment
   is available. To avoid this time, we can save the environment alternatively
   to two different locations in the NAND flash. Then at least one of the
   environments is always valid. Currently we don't use this feature. */
//#define CFG_ENV_OFFSET_REDUND   0x0007c000


/* We have one NAND chip, give it a name */
#define MTDIDS_DEFAULT		"nand0=pm6nand0"

/* Define the default partitions on this NAND chip; we have a version for
   64MB and 1GB flash (see comment at head of file) */
#ifdef __NAND_64MB__
#define MTDPARTS_DEFAULT	"mtdparts=pm6nand0:32k(NBoot)ro,448k(UBoot)ro,32k(UBootEnv)ro,512k(UserDef),3m(Kernel)ro,-(TargetFS)"
//#define MTDPARTS_DEFAULT	"mtdparts=pm6nand0:32k@0x78000(UBootEnv),512k(UserDef),3m(Kernel),-(TargetFS)"
#else
#define MTDPARTS_DEFAULT	"mtdparts=pm6nand0:256k(NBoot)ro,512k(UBoot),256k(UBootEnv),4m(UserDef),3m(Kernel),-(TargetFS)"
#endif


#define CONFIG_BOOTDELAY	3
#define CONFIG_BOOTARGS    	"console=ttySAC2,38400 init=linuxrc"
#define CONFIG_BOOTCOMMAND      "nand read.jffs2 51000000 Kernel ; bootm 51000000"
//#define CONFIG_BOOTCOMMAND	"setenv bootargs $(bootargs) root=/dev/mtd5 rw rootfstype=jffs2 ip=$(ipaddr):$(serverip):$(gatewayip):$(netmask) ; bootm 51000000"
//#define CONFIG_NFSBOOTCOMMAND	"setenv bootargs $(bootargs) root=/dev/nfs rw nfsroot=rootfs ip=$(ipaddr):$(serverip):$(gatewayip):$(netmask) ; bootm 51000000"
#define CONFIG_EXTRA_ENV_SETTINGS \
        "bootlocal=setenv bootargs console=ttySAC2,38400 $(mtdparts) init=linuxrc root=/dev/mtdblock5 ro rootfstype=jffs2\0" \
        "bootnfs=setenv bootargs console=ttySAC2,38400 $(mtdparts) init=linuxrc root=/dev/nfs rw nfsroot=/rootfs ip=$(ipaddr):$(serverip):$(gatewayip):$(netmask)\0" \
	"r=tftp 51000000 zImage ; bootm 51000000\0"
#define CONFIG_ETHADDR		00:05:51:02:69:19
#define CONFIG_NETMASK          255.255.255.0
#define CONFIG_IPADDR		10.0.0.252
#define CONFIG_SERVERIP		10.0.0.122
#define CONFIG_GATEWAYIP	10.0.0.5
#define CONFIG_BOOTFILE         zImage
#define CONFIG_LOADADDR         50000000

/* allow to overwrite serial and ethaddr */
#define CONFIG_ENV_OVERWRITE

#endif /* !__CONFIG_H */
