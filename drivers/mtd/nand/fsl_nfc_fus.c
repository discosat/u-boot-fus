/*
 * Vybrid NAND Flash Controller Driver with Hardware ECC
 * Based on original driver fsl_nfc.c
 *
 * Copyright 2014 F&S Elektronik Systeme GmbH
 *
 * This is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */

#include <common.h>
#include <malloc.h>

#include <linux/mtd/mtd.h>
#include <linux/mtd/nand.h>
#include <linux/mtd/nand_ecc.h>

#include <mtd/fsl_nfc.h>
#include <mtd/fsl_nfc_fus.h>		/* struct fsl_nfc_fus_platform_data */

#include <nand.h>
#include <errno.h>
#include <asm/io.h>
#include <asm/processor.h>

#define	DRV_NAME		"fsl_nfc_fus"
#define	DRV_VERSION		"V1.1"

#define ECC_STATUS_OFFS		0x840
#define ECC_STATUS_MASK		0x80
#define ECC_ERR_COUNT		0x3F

/* Timeout values. In our case they must enclose the transmission time for the
   command, address and data byte cycles. The timer resolution in U-Boot is
   1/1000s, i.e. CONFIG_SYS_HZ=1000 and any final value < 2 will not work well. */
#define NFC_TIMEOUT_DATA	(CONFIG_SYS_HZ * 100) / 1000
#define NFC_TIMEOUT_WRITE	(CONFIG_SYS_HZ * 100) / 1000
#define NFC_TIMEOUT_ERASE	(CONFIG_SYS_HZ * 400) / 1000

struct fsl_nfc_fus_prv {
	struct nand_chip chip;	/* Generic NAND chip info */
	uint	column;		/* Column to read in read_byte() */
	uint	last_command;	/* Previous command issued */
	u32	eccmode;	/* ECC_BYPASS .. ECC_60_BYTE */
	u32	cfgbase;	/* fix part in CFG: timeout, buswidth */
	u32	cmdclr;		/* Address cycle bits to clear in NFC cmd */
};

struct fsl_nfc_fus_prv nfc_infos[CONFIG_SYS_MAX_NAND_DEVICE];

static const uint bitflip_threshold[8] = {
	1, 3, 5, 7, 11, 15, 22, 30
};

/* NAND page layouts for all possible Vybrid ECC modes. The OOB area has the
   following appearance:

     Offset          Bytes               Meaning
     ---------------------------------------------------------------
     0               4                   Bad Block Marker (BBM)
     4               eccbytes            Error Correction Code (ECC)
     4 + eccbytes    oobfree.length      Free OOB area (user part)

   As the size of the OOB is in mtd->oobsize and the number of free bytes
   (= size of the user part) is stored in mtd->oobavail, we can compute the
   offset of the free OOB area also by mtd->oobsize - mtd->oobavail. */
static struct nand_ecclayout fus_nfc_ecclayout[8] =
{
	{
		/* 0 ECC bytes, no correctable bit errors */
		.eccbytes = 0,
		.eccpos = { },
		.oobfree = {
			{.offset = 4,
			 .length = 60} }
	},
	{
		/* 8 ECC bytes, 4 correctable bit errors */
		.eccbytes = 8,
		.eccpos = { 4,  5,  6,  7,  8,  9, 10, 11},
		.oobfree = {
			{.offset = 12,
			 .length = 52} }
	},
	{
		/* 12 ECC bytes, 6 correctable bit errors */
		.eccbytes = 12,
		.eccpos = { 4,  5,  6,  7,  8,  9, 10, 11, 12, 13,
			    14, 15},
		.oobfree = {
			{.offset = 16,
			 .length = 48} }
	},
	{
		/* 15 ECC bytes, 8 correctable bit errors */
		.eccbytes = 15,
		.eccpos = { 4,  5,  6,  7,  8,  9, 10, 11, 12, 13,
			    14, 15, 16, 17, 18},
		.oobfree = {
			{.offset = 19,
			 .length = 45} }
	},
	{
		/* 23 ECC bytes, 12 correctable bit errors */
		.eccbytes = 23,
		.eccpos = { 4,  5,  6,  7,  8,  9, 10, 11, 12, 13,
			    14, 15, 16, 17, 18, 19, 20, 21, 22, 23,
			    24, 25, 26},
		.oobfree = {
			{.offset = 27,
			 .length = 37} }
	},
	{
		/* 30 ECC bytes, 16 correctable bit errors */
		.eccbytes = 30,
		.eccpos = { 4,  5,  6,  7,  8,  9, 10, 11, 12, 13,
			    14, 15, 16, 17, 18, 19, 20, 21, 22, 23,
			    24, 25, 26, 27, 28, 29, 30, 31, 32, 33},
		.oobfree = {
			{.offset = 34,
			 .length = 30} }
	},
	{
		/* 45 ECC bytes, 24 correctable bit errors */
		.eccbytes = 45,
		.eccpos = { 4,  5,  6,  7,  8,  9, 10, 11, 12, 13,
			    14, 15, 16, 17, 18, 19, 20, 21, 22, 23,
			    24, 25, 26, 27, 28, 29, 30, 31, 32, 33,
			    34, 35, 36, 37, 38, 39, 40, 41, 42, 43,
			    44, 45, 46, 47, 48},
		.oobfree = {
			{.offset = 49,
			 .length = 15} }
	},
	{
		/* 60 ECC bytes, 32 correctable bit errors */
		.eccbytes = 60,
		.eccpos = { 4,  5,  6,  7,  8,  9, 10, 11, 12, 13,
			    14, 15, 16, 17, 18, 19, 20, 21, 22, 23,
			    24, 25, 26, 27, 28, 29, 30, 31, 32, 33,
			    34, 35, 36, 37, 38, 39, 40, 41, 42, 43,
			    44, 45, 46, 47, 48, 49, 50, 51, 52, 53,
			    54, 55, 56, 57, 58, 59, 60, 61, 62, 63},
		.oobfree = { }
	}
};


static ulong nfc_base_addresses[] = {
	NFC_BASE_ADDR
};


/* -------------------- LOCAL HELPER FUNCTIONS ----------------------------- */

/* Read NFC register */
static inline u32 nfc_read(const struct nand_chip *chip, uint reg)
{
	return in_le32(chip->IO_ADDR_R + reg);
}


/* Write NFC register */
static inline void nfc_write(const struct nand_chip *chip, uint reg, u32 val)
{
	out_le32(chip->IO_ADDR_R + reg, val);
}


/* Read a byte from NFC RAM; the RAM is connected in 32 bit big endian mode to
   the bus, so we have to be careful with the lower two offset bits */
static inline u8 nfc_read_ram(const struct nand_chip *chip, uint offset)
{
	return in_8(chip->IO_ADDR_R + NFC_MAIN_AREA(0) + (offset ^ 3));
}


/* Write a byte to NFC RAM; the RAM is connected in 32 bit big endian mode to
   the bus, so we have to be careful with the lower two offset bits */
static inline void nfc_write_ram(const struct nand_chip *chip,
				 uint offset, u8 val)
{
	out_8(chip->IO_ADDR_R + NFC_MAIN_AREA(0) + (offset ^ 3), val);
}


/* Copy data from NFC RAM */
static void nfc_copy_from_nfc(const struct nand_chip *chip, u8 *buffer,
			      uint size, uint ramoffset)
{
	while (size--)
		*buffer++ = nfc_read_ram(chip, ramoffset++);
}


/* Copy data to NFC RAM */
static void nfc_copy_to_nfc(const struct nand_chip *chip, const u8 *buffer,
			    uint size, uint ramoffset)
{
	while (size--)
		nfc_write_ram(chip, ramoffset++, *buffer++);
}


/* Start a comand with one command byte and wait for completion */
static inline void nfc_send_cmd_1(const struct nand_chip *chip,
				  u32 cmd_byte1, u32 cmd_code)
{
	struct fsl_nfc_fus_prv *prv = chip->priv;

	/* Clear unneeded column and row cycles */
	cmd_code &= ~prv->cmdclr;

	/* Clear IDLE and DONE status bits */
	nfc_write(chip, NFC_IRQ_STATUS,
		  (1 << CMD_DONE_CLEAR_SHIFT) | (1 << IDLE_CLEAR_SHIFT));

#if 0
	printf("###CMD1=%08x CMD2=%08x CAR=%08x RAR=%08x SIZE=%04x CFG=%08x\n",
	       nfc_read(chip, NFC_FLASH_CMD1),  (cmd_byte1 << CMD_BYTE1_SHIFT)
	       | (cmd_code << CMD_CODE_SHIFT) | (1 << START_SHIFT),
	       nfc_read(chip, NFC_COL_ADDR), nfc_read(chip, NFC_ROW_ADDR),
	       nfc_read(chip, NFC_SECTOR_SIZE), nfc_read(chip, NFC_FLASH_CONFIG));
#endif

	/* Write first command byte, command code, buffer 0, start transfer */
	nfc_write(chip, NFC_FLASH_CMD2,
		  (cmd_byte1 << CMD_BYTE1_SHIFT)
		  | (cmd_code << CMD_CODE_SHIFT)
		  | (0 << BUFNO_SHIFT)
		  | (1 << START_SHIFT));
}


/* Start a command with two (or three) command bytes, e.g. program page */
static inline void nfc_send_cmd_2(const struct nand_chip *chip,
				  u32 cmd_byte1, u32 cmd_byte2, u32 cmd_code)
{
	/* Write second command byte, the third command byte (if used) is
	   always NAND_CMD_STATUS */
	nfc_write(chip, NFC_FLASH_CMD1, (cmd_byte2 << CMD_BYTE2_SHIFT)
		  | (NAND_CMD_STATUS << CMD_BYTE3_SHIFT));

	/* The rest is like the one byte command */
	nfc_send_cmd_1(chip, cmd_byte1, cmd_code);
}


/* Set column for command */
static void nfc_set_column(struct nand_chip *chip, int column, uint command)
{
	if (column < 0) {
		printf("### Missing column for NAND_CMD 0x%02x\n", command);
		return;
	}

	nfc_write(chip, NFC_COL_ADDR, column << COL_ADDR_SHIFT);
}


/* Set row for command */
static void nfc_set_row(struct nand_chip *chip, int row, uint command)
{
	u32 rowreg;

	if (row < 0) {
		printf("### Missing row for NAND_CMD 0x%02x\n", command);
		return;
	}

	rowreg = nfc_read(chip, NFC_ROW_ADDR);
	rowreg &= ~ROW_ADDR_MASK;
	rowreg |= row << ROW_ADDR_SHIFT;
	nfc_write(chip, NFC_ROW_ADDR, rowreg);
}


static int nfc_do_write_oob(struct mtd_info *mtd, struct nand_chip *chip,
			    int page, uint offs)
{
	struct fsl_nfc_fus_prv *prv = chip->priv;

	/* Start programming sequence with NAND_CMD_SEQIN */
	chip->cmdfunc(mtd, NAND_CMD_SEQIN, offs + mtd->writesize, page);

	/* Copy OOB data to NFC RAM; as we only write these few bytes and no
	   main data, they have to be set at offset 0 in NFC RAM */
	nfc_copy_to_nfc(chip, chip->oob_poi + offs, mtd->oobsize - offs, 0);

	/* Write some OOB bytes */
	nfc_write(chip, NFC_SECTOR_SIZE, mtd->oobsize - offs);

	/* Set ECC mode to BYPASS, one virtual page */
	nfc_write(chip, NFC_FLASH_CONFIG,
		  prv->cfgbase
		  | (0 << CONFIG_STOP_ON_WERR_SHIFT)
		  | (0 << CONFIG_ECC_SRAM_ADDR_SHIFT)
		  | (0 << CONFIG_ECC_SRAM_REQ_SHIFT)
		  | (0 << CONFIG_DMA_REQ_SHIFT)
		  | (ECC_BYPASS << CONFIG_ECC_MODE_SHIFT)
		  | (0 << CONFIG_FAST_FLASH_SHIFT)
		  | (0 << CONFIG_ID_COUNT_SHIFT)
		  | (0 << CONFIG_BOOT_MODE_SHIFT)
		  | (0 << CONFIG_ADDR_AUTO_INCR_SHIFT)
		  | (0 << CONFIG_BUFNO_AUTO_INCR_SHIFT)
		  | (1 << CONFIG_PAGE_CNT_SHIFT));

	/* Now actually trigger the programming by sending NAND_CMD_PAGEPROG */
	chip->cmdfunc(mtd, NAND_CMD_PAGEPROG, -1, -1);

	/* Read programming status */
	if (chip->waitfunc(mtd, chip) & NAND_STATUS_FAIL)
		return -EIO;

	return 0;
}


/* Wait for operation complete */
static int nfc_wait_ready(struct mtd_info *mtd, unsigned long timeout)
{
	struct nand_chip *chip = mtd->priv;
	u32 time_start;

	time_start = get_timer(0);
	do {
		if (nfc_read(chip, NFC_IRQ_STATUS) & IDLE_IRQ_MASK) {
			nfc_write(chip, NFC_IRQ_STATUS,
				  (1 << CMD_DONE_CLEAR_SHIFT)
				  | (1 << IDLE_CLEAR_SHIFT));
			return 0;
		}
	} while (get_timer(time_start) < timeout);

	printf("Timeout waiting for Ready\n");

	return 1;
}


/* Count the number of zero bits in a value; start at the LSB to be fast in
   case of 8-bit values */
static uint count_zeroes(u32 value)
{
	uint count = 0;

	while (value != 0xFFFFFFFF) {
		if (!(value & 1))
			count++;
		value = (value >> 1) | 0x80000000;
	}

	return count;
}


/* -------------------- INTERFACE FUNCTIONS -------------------------------- */

/* Control chip select signal on the board */
static void fus_nfc_select_chip(struct mtd_info *mtd, int chipnr)
{
	struct nand_chip *chip = mtd->priv;
	u32 rowreg;

	rowreg = nfc_read(chip, NFC_ROW_ADDR);
	rowreg &= ~ROW_ADDR_CHIP_SEL_MASK;
	if (chipnr >= 0)
		rowreg |= (1 << chipnr) << ROW_ADDR_CHIP_SEL_SHIFT;
	rowreg |= 1 << ROW_ADDR_CHIP_SEL_RB_SHIFT;
	nfc_write(chip, NFC_ROW_ADDR, rowreg);
}


/* Check if command is done (in nand_wait_ready()) */
static int fus_nfc_dev_ready(struct mtd_info *mtd)
{
	/* We have called nfc_wait_ready() before, so we can be sure that our
	   command is already completed. So just return "done". */
	return 1;
}


/* Read byte from NFC buffers */
static uint8_t fus_nfc_read_byte(struct mtd_info *mtd)
{
	struct nand_chip *chip = mtd->priv;
	struct fsl_nfc_fus_prv *prv = chip->priv;
	u32 val;
	uint col = prv->column++;

	if (prv->last_command == NAND_CMD_STATUS)
		val = nfc_read(chip, NFC_FLASH_STATUS2) & STATUS_BYTE1_MASK;
	else if (prv->last_command == NAND_CMD_READID) {
		val = nfc_read(chip,
			       col < 4 ? NFC_FLASH_STATUS1 : NFC_FLASH_STATUS2);
		val >>= ((col & 3) ^ 3) << 3;
	} else
		val = nfc_read_ram(chip, col);

	return (uint8_t)val;
}


/* Read word from NFC buffers */
static u16 fus_nfc_read_word(struct mtd_info *mtd)
{
	struct nand_chip *chip = mtd->priv;

	return chip->read_byte(mtd) | (chip->read_byte(mtd) << 8);
}


/* Read data from NFC buffers */
static void fus_nfc_read_buf(struct mtd_info *mtd, u_char *buf, int len)
{
	struct nand_chip *chip = mtd->priv;
	struct fsl_nfc_fus_prv *prv = chip->priv;

	/* Never call this function for the main area of a page, as the main
	   area should not be swapped like nfc_copy_from_nfc() does. */
	nfc_copy_from_nfc(chip, buf, len, prv->column);
	prv->column += len;
}


/* Write data to NFC buffers */
static void fus_nfc_write_buf(struct mtd_info *mtd, const u_char *buf, int len)
{
	struct nand_chip *chip = mtd->priv;

	/* Never call this function for the main area of a page, as the main
	   area should not be swapped like nfc_copy_to_nfc() does. */
	nfc_copy_to_nfc(chip, buf, len, 0);
}


/* Wait until page is written or block is erased, return status */
static int fus_nfc_wait(struct mtd_info *mtd, struct nand_chip *chip)
{
	unsigned long timeout;

	if (chip->state == FL_ERASING)
		timeout = NFC_TIMEOUT_ERASE;
	else
		timeout = NFC_TIMEOUT_WRITE;

	if (nfc_wait_ready(mtd, timeout))
		return 0x01;

	return nfc_read(chip, NFC_FLASH_STATUS2) & STATUS_BYTE1_MASK;
}


/* Write command to NAND flash. As the Vybrid NFC combines all steps in one
   compund command, some of these commands are only dummies and the main task
   is done at the second command byte or in the read/write functions. */
static void fus_nfc_command(struct mtd_info *mtd, uint command, int column,
			    int page)
{
	struct nand_chip *chip = mtd->priv;
	struct fsl_nfc_fus_prv *prv = chip->priv;

	/* Restore column for fus_nfc_read_byte() to 0 on each new command */
	prv->column = 0;

	/* Some commands just store column and/or row for the address cycle.
	   As the Vybrid NFC does all in one go, the real action takes place
	   when the second command byte is sent:
	     NAND_CMD_READ0 -> NAND_CMD_READSTART
	     NAND_CMD_SEQIN -> NAND_CMD_RNDIN or NAND_CMD_PAGEPROG
	     NAND_CMD_RNDIN -> NAND_CMD_RNDIN or NAND_CMD_PAGEPROG
	     NAND_CMD_ERASE1 -> NAND_CMD_ERASE2
	     NAND_CMD_RNDOUT -> NAND_CMD_RNDOUTSTART */
	switch (command) {
	case NAND_CMD_RNDIN:
		/* NAND_CMD_RNDIN must move all the data since the last
		   NAND_CMD_SEQIN or NAND_CMD_RNDIN to the flash. */
		if (prv->last_command == NAND_CMD_SEQIN) {
			/* Write the data since NAND_CMD_SEQIN to the flash */
			nfc_send_cmd_1(chip, NAND_CMD_SEQIN,
				       SEQIN_RANDOM_IN_CMD_CODE);
		} else if (prv->last_command == NAND_CMD_RNDIN) {
			/* Write the data since NAND_CMD_RNDIN to the flash */
			nfc_send_cmd_1(chip, NAND_CMD_RNDIN,
				       RNDIN_RANDOM_IN_CMD_CODE);
		} else {
			printf("No NAND_CMD_SEQIN or NAND_CMD_RNDIN "
			       "before NAND_CMD_RNDIN, ignored\n");
			return;
		}

		/* Wait for command completion */
		nfc_wait_ready(mtd, NFC_TIMEOUT_DATA);
		/* Fall through to case NAND_CMD_RNDOUT */

	case NAND_CMD_RNDOUT:
		nfc_set_column(chip, column, command);
		goto done;

	case NAND_CMD_READ0:
	case NAND_CMD_SEQIN:
		nfc_set_column(chip, column, command);
		/* Fall through to case NAND_CMD_ERASE1 */

	case NAND_CMD_ERASE1:
		nfc_set_row(chip, page, command);
		goto done;

	case NAND_CMD_READSTART:
		if (prv->last_command != NAND_CMD_READ0)
			printf("No NAND_CMD_READ0 before NAND_CMD_READSTART\n");

		/* Start reading transfer */
		nfc_send_cmd_2(chip, NAND_CMD_READ0, NAND_CMD_READSTART,
			       READ_PAGE_CMD_CODE);
		break;

	case NAND_CMD_RNDOUTSTART:
		if (prv->last_command != NAND_CMD_RNDOUT)
			printf("No NAND_CMD_RNDOUT before NAND_CMD_RNDOUTSTART\n");
		/* Actually do the reading after NAND_CMD_RNDOUTSTART */
		nfc_send_cmd_2(chip, NAND_CMD_RNDOUT, NAND_CMD_RNDOUTSTART,
			       RANDOM_OUT_CMD_CODE);
		break;

	case NAND_CMD_PAGEPROG:
		/* Actually do the programming. This is in fact a 3-byte
		   command with NAND_CMD_STATUS as last command. */ 
		if (prv->last_command == NAND_CMD_SEQIN) {
			/* Write the data since NAND_CMD_SEQIN to the
			   flash and start programming. */
			nfc_send_cmd_2(chip, NAND_CMD_SEQIN, NAND_CMD_PAGEPROG,
				       SEQIN_PROGRAM_PAGE_CMD_CODE);
		} else if (prv->last_command == NAND_CMD_RNDIN) {
			/* Write the data since the last NAND_CMD_RNDIN to the
			   flash and start programming. */
			nfc_send_cmd_2(chip, NAND_CMD_RNDIN, NAND_CMD_PAGEPROG,
				       RNDIN_PROGRAM_PAGE_CMD_CODE);
		} else {
			printf("No NAND_CMD_SEQIN or NAND_CMD_RNDIN "
			       "before NAND_CMD_PAGEPROG, ignored\n");
			return;
		}
		/* chip->waitfunc() is called later, so we need not wait now */
		goto done;

	case NAND_CMD_ERASE2:
		/* Actually do the erasing after a NAND_CMD_ERASE1 */
		if (prv->last_command != NAND_CMD_ERASE1) {
			printf("No NAND_CMD_ERASE1 before "
			       "NAND_CMD_ERASE2, ignored\n");
			return;
		}

		/* Start erasing. This is in fact a 3-byte command with
		   NAND_CMD_STATUS as last command. */
		nfc_send_cmd_2(chip, NAND_CMD_ERASE1, NAND_CMD_ERASE2,
			       ERASESTAT_CMD_CODE);
		/* chip->waitfunc() is called later, so we need not wait now */
		goto done;

	case NAND_CMD_READOOB:
		/* Emulate with NAND_CMD_READ0 */
		if (column < 0) {
			printf("Missing column for NAND_CMD_READOOB\n");
			return;
		}
		chip->cmdfunc(mtd, NAND_CMD_READ0, column + mtd->writesize,
			      page);

		/* Read some OOB bytes */
		nfc_write(chip, NFC_SECTOR_SIZE, mtd->oobsize - column);

		/* Set ECC mode to BYPASS, one virtual page */
		nfc_write(chip, NFC_FLASH_CONFIG,
			  prv->cfgbase
			  | (0 << CONFIG_STOP_ON_WERR_SHIFT)
			  | (0 << CONFIG_ECC_SRAM_ADDR_SHIFT)
			  | (0 << CONFIG_ECC_SRAM_REQ_SHIFT)
			  | (0 << CONFIG_DMA_REQ_SHIFT)
			  | (ECC_BYPASS << CONFIG_ECC_MODE_SHIFT)
			  | (0 << CONFIG_FAST_FLASH_SHIFT)
			  | (0 << CONFIG_ID_COUNT_SHIFT)
			  | (0 << CONFIG_BOOT_MODE_SHIFT)
			  | (0 << CONFIG_ADDR_AUTO_INCR_SHIFT)
			  | (0 << CONFIG_BUFNO_AUTO_INCR_SHIFT)
			  | (1 << CONFIG_PAGE_CNT_SHIFT));

		/* Now actually trigger reading by sending NAND_CMD_READSTART;
		   wait for completion */
		chip->cmdfunc(mtd, NAND_CMD_READSTART, -1, -1);

		/* Copy OOB data from NFC RAM; as we have only read these few
		   bytes and no main data, they are at offset 0 in NFC RAM */
		nfc_copy_from_nfc(chip, chip->oob_poi + column,
				  mtd->oobsize - column, 0);
		return;

	case NAND_CMD_READID:
		/* Set ECC mode to BYPASS, 5 ID bytes, one virtual page */
		nfc_set_column(chip, column, command);
		nfc_write(chip, NFC_FLASH_CONFIG,
			  prv->cfgbase
			  | (0 << CONFIG_STOP_ON_WERR_SHIFT)
			  | (0 << CONFIG_ECC_SRAM_ADDR_SHIFT)
			  | (0 << CONFIG_ECC_SRAM_REQ_SHIFT)
			  | (0 << CONFIG_DMA_REQ_SHIFT)
			  | (ECC_BYPASS << CONFIG_ECC_MODE_SHIFT)
			  | (0 << CONFIG_FAST_FLASH_SHIFT)
			  | (5 << CONFIG_ID_COUNT_SHIFT)
			  | (0 << CONFIG_BOOT_MODE_SHIFT)
			  | (0 << CONFIG_ADDR_AUTO_INCR_SHIFT)
			  | (0 << CONFIG_BUFNO_AUTO_INCR_SHIFT)
			  | (1 << CONFIG_PAGE_CNT_SHIFT));

		/* Start ID request */
		nfc_send_cmd_1(chip, NAND_CMD_READID, READ_ID_CMD_CODE);
		break;

	case NAND_CMD_STATUS:
		/* Set ECC mode to BYPASS, one virtual page */
		nfc_write(chip, NFC_FLASH_CONFIG,
			  prv->cfgbase
			  | (0 << CONFIG_STOP_ON_WERR_SHIFT)
			  | (0 << CONFIG_ECC_SRAM_ADDR_SHIFT)
			  | (0 << CONFIG_ECC_SRAM_REQ_SHIFT)
			  | (0 << CONFIG_DMA_REQ_SHIFT)
			  | (ECC_BYPASS << CONFIG_ECC_MODE_SHIFT)
			  | (0 << CONFIG_FAST_FLASH_SHIFT)
			  | (0 << CONFIG_ID_COUNT_SHIFT)
			  | (0 << CONFIG_BOOT_MODE_SHIFT)
			  | (0 << CONFIG_ADDR_AUTO_INCR_SHIFT)
			  | (0 << CONFIG_BUFNO_AUTO_INCR_SHIFT)
			  | (1 << CONFIG_PAGE_CNT_SHIFT));

		/* Start status request */
		nfc_send_cmd_1(chip, NAND_CMD_STATUS, STATUS_READ_CMD_CODE);
		break;

	case NAND_CMD_RESET:
		/* Set ECC mode to BYPASS, one virtual page */
		nfc_write(chip, NFC_FLASH_CONFIG,
			  prv->cfgbase
			  | (0 << CONFIG_STOP_ON_WERR_SHIFT)
			  | (0 << CONFIG_ECC_SRAM_ADDR_SHIFT)
			  | (0 << CONFIG_ECC_SRAM_REQ_SHIFT)
			  | (0 << CONFIG_DMA_REQ_SHIFT)
			  | (ECC_BYPASS << CONFIG_ECC_MODE_SHIFT)
			  | (0 << CONFIG_FAST_FLASH_SHIFT)
			  | (0 << CONFIG_ID_COUNT_SHIFT)
			  | (0 << CONFIG_BOOT_MODE_SHIFT)
			  | (0 << CONFIG_ADDR_AUTO_INCR_SHIFT)
			  | (0 << CONFIG_BUFNO_AUTO_INCR_SHIFT)
			  | (1 << CONFIG_PAGE_CNT_SHIFT));

		/* Send reset */
		nfc_send_cmd_1(chip, NAND_CMD_RESET, RESET_CMD_CODE);
		break;

	case NAND_CMD_PARAM:
		/* Read ONFI parameter pages (3 copies) */
		nfc_set_column(chip, column, command);
		nfc_write(chip, NFC_SECTOR_SIZE, 3 * 256);

		/* Set ECC mode to BYPASS, one virtual page */
		nfc_write(chip, NFC_FLASH_CONFIG,
			  prv->cfgbase
			  | (0 << CONFIG_STOP_ON_WERR_SHIFT)
			  | (0 << CONFIG_ECC_SRAM_ADDR_SHIFT)
			  | (0 << CONFIG_ECC_SRAM_REQ_SHIFT)
			  | (0 << CONFIG_DMA_REQ_SHIFT)
			  | (ECC_BYPASS << CONFIG_ECC_MODE_SHIFT)
			  | (0 << CONFIG_FAST_FLASH_SHIFT)
			  | (0 << CONFIG_ID_COUNT_SHIFT)
			  | (0 << CONFIG_BOOT_MODE_SHIFT)
			  | (0 << CONFIG_ADDR_AUTO_INCR_SHIFT)
			  | (0 << CONFIG_BUFNO_AUTO_INCR_SHIFT)
			  | (1 << CONFIG_PAGE_CNT_SHIFT));

		/* Start ID request */
		nfc_send_cmd_1(chip, NAND_CMD_PARAM, READ_PARAM_CMD_CODE);
		break;

	default:
		printf("NAND_CMD 0x%02x not supported\n", command);
		return;
	}

	/* Wait for command completion */
	nfc_wait_ready(mtd, NFC_TIMEOUT_DATA);

done:
	prv->last_command = command;
}

static int fus_nfc_read_oob_raw(struct mtd_info *mtd, struct nand_chip *chip,
				int page)
{
	/* Send command to read oob data from flash */
	chip->cmdfunc(mtd, NAND_CMD_READOOB, 0, page);

	return 0;
}


/* Read OOB data from given offset */
static int fus_nfc_read_oob(struct mtd_info *mtd, struct nand_chip *chip,
			    int page)
{
	/* Send command to read oob data from flash */
	chip->cmdfunc(mtd, NAND_CMD_READOOB,
		      mtd->oobsize - mtd->oobavail, page);

	return 0;
}


/* Write OOB data at given offset */
static int fus_nfc_write_oob_raw(struct mtd_info *mtd, struct nand_chip *chip,
				 int page)
{
	return nfc_do_write_oob(mtd, chip, page, 0);
}


/* Write OOB data at given offset */
static int fus_nfc_write_oob(struct mtd_info *mtd, struct nand_chip *chip,
			     int page)
{
	return nfc_do_write_oob(mtd, chip, page, mtd->oobsize - mtd->oobavail);
}


/* Read the whole main and the whole OOB area in one go (without ECC) */
static int fus_nfc_read_page_raw(struct mtd_info *mtd, struct nand_chip *chip,
				 uint8_t *buf, int page)
{
	struct fsl_nfc_fus_prv *prv = chip->priv;

	/* This code assumes that the NAND_CMD_READ0 command was
	   already issued before */

	/* Set number of bytes to transfer */
	nfc_write(chip, NFC_SECTOR_SIZE, mtd->writesize + mtd->oobsize);

	/* Set ECC mode to BYPASS, position of ECC status, one virtual page */
	nfc_write(chip, NFC_FLASH_CONFIG,
		  prv->cfgbase
		  | (0 << CONFIG_STOP_ON_WERR_SHIFT)
		  | (0 << CONFIG_ECC_SRAM_ADDR_SHIFT)
		  | (0 << CONFIG_ECC_SRAM_REQ_SHIFT)
		  | (0 << CONFIG_DMA_REQ_SHIFT)
		  | (ECC_BYPASS << CONFIG_ECC_MODE_SHIFT)
		  | (0 << CONFIG_FAST_FLASH_SHIFT)
		  | (0 << CONFIG_ID_COUNT_SHIFT)
		  | (0 << CONFIG_BOOT_MODE_SHIFT)
		  | (0 << CONFIG_ADDR_AUTO_INCR_SHIFT)
		  | (0 << CONFIG_BUFNO_AUTO_INCR_SHIFT)
		  | (1 << CONFIG_PAGE_CNT_SHIFT));

	/* Actually trigger read transfer and wait for completion */
	chip->cmdfunc(mtd, NAND_CMD_READSTART, -1, -1);

	/* Copy main and OOB data from NFC RAM. Please note that we don't swap
	   the main data from Big Endian byte order. DMA does not swap data
	   either, so if we want to use DMA in the future, we have to keep it
	   this way. See also fus_nfc_write_page_raw(). However we do swap
	   the OOB data. */
	memcpy(buf, chip->IO_ADDR_R + NFC_MAIN_AREA(0), mtd->writesize);
	nfc_copy_from_nfc(chip, chip->oob_poi, mtd->oobsize, mtd->writesize);

	return 0;
}


/* Write the whole main and the whole OOB area without ECC in one go */
static void fus_nfc_write_page_raw(struct mtd_info *mtd, struct nand_chip *chip,
				   const uint8_t *buf)
{
	struct fsl_nfc_fus_prv *prv = chip->priv;
	uint size;

	/* NAND_CMD_SEQIN for column 0 was already issued by the caller */

	/* Copy main and OOB data to NFC RAM. Please note that we don't swap
	   the main data to Big Endian byte order. DMA does not swap the data
	   either, so if we want to use DMA in the future, we have to keep it
	   this way. */
	memcpy(chip->IO_ADDR_R + NFC_MAIN_AREA(0), buf, mtd->writesize);
	nfc_copy_to_nfc(chip, chip->oob_poi, mtd->oobsize, mtd->writesize);

	/* Set number of bytes to transfer */
	size = mtd->writesize + mtd->oobsize;
	nfc_write(chip, NFC_SECTOR_SIZE, size);

	/* Set ECC mode to BYPASS, one virtual page */
	nfc_write(chip, NFC_FLASH_CONFIG,
		  prv->cfgbase
		  | (0 << CONFIG_STOP_ON_WERR_SHIFT)
		  | (0 << CONFIG_ECC_SRAM_ADDR_SHIFT)
		  | (0 << CONFIG_ECC_SRAM_REQ_SHIFT)
		  | (0 << CONFIG_DMA_REQ_SHIFT)
		  | (ECC_BYPASS << CONFIG_ECC_MODE_SHIFT)
		  | (0 << CONFIG_FAST_FLASH_SHIFT)
		  | (0 << CONFIG_ID_COUNT_SHIFT)
		  | (0 << CONFIG_BOOT_MODE_SHIFT)
		  | (0 << CONFIG_ADDR_AUTO_INCR_SHIFT)
		  | (0 << CONFIG_BUFNO_AUTO_INCR_SHIFT)
		  | (1 << CONFIG_PAGE_CNT_SHIFT));

	/* The actual programming will take place in fus_nfc_command() when
	   command NAND_CMD_PAGEPROG is sent */
}


/* Read main data of page with ECC enabled; if there is an ECC error, don't
   change the related data areas of the buffers. */
static int fus_nfc_read_page(struct mtd_info *mtd, struct nand_chip *chip,
			     uint8_t *buf, int page)
{
	u8 ecc_status;
	uint i;
	uint size;
	uint zerobits;
	uint limit;
	struct fsl_nfc_fus_prv *prv = chip->priv;

	/* This code assumes that the NAND_CMD_READ0 command was
	   already issued before */

	/* Set ECC part of OOB area to empty, we will not store data there */
	size = mtd->oobsize - mtd->oobavail;
	memset(chip->oob_poi, 0xFF, size);

	/* Set size to load main area, BBM and ECC */
	nfc_write(chip, NFC_SECTOR_SIZE, mtd->writesize + size);

	/* Set ECC mode, position of ECC status, one virtual page */
	nfc_write(chip, NFC_FLASH_CONFIG,
		  prv->cfgbase
		  | (0 << CONFIG_STOP_ON_WERR_SHIFT)
		  | ((ECC_STATUS_OFFS >> 3) << CONFIG_ECC_SRAM_ADDR_SHIFT)
		  | (1 << CONFIG_ECC_SRAM_REQ_SHIFT)
		  | (0 << CONFIG_DMA_REQ_SHIFT)
		  | (prv->eccmode << CONFIG_ECC_MODE_SHIFT)
		  | (0 << CONFIG_FAST_FLASH_SHIFT)
		  | (0 << CONFIG_ID_COUNT_SHIFT)
		  | (0 << CONFIG_BOOT_MODE_SHIFT)
		  | (0 << CONFIG_ADDR_AUTO_INCR_SHIFT)
		  | (0 << CONFIG_BUFNO_AUTO_INCR_SHIFT)
		  | (1 << CONFIG_PAGE_CNT_SHIFT));

	/* Actually trigger read transfer and wait for completion */
	chip->cmdfunc(mtd, NAND_CMD_READSTART, -1, -1);

	/* Get the ECC status */
	ecc_status = nfc_read_ram(chip, ECC_STATUS_OFFS + 7);
	if (!(ecc_status & ECC_STATUS_MASK)) {
		/* Correctable error or no error at all: update ecc_stats */
		mtd->ecc_stats.corrected += ecc_status & ECC_ERR_COUNT;

		/* Copy main data from NFC RAM. Please note that we don't swap
		   the data from Big Endian byte order. DMA does not swap data
		   either, so if we want to use DMA in the future, we have to
		   keep it this way. See also fus_nfc_write_page(). */
		memcpy(buf, chip->IO_ADDR_R + NFC_MAIN_AREA(0), mtd->writesize);

#ifdef CONFIG_NAND_REFRESH
		/* If requested, read refresh block number from the four bytes
		   between main data and ECC and return it converted to an
		   offset. The caller has to make sure that this flag is not
		   set in the first two pages of a block, because there the
		   Bad Block Marker is stored there. */
		if (mtd->extraflags & MTD_EXTRA_REFRESHOFFS) {
			u32 refresh;

			memcpy(&refresh,
		        chip->IO_ADDR_R + NFC_MAIN_AREA(0) + mtd->writesize, 4);
			if (refresh == 0xFFFFFFFF)
				refresh = 0;
			mtd->extradata = refresh << chip->phys_erase_shift;
		}
#endif

		/* Reload the user part of the OOB area to NFC RAM. We can use
		   NAND_CMD_RNDOUT as the NAND flash still has the data in its
		   page cache. */
		chip->cmdfunc(mtd, NAND_CMD_RNDOUT, mtd->writesize + size, -1);

		/* Set size to load user OOB area */
		nfc_write(chip, NFC_SECTOR_SIZE, mtd->oobavail);

		/* Set ECC mode to BYPASS, one virtual page */
		nfc_write(chip, NFC_FLASH_CONFIG,
			  prv->cfgbase
			  | (0 << CONFIG_STOP_ON_WERR_SHIFT)
			  | (0 << CONFIG_ECC_SRAM_ADDR_SHIFT)
			  | (0 << CONFIG_ECC_SRAM_REQ_SHIFT)
			  | (0 << CONFIG_DMA_REQ_SHIFT)
			  | (ECC_BYPASS << CONFIG_ECC_MODE_SHIFT)
			  | (0 << CONFIG_FAST_FLASH_SHIFT)
			  | (0 << CONFIG_ID_COUNT_SHIFT)
			  | (0 << CONFIG_BOOT_MODE_SHIFT)
			  | (0 << CONFIG_ADDR_AUTO_INCR_SHIFT)
			  | (0 << CONFIG_BUFNO_AUTO_INCR_SHIFT)
			  | (1 << CONFIG_PAGE_CNT_SHIFT));

		/* Actually trigger reading and wait until done */
		chip->cmdfunc(mtd, NAND_CMD_RNDOUTSTART, -1, -1);

		/* Copy data to OOB buffer; here we do swap the bytes; the
		   data is at the beginning of the NFC RAM */
		nfc_copy_from_nfc(chip, chip->oob_poi + size, mtd->oobsize, 0);

		return 0;
	}

	/* The page is uncorrectable; however this can also happen with a
	   totally empty (=erased) page, in which case we should return OK.
	   But this means that we must reload the page in raw mode. However we
	   can use NAND_CMD_RNDOUT as the NAND flash still has the data in its
	   page cache. We start by reloading the OOB area of the page to NFC
	   RAM as we need this data anyway. */
	chip->cmdfunc(mtd, NAND_CMD_RNDOUT, mtd->writesize, -1);

	/* Set size to load OOB area */
	nfc_write(chip, NFC_SECTOR_SIZE, mtd->oobsize);

	/* Set ECC mode to BYPASS, one virtual page */
	nfc_write(chip, NFC_FLASH_CONFIG,
		  prv->cfgbase
		  | (0 << CONFIG_STOP_ON_WERR_SHIFT)
		  | (0 << CONFIG_ECC_SRAM_ADDR_SHIFT)
		  | (0 << CONFIG_ECC_SRAM_REQ_SHIFT)
		  | (0 << CONFIG_DMA_REQ_SHIFT)
		  | (ECC_BYPASS << CONFIG_ECC_MODE_SHIFT)
		  | (0 << CONFIG_FAST_FLASH_SHIFT)
		  | (0 << CONFIG_ID_COUNT_SHIFT)
		  | (0 << CONFIG_BOOT_MODE_SHIFT)
		  | (0 << CONFIG_ADDR_AUTO_INCR_SHIFT)
		  | (0 << CONFIG_BUFNO_AUTO_INCR_SHIFT)
		  | (1 << CONFIG_PAGE_CNT_SHIFT));

	/* Actually trigger reading and wait until done */
	chip->cmdfunc(mtd, NAND_CMD_RNDOUTSTART, -1, -1);

	/* Copy the user part of the OOB to the internal OOB buffer in any
	   case. This may or may not be empty. Here we do swap the bytes. */
	nfc_copy_from_nfc(chip, chip->oob_poi + size, mtd->oobavail, size);

	/* By checking the ECC area we can quickly detect a non-empty page.
	   Because also an empty page may show bitflips (e.g. by write
	   disturbs caused by writes to a nearby page), we accept up to
	   bitflip_threshold non-empty bits. */
	zerobits = 0;
	limit = mtd->bitflip_threshold;
	i = size;
	do {
		zerobits += count_zeroes(nfc_read_ram(chip, i-1) | 0xFFFFFF00);
		if (zerobits > limit)
			break;
	} while (--i);

	if (!i) {
		/* OK, ECC area is empty. Now reload main area to NFC RAM.
		   Theoretically we could split this into smaller parts to
		   speed up the non-empty case. However if the ECC part is
		   empty, it is highly probable that the whole main part is
		   empty, too. So it's not worth the trouble. */

		/* Issue NAND_CMD_RNDOUT to reload main area */
		chip->cmdfunc(mtd, NAND_CMD_RNDOUT, 0, -1);

		/* Set size to load main area */
		nfc_write(chip, NFC_SECTOR_SIZE, mtd->writesize);

		/* Set ECC mode to BYPASS, one virtual page */
		nfc_write(chip, NFC_FLASH_CONFIG,
			  prv->cfgbase
			  | (0 << CONFIG_STOP_ON_WERR_SHIFT)
			  | (0 << CONFIG_ECC_SRAM_ADDR_SHIFT)
			  | (0 << CONFIG_ECC_SRAM_REQ_SHIFT)
			  | (0 << CONFIG_DMA_REQ_SHIFT)
			  | (ECC_BYPASS << CONFIG_ECC_MODE_SHIFT)
			  | (0 << CONFIG_FAST_FLASH_SHIFT)
			  | (0 << CONFIG_ID_COUNT_SHIFT)
			  | (0 << CONFIG_BOOT_MODE_SHIFT)
			  | (0 << CONFIG_ADDR_AUTO_INCR_SHIFT)
			  | (0 << CONFIG_BUFNO_AUTO_INCR_SHIFT)
			  | (1 << CONFIG_PAGE_CNT_SHIFT));

		/* Actually trigger reading and wait until done */
		chip->cmdfunc(mtd, NAND_CMD_RNDOUTSTART, -1, -1);

		/* Do 4-byte compares to be faster; byte order does not matter
		   when comparing to 0xFFFFFFFF or when counting zero bits. */
		i = mtd->writesize;
		do {
			u32 data = nfc_read(chip, NFC_MAIN_AREA(0) + i - 4);

			zerobits += count_zeroes(data);
			if (zerobits > limit)
				break;
			i -= 4;
		} while (i);
		if (!i) {
			/* Main area is empty, too, fill with 0xFF */
			memset(buf, 0xFF, mtd->writesize);

			/* FIXME: If we had bitflips, we finally want to
			   return -EUCLEAN from the read call, to make the
			   filesystem aware of this fact. But we must not
			   return -EUCLEAN here, because then reading will be
			   stopped after this page with an error. Instead we
			   must trigger the calling function to return
			   -EUCLEAN after the whole read is done. We do this
			   by (incorrectly) increasing the stats.corrected
			   count by at least bitflip_threshold. In reality we
			   only have corrected zerobits bits. */
			if (zerobits)
				mtd->ecc_stats.corrected += limit;

			return 0;
		}
	}

	/* This page is not empty, it is an actual read error. Update
	   ecc_stats. Please note that buf and the first part of chip->oob_poi
	   remain completely unchanged in this case. */
	mtd->ecc_stats.failed++;
	printf("Non-correctable error in page at 0x%08llx\n",
	       (loff_t)page << chip->page_shift);

	return 0;
}


/* Write main data of page with ECC enabled */
static void fus_nfc_write_page(struct mtd_info *mtd, struct nand_chip *chip,
			       const uint8_t *buf)
{
	struct fsl_nfc_fus_prv *prv = chip->priv;
	uint size = mtd->oobsize - mtd->oobavail;
	uint i;
	uint8_t *oob;

	/* NAND_CMD_SEQIN for column 0 was already issued by the caller */

	/* Copy main data to NFC RAM. Please note that we don't swap the data
	   to Big Endian byte order. DMA does not swap data either, so if we
	   want to use DMA in the future, we have to keep it this way. Also
	   don't forget to write 0xFFFFFFFF to the BBM area. */
	memcpy(chip->IO_ADDR_R + NFC_MAIN_AREA(0), buf, mtd->writesize);
	nfc_write(chip, NFC_MAIN_AREA(0) + mtd->writesize, 0xFFFFFFFF);

#ifdef CONFIG_NAND_REFRESH
	/* If requested, store refresh offset as block number in the four
	   bytes between main data and ECC. The caller has to make sure that
	   this flag is not set when writing to the first two pages of the
	   block or the Bad Block Marker may be set unintentionally. */
	if ((mtd->extraflags & MTD_EXTRA_REFRESHOFFS) && mtd->extradata) {
		u32 refresh;

		refresh = (u32)(mtd->extradata >> chip->phys_erase_shift);
		memcpy(chip->IO_ADDR_R + NFC_MAIN_AREA(0) + mtd->writesize,
		       &refresh, 4);
	}
#endif

	/* Set number of bytes to transfer */
	nfc_write(chip, NFC_SECTOR_SIZE, size + mtd->writesize);

	/* Set ECC mode, position of ECC status, one virtual page */
	nfc_write(chip, NFC_FLASH_CONFIG,
		  prv->cfgbase
		  | (0 << CONFIG_STOP_ON_WERR_SHIFT)
		  | ((ECC_STATUS_OFFS >> 3) << CONFIG_ECC_SRAM_ADDR_SHIFT)
		  | (1 << CONFIG_ECC_SRAM_REQ_SHIFT)
		  | (0 << CONFIG_DMA_REQ_SHIFT)
		  | (prv->eccmode << CONFIG_ECC_MODE_SHIFT)
		  | (0 << CONFIG_FAST_FLASH_SHIFT)
		  | (0 << CONFIG_ID_COUNT_SHIFT)
		  | (0 << CONFIG_BOOT_MODE_SHIFT)
		  | (0 << CONFIG_ADDR_AUTO_INCR_SHIFT)
		  | (0 << CONFIG_BUFNO_AUTO_INCR_SHIFT)
		  | (1 << CONFIG_PAGE_CNT_SHIFT));

	/* Check if user OOB area is empty */
	oob = chip->oob_poi + size;
	for (i = mtd->oobavail; i > 0; i--) {
		if (oob[i - 1] != 0xFF)
			break;
	}
	if (i) {
		/* No: transfer main data, BBM and ECC to NAND chip, then move
		   to user OOB column */
		chip->cmdfunc(mtd, NAND_CMD_RNDIN, size + mtd->writesize, -1);

		/* Copy the user part of the OOB to NFC RAM */
		nfc_copy_to_nfc(chip, oob, mtd->oobavail, 0);

		/* Set number of bytes to transfer */
		nfc_write(chip, NFC_SECTOR_SIZE, mtd->oobavail);

		/* Set ECC mode to BYPASS, one virtual page */
		nfc_write(chip, NFC_FLASH_CONFIG,
			  prv->cfgbase
			  | (0 << CONFIG_STOP_ON_WERR_SHIFT)
			  | (0 << CONFIG_ECC_SRAM_ADDR_SHIFT)
			  | (0 << CONFIG_ECC_SRAM_REQ_SHIFT)
			  | (0 << CONFIG_DMA_REQ_SHIFT)
			  | (ECC_BYPASS << CONFIG_ECC_MODE_SHIFT)
			  | (0 << CONFIG_FAST_FLASH_SHIFT)
			  | (0 << CONFIG_ID_COUNT_SHIFT)
			  | (0 << CONFIG_BOOT_MODE_SHIFT)
			  | (0 << CONFIG_ADDR_AUTO_INCR_SHIFT)
			  | (0 << CONFIG_BUFNO_AUTO_INCR_SHIFT)
			  | (1 << CONFIG_PAGE_CNT_SHIFT));
	}

	/* The actual programming will take place in fus_nfc_command() when
	   command NAND_CMD_PAGEPROG is sent */
}


void vybrid_nand_register(int nfc_hw_id,
			  const struct fsl_nfc_fus_platform_data *pdata)
{
	static int index = 0;
	struct fsl_nfc_fus_prv *prv;
	struct nand_chip *chip;
	struct mtd_info *mtd;

	if (index >= CONFIG_SYS_MAX_NAND_DEVICE)
		return;

	if (nfc_hw_id > ARRAY_SIZE(nfc_base_addresses))
		return;

	mtd = &nand_info[index];
	prv = &nfc_infos[index];
	chip = &prv->chip;
	chip->priv = prv;

	/* Init the mtd device, most of it is done in nand_scan_ident() */
	mtd->priv = chip;
	mtd->size = 0;
	mtd->name = NULL;

	/* Setup all things required to detect the chip */
	chip->IO_ADDR_R = (void __iomem *)nfc_base_addresses[nfc_hw_id];
	chip->IO_ADDR_W = chip->IO_ADDR_R;
	chip->select_chip = fus_nfc_select_chip;
	chip->dev_ready = fus_nfc_dev_ready;
	chip->cmdfunc = fus_nfc_command;
	chip->read_byte = fus_nfc_read_byte;
	chip->read_word = fus_nfc_read_word;
	chip->read_buf = fus_nfc_read_buf;
	chip->write_buf = fus_nfc_write_buf;
	chip->waitfunc = fus_nfc_wait;
	chip->options = pdata ? pdata->options : 0;
	chip->options |= NAND_BBT_SCAN2NDPAGE;
	chip->badblockpos = 0;

	/* Set up our remaining private data */
	prv->cfgbase =
		nfc_read(chip, NFC_FLASH_CONFIG) & CONFIG_CMD_TIMEOUT_MASK;
	if (pdata) {
		prv->eccmode = pdata->eccmode;
		if (pdata->t_wb && (pdata->t_wb <= 0x1f))
			prv->cfgbase = pdata->t_wb << CONFIG_CMD_TIMEOUT_SHIFT;
		if (chip->options & NAND_BUSWIDTH_16)
			prv->cfgbase |= 1 << CONFIG_16BIT_SHIFT;
	} else
		prv->eccmode = ECC_30_BYTE;

	/* Identify the device, set page and block sizes, etc. */
	if (nand_scan_ident(mtd, CONFIG_SYS_NAND_MAX_CHIPS, NULL)) {
		mtd->name = NULL;
		return;
	}

	/* Set skipped region and backup region */
	if (pdata) {
#ifdef CONFIG_NAND_REFRESH
		mtd->backupoffs = pdata->backupstart;
		mtd->backupend = pdata->backupend;
#endif
		mtd->skip = pdata->skipblocks * mtd->erasesize;
		if (pdata->flags & VYBRID_NFC_SKIP_INVERSE) {
			mtd->size = mtd->skip;
			mtd->skip = 0;
		}
	}

	/* Set up ECC configuration */
	chip->ecc.layout = &fus_nfc_ecclayout[prv->eccmode];
	chip->ecc.mode = NAND_ECC_HW;
	chip->ecc.steps = 1;
	chip->ecc.bytes = chip->ecc.layout->eccbytes;
	chip->ecc.size = mtd->writesize;

	chip->ecc.read_page = fus_nfc_read_page;
	chip->ecc.write_page = fus_nfc_write_page;
	chip->ecc.read_oob = fus_nfc_read_oob;
	chip->ecc.write_oob = fus_nfc_write_oob;
	chip->ecc.read_page_raw = fus_nfc_read_page_raw;
	chip->ecc.write_page_raw = fus_nfc_write_page_raw;
	chip->ecc.read_oob_raw = fus_nfc_read_oob_raw;
	chip->ecc.write_oob_raw = fus_nfc_write_oob_raw;

	mtd->bitflip_threshold = bitflip_threshold[prv->eccmode];

#ifdef CONFIG_SYS_NAND_ONFI_DETECTION
	if (chip->onfi_version) {
		u8 addr_cycles;

		/* Get address cycles from ONFI data:
		   [7:4] column cycles, [3:0] row cycles */
		addr_cycles = chip->onfi_params.addr_cycles;
		prv->cmdclr = 0;
		if ((addr_cycles >> 4) < 2)
			prv->cmdclr |= (1 << 12); /* No column byte 2 */
		addr_cycles &= 0x0F;
		if (addr_cycles < 3)
			prv->cmdclr |= (1 << 9);  /* No row byte 3 */
		if (addr_cycles < 2)
			prv->cmdclr |= (1 << 10); /* No row byte 2 */
	} else
#endif
		/* Use only two row cycles if NAND is smaller than 32 MB */
		prv->cmdclr = (chip->chipsize > (32 << 20)) ? 0 : (1 << 9);

	if (nand_scan_tail(mtd)) {
		mtd->name = NULL;
		return;
	}

	nand_register(index++);
}
