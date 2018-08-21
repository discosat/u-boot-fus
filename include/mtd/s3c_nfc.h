/*
 * Samsung S3C NAND Flash Controller Driver with Hardware ECC
 *
 * Copyright 2014 F&S Elektronik Systeme GmbH
 *
 * SPDX-License-Identifier:	GPL-2.0+
 */

#ifndef __S3C_NFC_H__
#define __S3C_NFC_H__

/* Possible values for flags entry */
#define S3C_NFC_SKIP_INVERSE 0x01	/* Use skip region only, skip rest */

/* Possible values for eccmode entry if CONFIG_SYS_S3C_NAND_HWECC is set */
#define S3C_NFC_ECCMODE_OFF   0		/* No ECC */
#define S3C_NFC_ECCMODE_1BIT  1		/* 4 ECC Bytes, unit <= 2048 bytes */
#define S3C_NFC_ECCMODE_4BIT  2		/* 7 ECC Bytes, unit == 512 bytes */
#define S3C_NFC_ECCMODE_8BIT  3		/* 13 ECC Bytes, unit == 512 bytes */

/* Sizes and offsets are given in blocks because the NAND parameters (like
   block size) are not yet known when this structure is filled in. */
struct s3c_nfc_platform_data {
	unsigned int	options;	/* Basic set of options */
	unsigned int	t_wb;		/* Wait cycles for T_WB and T_WHR
					   (0: use current value from NFC) */
	unsigned int	eccmode;	/* See S3C_NFC_ECCMODE_* above */
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


extern void s3c_nand_register(int nfc_hw_id,
			      const struct s3c_nfc_platform_data *pdata);

#endif /* !__S3C_NFC_H__ */
