/*
 * Samsung S5P NAND Flash Controller Driver with Hardware ECC
 *
 * Copyright 2014 F&S Elektronik Systeme GmbH
 *
 * This is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */

#ifndef __S5P_NFC_H__
#define __S5P_NFC_H__

/* Possible values for flags entry */
#define S5P_NFC_SKIP_INVERSE 0x01	/* Use skip region only, skip rest */

/* Possible values for eccmode entry if CONFIG_SYS_S5P_NAND_HWECC is set */
#define S5P_NFC_ECCMODE_OFF   0		/* No ECC */
#define S5P_NFC_ECCMODE_1BIT  1		/* 4 ECC Bytes, unit <= 2048 bytes */
#define S5P_NFC_ECCMODE_4BIT  2		/* 7 ECC Bytes, unit == 512 bytes */
#define S5P_NFC_ECCMODE_8BIT  3		/* 13 ECC Bytes, unit <= 1024 bytes */
#define S5P_NFC_ECCMODE_12BIT 4		/* 20 ECC Bytes, unit <= 1024 bytes */
#define S5P_NFC_ECCMODE_16BIT 5		/* 26 ECC Bytes, unit <= 1024 bytes */

/* Sizes and offsets are given in blocks because the NAND parameters (like
   block size) are not yet known when this structure is filled in. */
struct s5p_nfc_platform_data {
	unsigned int	options;	/* Basic set of options */
	unsigned int	t_wb;		/* Wait cycles for T_WB and T_WHR
					   (0: use current value from NFC) */
	unsigned int	eccmode;	/* See S5P_NFC_ECCMODE_* above */
	unsigned int	flags;		/* See flag values above */
	unsigned int	skipblocks;	/* Number of blocks to skip at
					   beginning of device */
#ifdef CONFIG_NAND_REFRESH
	unsigned int	backup_sblock;	/* First block for backup */
	unsigned int	backup_eblock;	/* Last block for backup; if
					   backup_sblock > backup_eblock, use
					   decreasing order */
#endif
};


extern void s5p_nand_register(int nfc_hw_id,
			      const struct s5p_nfc_platform_data *pdata);

#endif /* !__S5P_NFC_H__ */
