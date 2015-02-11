/*
 * Freescale i.MX28 NAND flash driver
 *
 * Copyright (C) 2011 Marek Vasut <marek.vasut@gmail.com>
 * on behalf of DENX Software Engineering GmbH
 *
 * Based on code from LTIB:
 * Freescale GPMI NFC NAND Flash Driver
 *
 * Copyright (C) 2010 Freescale Semiconductor, Inc.
 * Copyright (C) 2008 Embedded Alley Solutions, Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */

#include <common.h>
#include <linux/mtd/mtd.h>
#include <linux/mtd/nand.h>
#include <linux/types.h>
#include <malloc.h>
#include <asm/errno.h>
#include <asm/io.h>
#include <asm/arch/clock.h>
#include <asm/arch/imx-regs.h>
#include <asm/imx-common/regs-bch.h>
#include <asm/imx-common/regs-gpmi.h>
#include <asm/arch/sys_proto.h>
#include <asm/imx-common/dma.h>
#include <mtd/mxs_nand_fus.h>
#include <nand.h>			/* nand_info[] */

#define	MXS_NAND_DMA_DESCRIPTOR_COUNT		4

#define	MXS_NAND_CHUNK_DATA_CHUNK_SIZE		512
#if defined(CONFIG_MX6)
#define	MXS_NAND_CHUNK_DATA_CHUNK_SIZE_SHIFT	2
#else
#define	MXS_NAND_CHUNK_DATA_CHUNK_SIZE_SHIFT	0
#endif
#define	MXS_NAND_METADATA_SIZE			10

#define	MXS_NAND_COMMAND_BUFFER_SIZE		32

#define	MXS_NAND_BCH_TIMEOUT			10000

struct mxs_nand_priv {
	struct nand_chip chip;		/* Generic NAND chip info */
	int cur_chip;			/* Current NAND chip number (0..3) */
	uint32_t cmd_queue_len;		/* Current command queue length */
	uint32_t desc_index;		/* Current DMA descriptor index */
	uint8_t marking_block_bad;	/* Flag if currently marking bad */
	uint32_t chunks;		/* Number of chunks per page */

	/* Functions with altered behaviour */
	int (*hooked_block_markbad)(struct mtd_info *mtd, loff_t ofs);
};

static ulong nfc_base_addresses[] = {
	CONFIG_SYS_NAND_BASE
};

static struct mxs_nand_priv nfc_infos[CONFIG_SYS_MAX_NAND_DEVICE];

static struct nand_ecclayout fake_ecc_layout;

static uint8_t cmd_buf[MXS_NAND_COMMAND_BUFFER_SIZE]
				__attribute__((aligned(MXS_DMA_ALIGNMENT)));

static struct mxs_dma_desc dma_desc[MXS_NAND_DMA_DESCRIPTOR_COUNT]
				__attribute__((aligned(MXS_DMA_ALIGNMENT)));

static uint8_t data_buf[NAND_MAX_PAGESIZE + NAND_MAX_OOBSIZE]
				__attribute__((aligned(MXS_DMA_ALIGNMENT)));
/*
 * Cache management functions
 */
#ifndef	CONFIG_SYS_DCACHE_OFF
static void mxs_nand_flush_data_buf(uint32_t size)
{
	unsigned long addr = (unsigned long)data_buf;

	size = ALIGN(size, MXS_DMA_ALIGNMENT);
	flush_dcache_range(addr, addr + size);
}

static void mxs_nand_inval_data_buf(uint32_t size)
{
	unsigned long addr = (unsigned long)data_buf;

	size = ALIGN(size, MXS_DMA_ALIGNMENT);
	invalidate_dcache_range(addr, addr + size);
}

static void mxs_nand_flush_cmd_buf(void)
{
	unsigned long addr = (unsigned long)cmd_buf;

	flush_dcache_range(addr, addr + MXS_NAND_COMMAND_BUFFER_SIZE);
}
#else
static inline void mxs_nand_flush_data_buf(uint32_t size) {}
static inline void mxs_nand_inval_data_buf(uint32_t size) {}
static inline void mxs_nand_flush_cmd_buf(void) {}
#endif

static struct mxs_dma_desc *mxs_nand_get_dma_desc(struct mxs_nand_priv *priv)
{
	if (priv->desc_index >= MXS_NAND_DMA_DESCRIPTOR_COUNT) {
		printf("MXS NAND: Too many DMA descriptors requested\n");
		return NULL;
	}

	return &dma_desc[priv->desc_index++];
}

static void mxs_nand_return_dma_descs(struct mxs_nand_priv *priv)
{
	int i;
	struct mxs_dma_desc *desc;

	for (i = 0; i < priv->desc_index; i++) {
		desc = &dma_desc[i];
		memset(desc, 0, sizeof(struct mxs_dma_desc));
		desc->address = (dma_addr_t)desc;
	}

	priv->desc_index = 0;
}

static uint32_t mxs_nand_ecc_chunk_cnt(uint32_t page_data_size)
{
	return page_data_size / MXS_NAND_CHUNK_DATA_CHUNK_SIZE;
}

static uint32_t mxs_nand_ecc_size_in_bits(uint32_t ecc_strength)
{
	return ecc_strength * 13;
}

static uint32_t mxs_nand_aux_status_offset(void)
{
	return (MXS_NAND_METADATA_SIZE + 0x3) & ~0x3;
}

static inline uint32_t mxs_nand_get_ecc_strength(uint32_t page_data_size,
						uint32_t page_oob_size)
{
	if (page_data_size == 2048)
		return 8;

	if (page_data_size == 4096) {
		if (page_oob_size == 128)
			return 8;

		if (page_oob_size == 218)
			return 16;

		if (page_oob_size == 224)
			return 16;
	}

	return 0;
}

static inline uint32_t mxs_nand_get_mark_offset(uint32_t page_data_size,
						uint32_t ecc_strength)
{
	uint32_t chunk_data_size_in_bits;
	uint32_t chunk_ecc_size_in_bits;
	uint32_t chunk_total_size_in_bits;
	uint32_t block_mark_chunk_number;
	uint32_t block_mark_chunk_bit_offset;
	uint32_t block_mark_bit_offset;

	chunk_data_size_in_bits = MXS_NAND_CHUNK_DATA_CHUNK_SIZE * 8;
	chunk_ecc_size_in_bits  = mxs_nand_ecc_size_in_bits(ecc_strength);

	chunk_total_size_in_bits =
			chunk_data_size_in_bits + chunk_ecc_size_in_bits;

	/* Compute the bit offset of the block mark within the physical page. */
	block_mark_bit_offset = page_data_size * 8;

	/* Subtract the metadata bits. */
	block_mark_bit_offset -= MXS_NAND_METADATA_SIZE * 8;

	/*
	 * Compute the chunk number (starting at zero) in which the block mark
	 * appears.
	 */
	block_mark_chunk_number =
			block_mark_bit_offset / chunk_total_size_in_bits;

	/*
	 * Compute the bit offset of the block mark within its chunk, and
	 * validate it.
	 */
	block_mark_chunk_bit_offset = block_mark_bit_offset -
			(block_mark_chunk_number * chunk_total_size_in_bits);

	if (block_mark_chunk_bit_offset > chunk_data_size_in_bits)
		return 1;

	/*
	 * Now that we know the chunk number in which the block mark appears,
	 * we can subtract all the ECC bits that appear before it.
	 */
	block_mark_bit_offset -=
		block_mark_chunk_number * chunk_ecc_size_in_bits;

	return block_mark_bit_offset;
}


/*
 * Wait for BCH complete IRQ and clear the IRQ
 */
static int mxs_nand_wait_for_bch_complete(void)
{
	struct mxs_bch_regs *bch_regs = (struct mxs_bch_regs *)MXS_BCH_BASE;
	int timeout = MXS_NAND_BCH_TIMEOUT;
	int ret;

	ret = mxs_wait_mask_set(&bch_regs->hw_bch_ctrl_reg,
		BCH_CTRL_COMPLETE_IRQ, timeout);

	writel(BCH_CTRL_COMPLETE_IRQ, &bch_regs->hw_bch_ctrl_clr);

	return ret;
}

/*
 * This is the function that we install in the cmd_ctrl function pointer of the
 * owning struct nand_chip. The only functions in the reference implementation
 * that use these functions pointers are cmdfunc and select_chip.
 *
 * In this driver, we implement our own select_chip, so this function will only
 * be called by the reference implementation's cmdfunc. For this reason, we can
 * ignore the chip enable bit and concentrate only on sending bytes to the NAND
 * Flash.
 */
static void mxs_nand_cmd_ctrl(struct mtd_info *mtd, int data, unsigned int ctrl)
{
	struct nand_chip *chip = mtd->priv;
	struct mxs_nand_priv *priv = chip->priv;
	struct mxs_dma_desc *d;
	uint32_t channel = MXS_DMA_CHANNEL_AHB_APBH_GPMI0 + priv->cur_chip;
	int ret;

	/*
	 * If this condition is true, something is _VERY_ wrong in MTD
	 * subsystem!
	 */
	if (priv->cmd_queue_len == MXS_NAND_COMMAND_BUFFER_SIZE) {
		printf("MXS NAND: Command queue too long\n");
		return;
	}

	/*
	 * Every operation begins with a command byte and a series of zero or
	 * more address bytes. These are distinguished by either the Address
	 * Latch Enable (ALE) or Command Latch Enable (CLE) signals being
	 * asserted. When MTD is ready to execute the command, it will
	 * deasert both latch enables.
	 *
	 * Rather than run a separate DMA operation for every single byte, we
	 * queue them up and run a single DMA operation for the entire series
	 * of command and data bytes.
	 */
	if (ctrl & (NAND_ALE | NAND_CLE)) {
		if (data != NAND_CMD_NONE)
			cmd_buf[priv->cmd_queue_len++] = data;
		return;
	}

	/*
	 * If control arrives here, MTD has deasserted both the ALE and CLE,
	 * which means it's ready to run an operation. Check if we have any
	 * bytes to send.
	 */
	if (priv->cmd_queue_len == 0)
		return;

	/* Compile the DMA descriptor -- a descriptor that sends command. */
	d = mxs_nand_get_dma_desc(priv);
	d->cmd.data =
		MXS_DMA_DESC_COMMAND_DMA_READ | MXS_DMA_DESC_IRQ |
		MXS_DMA_DESC_CHAIN | MXS_DMA_DESC_DEC_SEM |
		MXS_DMA_DESC_WAIT4END | (3 << MXS_DMA_DESC_PIO_WORDS_OFFSET) |
		(priv->cmd_queue_len << MXS_DMA_DESC_BYTES_OFFSET);

	d->cmd.address = (dma_addr_t)cmd_buf;

	d->cmd.pio_words[0] =
		GPMI_CTRL0_COMMAND_MODE_WRITE |
		GPMI_CTRL0_WORD_LENGTH |
		(priv->cur_chip << GPMI_CTRL0_CS_OFFSET) |
		GPMI_CTRL0_ADDRESS_NAND_CLE |
		GPMI_CTRL0_ADDRESS_INCREMENT |
		priv->cmd_queue_len;

	mxs_dma_desc_append(channel, d);

	/* Flush caches */
	mxs_nand_flush_cmd_buf();

	/* Execute the DMA chain. */
	ret = mxs_dma_go(channel);
	if (ret)
		printf("MXS NAND: Error sending command\n");

	mxs_nand_return_dma_descs(priv);

	/* Reset the command queue. */
	priv->cmd_queue_len = 0;
}

/*
 * Test if the NAND flash is ready.
 */
static int mxs_nand_device_ready(struct mtd_info *mtd)
{
	struct nand_chip *chip = mtd->priv;
	struct mxs_nand_priv *priv = chip->priv;
	struct mxs_gpmi_regs *gpmi_regs =
		(struct mxs_gpmi_regs *)MXS_GPMI_BASE;
	uint32_t tmp;

	tmp = readl(&gpmi_regs->hw_gpmi_stat);
	tmp >>= (GPMI_STAT_READY_BUSY_OFFSET + priv->cur_chip);

	return tmp & 1;
}

/*
 * Select the NAND chip.
 */
static void mxs_nand_select_chip(struct mtd_info *mtd, int cur_chip)
{
	struct nand_chip *chip = mtd->priv;
	struct mxs_nand_priv *priv = chip->priv;

	priv->cur_chip = cur_chip;
}

/*
 * Handle block mark swapping.
 *
 * Note that, when this function is called, it doesn't know whether it's
 * swapping the block mark, or swapping it *back* -- but it doesn't matter
 * because the the operation is the same.
 */
static void mxs_nand_swap_block_mark(struct mtd_info *mtd)
{

	uint32_t src;
	uint32_t dst;
	uint32_t mark_offset = mxs_nand_get_mark_offset(mtd->writesize,
							mtd->ecc_strength);
	uint32_t bit_offset = mark_offset & 0x07;
	uint32_t buf_offset = mark_offset >> 3;

	/*
	 * Get the byte from the data area that overlays the block mark. Since
	 * the ECC engine applies its own view to the bits in the page, the
	 * physical block mark won't (in general) appear on a byte boundary in
	 * the data.
	 */
	src = data_buf[buf_offset] >> bit_offset;
	src |= data_buf[buf_offset + 1] << (8 - bit_offset);

	dst = data_buf[mtd->writesize];

	data_buf[mtd->writesize] = src;

	data_buf[buf_offset] &= ~(0xff << bit_offset);
	data_buf[buf_offset + 1] &= 0xff << bit_offset;

	data_buf[buf_offset] |= dst << bit_offset;
	data_buf[buf_offset + 1] |= dst >> (8 - bit_offset);
}

/*
 * Read data from NAND.
 */
static void mxs_nand_read_buf(struct mtd_info *mtd, uint8_t *buf, int length)
{
	struct nand_chip *chip = mtd->priv;
	struct mxs_nand_priv *priv = chip->priv;
	struct mxs_dma_desc *d;
	uint32_t channel = MXS_DMA_CHANNEL_AHB_APBH_GPMI0 + priv->cur_chip;
	int ret;

	if (length > NAND_MAX_PAGESIZE) {
		printf("MXS NAND: DMA buffer too big\n");
		return;
	}

	if (!buf) {
		printf("MXS NAND: DMA buffer is NULL\n");
		return;
	}

	/* Compile the DMA descriptor - a descriptor that reads data. */
	d = mxs_nand_get_dma_desc(priv);
	d->cmd.data =
		MXS_DMA_DESC_COMMAND_DMA_WRITE | MXS_DMA_DESC_IRQ |
		MXS_DMA_DESC_DEC_SEM | MXS_DMA_DESC_WAIT4END |
		(1 << MXS_DMA_DESC_PIO_WORDS_OFFSET) |
		(length << MXS_DMA_DESC_BYTES_OFFSET);

	d->cmd.address = (dma_addr_t)data_buf;

	d->cmd.pio_words[0] =
		GPMI_CTRL0_COMMAND_MODE_READ |
		GPMI_CTRL0_WORD_LENGTH |
		(priv->cur_chip << GPMI_CTRL0_CS_OFFSET) |
		GPMI_CTRL0_ADDRESS_NAND_DATA |
		length;

	mxs_dma_desc_append(channel, d);

	/*
	 * A DMA descriptor that waits for the command to end and the chip to
	 * become ready.
	 *
	 * I think we actually should *not* be waiting for the chip to become
	 * ready because, after all, we don't care. I think the original code
	 * did that and no one has re-thought it yet.
	 */
	d = mxs_nand_get_dma_desc(priv);
	d->cmd.data =
		MXS_DMA_DESC_COMMAND_NO_DMAXFER | MXS_DMA_DESC_IRQ |
		MXS_DMA_DESC_NAND_WAIT_4_READY | MXS_DMA_DESC_DEC_SEM |
		MXS_DMA_DESC_WAIT4END | (4 << MXS_DMA_DESC_PIO_WORDS_OFFSET);

	d->cmd.address = 0;

	d->cmd.pio_words[0] =
		GPMI_CTRL0_COMMAND_MODE_WAIT_FOR_READY |
		GPMI_CTRL0_WORD_LENGTH |
		(priv->cur_chip << GPMI_CTRL0_CS_OFFSET) |
		GPMI_CTRL0_ADDRESS_NAND_DATA;

	mxs_dma_desc_append(channel, d);

	/* Execute the DMA chain. */
	ret = mxs_dma_go(channel);
	if (ret) {
		printf("MXS NAND: DMA read error\n");
		goto rtn;
	}

	/* Invalidate caches */
	mxs_nand_inval_data_buf(length);

	memcpy(buf, data_buf, length);

rtn:
	mxs_nand_return_dma_descs(priv);
}

/*
 * Write data to NAND.
 */
static void mxs_nand_write_buf(struct mtd_info *mtd, const uint8_t *buf,
			       int length)
{
	struct nand_chip *chip = mtd->priv;
	struct mxs_nand_priv *priv = chip->priv;
	struct mxs_dma_desc *d;
	uint32_t channel = MXS_DMA_CHANNEL_AHB_APBH_GPMI0 + priv->cur_chip;
	int ret;

	if (length > NAND_MAX_PAGESIZE) {
		printf("MXS NAND: DMA buffer too big\n");
		return;
	}

	if (!buf) {
		printf("MXS NAND: DMA buffer is NULL\n");
		return;
	}

	memcpy(data_buf, buf, length);

	/* Compile the DMA descriptor - a descriptor that writes data. */
	d = mxs_nand_get_dma_desc(priv);
	d->cmd.data =
		MXS_DMA_DESC_COMMAND_DMA_READ | MXS_DMA_DESC_IRQ |
		MXS_DMA_DESC_DEC_SEM | MXS_DMA_DESC_WAIT4END |
		(4 << MXS_DMA_DESC_PIO_WORDS_OFFSET) |
		(length << MXS_DMA_DESC_BYTES_OFFSET);

	d->cmd.address = (dma_addr_t)data_buf;

	d->cmd.pio_words[0] =
		GPMI_CTRL0_COMMAND_MODE_WRITE |
		GPMI_CTRL0_WORD_LENGTH |
		(priv->cur_chip << GPMI_CTRL0_CS_OFFSET) |
		GPMI_CTRL0_ADDRESS_NAND_DATA |
		length;

	mxs_dma_desc_append(channel, d);

	/* Flush caches */
	mxs_nand_flush_data_buf(length);

	/* Execute the DMA chain. */
	ret = mxs_dma_go(channel);
	if (ret)
		printf("MXS NAND: DMA write error\n");

	mxs_nand_return_dma_descs(priv);
}

/*
 * Read a single byte from NAND.
 */
static uint8_t mxs_nand_read_byte(struct mtd_info *mtd)
{
	uint8_t buf;

	mxs_nand_read_buf(mtd, &buf, 1);
	return buf;
}

/*
 * Read a page from NAND.
 */
static int mxs_nand_ecc_read_page(struct mtd_info *mtd, struct nand_chip *chip,
					uint8_t *buf, int oob_required,
					int page)
{
	struct mxs_nand_priv *priv = chip->priv;
	struct mxs_dma_desc *d;
	uint32_t channel = MXS_DMA_CHANNEL_AHB_APBH_GPMI0 + priv->cur_chip;
	uint32_t corrected = 0, failed = 0;
	uint8_t	*status;
	int ret;
	uint32_t i;

	/* Compile the DMA descriptor - wait for ready. */
	d = mxs_nand_get_dma_desc(priv);
	d->cmd.data =
		MXS_DMA_DESC_COMMAND_NO_DMAXFER | MXS_DMA_DESC_CHAIN |
		MXS_DMA_DESC_NAND_WAIT_4_READY | MXS_DMA_DESC_WAIT4END |
		(1 << MXS_DMA_DESC_PIO_WORDS_OFFSET);

	d->cmd.address = 0;

	d->cmd.pio_words[0] =
		GPMI_CTRL0_COMMAND_MODE_WAIT_FOR_READY |
		GPMI_CTRL0_WORD_LENGTH |
		(priv->cur_chip << GPMI_CTRL0_CS_OFFSET) |
		GPMI_CTRL0_ADDRESS_NAND_DATA;

	mxs_dma_desc_append(channel, d);

	/* Compile the DMA descriptor - enable the BCH block and read. */
	d = mxs_nand_get_dma_desc(priv);
	d->cmd.data =
		MXS_DMA_DESC_COMMAND_NO_DMAXFER | MXS_DMA_DESC_CHAIN |
		MXS_DMA_DESC_WAIT4END |	(6 << MXS_DMA_DESC_PIO_WORDS_OFFSET);

	d->cmd.address = 0;

	d->cmd.pio_words[0] =
		GPMI_CTRL0_COMMAND_MODE_READ |
		GPMI_CTRL0_WORD_LENGTH |
		(priv->cur_chip << GPMI_CTRL0_CS_OFFSET) |
		GPMI_CTRL0_ADDRESS_NAND_DATA |
		(mtd->writesize + mtd->oobsize);
	d->cmd.pio_words[1] = 0;
	d->cmd.pio_words[2] =
		GPMI_ECCCTRL_ENABLE_ECC |
		GPMI_ECCCTRL_ECC_CMD_DECODE |
		GPMI_ECCCTRL_BUFFER_MASK_BCH_PAGE;
	d->cmd.pio_words[3] = mtd->writesize + mtd->oobsize;
	d->cmd.pio_words[4] = (dma_addr_t)data_buf;
	d->cmd.pio_words[5] = (dma_addr_t)(data_buf + mtd->writesize);

	mxs_dma_desc_append(channel, d);

	/* Compile the DMA descriptor - disable the BCH block. */
	d = mxs_nand_get_dma_desc(priv);
	d->cmd.data =
		MXS_DMA_DESC_COMMAND_NO_DMAXFER | MXS_DMA_DESC_CHAIN |
		MXS_DMA_DESC_NAND_WAIT_4_READY | MXS_DMA_DESC_WAIT4END |
		(3 << MXS_DMA_DESC_PIO_WORDS_OFFSET);

	d->cmd.address = 0;

	d->cmd.pio_words[0] =
		GPMI_CTRL0_COMMAND_MODE_WAIT_FOR_READY |
		GPMI_CTRL0_WORD_LENGTH |
		(priv->cur_chip << GPMI_CTRL0_CS_OFFSET) |
		GPMI_CTRL0_ADDRESS_NAND_DATA |
		(mtd->writesize + mtd->oobsize);
	d->cmd.pio_words[1] = 0;
	d->cmd.pio_words[2] = 0;

	mxs_dma_desc_append(channel, d);

	/* Compile the DMA descriptor - deassert the NAND lock and interrupt. */
	d = mxs_nand_get_dma_desc(priv);
	d->cmd.data =
		MXS_DMA_DESC_COMMAND_NO_DMAXFER | MXS_DMA_DESC_IRQ |
		MXS_DMA_DESC_DEC_SEM;

	d->cmd.address = 0;

	mxs_dma_desc_append(channel, d);

	/* Execute the DMA chain. */
	ret = mxs_dma_go(channel);
	if (ret) {
		printf("MXS NAND: DMA read error\n");
		goto rtn;
	}

	ret = mxs_nand_wait_for_bch_complete();
	if (ret) {
		printf("MXS NAND: BCH read timeout\n");
		goto rtn;
	}

	/* Invalidate caches */
	mxs_nand_inval_data_buf(mtd->writesize + mtd->oobsize + priv->chunks);

	/* Read DMA completed, now do the mark swapping. */
	mxs_nand_swap_block_mark(mtd);

	/* Loop over status bytes, accumulating ECC status. */
	status = data_buf + mtd->writesize + mxs_nand_aux_status_offset();
	for (i = 0; i < priv->chunks; i++) {
		if (status[i] == 0x00)
			continue;

		if (status[i] == 0xff)
			continue;

		if (status[i] == 0xfe) {
			failed++;
			continue;
		}

		corrected += status[i];
	}

	/* Propagate ECC status to the owning MTD. */
	mtd->ecc_stats.failed += failed;
	mtd->ecc_stats.corrected += corrected;
	ret = corrected;		/* Return number of bitflips */

	/*
	 * It's time to deliver the OOB bytes. See mxs_nand_ecc_read_oob() for
	 * details about our policy for delivering the OOB.
	 *
	 * We fill the caller's buffer with set bits, and then copy the block
	 * mark to the caller's buffer. Note that, if block mark swapping was
	 * necessary, it has already been done, so we can rely on the first
	 * byte of the auxiliary buffer to contain the block mark.
	 */
	memset(chip->oob_poi, 0xff, mtd->oobsize);
	chip->oob_poi[0] = data_buf[mtd->writesize];
	memcpy(buf, data_buf, mtd->writesize);

rtn:
	mxs_nand_return_dma_descs(priv);

	return ret;
}

/*
 * Write a page to NAND.
 */
static int mxs_nand_ecc_write_page(struct mtd_info *mtd,
				struct nand_chip *chip, const uint8_t *buf,
				int oob_required)
{
	struct mxs_nand_priv *priv = chip->priv;
	struct mxs_dma_desc *d;
	uint32_t channel = MXS_DMA_CHANNEL_AHB_APBH_GPMI0 + priv->cur_chip;
	int ret;

	memcpy(data_buf, buf, mtd->writesize);
	memcpy(data_buf + mtd->writesize, chip->oob_poi, mtd->oobsize);

	/* Handle block mark swapping. */
	mxs_nand_swap_block_mark(mtd);

	/* Compile the DMA descriptor - write data. */
	d = mxs_nand_get_dma_desc(priv);
	d->cmd.data =
		MXS_DMA_DESC_COMMAND_NO_DMAXFER | MXS_DMA_DESC_IRQ |
		MXS_DMA_DESC_DEC_SEM | MXS_DMA_DESC_WAIT4END |
		(6 << MXS_DMA_DESC_PIO_WORDS_OFFSET);

	d->cmd.address = 0;

	d->cmd.pio_words[0] =
		GPMI_CTRL0_COMMAND_MODE_WRITE |
		GPMI_CTRL0_WORD_LENGTH |
		(priv->cur_chip << GPMI_CTRL0_CS_OFFSET) |
		GPMI_CTRL0_ADDRESS_NAND_DATA;
	d->cmd.pio_words[1] = 0;
	d->cmd.pio_words[2] =
		GPMI_ECCCTRL_ENABLE_ECC |
		GPMI_ECCCTRL_ECC_CMD_ENCODE |
		GPMI_ECCCTRL_BUFFER_MASK_BCH_PAGE;
	d->cmd.pio_words[3] = (mtd->writesize + mtd->oobsize);
	d->cmd.pio_words[4] = (dma_addr_t)data_buf;
	d->cmd.pio_words[5] = (dma_addr_t)(data_buf + mtd->writesize);

	mxs_dma_desc_append(channel, d);

	/* Flush caches */
	mxs_nand_flush_data_buf(mtd->writesize + mtd->oobsize);

	/* Execute the DMA chain. */
	ret = mxs_dma_go(channel);
	if (ret) {
		printf("MXS NAND: DMA write error\n");
		goto rtn;
	}

	ret = mxs_nand_wait_for_bch_complete();
	if (ret) {
		printf("MXS NAND: BCH write timeout\n");
		goto rtn;
	}

rtn:
	mxs_nand_return_dma_descs(priv);
	return 0;
}

/*
 * Mark a block bad in NAND.
 *
 * This function is a veneer that replaces the function originally installed by
 * the NAND Flash MTD code.
 */
static int mxs_nand_hook_block_markbad(struct mtd_info *mtd, loff_t ofs)
{
	struct nand_chip *chip = mtd->priv;
	struct mxs_nand_priv *priv = chip->priv;
	int ret;

	priv->marking_block_bad = 1;

	ret = priv->hooked_block_markbad(mtd, ofs);

	priv->marking_block_bad = 0;

	return ret;
}

/*
 * There are several places in this driver where we have to handle the OOB and
 * block marks. This is the function where things are the most complicated, so
 * this is where we try to explain it all. All the other places refer back to
 * here.
 *
 * These are the rules, in order of decreasing importance:
 *
 * 1) Nothing the caller does can be allowed to imperil the block mark, so all
 *    write operations take measures to protect it.
 *
 * 2) In read operations, the first byte of the OOB we return must reflect the
 *    true state of the block mark, no matter where that block mark appears in
 *    the physical page.
 *
 * 3) ECC-based read operations return an OOB full of set bits (since we never
 *    allow ECC-based writes to the OOB, it doesn't matter what ECC-based reads
 *    return).
 *
 * 4) "Raw" read operations return a direct view of the physical bytes in the
 *    page, using the conventional definition of which bytes are data and which
 *    are OOB. This gives the caller a way to see the actual, physical bytes
 *    in the page, without the distortions applied by our ECC engine.
 *
 * What we do for this specific read operation depends on whether we're doing
 * "raw" read, or an ECC-based read.
 */
static int mxs_nand_ecc_read_oob_raw(struct mtd_info *mtd,
				     struct nand_chip *chip, int page)
{
	/*
	 * If control arrives here, we're doing a "raw" read. Send the
	 * command to read the conventional OOB and read it.
	 */
	chip->cmdfunc(mtd, NAND_CMD_READ0, mtd->writesize, page);
	chip->read_buf(mtd, chip->oob_poi, mtd->oobsize);

	return 0;
}

static int mxs_nand_ecc_read_oob(struct mtd_info *mtd, struct nand_chip *chip,
				 int page)
{
	/* Fill the OOB buffer with set bits and correct the block mark. */
	memset(chip->oob_poi, 0xff, mtd->oobsize);

	chip->cmdfunc(mtd, NAND_CMD_READ0, mtd->writesize, page);
	mxs_nand_read_buf(mtd, chip->oob_poi, 1);

	return 0;
}

/*
 * Write OOB data to NAND.
 */
static int mxs_nand_ecc_write_oob_raw(struct mtd_info *mtd,
				      struct nand_chip *chip, int page)
{
	struct mxs_nand_priv *priv = chip->priv;
	uint8_t block_mark = 0;

	/*
	 * There are fundamental incompatibilities between the i.MX GPMI NFC and
	 * the NAND Flash MTD model that make it essentially impossible to write
	 * the out-of-band bytes.
	 *
	 * We permit *ONE* exception. If the *intent* of writing the OOB is to
	 * mark a block bad, we can do that.
	 */

	if (!priv->marking_block_bad) {
		printf("NXS NAND: Writing OOB isn't supported\n");
		return -EIO;
	}

	/* Write the block mark. */
	chip->cmdfunc(mtd, NAND_CMD_SEQIN, mtd->writesize, page);
	chip->write_buf(mtd, &block_mark, 1);
	chip->cmdfunc(mtd, NAND_CMD_PAGEPROG, -1, -1);

	/* Check if it worked. */
	if (chip->waitfunc(mtd, chip) & NAND_STATUS_FAIL)
		return -EIO;

	return 0;
}

static int mxs_nand_ecc_write_oob(struct mtd_info *mtd, struct nand_chip *chip,
				  int page)
{
	printf("NXS NAND: Writing OOB isn't supported\n");
	return -EIO;
}

/*
 * Claims all blocks are good.
 *
 * In principle, this function is *only* called when the NAND Flash MTD system
 * isn't allowed to keep an in-memory bad block table, so it is forced to ask
 * the driver for bad block information.
 *
 * In fact, we permit the NAND Flash MTD system to have an in-memory BBT, so
 * this function is *only* called when we take it away.
 *
 * Thus, this function is only called when we want *all* blocks to look good,
 * so it *always* return success.
 *
 * ### HK: This is not completely correct. This function is also called when
 *     the in-memory BBT could not be created, for whatever reason. So we
 *     should read the first byte and return it. ### TODO
 */
static int mxs_nand_block_bad(struct mtd_info *mtd, loff_t ofs, int getchip)
{
	return 0;
}

/*
 * Called after nand_scan_ident(). Now the flash geometry is known.
 */
static void mxs_nand_init_cont(struct mtd_info *mtd)
{
	struct nand_chip *chip = mtd->priv;
	struct mxs_nand_priv *priv = chip->priv;
	struct mxs_bch_regs *bch_regs = (struct mxs_bch_regs *)MXS_BCH_BASE;
	uint32_t tmp;

	/* Configure BCH and set NFC geometry */
	mxs_reset_block(&bch_regs->hw_bch_ctrl_reg);

	/* Configure layout 0 */
	tmp = (priv->chunks - 1) << BCH_FLASHLAYOUT0_NBLOCKS_OFFSET;
	tmp |= MXS_NAND_METADATA_SIZE << BCH_FLASHLAYOUT0_META_SIZE_OFFSET;
	tmp |= (mtd->ecc_strength >> 1) << BCH_FLASHLAYOUT0_ECC0_OFFSET;
	tmp |= MXS_NAND_CHUNK_DATA_CHUNK_SIZE
		>> MXS_NAND_CHUNK_DATA_CHUNK_SIZE_SHIFT;
	writel(tmp, &bch_regs->hw_bch_flash0layout0);

	tmp = (mtd->writesize + mtd->oobsize)
		<< BCH_FLASHLAYOUT1_PAGE_SIZE_OFFSET;
	tmp |= (mtd->ecc_strength >> 1) << BCH_FLASHLAYOUT1_ECCN_OFFSET;
	tmp |= MXS_NAND_CHUNK_DATA_CHUNK_SIZE
		>> MXS_NAND_CHUNK_DATA_CHUNK_SIZE_SHIFT;
	writel(tmp, &bch_regs->hw_bch_flash0layout1);

	/* Set *all* chip selects to use layout 0 */
	writel(0, &bch_regs->hw_bch_layoutselect);

	/* Enable BCH complete interrupt */
	writel(BCH_CTRL_COMPLETE_IRQ_EN, &bch_regs->hw_bch_ctrl_set);

	/* Hook some operations at the MTD level. */
	if (mtd->_block_markbad != mxs_nand_hook_block_markbad) {
		priv->hooked_block_markbad = mtd->_block_markbad;
		mtd->_block_markbad = mxs_nand_hook_block_markbad;
	}
}

/*
 * Called before nand_scan_ident(). Initialize some basic NFC hardware to be
 * able to read the NAND ID and ONFI data to detect the block/page/OOB sizes.
 */
int mxs_nand_init(void)
{
	struct mxs_gpmi_regs *gpmi_regs =
		(struct mxs_gpmi_regs *)MXS_GPMI_BASE;
	struct mxs_bch_regs *bch_regs =
		(struct mxs_bch_regs *)MXS_BCH_BASE;
	int j;

	/* Init the DMA controller. */
	for (j = MXS_DMA_CHANNEL_AHB_APBH_GPMI0;
	     j <= MXS_DMA_CHANNEL_AHB_APBH_GPMI7; j++) {
		if (mxs_dma_init_channel(j)) {
			for (--j; j >= 0; j--)
				mxs_dma_release(j);
			printf("MXS NAND: Unable to init DMA channels\n");
			return -ENOMEM;
		}
	}

	/* Reset the GPMI block. */
	mxs_reset_block(&gpmi_regs->hw_gpmi_ctrl0_reg);
	mxs_reset_block(&bch_regs->hw_bch_ctrl_reg);

	/*
	 * Choose NAND mode, set IRQ polarity, disable write protection and
	 * select BCH ECC.
	 */
	clrsetbits_le32(&gpmi_regs->hw_gpmi_ctrl1,
			GPMI_CTRL1_GPMI_MODE,
			GPMI_CTRL1_ATA_IRQRDY_POLARITY
			| GPMI_CTRL1_DEV_RESET | GPMI_CTRL1_BCH_MODE);

	return 0;
}

/*
 * ### At the moment this will only work if called once! To be called several
 * ### times on the same NAND controller, we have to rewrite the buffer and
 * ### DMA descriptor allocation code to allocate some stuff only once.
*/
void mxs_nand_register(int nfc_hw_id,
		       const struct mxs_nand_fus_platform_data *pdata)
{
	static int index = 0;
	struct mxs_nand_priv *priv;
	struct nand_chip *chip;
	struct mtd_info *mtd;

	if (index >= CONFIG_SYS_MAX_NAND_DEVICE)
		return;

	if (nfc_hw_id > ARRAY_SIZE(nfc_base_addresses))
		return;

	mtd = &nand_info[index];
	priv = &nfc_infos[index];
	chip = &priv->chip;
	chip->priv = priv;

	/* Init the mtd device, most of it is done in nand_scan_ident() */
	mtd->priv = chip;
	mtd->size = 0;
	mtd->name = NULL;

	/*
	 * Use buffers already available in struct nand_chip. These buffers
	 * are large enough, but make sure that we use an address that is
	 * correctly aligned for DMA access.
	 */
	priv->cmd_queue_len = 0;

	/* Setup all things required to detect the chip */
	chip->IO_ADDR_R = (void __iomem *)nfc_base_addresses[nfc_hw_id];
	chip->IO_ADDR_W = chip->IO_ADDR_R;
	chip->select_chip = mxs_nand_select_chip;
	chip->dev_ready = mxs_nand_device_ready;
//###	chip->cmdfunc = fus_nfc_command;
	chip->cmd_ctrl = mxs_nand_cmd_ctrl;
	chip->read_byte = mxs_nand_read_byte;
//###	chip->read_word = fus_nfc_read_word;
	chip->read_buf = mxs_nand_read_buf;
	chip->write_buf = mxs_nand_write_buf;
//###	chip->waitfunc = fus_nfc_wait;
	chip->block_bad = mxs_nand_block_bad;
	chip->options = pdata ? pdata->options : 0;
	chip->options |= NAND_BBT_SCAN2NDPAGE | NAND_NO_SUBPAGE_WRITE;
	chip->badblockpos = 0;

	/* If this is the first call, init the GPMI/BCH/DMA system */
	if (!index) {
		/* Initialize the DMA descriptors */
		priv->desc_index = MXS_NAND_DMA_DESCRIPTOR_COUNT;
		mxs_nand_return_dma_descs(priv);
		if (mxs_nand_init())
			return;
	}

	/* Identify the device, set page and block sizes, etc. */
	if (nand_scan_ident(mtd, CONFIG_SYS_NAND_MAX_CHIPS, NULL)) {
		mtd->name = NULL;
		return;
	}

	/* Set skipped region and backup region */
	if (pdata) {
#ifdef CONFIG_NAND_REFRESH
		mtd->backupoffs = pdata->backup_sblock * mtd->erasesize;
		mtd->backupend = pdata->backup_eblock * mtd->erasesize;
#endif
		mtd->skip = pdata->skipblocks * mtd->erasesize;
		if (pdata->flags & MXS_NAND_SKIP_INVERSE) {
			mtd->size = mtd->skip;
			mtd->skip = 0;
		}
	}

	/* Set up ECC configuration */
	chip->ecc.layout = &fake_ecc_layout;
	chip->ecc.mode = NAND_ECC_HW;
	chip->ecc.steps = 1;
	chip->ecc.bytes = 9;
	chip->ecc.size = 512;
	chip->ecc.strength =
//###           pdata ? pdata->ecc_strength :
		mxs_nand_get_ecc_strength(mtd->writesize, mtd->oobsize);
	mtd->ecc_strength = chip->ecc.strength;

	chip->ecc.read_page = mxs_nand_ecc_read_page;
	chip->ecc.write_page = mxs_nand_ecc_write_page;
	chip->ecc.read_oob = mxs_nand_ecc_read_oob;
	chip->ecc.write_oob = mxs_nand_ecc_write_oob;
//###	chip->ecc.read_page_raw = fus_nfc_read_page_raw;
//###	chip->ecc.write_page_raw = fus_nfc_write_page_raw;
	chip->ecc.read_oob_raw = mxs_nand_ecc_read_oob_raw;
	chip->ecc.write_oob_raw = mxs_nand_ecc_write_oob_raw;

	if (chip->ecc.strength >= 20)
		mtd->bitflip_threshold = chip->ecc.strength - 2;
	else
		mtd->bitflip_threshold = chip->ecc.strength - 1;

	priv->chunks = mxs_nand_ecc_chunk_cnt(mtd->writesize);
	mxs_nand_init_cont(mtd);

	if (nand_scan_tail(mtd)) {
		mtd->name = NULL;
		return;
	}

	nand_register(index++);
}
