/*
 * fs_board_common.h
 *
 * (C) Copyright 2018
 * Hartmut Keller, F&S Elektronik Systeme GmbH, keller@fs-net.de
 *
 * Common board configuration and information
 *
 * SPDX-License-Identifier:	GPL-2.0+
 */

#ifndef __FS_BOARD_COMMON_H__
#define __FS_BOARD_COMMON_H__

#include <config.h>

#ifdef CONFIG_ARCH_IMX8M
#define HAVE_BOARD_CFG			/* Use BOARD-CFG, no fs_nboot_args */
#endif

#include <asm/mach-imx/boot_mode.h>	/* enum boot_device */

#ifndef HAVE_BOARD_CFG

#define FSHWCONFIG_ARGS_ID 0x4E424F54	/* Magic number for dwID: 'NBOT' */ 
struct fs_nboot_args {
	u32	dwID;			/* 'NBOT' if valid */
	u32	dwSize;			/* 16*4 */
	u32	dwNBOOT_VER;
	u32	dwMemSize;		/* size of SDRAM in MB */
	u32	dwFlashSize;		/* size of NAND flash in MB */
	u32	dwDbgSerPortPA;		/* Phys addr of serial debug port */
	u32	dwNumDram;		/* Installed memory chips */
	u32	dwAction;		/* (unused in U-Boot) */
	u32	dwCompat;		/* (unused in U-Boot) */
	char	chPassword[8];		/* (unused in U-Boot) */
	u8	chBoardType;
	u8	chBoardRev;
	u8	chFeatures1;
	u8	chFeatures2;
	u16	wBootStartBlock;	/* Start block number of bootloader */
	u8	chECCtype;		/* ECC type used */
	u8	chECCstate;		/* NAND error state */
	u32	dwReserved[3];
};

#define FSM4CONFIG_ARGS_ID 0x4D344D34	/* Magic number for dwID: 'M4M4' */
struct fs_m4_args
{
	u32	dwID;			/* 'M4M4' if valid */
	u32	dwSize;			/* 8*4 */
	u32	dwCRC;			/* Checksum of M4 image */
	u32	dwLoadAddress;		/* Load address of M4 image */
	u32	dwMCCAddress;		/* Load address of MCC library */
	u32	dwReserved[3];
};

/* List NBoot args for debugging */
void fs_board_show_nboot_args(struct fs_nboot_args *pargs);

/* Get a pointer to the NBoot args */
struct fs_nboot_args *fs_board_get_nboot_args(void);

#else

struct cfg_info {
	unsigned int board_type;
	unsigned int board_rev;
	enum boot_device boot_dev;
	unsigned int features;
	unsigned int dram_size;
	unsigned int dram_chips;
};

/* Get the boot device that is programmed in the fuses. */
enum boot_device fs_board_get_boot_dev_from_fuses(void);

#endif /* !HAVE_BOARD_CFG */

struct fs_board_info {
	char *name;			/* Device name */
	char *bootdelay;		/* Default value for bootdelay */
	char *updatecheck;		/* Default value for updatecheck */
	char *installcheck;		/* Default value for installcheck */
	char *recovercheck;		/* Default value for recovercheck */
	char *console;			/* Default variable for console */
	char *login;			/* Default variable for login */
	char *mtdparts;			/* Default variable for mtdparts */
	char *network;			/* Default variable for network */
	char *init;			/* Default variable for init */
};

/* Get the configured boot device (also valid before fuses are programmed) */
enum boot_device fs_board_get_boot_dev(void);

/* Get the boot device number from the string */
enum boot_device fs_board_get_boot_dev_from_name(const char *name);

/* Get the string from the boot device number */
const char *fs_board_get_name_from_boot_dev(enum boot_device boot_dev);

/* Get Pointer to struct cfg_info */
struct cfg_info *fs_board_get_cfg_info(void);

/* Get the board features */
unsigned int fs_board_get_features(void);

/* Get board type (zero-based) */
unsigned int fs_board_get_type(void);

/* Get board revision (major * 100 + minor, e.g. 120 for rev 1.20) */
unsigned int fs_board_get_rev(void);

/* Get NBoot version (VNxx or YYYY.MM[.P]) */
const char *fs_board_get_nboot_version(void);

/* Issue reset signal on up to three gpios (~0: gpio unused) */
void fs_board_issue_reset(uint active_us, uint delay_us,
			  uint gpio0, uint gpio1, uint gpio2);

/* Copy NBoot args to variables and prepare command prompt string */
void fs_board_init_common(const struct fs_board_info *board_info);

/* Set up all board specific variables */
void fs_board_late_init_common(const char *serial_name);

#endif /* !__FS_BOARD_COMMON_H__ */
