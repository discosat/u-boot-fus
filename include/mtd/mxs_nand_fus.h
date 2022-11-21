/*
 * Vybrid NAND Flash Controller Driver with Hardware ECC
 *
 * Copyright 2014 F&S Elektronik Systeme GmbH
 *
 * SPDX-License-Identifier:	GPL-2.0+
 */

#ifndef __MXS_NAND_FUS_H__
#define __MXS_NAND_FUS_H__

/* Possible values for flags entry */
#define MXS_NAND_SKIP_INVERSE 0x01	/* Use skip region only, skip rest */
#define MXS_NAND_CHUNK_1K     0x02	/* Chunk size is 1024, not 512 bytes */

/* Sizes and offsets are given in blocks because the NAND parameters (like
   block size) are not yet known when this structure is filled in. */
struct mxs_nand_fus_platform_data {
	unsigned int	options;	/* Basic set of options */
	unsigned int	timing0;	/* Wait cycles for T_WB and T_WHR
					   (0: use current value from NFC) */
	unsigned int	ecc_strength;
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


extern void mxs_nand_register(int nfc_hw_id,
			      const struct mxs_nand_fus_platform_data *pdata);

#endif /* !__MXS_NAND_FUS_H__ */
