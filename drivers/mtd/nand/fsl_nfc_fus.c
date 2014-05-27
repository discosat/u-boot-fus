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

#include <common.h>
#include <malloc.h>

#include <linux/mtd/mtd.h>
#include <linux/mtd/nand.h>
#include <linux/mtd/nand_ecc.h>

#include <mtd/fsl_nfc.h>
#include <mtd/fsl_nfc_fus.h>		/* struct fsl_nfc_fus_prv */

#include <nand.h>
#include <errno.h>
#include <asm/io.h>
#include <asm/processor.h>

#ifndef CONFIG_SYS_NAND_FSL_NFC_ECC
#define CONFIG_SYS_NAND_FSL_NFC_ECC ECC_30_BYTE
#endif

#define	DRV_NAME		"fsl_nfc_fus"
#define	DRV_VERSION		"V1.0"

#define ECC_STATUS_OFFS		0x840
#define ECC_STATUS_MASK		0x80
#define ECC_ERR_COUNT		0x3F


/* NAND layouts for all possible Vybrid ECC modes */
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
			      uint size, uint offset)
{
	while (size--)
		*buffer++ = nfc_read_ram(chip, offset++);
}


/* Copy data to NFC RAM */
static void nfc_copy_to_nfc(const struct nand_chip *chip, const u8 *buffer,
			    uint size, uint offset)
{
	while (size--)
		nfc_write_ram(chip, offset++, *buffer++);
}


/* Start a comand with one command byte and wait for completion */
static inline void nfc_send_cmd_1(const struct nand_chip *chip,
				  u32 cmd_byte1, u32 cmd_code)
{
	/* Clear IDLE and DONE status bits */
	nfc_write(chip, NFC_IRQ_STATUS,
		  (1 << CMD_DONE_CLEAR_SHIFT) | (1 << IDLE_CLEAR_SHIFT));

	/* Write first command byte, command code, buffer 0, start transfer */
	nfc_write(chip, NFC_FLASH_CMD2,
		  (cmd_byte1 << CMD_BYTE1_SHIFT)
		  | (cmd_code << CMD_CODE_SHIFT)
		  | (0 << BUFNO_SHIFT)
		  | (1 << START_SHIFT));
}


/* Start a command with two command bytes, e.g. program page */
static inline void nfc_send_cmd_2(const struct nand_chip *chip,
				  u32 cmd_byte1, u32 cmd_byte2, u32 cmd_code)
{
	/* Write second command byte */
	nfc_write(chip, NFC_FLASH_CMD1, cmd_byte2 << CMD_BYTE2_SHIFT);

	/* The rest is like the one byte command */
	nfc_send_cmd_1(chip, cmd_byte1, cmd_code);
}


/* Reload page without ECC and check if it is really empty. Use NAND_CMD_RNDOUT
   instead of NAND_CMD_READ0 as this does not load the data again from the
   NAND cells. */
static int nfc_page_is_empty(struct mtd_info *mtd, const struct nand_chip *chip)
{
	struct fsl_nfc_fus_prv *prv = chip->priv;
	uint i;
	uint size;

	/* First reload OOB area, because the ECC is the place that is most
	   probably not empty. This speeds up the non-empty case. */
	chip->cmdfunc(mtd, NAND_CMD_RNDOUT, mtd->writesize, -1);

	/* Set size to load BBM + ECC */
	size = 4 + fus_nfc_ecclayout[prv->eccmode].eccbytes;
	nfc_write(chip, NFC_SECTOR_SIZE, size);

	/* Set ECC mode to BYPASS, one virtual page */
	nfc_write(chip, NFC_FLASH_CONFIG,
		  prv->timeout
		  | (0 << CONFIG_STOP_ON_WERR_SHIFT)
		  | (0 << CONFIG_ECC_SRAM_ADDR_SHIFT)
		  | (0 << CONFIG_ECC_SRAM_REQ_SHIFT)
		  | (0 << CONFIG_DMA_REQ_SHIFT)
		  | (ECC_BYPASS << CONFIG_ECC_MODE_SHIFT)
		  | (0 << CONFIG_FAST_FLASH_SHIFT)
		  | (0 << CONFIG_ID_COUNT_SHIFT)
		  | (0 << CONFIG_16BIT_SHIFT)
		  | (0 << CONFIG_BOOT_MODE_SHIFT)
		  | (0 << CONFIG_ADDR_AUTO_INCR_SHIFT)
		  | (0 << CONFIG_BUFNO_AUTO_INCR_SHIFT)
		  | (1 << CONFIG_PAGE_CNT_SHIFT));

	/* Actually trigger reading and wait until done */
	chip->cmdfunc(mtd, NAND_CMD_RNDOUTSTART, -1, -1);

	/* Check if empty (0xFF) */
	for (i = 0; i < size; i++) {
		if (nfc_read_ram(chip, i) != 0xFF)
			return 0;
	}

	/* Now reload the main data. We could split this into smaller parts to
	   speed up the non-empty case. However if the ECC part is empty, it
	   is highly probable that the whole main part is empty, too. */
	chip->cmdfunc(mtd, NAND_CMD_RNDOUT, 0, -1);
	size = mtd->writesize;
	nfc_write(chip, NFC_SECTOR_SIZE, size);

	/* Set ECC mode to BYPASS, one virtual page */
	nfc_write(chip, NFC_FLASH_CONFIG,
		  prv->timeout
		  | (0 << CONFIG_STOP_ON_WERR_SHIFT)
		  | (0 << CONFIG_ECC_SRAM_ADDR_SHIFT)
		  | (0 << CONFIG_ECC_SRAM_REQ_SHIFT)
		  | (0 << CONFIG_DMA_REQ_SHIFT)
		  | (ECC_BYPASS << CONFIG_ECC_MODE_SHIFT)
		  | (0 << CONFIG_FAST_FLASH_SHIFT)
		  | (0 << CONFIG_ID_COUNT_SHIFT)
		  | (0 << CONFIG_16BIT_SHIFT)
		  | (0 << CONFIG_BOOT_MODE_SHIFT)
		  | (0 << CONFIG_ADDR_AUTO_INCR_SHIFT)
		  | (0 << CONFIG_BUFNO_AUTO_INCR_SHIFT)
		  | (1 << CONFIG_PAGE_CNT_SHIFT));

	/* Actually trigger reading and wait until done */
	chip->cmdfunc(mtd, NAND_CMD_RNDOUTSTART, -1, -1);

	/* Check if empty; do 4-bytes-reads to be faster, byte order does not
	   matter when comparing to 0xFFFFFFFF */
	for (i = 0; i < size; i += 4) {
		if (nfc_read(chip, NFC_MAIN_AREA(0) + i) != 0xFFFFFFFF)
			return 0;
	}

	return 1;
}


/* -------------------- INTERFACE FUNCTIONS -------------------------------- */

/* Control chip select signal on the board */
static void fus_nfc_select_chip(struct mtd_info *mtd, int chipnr)
{
	struct nand_chip *chip = mtd->priv;
	u32 rowreg;

	rowreg = nfc_read(chip, NFC_ROW_ADDR);
	rowreg &= ~ROW_ADDR_CHIP_SEL_MASK;
	if ((chipnr >= 0) && (chipnr < 2))
		rowreg |= (1 << chipnr) << ROW_ADDR_CHIP_SEL_SHIFT;
	rowreg |= 1 << ROW_ADDR_CHIP_SEL_RB_SHIFT;
	nfc_write(chip, NFC_ROW_ADDR, rowreg);
}


/* Read NAND Ready/Busy signal; as this is handled automatically by NFC, we
   just check if NFC is idle again */
static int fus_nfc_dev_ready(struct mtd_info *mtd)
{
	struct nand_chip *chip = mtd->priv;

	return (nfc_read(chip, NFC_IRQ_STATUS) & IDLE_IRQ_MASK) != 0;
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


/* Write command to NAND flash. As the Vybrid NFC combines all steps in one
   compund command, some of these commands are only dummies and the main task
   is done at the second command byte or in the read/write functions. */
static void fus_nfc_command(struct mtd_info *mtd, uint command, int column,
			    int page)
{
	struct nand_chip *chip = mtd->priv;
	struct fsl_nfc_fus_prv *prv = chip->priv;

	/* Write column and row (=page) */
	if (column >= 0) {
		int tmp = column;

		/* We emulate NAND_CMD_READOOB by the standard read sequence,
		   so we need to add the main size of the page to the column
		   in this case */
		if (command == NAND_CMD_READOOB)
			tmp += mtd->writesize;

		nfc_write(chip, NFC_COL_ADDR, tmp << COL_ADDR_SHIFT);
	}
	if (page >= 0) {
		u32 rowreg;

		rowreg = nfc_read(chip, NFC_ROW_ADDR);
		rowreg &= ~ROW_ADDR_MASK;
		rowreg |= page << ROW_ADDR_SHIFT;
		nfc_write(chip, NFC_ROW_ADDR, rowreg);
	}

	/* Restore column for fus_nfc_read_byte() to 0 on each new command */
	prv->column = 0;

	switch (command) {
	case NAND_CMD_READ0:
	case NAND_CMD_SEQIN:
	case NAND_CMD_ERASE1:
	case NAND_CMD_RNDOUT:
		/* First command byte of two-byte-command. Just store column
		   and row for the address cycle. As the Vybrid NFC does all
		   in one go, the real action takes place when the second
		   command byte is sent:
		     NAND_CMD_READ0 -> NAND_CMD_READSTART
		     NAND_CMD_SEQIN -> NAND_CMD_PAGEPROG
		     NAND_CMD_ERASE1 -> NAND_CMD_ERASE2
		     NAND_CMD_RNDOUT -> NAND_CMD_RNDOUTSTART */
		prv->last_command = command;
		return;

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
		/* Actually do the programming after a NAND_CMD_SEQIN */
		if (prv->last_command != NAND_CMD_SEQIN)
			printf("No NAND_CMD_SEQIN before NAND_CMD_PAGEPROG\n");

		/* Start programming */
		nfc_send_cmd_2(chip, NAND_CMD_SEQIN, NAND_CMD_PAGEPROG,
			       PROGRAM_PAGE_CMD_CODE);
		break;

	case NAND_CMD_ERASE2:
		/* Actually do the erasing after a NAND_CMD_ERASE1 */
		if (prv->last_command != NAND_CMD_ERASE1)
			printf("No NAND_CMD_ERASE1 before NAND_CMD_ERASE2\n");

		/* Start erasing */
		nfc_send_cmd_2(chip, NAND_CMD_ERASE1, NAND_CMD_ERASE2,
			       ERASE_CMD_CODE);
		break;

	case NAND_CMD_READOOB:
		/* Emulate with NAND_CMD_READ0 followed by NAND_CMD_READSTART */
		prv->last_command = NAND_CMD_READ0;

		/* Read some OOB bytes */
		nfc_write(chip, NFC_SECTOR_SIZE, chip->ops.ooblen);

		/* Set ECC mode to BYPASS, one virtual page */
		nfc_write(chip, NFC_FLASH_CONFIG,
			  prv->timeout
			  | (0 << CONFIG_STOP_ON_WERR_SHIFT)
			  | (0 << CONFIG_ECC_SRAM_ADDR_SHIFT)
			  | (0 << CONFIG_ECC_SRAM_REQ_SHIFT)
			  | (0 << CONFIG_DMA_REQ_SHIFT)
			  | (ECC_BYPASS << CONFIG_ECC_MODE_SHIFT)
			  | (0 << CONFIG_FAST_FLASH_SHIFT)
			  | (0 << CONFIG_ID_COUNT_SHIFT)
			  | (0 << CONFIG_16BIT_SHIFT)
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
				  chip->ops.ooblen, 0);
		return;

	case NAND_CMD_READID:
		/* Set ECC mode to BYPASS, 5 ID bytes, one virtual page */
		nfc_write(chip, NFC_FLASH_CONFIG,
			  prv->timeout
			  | (0 << CONFIG_STOP_ON_WERR_SHIFT)
			  | (0 << CONFIG_ECC_SRAM_ADDR_SHIFT)
			  | (0 << CONFIG_ECC_SRAM_REQ_SHIFT)
			  | (0 << CONFIG_DMA_REQ_SHIFT)
			  | (ECC_BYPASS << CONFIG_ECC_MODE_SHIFT)
			  | (0 << CONFIG_FAST_FLASH_SHIFT)
			  | (5 << CONFIG_ID_COUNT_SHIFT)
			  | (0 << CONFIG_16BIT_SHIFT)
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
			  prv->timeout
			  | (0 << CONFIG_STOP_ON_WERR_SHIFT)
			  | (0 << CONFIG_ECC_SRAM_ADDR_SHIFT)
			  | (0 << CONFIG_ECC_SRAM_REQ_SHIFT)
			  | (0 << CONFIG_DMA_REQ_SHIFT)
			  | (ECC_BYPASS << CONFIG_ECC_MODE_SHIFT)
			  | (0 << CONFIG_FAST_FLASH_SHIFT)
			  | (0 << CONFIG_ID_COUNT_SHIFT)
			  | (0 << CONFIG_16BIT_SHIFT)
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
			  prv->timeout
			  | (0 << CONFIG_STOP_ON_WERR_SHIFT)
			  | (0 << CONFIG_ECC_SRAM_ADDR_SHIFT)
			  | (0 << CONFIG_ECC_SRAM_REQ_SHIFT)
			  | (0 << CONFIG_DMA_REQ_SHIFT)
			  | (ECC_BYPASS << CONFIG_ECC_MODE_SHIFT)
			  | (0 << CONFIG_FAST_FLASH_SHIFT)
			  | (0 << CONFIG_ID_COUNT_SHIFT)
			  | (0 << CONFIG_16BIT_SHIFT)
			  | (0 << CONFIG_BOOT_MODE_SHIFT)
			  | (0 << CONFIG_ADDR_AUTO_INCR_SHIFT)
			  | (0 << CONFIG_BUFNO_AUTO_INCR_SHIFT)
			  | (1 << CONFIG_PAGE_CNT_SHIFT));

		/* Send reset */
		nfc_send_cmd_1(chip, NAND_CMD_RESET, RESET_CMD_CODE);
		break;

	case NAND_CMD_PARAM:
		/* Read ONFI parameter page with 1024 bytes */
		nfc_write(chip, NFC_SECTOR_SIZE, 1024);

		/* Set ECC mode to BYPASS, one virtual page */
		nfc_write(chip, NFC_FLASH_CONFIG,
			  prv->timeout
			  | (0 << CONFIG_STOP_ON_WERR_SHIFT)
			  | (0 << CONFIG_ECC_SRAM_ADDR_SHIFT)
			  | (0 << CONFIG_ECC_SRAM_REQ_SHIFT)
			  | (0 << CONFIG_DMA_REQ_SHIFT)
			  | (ECC_BYPASS << CONFIG_ECC_MODE_SHIFT)
			  | (0 << CONFIG_FAST_FLASH_SHIFT)
			  | (0 << CONFIG_ID_COUNT_SHIFT)
			  | (0 << CONFIG_16BIT_SHIFT)
			  | (0 << CONFIG_BOOT_MODE_SHIFT)
			  | (0 << CONFIG_ADDR_AUTO_INCR_SHIFT)
			  | (0 << CONFIG_BUFNO_AUTO_INCR_SHIFT)
			  | (1 << CONFIG_PAGE_CNT_SHIFT));

		/* Start ID request */
		nfc_send_cmd_1(chip, NAND_CMD_PARAM, READ_PARAM_CMD_CODE);
		break;

	default:
		printf("NAND_CMD 0x%x not supported\n", command);
		return;
	}

	prv->last_command = command;

	/* Wait for command completion */
	nand_wait_ready(mtd);
}


/* Read OOB data from given offset */
static int fus_nfc_read_oob(struct mtd_info *mtd, struct nand_chip *chip,
			    int page, int sndcmd)
{
	uint offs = chip->ops.ooboffs;

	/* Our free OOB bytes are grouped in one chunk at a fix offset behind
	   the ECC */
	if (chip->ops.mode == MTD_OOB_AUTO)
		offs += chip->ecc.layout->oobfree[0].offset;

	/* Send command to read oob data from flash */
	chip->cmdfunc(mtd, NAND_CMD_READOOB, offs, page);

	return 1;
}


/* Write OOB data at given offset */
static int fus_nfc_write_oob(struct mtd_info *mtd, struct nand_chip *chip,
			     int page)
{
	struct fsl_nfc_fus_prv *prv = chip->priv;
	uint offs = chip->ops.ooboffs;

	/* Our free OOB bytes are grouped in one chunk at a fix offset behind
	   the ECC */
	if (chip->ops.mode == MTD_OOB_AUTO)
		offs += chip->ecc.layout->oobfree[0].offset;

	/* Start programming sequence with NAND_CMD_SEQIN */
	chip->cmdfunc(mtd, NAND_CMD_SEQIN, offs + mtd->writesize, page);

	/* Copy OOB data to NFC RAM; as we only write these few bytes and no
	   main data, they have to be set at offset 0 in NFC RAM */
	nfc_copy_to_nfc(chip, chip->oob_poi + offs, chip->ops.ooblen, 0);

	/* Write some OOB bytes */
	nfc_write(chip, NFC_SECTOR_SIZE, chip->ops.ooblen);

	/* Set ECC mode to BYPASS, one virtual page */
	nfc_write(chip, NFC_FLASH_CONFIG,
		  prv->timeout
		  | (0 << CONFIG_STOP_ON_WERR_SHIFT)
		  | (0 << CONFIG_ECC_SRAM_ADDR_SHIFT)
		  | (0 << CONFIG_ECC_SRAM_REQ_SHIFT)
		  | (0 << CONFIG_DMA_REQ_SHIFT)
		  | (ECC_BYPASS << CONFIG_ECC_MODE_SHIFT)
		  | (0 << CONFIG_FAST_FLASH_SHIFT)
		  | (0 << CONFIG_ID_COUNT_SHIFT)
		  | (0 << CONFIG_16BIT_SHIFT)
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

/* Common part for reading page + OOB without ECC */
static void fus_nfc_do_read_page_raw(struct mtd_info *mtd,
				     struct nand_chip *chip)
{
	struct fsl_nfc_fus_prv *prv = chip->priv;
	uint size;

	/* Set number of bytes to transfer */
	size = mtd->writesize + mtd->oobsize;
	nfc_write(chip, NFC_SECTOR_SIZE, size);

	/* Set ECC mode to BYPASS, position of ECC status, one virtual page */
	nfc_write(chip, NFC_FLASH_CONFIG,
		  prv->timeout
		  | (0 << CONFIG_STOP_ON_WERR_SHIFT)
		  | (0 << CONFIG_ECC_SRAM_ADDR_SHIFT)
		  | (0 << CONFIG_ECC_SRAM_REQ_SHIFT)
		  | (0 << CONFIG_DMA_REQ_SHIFT)
		  | (ECC_BYPASS << CONFIG_ECC_MODE_SHIFT)
		  | (0 << CONFIG_FAST_FLASH_SHIFT)
		  | (0 << CONFIG_ID_COUNT_SHIFT)
		  | (0 << CONFIG_16BIT_SHIFT)
		  | (0 << CONFIG_BOOT_MODE_SHIFT)
		  | (0 << CONFIG_ADDR_AUTO_INCR_SHIFT)
		  | (0 << CONFIG_BUFNO_AUTO_INCR_SHIFT)
		  | (1 << CONFIG_PAGE_CNT_SHIFT));

	/* Actually trigger read transfer and wait for completion */
	chip->cmdfunc(mtd, NAND_CMD_READSTART, -1, -1);
}


/* Read the whole main and the whole OOB area in one go (without ECC) */
static int fus_nfc_read_page_raw(struct mtd_info *mtd, struct nand_chip *chip,
				 uint8_t *buf, int page)
{
	/* Load page and OOB in raw mode to NFC RAM */
	fus_nfc_do_read_page_raw(mtd, chip);

	/* Copy main and OOB data from NFC RAM. Please note that we don't swap
	   the main data from Big Endian byte order. DMA does not swap data
	   either, so if we want to use DMA in the future, we have to keep it
	   this way. See also fus_nfc_write_page_raw(). */

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
		  prv->timeout
		  | (0 << CONFIG_STOP_ON_WERR_SHIFT)
		  | (0 << CONFIG_ECC_SRAM_ADDR_SHIFT)
		  | (0 << CONFIG_ECC_SRAM_REQ_SHIFT)
		  | (0 << CONFIG_DMA_REQ_SHIFT)
		  | (ECC_BYPASS << CONFIG_ECC_MODE_SHIFT)
		  | (0 << CONFIG_FAST_FLASH_SHIFT)
		  | (0 << CONFIG_ID_COUNT_SHIFT)
		  | (0 << CONFIG_16BIT_SHIFT)
		  | (0 << CONFIG_BOOT_MODE_SHIFT)
		  | (0 << CONFIG_ADDR_AUTO_INCR_SHIFT)
		  | (0 << CONFIG_BUFNO_AUTO_INCR_SHIFT)
		  | (1 << CONFIG_PAGE_CNT_SHIFT));

	/* The actual programming will take place in fus_nfc_command() when
	   command NAND_CMD_PAGEPROG is sent */
}


/* Common part for reading a page with ECC */
static u8 fus_nfc_do_read_page(struct mtd_info *mtd, struct nand_chip *chip)
{
	struct fsl_nfc_fus_prv *prv = chip->priv;
	uint status_offset;
	uint size;

	/* Clear OOB area */
	memset(chip->IO_ADDR_R + NFC_SPARE_AREA(0), 0xff, mtd->oobsize);
	size = mtd->writesize + 4
		+ fus_nfc_ecclayout[prv->eccmode].eccbytes;
	nfc_write(chip, NFC_SECTOR_SIZE, size);

	/* Set ECC mode, position of ECC status, one virtual page */
	size = mtd->writesize + mtd->oobsize;
	nfc_write(chip, NFC_FLASH_CONFIG,
		  prv->timeout
		  | (0 << CONFIG_STOP_ON_WERR_SHIFT)
		  | ((ECC_STATUS_OFFS >>3) << CONFIG_ECC_SRAM_ADDR_SHIFT)
		  | (1 << CONFIG_ECC_SRAM_REQ_SHIFT)
		  | (0 << CONFIG_DMA_REQ_SHIFT)
		  | (prv->eccmode << CONFIG_ECC_MODE_SHIFT)
		  | (0 << CONFIG_FAST_FLASH_SHIFT)
		  | (0 << CONFIG_ID_COUNT_SHIFT)
		  | (0 << CONFIG_16BIT_SHIFT)
		  | (0 << CONFIG_BOOT_MODE_SHIFT)
		  | (0 << CONFIG_ADDR_AUTO_INCR_SHIFT)
		  | (0 << CONFIG_BUFNO_AUTO_INCR_SHIFT)
		  | (1 << CONFIG_PAGE_CNT_SHIFT));

	/* Actually trigger read transfer and wait for completion */
	chip->cmdfunc(mtd, NAND_CMD_READSTART, -1, -1);

	/* Get the ECC status */
	status_offset =
		nfc_read(chip, NFC_FLASH_CONFIG) & CONFIG_ECC_SRAM_ADDR_MASK;
	status_offset >>= CONFIG_ECC_SRAM_ADDR_SHIFT;
	status_offset <<= 3;
	status_offset += 7;

	return nfc_read_ram(chip, status_offset);
}


/* Read main data of page with ECC enabled */
static int fus_nfc_read_page(struct mtd_info *mtd, struct nand_chip *chip,
			     uint8_t *buf, int page)
{
	u8 ecc_status;

	/* Read the page to NFC RAM */
	ecc_status = fus_nfc_do_read_page(mtd, chip);

	/* Update ecc_stats */
	if (ecc_status & ECC_STATUS_MASK) {
		/* The page is uncorrectable; however this can also happen
		   with a totally empty (=erased) page, in which case we
		   should return OK */
		if (!nfc_page_is_empty(mtd, chip)) {
			mtd->ecc_stats.failed++;
			printf("Non-correctable error at page 0x%x\n", page);
		}
	} else {
		/* Write ecc stats to struct */
		mtd->ecc_stats.corrected += ecc_status & ECC_ERR_COUNT;
	}

	/* Copy main data from NFC RAM. Please note that we don't swap the
	   data from Big Endian byte order. DMA does not swap data either, so
	   if we want to use DMA in the future, we have to keep it this way.
	   See also fus_nfc_write_page(). */
	memcpy(buf, chip->IO_ADDR_R + NFC_MAIN_AREA(0), mtd->writesize);

	return 0;
}


/* Write main data of page with ECC enabled */
static void fus_nfc_write_page(struct mtd_info *mtd, struct nand_chip *chip,
			       const uint8_t *buf)
{
	struct fsl_nfc_fus_prv *prv = chip->priv;
	uint size;

	/* Copy main data to NFC RAM. Please note that we don't swap the data
	   to Big Endian byte order. DMA does not swap data either, so if we
	   want to use DMA in the future, we have to keep it this way. Also
	   don't forget to write 0xFFFFFFFF to the BBM area. */
	memcpy(chip->IO_ADDR_R + NFC_MAIN_AREA(0), buf, mtd->writesize);
	memset(chip->IO_ADDR_R + NFC_SPARE_AREA(0), 0xFF, 4); //### X/R-Marker

	/* Set number of bytes to transfer */
	size = mtd->writesize + 4
		+ fus_nfc_ecclayout[prv->eccmode].eccbytes;
	nfc_write(chip, NFC_SECTOR_SIZE, size);

	/* Set ECC mode, position of ECC status, one virtual page */
	size = mtd->writesize + mtd->oobsize;
	nfc_write(chip, NFC_FLASH_CONFIG,
		  prv->timeout
		  | (0 << CONFIG_STOP_ON_WERR_SHIFT)
		  | ((ECC_STATUS_OFFS >> 3) << CONFIG_ECC_SRAM_ADDR_SHIFT)
		  | (1 << CONFIG_ECC_SRAM_REQ_SHIFT)
		  | (0 << CONFIG_DMA_REQ_SHIFT)
		  | (prv->eccmode << CONFIG_ECC_MODE_SHIFT)
		  | (0 << CONFIG_FAST_FLASH_SHIFT)
		  | (0 << CONFIG_ID_COUNT_SHIFT)
		  | (0 << CONFIG_16BIT_SHIFT)
		  | (0 << CONFIG_BOOT_MODE_SHIFT)
		  | (0 << CONFIG_ADDR_AUTO_INCR_SHIFT)
		  | (0 << CONFIG_BUFNO_AUTO_INCR_SHIFT)
		  | (1 << CONFIG_PAGE_CNT_SHIFT));

	/* The actual programming will take place in fus_nfc_command() when
	   command NAND_CMD_PAGEPROG is sent */
}


/* Read back recently written page from NAND and compare with buffer */
static int fus_nfc_verify_buf(struct mtd_info *mtd, const uint8_t *buf, int len)
{
	struct nand_chip *chip = mtd->priv;

	if (chip->ops.mode == MTD_OOB_RAW) {
		uint i;

		/* Read page and OOB without ECC */
		fus_nfc_do_read_page_raw(mtd, chip);

		/* Check if OOB differs */
		for (i = 0; i < mtd->oobsize; i++) {
			if (chip->oob_poi[i] != nfc_read_ram(chip, i))
				return 1;
		}
	} else {
		/* Read the page witch ECC to NFC RAM. If we have
		   uncorrectable errors, simply return; we don't need to check
		   for an empty page as we had written it right before calling
		   fus_nfc_verify_buf(). So it is definitely not erased. We
		   also don't update the err_stats as this is not a regular
		   read, just a write verification. */
		if (fus_nfc_do_read_page(mtd, chip) & ECC_STATUS_MASK)
			return 1;
	}

	/* Check if the main area differs */
	if (memcmp(buf, chip->IO_ADDR_R + NFC_MAIN_AREA(0), mtd->writesize))
		return 1;

	return 0;
}


/* Vybrid NFC specific initialization */
int board_nand_init(struct nand_chip *chip)
{
	struct fsl_nfc_fus_prv *prv;

	if (chip->IO_ADDR_R == NULL)
		return -EINVAL;

	prv = malloc(sizeof(struct fsl_nfc_fus_prv));
	if (!prv) {
		printf(KERN_ERR DRV_NAME ": Memory exhausted!\n");
		return -ENOMEM;
	}

	chip->priv = prv;

	/* Get the default timeout value */
	prv->timeout =
		nfc_read(chip, NFC_FLASH_CONFIG) & CONFIG_CMD_TIMEOUT_MASK;

	/* Setup all things required to detect the chip */
	chip->select_chip = fus_nfc_select_chip;
	chip->dev_ready = fus_nfc_dev_ready;
	chip->cmdfunc = fus_nfc_command;
	chip->read_byte = fus_nfc_read_byte;
	chip->read_word = fus_nfc_read_word;
	chip->read_buf = fus_nfc_read_buf;
	chip->write_buf = fus_nfc_write_buf;
	chip->verify_buf = fus_nfc_verify_buf;
	chip->options = NAND_NO_AUTOINCR | NAND_BBT_SCAN2NDPAGE | NAND_CACHEPRG;
	chip->badblockpos = 0;

	return 0;
}


static int __board_nand_setup_vybrid(struct mtd_info *mtd,
				     struct nand_chip *chip,
				     struct fsl_nfc_fus_prv *prv, int id)
{
	/* Set the Vybrid ECC mode and chip select */
	prv->eccmode = CONFIG_SYS_NAND_FSL_NFC_ECC;

	return 0;
}


int board_nand_setup_vybrid(struct mtd_info *mtd, struct nand_chip *chip,
			    struct fsl_nfc_fus_prv *prv, int id)
	__attribute__((weak, alias("__board_nand_setup_vybrid")));


int board_nand_setup(struct mtd_info *mtd, struct nand_chip *chip, int id)
{
	int ret;
	struct fsl_nfc_fus_prv *prv = chip->priv;

	/* We can only handle 2K pages here */
	if ((mtd->writesize != 0x800) || (mtd->oobsize != 0x40))
		return -EINVAL;

	/* Despite the name, board_nand_setup() is basically arch-specific, so
	   first set things that are really board-specific, especially the
	   ECC mode. */
	ret = board_nand_setup_vybrid(mtd, chip, prv, id);
	if (ret)
		return ret;

	/* Move ECC mode to the correct place and set the correct strength */
	if ((prv->eccmode < ECC_BYPASS) || (prv->eccmode > ECC_60_BYTE))
		prv->eccmode = CONFIG_SYS_NAND_FSL_NFC_ECC;

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

	return 0;
}
