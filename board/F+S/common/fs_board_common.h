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

struct fs_board_info {
	char *name;			/* Device name */
	char *bootdelay;		/* Default value for bootdelay */
	char *updatecheck;		/* Default value for updatecheck */
	char *installcheck;		/* Default value for installcheck */
	char *recovercheck;		/* Default value for recovercheck */
	char *earlyusbinit;		/* Default value for earlyusbinit */
	char *console;			/* Default variable for console */
	char *login;			/* Default variable for login */
	char *mtdparts;			/* Default variable for mtdparts */
	char *network;			/* Default variable for network */
	char *init;			/* Default variable for init */
	char *rootfs;			/* Default variable for rootfs */
	char *kernel;			/* Default variable for kernel */
	char *fdt;			/* Default variable for device tree */
};

/* List NBoot args for debugging */
void fs_board_show_nboot_args(struct fs_nboot_args *pargs);

/* Get a pointer to the NBoot args */
struct fs_nboot_args *fs_board_get_nboot_args(void);

/* Get board type (zero-based) */
unsigned int fs_board_get_type(void);

/* Get board revision (major * 100 + minor, e.g. 120 for rev 1.20) */
unsigned int fs_board_get_rev(void);

/* Issue reset signal on up to three gpios (~0: gpio unused) */
void fs_board_issue_reset(uint active_us, uint delay_us,
			  uint gpio0, uint gpio1, uint gpio2);

/* Copy NBoot args to variables and prepare command prompt string */
void fs_board_init_common(const struct fs_board_info *board_info);

/* Set up all board specific variables */
void fs_board_late_init_common(void);


#endif /* !__FS_BOARD_COMMON_H__ */
