/*
 * Vybrid NAND Flash Controller Driver with Hardware ECC
 *
 * Copyright 2014 F&S Elektronik Systeme GmbH
 *
 * This is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */

#ifndef __FSL_NFC_FUS_H__
#define __FSL_NFC_FUS_H__

/* Possible values for flags entry */
#define VYBRID_NFC_SKIP_INVERSE 0x01	/* Use skip region only, skip rest */

struct fsl_nfc_fus_platform_data {
	unsigned int	options;	/* Basic set of options */
	unsigned int	t_wb;		/* Wait cycles for T_WB and T_WHR
					   (0: use current value from NFC) */
	unsigned int	eccmode;	/* Mode  ECC Bytes  Correctable Errors
					   -----------------------------------
					   0:       0              0
					   1:       8              4
					   2:      12              6
					   3:      15              8
					   4:      23             12
					   5:      30             16
					   6:      45             24
                                           7:      60             32  */
	unsigned int	flags;		/* See flag values above */
	unsigned int	skipblocks;	/* Number of blocks to skip at
					   beginning of device */
};

#endif /* !__FSL_NFC_FUS_H__ */
