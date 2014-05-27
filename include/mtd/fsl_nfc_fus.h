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

struct fsl_nfc_fus_prv {
	uint	column;		/* Column to read in read_byte() */
	uint	last_command;	/* Previous command issued */
	u32	timeout;	/* << CONFIG_CMD_TIMEOUT_SHIFT */
	u32	eccmode;	/* ECC_BYPASS .. ECC_60_BYTE */
};

#endif /* !__FSL_NFC_FUS_H__ */
