/*
 * (C) Copyright 2012
 * F&S Elektronik Systeme GmbH
 *
 * Configuation settings for the F&S armStoneA8 board. Activate with
 * one of the following targets:
 *   make amstonea8_config      Configure for armStoneA8
 *   make amstonea8             Configure for armStoneA8 and build
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
 * RAM layout of armStoneA8 (RAM starts at 0x40000000) (128MB)
 * -----------------------------------------------------------
 * Offset 0x0000_0000 - 0x0000_00FF:
 * Offset 0x0000_0100 - 0x0000_7FFF: bi_boot_params
 * Offset 0x0000_8000 - 0x007F_FFFF: Linux zImage
 * Offset 0x0080_0000 - 0x00FF_FFFF: Linux BSS (decompressed kernel)
 * Offset 0x0100_0000 - 0x07EF_FFFF: (unused, e.g. INITRD)
 * Offset 0x07F0_0000 - 0x07FF_FFFF: U-Boot (inkl. malloc area)
 *
 * Remark: Additional 128MB of RAM are unused @ 0x20000000)
 * 
 *
 * NAND flash layout of armStoneA8 (1GB) (Block size 128KB)
 * --------------------------------------------------------
 * Offset 0x0000_0000 - 0x0003_FFFF: NBoot (256KB)
 * Offset 0x0004_0000 - 0x000B_FFFF: U-Boot (512KB)
 * Offset 0x000C_0000 - 0x000F_FFFF: U-Boot environment (256KB)
 * Offset 0x0010_0000 - 0x004F_FFFF: Space for user defined data (4MB)
 * Offset 0x0050_0000 - 0x007F_FFFF: Linux Kernel zImage (3MB)
 * Offset 0x0080_0000 - 0x3FFF_FFFF: Linux Target System (1016MB)
 *
 * With the new ARM specific loader code introduced in u-boot-2010.12, u-boot
 * now can be loaded to a rather low RAM address from NBoot. It only needs a
 * rather small stack and some room for a (unitialized) gd_t structure behind
 * the stack. Then u-boot automatically computes its size and relocates itself
 * to the end of the available RAM. This only requires CONFIG_SYS_SDRAM_BASE
 * and the board-specific dram_init() function has to set gd->ram_size
 * correctly to the available RAM. That's all. So there is no need for
 * different u-boot versions just because of differently mounted RAM sizes
 * anymore.
 *
 * Memory layout within U-Boot (from top to bottom!). For more details see
 * board_init_f() in arch/arm/lib/board.c.
 *
 * Addr             Size                      Comment
 * ------------------------------------------------------------------------
 * CONFIG_SYS_SDRAM_BASE
 * + gd->ram_size   CONFIG_SYS_MEM_TOP_HIDE   Hidden memory (unused)
 *                  LOGBUFF_RESERVE           Linux kernel logbuffer (unused)
 *                  getenv("pram") (in KB)    Protected RAM set in env (unused)
 * gd->tlb_addr     16KB (16KB aligned)       MMU page tables (TLB)
 * gd->fb_base      lcd_setmen()              LCD framebuffer (unused?)
 *                  gd->monlen (4KB aligned)  U-boot code, data and bss
 *                  TOTAL_MALLOC_LEN          malloc heap
 * bd               sizeof(bd_t)              Board info struct
 * gd->irq_sp       sizeof(gd_t)              Global data
 *                  CONFIG_STACKSIZE_IRQ      IRQ stack
 *                  CONFIG_STACKSIZE_FIQ      FIQ stack
 *                  12 (8-byte aligned)       Abort-stack
 *
 * Remark: TOTAL_MALLOC_LEN depends on CONFIG_SYS_MALLOC_LEN and CONFIG_ENV_SIZE
 *
 */

#ifndef __CONFIG_H
#define __CONFIG_H

/* ### TODO: Files/Switches, die noch zu testen und ggf. einzubinden sind:
   Define              File           Kommentar
   -------------------------------------------------------------------- 
   CONFIG_CMD_DIAG     cmd_diag.c     Selbsttests
   CONFIG_CMD_FAT      cmd_fat.c      fatls, fatinfo, fatload
   CONFIG_CMD_SAVES    cmd_load.c     Upload (save)
   CONFIG_CMD_HWFLOW   cmd_load.c     HW-Handshake (RTS/CTS)
   CONFIG_LOGBUFFER    cmd_log.c      Log-File-Funktionen
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
   CONFIG_USB_KEYBOARD usb_kbd.c      Unterstützung für USB-Tastaturen
 */


/************************************************************************
 * High Level Configuration Options
 ************************************************************************/

/* We are on an armStoneA8 */
#define CONFIG_IDENT_STRING	" for F&S"

/* CPU, family and board defines */
#define CONFIG_ARMV7		1	  /* This is an ARM v7 CPU core */
#define CONFIG_SAMSUNG		1	  /* in a SAMSUNG core */
#define CONFIG_S5P		1	  /* wich is in S5P family */
#define CONFIG_S5PC1XX		1	  /* more specific in S5PC1XX family */
#define CONFIG_S5PV210		1	  /* it's an S5PV210 SoC */
#define CONFIG_ARMSTONEA8	1	  /* F&S armStoneA8 Board */

/* Architecture magic and machine types; we don't have a separate value for
   EASYsom1 */
#define MACH_TYPE_ARMSTONEA8	4077
#define MACH_TYPE_NETDCU14	4078
#define MACH_TYPE_QBLISSA8B	MACH_TYPE_ARMSTONEA8 /* ###TODO, if ever */
#define MACH_TYPE_EASYSOM1	MACH_TYPE_ARMSTONEA8 /* no extra number */
#define UBOOT_MAGIC		(0x43090000 | MACH_TYPE)

/* Input clock of PLL */
#define CONFIG_SYS_CLK_FREQ	12000000  /* 12MHz (only on Rev 1.0) */

/* We do need to set our CPU type rather early */
#define CONFIG_ARCH_CPU_INIT

/************************************************************************
 * Command line editor (shell)
 ************************************************************************/
/* Use standard command line parser */
#undef CONFIG_SYS_HUSH_PARSER		  /* use "hush" command parser */
#ifdef CONFIG_SYS_HUSH_PARSER
#define CONFIG_SYS_PROMPT_HUSH_PS2	"> "
#endif

#define CONFIG_SYS_PROMPT	fs_board_prompt /* Monitor Command Prompt */

/* Allow editing (scroll between commands, etc.) */
#define CONFIG_CMDLINE_EDITING


/************************************************************************
 * Miscellaneous configurable options
 ************************************************************************/
#define CONFIG_SYS_LONGHELP		   /* undef to save memory */
#define CONFIG_SYS_CBSIZE	256	   /* Console I/O Buffer Size */
#define CONFIG_SYS_PBSIZE	384	   /* Print Buffer Size */
#define CONFIG_SYS_MAXARGS	16	   /* max number of command args */
#define CONFIG_SYS_BARGSIZE	CONFIG_SYS_CBSIZE /* Boot Arg Buffer Size */
#define CONFIG_UBOOTNB0_SIZE    384	   /* size of uboot.nb0 (in kB) */

/* Stack is above U-Boot code, at the end of CONFIG_SYS_UBOOT_SIZE */
#define CONFIG_MEMORY_UPPER_CODE

/* No need to call special board_late_init() function */
#undef BOARD_LATE_INIT

/* Power Management is enabled */
#define CONFIG_PM

/* Allow stopping of automatic booting even if boot delay is zero */
#define CONFIG_ZERO_BOOTDELAY_CHECK

#define CONFIG_DISPLAY_CPUINFO		  /* Show CPU type and speed */
#define CONFIG_DISPLAY_BOARDINFO	  /* Show board information */

/* No need for IRQ/FIQ stuff; this also obsoletes the IRQ/FIQ stacks */
#undef CONFIG_USE_IRQ

/* The PWM Timer 4 uses a prescaler of 16 and a divider of 2 for PCLK; this
   results in the following value for PCLK=66.7MHz */
#define CONFIG_SYS_HZ		2084375


/************************************************************************
 * Memory layout
 ************************************************************************/
/* Use MMU */
//#define CONFIG_ENABLE_MMU //#### MMU 1:1 Test
//#define CONFIG_SOFT_MMUTABLE

/* Physical RAM addresses; however we compute the base address in function
   checkboard() */
#define CONFIG_NR_DRAM_BANKS	2	        /* we use 2 banks of DRAM */
#define PHYS_SDRAM_0		0x20000000	/* SDRAM Bank #0 */
#define PHYS_SDRAM_1		0x40000000	/* SDRAM Bank #1 */

/* Total memory required by uboot: 1MB */
#define CONFIG_SYS_UBOOT_SIZE	(1*1024*1024)

/* This results in the following base address for uboot */
#define CONFIG_SYS_PHY_UBOOT_BASE 0x40f00000


#if 0 //###def CONFIG_ENABLE_MMU
#define CONFIG_SYS_UBOOT_BASE	(0xc0000000 + OUR_UBOOT_OFFS)
#else
#define CONFIG_SYS_UBOOT_BASE	CONFIG_SYS_PHY_UBOOT_BASE
#endif

/* Init value for stack pointer; we have the stack behind the u-boot code and
   heap ate the end of the memory region reserved for u-boot; 12 bytes are
   subtracted to leave 3 words for the abort-stack */
#if 0
#define CONFIG_SYS_INIT_SP_ADDR	\
	(CONFIG_SYS_TEXT_BASE - CONFIG_SYS_GBL_DATA_SIZE)
#else
/* Set to internal RAM (IRAM), mapped from 0xD002000-0xD0037FFF */
#define CONFIG_SYS_INIT_SP_ADDR	(0xD0038000 - CONFIG_SYS_GBL_DATA_SIZE)
#endif

/* Size of malloc() pool (heap) */
#define CONFIG_SYS_MALLOC_LEN	(CONFIG_ENV_SIZE + 512*1024)

/* Size in bytes reserved for initial data */
#define CONFIG_SYS_GBL_DATA_SIZE	256

/* Alignment mask for MMU pagetable: 16kB */
#define CONFIG_SYS_TLB_ALIGN 0xFFFFC000

/* Stack */

/* The stack sizes are set up in start.S using the settings below */
#define CONFIG_SYS_STACK_SIZE	(128*1024)  /* 128KB */
//#define CONFIG_STACKSIZE	0x20000	  /* (unused on S3C6410) */
#ifdef CONFIG_USE_IRQ
#define CONFIG_STACKSIZE_IRQ	(4*1024)  /* IRQ stack */
#define CONFIG_STACKSIZE_FIQ	(4*1024)  /* FIQ stack */
#endif

/* Memory test checks all RAM before U-Boot (i.e. leaves last MB with U-Boot
   untested) ### If not set, test from beginning of RAM to before stack. */
//####define CONFIG_SYS_MEMTEST_START CONFIG_SYS_SDRAM_BASE
//####define CONFIG_SYS_MEMTEST_END	(CONFIG_SYS_SDRAM_BASE + OUR_UBOOT_OFFS)

/* Default load address */
#define CONFIG_SYS_LOAD_ADDR	(CONFIG_SYS_SDRAM_BASE + 0x8000)




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
#define CONFIG_SERIAL0          1	  /* UART0 on armStoneA8 */
#define CONFIG_SERIAL_MULTI		  /* Support several serial lines */
//#define CONFIG_CONSOLE_MUX		  /* Allow several consoles at once */

#define CONFIG_BAUDRATE		38400

/* valid baudrates */
#define CONFIG_SYS_BAUDRATE_TABLE	{9600, 19200, 38400, 57600, 115200}

#if defined(CONFIG_CMD_KGDB)
#define CONFIG_KGDB_BAUDRATE	115200	  /* speed to run kgdb serial port */
#define CONFIG_KGDB_SER_INDEX	1	  /* which serial port to use */
#endif

#define CONFIG_SYS_CONSOLE_IS_IN_ENV	  /* Console can be saved in env */


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
 * Real Time Clock (RTC)
 ************************************************************************/
#define CONFIG_RTC_S5P	1

/************************************************************************
 * Command definition
 ************************************************************************/
//#include <config_cmd_default.h>
#define CONFIG_CMD_SOURCE	/* Source support (was autoscr)	*/
#define CONFIG_CMD_BDI		/* bdinfo			*/
#define CONFIG_CMD_BOOTD	/* bootd			*/
#define CONFIG_CMD_CONSOLE	/* coninfo			*/
#define CONFIG_CMD_ECHO		/* echo arguments		*/
#define CONFIG_CMD_SAVEENV	/* saveenv			*/
#define CONFIG_CMD_ASKENV	/* askenv			*/
#define CONFIG_CMD_EDITENV	/* editenv			*/
#define CONFIG_CMD_RUN		/* run command in env variable	*/
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
#undef CONFIG_CMD_SETGETDCR	/* DCR support on 4xx		*/
#undef CONFIG_CMD_XIMG		/* Load part of Multi Image	*/


#undef CONFIG_CMD_FLASH
#undef CONFIG_CMD_FPGA

#define CONFIG_CMD_CACHE
#define CONFIG_CMD_USB
//#####define CONFIG_CMD_MMC
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
/* This board has a NE2000 compatible AX88769B ethernet chip */
#define CONFIG_DRIVER_NE2000
#define CONFIG_DRIVER_NE2000_BASE	0x80000000
#define CONFIG_DRIVER_NE2000_BASE2	0x88000000
#define CONFIG_DRIVER_NE2000_SOFTMAC
#define CONFIG_DRIVER_AX88796L


#ifndef CONFIG_ARMSTONEA8 /* Already done in NBoot */
/************************************************************************
 * CPU (PLL) timings
 ************************************************************************/

//#define CONFIG_CLK_800_133_66
//#define CONFIG_CLK_666_133_66
#define CONFIG_CLK_532_133_66
//#define CONFIG_CLK_400_133_66
//#define CONFIG_CLK_400_100_50
#endif /* !CONFIG_ARMSTONEA8 */


#ifndef CONFIG_ARMSTONEA8 /* Already done in NBoot */
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
#endif /* !CONFIG_ARMSTONEA8 */


/************************************************************************
 * USB host
 ************************************************************************/

/* Define one of the following two lines to select the USB host driver */
#undef CONFIG_USB_OHCI_NEW		  /* Use OHCI (only USB 1.1) */
#define CONFIG_USB_EHCI			  /* Use EHCI (USB 2.0 capable) */

/* Settings for OHCI driver */
#ifdef CONFIG_USB_OHCI_NEW
#define CONFIG_USB_S5P			  /* Include S5P specific stuff */
#define CONFIG_SYS_USB_OHCI_REGS_BASE samsung_get_base_ohci()
#define CONFIG_SYS_USB_OHCI_CPU_INIT	  /* Call S5P specific stuff */
#define CONFIG_SYS_USB_OHCI_SLOT_NAME "s5p"
#define CONFIG_SYS_USB_OHCI_MAX_ROOT_PORTS 2
#endif

/* Settings for EHCI driver */
#ifdef CONFIG_USB_EHCI
#define CONFIG_USB_EHCI_S5P		  /* Use S5P driver for EHCI */
#endif

/* In both cases activate storage devices */
#define CONFIG_USB_STORAGE


/************************************************************************
 * Keyboard
 ************************************************************************/
//#define CONFIG_USB_KEYBOARD
//#define CONFIG_SYS_DEVICE_DEREGISTER	/* Required for CONFIG_USB_KEYBOARD */


/************************************************************************
 * USB device
 ************************************************************************/
/* No support for special USB device download on this board */
#undef CONFIG_S3C_USBD
//#define USBD_DOWN_ADDR		0xc0000000


/************************************************************************
 * OneNAND Flash
 ************************************************************************/
/* No support for OneNAND */
#undef CONFIG_ONENAND
#define CONFIG_SYS_MAX_ONENAND_DEVICE	0
#define CONFIG_SYS_ONENAND_BASE 	(0x70100000)


/************************************************************************
 * MoviNAND Flash
 ************************************************************************/
/* No support for MoviNAND */
#undef CONFIG_MOVINAND


/************************************************************************
 * NOR Flash
 ************************************************************************/
/* No support for NOR flash */
#define CONFIG_SYS_NO_FLASH	1	  /* no NOR flash */     


/************************************************************************
 * SD/MMC card support
 ************************************************************************/
#define CONFIG_MMC			  /* Support for SD/MMC card */
#define CONFIG_GENERIC_MMC		  /* Using the generic driver */
#define CONFIG_S5P_MMC			  /* with support for S5P */


/************************************************************************
 * NAND flash organization
 ************************************************************************/
/* We have one NAND device */
#define CONFIG_NAND_S5P    1
#define CONFIG_SYS_MAX_NAND_DEVICE	1

/* One chip per device */
#define CONFIG_SYS_NAND_MAX_CHIPS	1

/* Address of the DATA register for reading and writing data */
#define CONFIG_SYS_NAND_BASE	(0x70200010)

#define CONFIG_NAND_NBOOT	1	  /* Support NBoot with ECC8 */

#define CONFIG_SYS_NAND_NBOOT_SIZE 0x40000 /* 256KB NBoot, uses ECC8 */
#define CONFIG_SYS_NAND_PAGE_SIZE  2048	  /* 2048 bytes per page */
#define CONFIG_SYS_NAND_PAGE_COUNT 64	  /* Pages per block */
#define CONFIG_SYS_NAND_ECCSIZE    512	  /* Full page in one ECC cycle */
#define CONFIG_SYS_NAND_ECCBYTES   4	  /* 4 ECC bytes per 512 data bytes */

#define CONFIG_SYS_NAND_BLOCK_SIZE \
		(CONFIG_SYS_NAND_PAGE_COUNT * CONFIG_SYS_NAND_PAGE_SIZE)

/* Use hardware ECC */
#define CONFIG_SYS_S5P_NAND_HWECC

/* Support JFFS2 in NAND (commands: fsload, ls, fsinfo) */
#define CONFIG_JFFS2_NAND

/* Support mtd partitions (commands: mtdparts, chpart) */
#define CONFIG_CMD_MTDPARTS


/************************************************************************
 * Environment
 ************************************************************************/
/* Use this if the environment should be in the NAND flash */
#define CONFIG_ENV_IS_IN_NAND

#define CONFIG_ENV_ADDR		0	  /* Only needed for NOR flash */

/* The environment is 128KB and can use a region of 256KB, allowing for one
   bad block. */
#define CONFIG_ENV_SIZE	   0x00020000	  /* 128KB actual environment */
#define CONFIG_ENV_RANGE   0x00040000	  /* 256KB region for environment */
#define CONFIG_ENV_OFFSET  0x000C0000

/* When saving the environment, we usually have a short period of time between
   erasing the NAND region and writing the new data where no valid environment
   is available. To avoid this time, we can save the environment alternatively
   to two different locations in the NAND flash. Then at least one of the
   environments is always valid. Currently we don't use this feature. */
//#define CONFIG_SYS_ENV_OFFSET_REDUND   0x0007c000


/* We have one NAND chip, give it a name */
#define MTDIDS_DEFAULT		"nand0=pm6nand0"

/* Define the default partitions on this NAND chip */
#define MTDPARTS_DEFAULT	"mtdparts=pm6nand0:256k(NBoot)ro,512k(UBoot),256k(UBootEnv),4m(UserDef),3m(Kernel),-(TargetFS)"

#define CONFIG_MTD_DEVICE
//#define CONFIG_MTD_PARTITIONS  /* For UBI */

#define CONFIG_BOOTDELAY	3
#define CONFIG_BOOTARGS    	"console=ttySAC0,38400 init=linuxrc"
#define CONFIG_BOOTCOMMAND      "nand read.jffs2 41000000 Kernel ; bootm 41000000"
#define CONFIG_EXTRA_ENV_SETTINGS \
        "bootlocal=setenv bootargs console=ttySAC2,38400 $(mtdparts) init=linuxrc root=/dev/mtdblock5 ro rootfstype=jffs2\0" \
        "bootnfs=setenv bootargs console=ttySAC2,38400 $(mtdparts) init=linuxrc root=/dev/nfs rw nfsroot=/rootfs ip=$(ipaddr):$(serverip):$(gatewayip):$(netmask)\0" \
	"r=tftp 41000000 zImage ; bootm 41000000\0" \
	"autoload=armStoneA8/autoload.scr\0" \
	"autommc=1\0" \
	"autousb=3\0" \
	"autoaddr=40000000\0"
#define CONFIG_ETHADDR		00:05:51:05:2a:73
#define CONFIG_NETMASK          255.255.255.0
#define CONFIG_IPADDR		10.0.0.252
#define CONFIG_SERVERIP		10.0.0.122
#define CONFIG_GATEWAYIP	10.0.0.5
#define CONFIG_BOOTFILE         "zImage"
#define CONFIG_LOADADDR         40000000

/* allow to overwrite serial and ethaddr */
#define CONFIG_ENV_OVERWRITE

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

/************************************************************************
 * Some definitions
 ************************************************************************/
#ifndef __ASSEMBLY__
extern char fs_board_prompt[];
#endif

#endif /* !__CONFIG_H */
