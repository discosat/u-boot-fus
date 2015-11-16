/*
 * F&S i.MX6 GPMI NAND flash driver
 *
 * Copyright (C) 2015 F&S Elektronik Systeme GmbH
 *
 * This driver uses parts of the code of the Freescale GPMI NAND flash driver
 * (see mxs_nand.c).
 *
 * SPDX-License-Identifier:	GPL-2.0+
 */

/* 
 * ### TODO-List ###
 *
 * - Set real timeout values in mxs_nand_wait_ready().
 *
 * - Move cmd_buf[] and data_buf[] to uncached (!) SRAM and remove all
 *   cache handling. Check with page tables that SRAM is really uncached!
 *
 * - Check if GPMI access without all the DMA stuff is faster in U-Boot. We
 *   only wait for the DMA chain to finish anyway, we do nothing else in the
 *   meantime. So DMA handling may be some unnecessary overhead.
 */

#include <common.h>
#include <linux/mtd/mtd.h>
#include <linux/mtd/nand.h>
#include <linux/types.h>
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

#undef DEBUG
#ifdef DEBUG
/*
 * Descides whether to show DMA descriptors. By default this is set to 1 in
 * mxs_nand_read_page(), when the first regular page for the environment is
 * read. We don't enable it earlier to avoid a looong output while the flash
 * is scanned for bad blocks during bad block tabel (BBT) creation. If you
 * want to see any earlier descriptors nonetheless (e.g. when reading the NAND
 * ID or the ONFI parameters), init showdesc to 1 right here.
 */
static uint8_t showdesc;
#endif

#define MXS_NAND_METADATA_SIZE		32

/* Worst case is mxs_nand_read_oob_raw() with 3 * chunkcount + 1 descriptors;
   for 512 bytes chunks and 4K pages this is 3 * 8 + 1 = 25 descriptors! */
#define MXS_NAND_DMA_DESCRIPTOR_COUNT	25

/* When loading ECC data in mxs_nand_do_read_oob(), we need NAND_CMD_READ0 +
   column + row + NAND_CMD_READSTART and one entry with NAND_CMD_RNDOUT +
   column + NAND_CMD_RNDOUTSTART for each chunk. For 4K pages and 512 bytes
   chunks this may be up to 7 + 4*8 = 39 bytes in the command buffer. So use
   two cache lines to be sure. */
#define MXS_NAND_COMMAND_BUFFER_SIZE	64

/*
 * Timeout values. In our case they must enclose the transmission time for the
 * command, address and data byte cycles. The timer resolution in U-Boot is
 * 1/1000s, i.e. CONFIG_SYS_HZ=1000 and any final value < 2 will not work well.
 */
#define MXS_NAND_TIMEOUT_RESET	(CONFIG_SYS_HZ * 10) / 1000
#define MXS_NAND_TIMEOUT_DATA	(CONFIG_SYS_HZ * 100) / 1000
#define MXS_NAND_TIMEOUT_WRITE	(CONFIG_SYS_HZ * 100) / 1000
#define MXS_NAND_TIMEOUT_ERASE	(CONFIG_SYS_HZ * 400) / 1000

/* Timout value for mxs_wait_mask_set(), in us */
#define MXS_NAND_BCH_TIMEOUT	10000

#if defined(CONFIG_MX6)
#define MXS_NAND_CHUNK_DATA_CHUNK_SIZE_SHIFT	2
#else
#define MXS_NAND_CHUNK_DATA_CHUNK_SIZE_SHIFT	0
#endif

struct mxs_nand_priv {
	struct nand_chip chip;		/* Generic NAND chip info */
	int cur_chip;			/* Current NAND chip number (0..3) */
	uint32_t timing0;		/* Value for TIMING0 register */
	uint32_t gf;			/* 13 or 14, depending on chunk size */
	uint32_t bch_layout;		/* Number of BCH layout */
	uint32_t desc_index;		/* Current DMA descriptor index */
	uint32_t cmd_queue_len;		/* Current command queue length */
	uint8_t column_cycles;		/* Number of column cycles */
	uint8_t row_cycles;		/* Number of row cycles */
};

/*
 * F&S NAND page layout for i.MX6. The layout is heavily influenced by data
 * flow requirements of the BCH error correction engine.
 *
 * The OOB area is put first in the page. Then the main page data follows in
 * chunks of 512 or 1024 bytes, interleaved with ECC. For 1024 bytes chunk
 * size, GF14 is needed, which means 14 bits of ECC data per ECC step. For 512
 * bytes chunk size, GF13 is sufficient, which means 13 bits per ECC step. If
 * MXS_NAND_CHUNK_1K is set in the platform flags, the driver uses 1024 bytes
 * chunks, otherwise 512 bytes chunks.
 *
 * The bad block marker will also be located at byte 0 of the (main) page, NOT
 * on the first byte of the spare area anymore! We need at least 4 bytes of
 * OOB data for the bad block marker and the backup block number.
 *
 * |                  NAND flash main area                      | Spare area |
 * +-----+----------+--------+-------+--------+-------+-----+--------+-------+
 * | BBM | User OOB | Main 0 | ECC 0 | Main 1 | ECC 1 | ... | Main n | ECC n |
 * +-----+----------+--------+-------+--------+-------+-----+--------+-------+
 *    4    oobavail  512/1024         512/1024               512/1024
 *
 * The ECC size depends on the ECC strength and the chunk size. It is not
 * necessarily a multiple of 8 bits, which means that subsequent sections may
 * not be byte aligned anymore! But as long as we use NAND flashes with at
 * least 2K page sizes, the final ECC section will always end byte aligned, no
 * matter how odd the intermediate section crossings may be. This is even true
 * for GF13.
 *
 * Example 1
 * ---------
 * NAND page size is 2048 + 64 = 2112 bytes. Chunk size is set to 512 bytes
 * which means we can use GF13 and have four chunks. NBoot reports ECC2, which
 * means 13 bits * 2 = 26 bits ECC data (3.25 bytes). So the main data will
 * take 4x (512 + 3.25) = 2061 bytes. So we will have 2112 - 2061 = 51 bytes
 * free space for OOB, from which we need 4 bytes for internal purposes. The
 * remaining free space is 47 bytes for the User OOB data (mtd->oobavail).
 *
 * This results in the following rather complicated layout. Please note that
 * the ECC data is not byte aligned and therefore all subsequent sections will
 * also not be aligned to byte boundaries. However the final ECC section will
 * end byte aligned again.
 *
 *                       Raw Page Data
 *                      _______________
 *                     |               | 4 Bytes
 *                     | BBM           |
 *                     |_______________|
 *                     |               | 47 Bytes (oobavail)
 *                     | User-OOB      |
 *                     |_______________|
 *                     |               | 512 Bytes
 *                     | Main 0        |
 *                     |_______________|
 *                     |               | 26 Bits
 *                     | ECC 0         |
 *                     |___________    |
 *                     |           |___|
 *                     | Main 1        | 512 Bytes
 *                     |___________    |
 *                     |           |___|
 *                     | ECC 1         | 26 Bits
 *                     |_______        |
 *                     |       |_______|
 *                     |               | 512 Bytes
 *                     |_______ Main 2 |
 *                     |       |_______|
 *                     |               | 26 Bits
 *                     |___      ECC 2 |
 *                     |   |___________|
 *                     |               | 512 Bytes
 *                     |___     Main 3 |
 *                     |   |___________|
 *                     |               | 26 Bits
 *                     |         ECC 3 |
 *                     |_______________|
 *                      7 6 5 4 3 2 1 0
 *                            Bit        2112 Bytes total
 *
 * Example 2
 * ---------
 * NAND page size is 2048 + 64 = 2112 bytes. Chunk size is set to 1024 bytes
 * which means we must use GF14 and have two chunks. NBoot reports ECC8, which
 * means 14 bits * 8 = 112 bits ECC data (14 bytes). So the main data will
 * take 2x (1024 + 14) = 2076 bytes. So we will have 2112 - 2076 = 36 bytes
 * free space for OOB, from which we need 4 bytes for internal purposes. The
 * remaining free space is 32 bytes for the User OOB data (mtd->oobavail).
 *
 * This results in the following, significantly simpler layout where all
 * sections are byte aligned. This is the default layout for F&S usages.
 *
 *                       Raw Page Data
 *                      _______________
 *                     |               | 4 Bytes
 *                     | BBM           |
 *                     |_______________|
 *                     |               | 32 Bytes (oobavail)
 *                     | User-OOB      |
 *                     |_______________|
 *                     |               | 1024 Bytes
 *                     | Main 0        |
 *                     |_______________|
 *                     |               | 14 Bytes
 *                     | ECC 0         |
 *                     |_______________|
 *                     |               | 1024 Bytes
 *                     | Main 1        |
 *                     |_______________|
 *                     |               | 14 Bytes
 *                     | ECC 1         |
 *                     |_______________|
 *                      7 6 5 4 3 2 1 0
 *                            Bit        2112 Bytes total
 */

static struct nand_ecclayout fus_nfc_ecclayout;

static ulong nfc_base_addresses[] = {
	CONFIG_SYS_NAND_BASE
};

static struct mxs_nand_priv nfc_infos[CONFIG_SYS_MAX_NAND_DEVICE];

static uint8_t cmd_buf[MXS_NAND_COMMAND_BUFFER_SIZE]
				__attribute__((aligned(MXS_DMA_ALIGNMENT)));

static struct mxs_dma_desc dma_desc[MXS_NAND_DMA_DESCRIPTOR_COUNT]
				__attribute__((aligned(MXS_DMA_ALIGNMENT)));

static uint8_t data_buf[NAND_MAX_PAGESIZE + NAND_MAX_OOBSIZE]
				__attribute__((aligned(MXS_DMA_ALIGNMENT)));

/* -------------------- CACHE MANAGEMENT FUNCTIONS ------------------------- */

/*
 * Invalidate cache for a buffer that was written by DMA so that further reads
 * will return the new data from the buffer, not the old cache data.
 */
static void mxs_nand_inval_buf(void *buf, uint32_t size)
{
#ifndef CONFIG_SYS_DCACHE_OFF
	unsigned long addr = (unsigned long)(buf);
	uint32_t offs;

	/*
	 * Align beginning and end to full cache lines. Please note that this
	 * will only work inside our own buffers like data_buf[] and cmd_buf[]
	 * where we know when we can discard data. If the new size would
	 * extend to other data, we would probably throw away cached values
	 * that are not yet stored in RAM.
	 */
	offs = addr & (MXS_DMA_ALIGNMENT - 1);
	addr -= offs;
	size += offs;
	size = ALIGN(size, MXS_DMA_ALIGNMENT);
	invalidate_dcache_range(addr, addr + size);
#endif
}

/*
 * Flush cache for a buffer so that a later DMA access will not miss any data.
 */
static void mxs_nand_flush_buf(void *buf, uint32_t size)
{
#ifndef CONFIG_SYS_DCACHE_OFF
	unsigned long addr = (unsigned long)buf;
	uint32_t offs;

	/*
	 * Align beginning and end to full cache lines. Flushing cache will
	 * always work, even if the new size does extend beyond our own
	 * buffers. We probably only store some extra data bytes to RAM
	 * earlier than necessary.
	 */
	offs = addr & (MXS_DMA_ALIGNMENT - 1);
	addr -= offs;
	size += offs;
	size = ALIGN(size, MXS_DMA_ALIGNMENT);
	flush_dcache_range(addr, addr + size);
#endif
}

/* -------------------- DMA ------------------------------------------------ */

/*
 * Get the next free DMA descriptor
 */
static struct mxs_dma_desc *mxs_nand_get_dma_desc(struct mxs_nand_priv *priv)
{
	if (priv->desc_index >= MXS_NAND_DMA_DESCRIPTOR_COUNT) {
		printf("MXS NAND: Too many DMA descriptors requested: %u\n",
		       priv->desc_index);
		return NULL;
	}

	return &dma_desc[priv->desc_index++];
}

/*
 * Free all DMA descriptors that were used in the previous DMA chain
 */
static void mxs_nand_init_dma_descs(struct mxs_nand_priv *priv)
{
	int i;
	struct mxs_dma_desc *desc;

	for (i = 0; i < MXS_NAND_DMA_DESCRIPTOR_COUNT; i++) {
		desc = &dma_desc[i];
		memset(desc, 0, sizeof(struct mxs_dma_desc));
		desc->address = (dma_addr_t)desc;
	}

	priv->desc_index = 0;
}

/*
 * Append a DMA descriptor to the DMA chain; we do not take care about the
 * MXS_DMA_DESC_CHAIN bit, it is added automatically to the previous
 * descriptor in mxs_dma_desc_append() when another one is appended.
 */
static void mxs_nand_dma_desc_append(struct mxs_nand_priv *priv,
				     struct mxs_dma_desc *d)
{
	uint32_t channel = MXS_DMA_CHANNEL_AHB_APBH_GPMI0 + priv->cur_chip;
#ifdef DEBUG
	unsigned int count, i;
	unsigned long data = d->cmd.data;
	unsigned long cmd = data & MXS_DMA_DESC_COMMAND_MASK;
	char *cmd_info[] = {"NO_DMAXFER", "DMA_WRITE", "DMA_READ", "DMA_SENSE"};
	uint8_t *address = (uint8_t *)d->cmd.address;


	if (showdesc) {
		printf("### --- DMA[%i] --- \n", priv->desc_index-1);
		printf("### CMD %08lx: %s%s%s%s%s%s%s%s\n",
		       data, cmd_info[cmd],
		       (data & MXS_DMA_DESC_CHAIN) ? " CHAIN" : "",
		       (data & MXS_DMA_DESC_IRQ) ? " IRQ" : "",
		       (data & MXS_DMA_DESC_NAND_LOCK) ? " NAND_LOCK" : "",
		       (data & MXS_DMA_DESC_NAND_WAIT_4_READY) ? " WAIT4READY" : "",
		       (data & MXS_DMA_DESC_DEC_SEM) ? " DEC_SEM" : "",
		       (data & MXS_DMA_DESC_WAIT4END) ? " WAIT4END" : "",
		       (data & MXS_DMA_DESC_HALT_ON_TERMINATE) ? " HOT" : "");

		count = data & MXS_DMA_DESC_PIO_WORDS_MASK;
		count >>= MXS_DMA_DESC_PIO_WORDS_OFFSET;
		if (count) {
			printf("### %u PIO WORDS:", count);
			for (i = 0; i < count; i++)
				printf(" %08lx", d->cmd.pio_words[i]);
			printf("\n");
		}

		count = d->cmd.data & MXS_DMA_DESC_BYTES_MASK;
		count >>= MXS_DMA_DESC_BYTES_OFFSET;
		if (count && (cmd == MXS_DMA_DESC_COMMAND_DMA_READ)) {
			printf("### %u BYTES @ %p:", count, address);
			for (i = 0; i < count; i++) {
				printf(" %02X", address[i]);
				if (i == 20) {
					/* Output would exceed line length */
					printf(" ...");
					break;
				}
			}
			printf("\n");
		}
	}
#endif

	/* Append the DMA descriptor to the current list */
	mxs_dma_desc_append(channel, d);
}

/*
 * Add a DMA descriptor for a command byte with optional arguments (row,
 * column or a one byte argument)
 */
static void mxs_nand_add_cmd_desc(struct mxs_nand_priv *priv, uint command,
				  int column, int row, int param)
{
	uint32_t this_cmd_len = 0;
	uint8_t *buf = cmd_buf + priv->cmd_queue_len;
	struct mxs_dma_desc *d;
	int temp;
	unsigned i;

	/* Prepare the data to send in cmd_buf[] */
	buf[this_cmd_len++] = command;
	if (column != -1) {
		temp = column;
		for (i = 0; i < priv->column_cycles; i++) {
			buf[this_cmd_len++] = temp & 0xFF;
			temp >>= 8;
		}
	}
	if (row != -1) {
		temp = row;
		for (i = 0; i < priv->row_cycles; i++) {
			buf[this_cmd_len++] = temp & 0xFF;
			temp >>= 8;
		}
	}
	if (param != -1)
		buf[this_cmd_len++] = param & 0xFF;

	priv->cmd_queue_len += this_cmd_len;

	/* Add a DMA descriptor that sends the command byte (and address) */
	d = mxs_nand_get_dma_desc(priv);
	d->cmd.data =
		MXS_DMA_DESC_COMMAND_DMA_READ |
		MXS_DMA_DESC_NAND_LOCK |
		MXS_DMA_DESC_WAIT4END |
		(3 << MXS_DMA_DESC_PIO_WORDS_OFFSET) |
		(this_cmd_len << MXS_DMA_DESC_BYTES_OFFSET);

	d->cmd.address = (dma_addr_t)buf;

	d->cmd.pio_words[0] =
		GPMI_CTRL0_COMMAND_MODE_WRITE |
		GPMI_CTRL0_LOCK_CS |
		GPMI_CTRL0_WORD_LENGTH |
		(priv->cur_chip << GPMI_CTRL0_CS_OFFSET) |
		GPMI_CTRL0_ADDRESS_NAND_CLE |
		this_cmd_len;
	if ((column != -1) || (row != -1) || (param != -1))
		d->cmd.pio_words[0] |= GPMI_CTRL0_ADDRESS_INCREMENT;

	d->cmd.pio_words[1] = 0;
	d->cmd.pio_words[2] = 0;

	mxs_nand_dma_desc_append(priv, d);
}

/*
 * Run the DMA chain and wait for completion
 */
static int mxs_nand_dma_go(struct mxs_nand_priv *priv)
{
	int ret;
	uint32_t channel = MXS_DMA_CHANNEL_AHB_APBH_GPMI0 + priv->cur_chip;

	/* Flush cache for command bytes (if there are any) */
	if (priv->cmd_queue_len) {
		mxs_nand_flush_buf(cmd_buf, priv->cmd_queue_len);
		priv->cmd_queue_len = 0;
	}

	/* Execute the DMA chain */
	ret = mxs_dma_go(channel);
	if (ret)
		printf("Error %d running DMA chain\n", ret);

	/* Reset DMA descriptor count */
	priv->desc_index = 0;

	return ret;
}

/* -------------------- LOCAL HELPER FUNCTIONS ----------------------------- */

/*
 * Compute the default ECC strength if no information is available from NBoot
 */
static inline uint32_t mxs_nand_get_ecc_strength(struct mtd_info *mtd,
						 uint32_t chunk_shift,
						 uint32_t gf)
{
	uint32_t oobecc;
	uint32_t eccsteps;

	/* Get number of 1024 bytes chunks */
	eccsteps = mtd->writesize >> chunk_shift;

	/* Get available space for ECC, in bits */
	oobecc = mtd->oobsize - MXS_NAND_METADATA_SIZE - 4;
	oobecc <<= 3;

	/* We need an even number for the ECC strength, therefore divide by
	   twice the value and multiply by two again */
	oobecc /= gf * 2 * eccsteps;
	oobecc <<= 1;

	return oobecc;
}

/*
 * Wait until chip is ready.
 */
static int mxs_nand_wait_ready(struct mtd_info *mtd, unsigned long timeout)
{
	struct nand_chip *chip = mtd->priv;
	struct mxs_nand_priv *priv = chip->priv;
	struct mxs_gpmi_regs *gpmi_regs =
		(struct mxs_gpmi_regs *)MXS_GPMI_BASE;
	struct mxs_dma_desc *d;
	uint32_t tmp;
	uint32_t channel;
	int ret;

	/* ### TODO: Convert timeout value to GPMI ticks and set in
	   gpmi_regs->hw_gpmi_timing1 */

	/* Add DMA descriptor to wait for ready */
	d = mxs_nand_get_dma_desc(priv);
	d->cmd.data =
		MXS_DMA_DESC_COMMAND_NO_DMAXFER |
		MXS_DMA_DESC_NAND_WAIT_4_READY |
		MXS_DMA_DESC_WAIT4END |
		MXS_DMA_DESC_IRQ |
		MXS_DMA_DESC_DEC_SEM |
		(1 << MXS_DMA_DESC_PIO_WORDS_OFFSET);

	d->cmd.address = 0;

	d->cmd.pio_words[0] =
		GPMI_CTRL0_COMMAND_MODE_WAIT_FOR_READY |
		GPMI_CTRL0_WORD_LENGTH |
		(priv->cur_chip << GPMI_CTRL0_CS_OFFSET) |
		GPMI_CTRL0_ADDRESS_NAND_DATA;

	mxs_nand_dma_desc_append(priv, d);

	/* Flush cache for command bytes and execute the DMA chain */
	ret = mxs_nand_dma_go(priv);
	if (ret) {
		printf("DMA error/timeout waiting for Ready\n");
		return ret;
	}

	/* Check for NAND timeout */
	tmp = readl(&gpmi_regs->hw_gpmi_stat);
	channel = MXS_DMA_CHANNEL_AHB_APBH_GPMI0 + priv->cur_chip;
	if (tmp & (1 << (channel + GPMI_STAT_RDY_TIMEOUT_OFFSET))) {
		printf("NAND timeout waiting for Ready\n");
		return -ETIMEDOUT;
	}

	return 0;
}

/*
 * Add DMA descriptor to read data to data_buf[]
 */
static void mxs_nand_read_data_buf(struct mtd_info *mtd, uint32_t offs,
				   uint32_t length, int last)
{
	struct nand_chip *chip = mtd->priv;
	struct mxs_nand_priv *priv = chip->priv;
	struct mxs_dma_desc *d;
	uint32_t data;

	/* Add DMA descriptor to read data */
	d = mxs_nand_get_dma_desc(priv);
	data = MXS_DMA_DESC_COMMAND_DMA_WRITE |
		MXS_DMA_DESC_WAIT4END |
		(1 << MXS_DMA_DESC_PIO_WORDS_OFFSET) |
		(length << MXS_DMA_DESC_BYTES_OFFSET);
	if (last)
		data |= MXS_DMA_DESC_IRQ | MXS_DMA_DESC_DEC_SEM;

	d->cmd.data = data;
		
	d->cmd.address = (dma_addr_t)(data_buf + offs);

	d->cmd.pio_words[0] =
		GPMI_CTRL0_COMMAND_MODE_READ |
		GPMI_CTRL0_WORD_LENGTH |
		(priv->cur_chip << GPMI_CTRL0_CS_OFFSET) |
		GPMI_CTRL0_ADDRESS_NAND_DATA |
		length;

	mxs_nand_dma_desc_append(priv, d);

	/* Invalidate cache for data_buf */
	mxs_nand_inval_buf(data_buf + offs, length);
}

/*
 * Add DMA descriptor to write data from data_buf[] to flash. This does *not*
 * execute the DMA chain right away, it will be executed later when the
 * command waits for ready.
 */
static void mxs_nand_write_data_buf(struct mtd_info *mtd, uint32_t offs,
				    uint32_t length)
{
	struct nand_chip *chip = mtd->priv;
	struct mxs_nand_priv *priv = chip->priv;
	struct mxs_dma_desc *d;

	/* Add DMA descriptor to write data */
	d = mxs_nand_get_dma_desc(priv);
	d->cmd.data =
		MXS_DMA_DESC_COMMAND_DMA_READ |
		MXS_DMA_DESC_NAND_LOCK |
		MXS_DMA_DESC_WAIT4END |
		(1 << MXS_DMA_DESC_PIO_WORDS_OFFSET) |
		(length << MXS_DMA_DESC_BYTES_OFFSET);

	d->cmd.address = (dma_addr_t)(data_buf + offs);

	d->cmd.pio_words[0] =
		GPMI_CTRL0_COMMAND_MODE_WRITE |
		GPMI_CTRL0_LOCK_CS |
		GPMI_CTRL0_WORD_LENGTH |
		(priv->cur_chip << GPMI_CTRL0_CS_OFFSET) |
		GPMI_CTRL0_ADDRESS_NAND_DATA |
		length;

	mxs_nand_dma_desc_append(priv, d);

	/* Flush cache for write data */
	mxs_nand_flush_buf(data_buf + offs, length);
}

/*
 * Copy the main data from the raw page data in data_buf[] to buf[]. The main
 * data is interleaved with the ECC data and ECC data is not necessarily
 * aligned to bytes. So for some chunks we might need to shift bits.
 *
 * The following example shows a rather complicated case on a NAND flash with
 * 2048+64 = 2112 bytes pages. The layout uses ECC2 on 512 bytes chunks (i.e.
 * 13 bits ECC data needed per ECC step = 26 bits). This requires four data
 * chunks, and we have 51 free bytes for the Bad Block Marker and User OOB.
 * The markings show the section boundaries and the number of bits in each
 * section at the transitions.
 *
 *   Raw Page Data   data_buf[]
 *  _______________
 * |               | 4 Bytes
 * | BBM           |
 * |_______________|
 * |               | 47 Bytes (oobavail)
 * | User-OOB      |
 * |_______________|
 * |               | 512 Bytes
 * | Main 0        |<-----------+
 * |_______________|            |             Main Data     buf[]
 * |               | 26 Bits    |          _______________
 * | ECC 0         |            |     1:1 |               | 512 Bytes
 * |___________    |            +-------->| Main 0        |
 * |           |___|                      |_______________|
 * | Main 1        | 512 Bytes      shift |               | 512 Bytes
 * |___________    |--------------------->| Main 1        |
 * |           |___|                      |_______________|
 * | ECC 1         | 26 Bits        shift |               | 512 Bytes
 * |_______        |            +-------->| Main 2        |
 * |       |_______|            |         |_______________|
 * |               | 512 Bytes  |   shift |               | 512 Bytes
 * |_______ Main 2 |<-----------+    +--->| Main 3        |
 * |       |_______|                 |    |_______________|
 * |               | 26 Bits         |     7 6 5 4 3 2 1 0
 * |___      ECC 2 |                 |           Bit        2048 Bytes total
 * |   |___________|                 |
 * |               | 512 Bytes       |
 * |___     Main 3 |<----------------+
 * |   |___________|
 * |               | 26 Bits
 * |         ECC 3 |
 * |_______________|
 *  7 6 5 4 3 2 1 0
 *        Bit        2112 Bytes total
 *
 * The standard F&S layout with 1024 bytes chunks and ECC8 is far less
 * complicated as the ECC data will always match byte boundaries. However we
 * want the code to work in all possible cases.
 */
static void mxs_nand_read_main_data(struct mtd_info *mtd, uint8_t *buf)
{
	struct nand_chip *chip = mtd->priv;
	struct mxs_nand_priv *priv = chip->priv;
	uint val;
	uint bit = 0;
	int chunk, remaining;
	int chunk_size = chip->ecc.size;
	uint8_t *raw_data = data_buf + mtd->oobavail + 4;

	for (chunk = 0; chunk < chip->ecc.steps; chunk++) {
		if (bit) {
			/* Copy split bytes with shifting */
			val = *raw_data >> bit;
			remaining = chunk_size;
			do {
				val |= *(++raw_data) << (8 - bit);
				*buf++ = (uint8_t)val;
				val >>= 8;
			} while (--remaining);
		} else {
			/* Copy whole bytes */
			memcpy(buf, raw_data, chunk_size);
			buf += chunk_size;
			raw_data += chunk_size;
		}
		bit += priv->gf * mtd->ecc_strength;
		raw_data += bit >> 3;
		bit &= 7;
	}
}

/*
 * Copy the main data from buf[] to the raw page data in data_buf[]. The main
 * data is interleaved with the ECC data and ECC data is not necessarily
 * aligned to bytes. So for some chunks we might need to shift bits.
 *
 * See mxs_nand_read_main_data() for a detailed explanation and an example.
 */
static void mxs_nand_write_main_data(struct mtd_info *mtd, const uint8_t *buf)
{
	struct nand_chip *chip = mtd->priv;
	struct mxs_nand_priv *priv = chip->priv;
	uint val, tmp, mask;
	uint bit = 0;
	int chunk, remaining;
	int chunk_size = chip->ecc.size;
	uint8_t *raw_data = data_buf + mtd->oobavail + 4;

	for (chunk = 0; chunk < chip->ecc.steps; chunk++) {
		if (bit) {
			/* Copy split bytes with shifting */
			mask = (1 << bit) - 1;
			val = *raw_data & mask;
			remaining = chunk_size;
			do {
				tmp = *buf++;
				val |= tmp << bit;
				*raw_data++ = (uint8_t)val;
				val = tmp >> (8 - bit);
			} while (--remaining);
			val |= *raw_data & ~mask;
			*raw_data = (uint8_t)val;
		} else {
			/* Copy whole bytes */
			memcpy(raw_data, buf, chunk_size);
			buf += chunk_size;
			raw_data += chunk_size;
		}
		bit += priv->gf * mtd->ecc_strength;
		raw_data += bit >> 3;
		bit &= 7;
	}
}

/*
 * Copy the ECC data from raw page data in data_buf[] to oob[]. The BBM and
 * user part of the OOB area are not touched!
 *
 * The complicated part is the ECC data. It is interleaved with the main data
 * and ECC data is not necessarily aligned to bytes. But because the main data
 * chunks always consist of full bytes, the ECC parts will always fit together
 * without the need for bit shifting.
 *
 * The following example shows a layout with ECC2 on 512 bytes chunks (13 bits
 * ECC data per ECC step = 26 bits). This requires four data chunks on a NAND
 * flash with 2048+64 bytes pages, and we have 51 free bytes for the Bad Block
 * Marker and User OOB. The markings show the section boundaries and the
 * number of bits in each section at the transitions.
 *
 *   Raw Page Data   data_buf[]
 *  _______________
 * |               | 4 Bytes
 * | BBM           |
 * |_______________|
 * |               | 47 Bytes (oobavail)
 * | User-OOB      |
 * |_______________|                          OOB Data      oob[]
 * |               | 512 Bytes             _______________
 * | Main 0        |                      |               | 4 Bytes
 * |_______________|                      | BBM           |
 * |               | 26 Bits              |_______________|
 * | ECC 0         |<-------------+       |               | 47 Bytes (oobavail)
 * |___________    |              |       | User-OOB      |
 * |           |___|              |       |_______________|
 * | Main 1        | 512 Bytes    |       |               | 26 Bits
 * |___________    |              +------>| ECC 0         |
 * |           |___|                      |___________    |
 * | ECC 1         | 26 Bits              |           |___|
 * |_______        |<-------------------->| ECC 1         | 26 Bits
 * |       |_______|                      |_______        |
 * |               | 512 Bytes            |       |_______|
 * |_______ Main 2 |              +------>|               | 26 Bits
 * |       |_______|              |       |___      ECC 2 |
 * |               | 26 Bits      |       |   |___________|
 * |___      ECC 2 |<-------------+       |               | 26 Bits
 * |   |___________|                 +--->|         ECC 3 |
 * |               | 512 Bytes       |    |_______________|
 * |___     Main 3 |                 |     7 6 5 4 3 2 1 0
 * |   |___________|                 |           Bit        64 Bytes total
 * |               | 26 Bits         |
 * |         ECC 3 |<----------------+
 * |_______________|
 *  7 6 5 4 3 2 1 0
 *        Bit        2112 Bytes total
 *
 * The standard F&S layout with 1024 bytes chunks and ECC8 is far less
 * complicated as the ECC data will always match byte boundaries. However we
 * want the code to work in all possible cases.
 */
static void mxs_nand_read_ecc_data(struct mtd_info *mtd, uint8_t *oob)
{
	struct nand_chip *chip = mtd->priv;
	struct mxs_nand_priv *priv = chip->priv;
	int chunk, remaining = 0;
	int chunk_size = chip->ecc.size;
	uint8_t val, mask;
	uint headersize = mtd->oobavail + 4;
	uint8_t *raw_data = data_buf;

	/* Adjust pointers to first ECC section */
	oob += headersize;
	raw_data += headersize + chunk_size;

	/* Copy ECC sections of all chunks */
	for (chunk = 0; chunk < chip->ecc.steps; chunk++) {
		remaining += priv->gf * mtd->ecc_strength;
		while (remaining >= 8) {
			/* Copy whole bytes; the sections are only a few bytes
			   long, so don't bother to use memcpy() */
			*oob++ = *raw_data++;
			remaining -= 8;
		}
		if (remaining) {
			/* The transition to the next section of ECC bytes is
			   within a byte. Get part of the byte from the
			   previous section and part of the byte from the next
			   section. These will always fit together without
			   shifting, we just need to mask the non-ECC bits. */
			mask = (1 << remaining) - 1;
			val = *raw_data & mask;
			raw_data += chunk_size;
			val |= *raw_data++ & ~mask;
			*oob++ = val;
			remaining -= 8;
			/* Now remaining is negative because we have borrowed
			   a few bits from the next chunk already. The next
			   chunk always has more than 8 bits of ECC data, so
			   we do not have to care about special cases.*/
		} else
			raw_data += chunk_size;
	}
}

/*
 * Copy the ECC data from oob[] to raw page data in data_buf[]. The BBM and
 * user part of the OOB area are not touched!
 *
 * The complicated part is the ECC data. It is interleaved with the main data
 * and ECC data is not necessarily aligned to bytes. But because the main data
 * chunks always consist of full bytes, the ECC parts will always fit together
 * without the need for bit shifting.
 *
 * See mxs_nand_read_ecc_data() for a detailed explanation and an example.
 */
static void mxs_nand_write_ecc_data(struct mtd_info *mtd, const uint8_t *oob)
{
	struct nand_chip *chip = mtd->priv;
	struct mxs_nand_priv *priv = chip->priv;
	int chunk, remaining = 0;
	int chunk_size = chip->ecc.size;
	uint8_t val, mask;
	uint headersize = mtd->oobavail + 4;
	uint8_t *raw_data = data_buf;

	/* Adjust pointers to first ECC section */
	oob += headersize;
	raw_data += headersize + chunk_size;

	/* Copy ECC sections of all chunks */
	for (chunk = 0; chunk < chip->ecc.steps; chunk++) {
		remaining += priv->gf * mtd->ecc_strength;
		while (remaining >= 8) {
			/* Copy whole bytes; the sections are only a few bytes
			   long, so don't bother to use memcpy() */
			*raw_data++ = *oob++;
			remaining -= 8;
		}
		if (remaining) {
			/* The transition to the next section of ECC bytes is
			   within a byte. This goes without shifting, we just
			   need to split the byte in a part that goes to the
			   previous section and a part that goes to the next
			   section. Remark: we fill unused bits with 1 to
			   make it possible to store the OOB only, where we
			   only write the ECC and not the main data. 1-bits
			   should keep the old content. */
			val = *oob++;
			mask = (1 << remaining) - 1;
			*raw_data = ~mask | (val & mask);
			raw_data += chunk_size;
			*raw_data++ = mask | (val & ~mask);
			remaining -= 8;
			/* Now remaining is negative because we have borrowed
			   a few bits from the next chunk already. The next
			   chunk always has more than 8 bits of ECC data, so
			   we do not have to care about special cases.*/
		} else
			raw_data += chunk_size;
	}
}

/*
 * Read OOB and create virtual OOB area. If column is set, skip a part at the
 * beginning.
 *
 * See mxs_nand_read_ecc_data() for a description of the OOB area layout.
 */
static int mxs_nand_do_read_oob(struct mtd_info *mtd, struct nand_chip *chip,
				int page, int raw)
{
	struct mxs_nand_priv *priv = chip->priv;
	uint32_t boffs;
	int column = raw ? 0 : 4;
	int length = mtd->oobavail + 4 - column;
	uint32_t from, to;
	int i, ret;

	/* Issue read command for the page */
	chip->cmdfunc(mtd, NAND_CMD_READ0, column, page);
	chip->cmdfunc(mtd, NAND_CMD_READSTART, -1, -1);
	ret = mxs_nand_wait_ready(mtd, MXS_NAND_TIMEOUT_DATA);
	if (ret)
		return ret;

	/* Read the BBM (optional) and the user part of the OOB area */
	mxs_nand_read_data_buf(mtd, 0, length, 0);
	if (raw) {
		/* Now read all the ECC bytes, section by section. The data is
		   still in the NAND page buffer, so we can use RNDOUT */
		boffs = (mtd->oobavail + 4) << 3;
		i = chip->ecc.steps;
		do {
			/* Add DMA descriptor to read only a few bytes */
			boffs += chip->ecc.size << 3;
			from = boffs >> 3;
			boffs += priv->gf * mtd->ecc_strength;
			to = (boffs - 1) >> 3;
			chip->cmdfunc(mtd, NAND_CMD_RNDOUT, from, -1);
			chip->cmdfunc(mtd, NAND_CMD_RNDOUTSTART, -1, -1);
			mxs_nand_read_data_buf(mtd, from, to - from + 1, !--i);
		} while (i);
	}

	/* Run the DMA chain that reads all in one go */
	ret = mxs_nand_dma_go(priv);
	if (ret)
		return ret;

	/* Read BBM (optional) and user part of the OOB area */
	memcpy(chip->oob_poi + column, data_buf, length);

	if (raw) {
		/* Compile the ECC data at the end of the OOB area */
		mxs_nand_read_ecc_data(mtd, chip->oob_poi);
	}

	return 0;
}

/*
 * Write the virtual OOB area to NAND flash.
 *
 * See mxs_nand_read_ecc_data() for a description of the OOB area layout.
 */
static int mxs_nand_do_write_oob(struct mtd_info *mtd, struct nand_chip *chip,
				 int page, int raw)
{
	struct mxs_nand_priv *priv = chip->priv;
	uint32_t boffs;
	int column = raw ? 0 : 4;
	int length = mtd->oobavail + 4 - column;
	uint32_t from, to;
	int i;

	/* Write the BBM (optional) and the user part of the OOB area */
	chip->cmdfunc(mtd, NAND_CMD_SEQIN, column, page);
	memcpy(data_buf, chip->oob_poi + column, length);
	mxs_nand_write_data_buf(mtd, 0, length);

	if (raw) {
		/* Store ECC data in data_buf at the appropriate positions */
		mxs_nand_write_ecc_data(mtd, chip->oob_poi);

		/* Write all the ECC bytes to the page, section by section */
		boffs = (mtd->oobavail + 4) << 3;
		i = chip->ecc.steps;
		do {
			/* We actually only write a few bytes */
			boffs += chip->ecc.size << 3;
			from = boffs >> 3;
			boffs += priv->gf * mtd->ecc_strength;
			to = (boffs - 1) >> 3;
			chip->cmdfunc(mtd, NAND_CMD_RNDIN, from, -1);
			mxs_nand_write_data_buf(mtd, from, to - from + 1);
		} while (--i);
	}

	/* Now actually do the programming */
	chip->cmdfunc(mtd, NAND_CMD_PAGEPROG, -1, -1);

	/* Check if it worked */
	if (chip->waitfunc(mtd, chip) & NAND_STATUS_FAIL)
		return -EIO;

	return 0;
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

/* -------------------- INTERFACE FUNCTIONS -------------------------------- */

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
 * Test if the NAND flash is ready.
 */
static int mxs_nand_device_ready(struct mtd_info *mtd)
{
	/* We have called mxs_nand_wait_ready() before, so we can be sure that
	   our command is already completed. So just return "done". */
	return 1;
}

/*
 * Read a single byte from NAND.
 */
static uint8_t mxs_nand_read_byte(struct mtd_info *mtd)
{
	struct nand_chip *chip = mtd->priv;
	struct mxs_nand_priv *priv = chip->priv;

	mxs_nand_read_data_buf(mtd, 0, 1, 1);
	if (mxs_nand_dma_go(priv))
		return 0xFF;

	return data_buf[0];
}

/*
 * Read a word from NAND.
 */
static u16 mxs_nand_read_word(struct mtd_info *mtd)
{
	struct nand_chip *chip = mtd->priv;
	struct mxs_nand_priv *priv = chip->priv;

	mxs_nand_read_data_buf(mtd, 0, 2, 1);
	if (mxs_nand_dma_go(priv))
		return 0xFFFF;

	return data_buf[0] | (data_buf[1] << 8);
}

/*
 * Read arbitrary data from NAND.
 */
static void mxs_nand_read_buf(struct mtd_info *mtd, uint8_t *buf, int length)
{
	struct nand_chip *chip = mtd->priv;
	struct mxs_nand_priv *priv = chip->priv;

	mxs_nand_read_data_buf(mtd, 0, length, 1);
	if (!mxs_nand_dma_go(priv))
		memcpy(buf, data_buf, length);
}

/*
 * Write arbitrary data to NAND.
 */
static void mxs_nand_write_buf(struct mtd_info *mtd, const uint8_t *buf,
			       int length)
{
	memcpy(data_buf, buf, length);
	mxs_nand_write_data_buf(mtd, 0, length);
}

/*
 * Wait until chip is ready, then read status.
 */
static int mxs_nand_waitfunc(struct mtd_info *mtd, struct nand_chip *chip)
{
	unsigned long timeout;

	if (chip->state == FL_ERASING)
		timeout = MXS_NAND_TIMEOUT_ERASE;
	else
		timeout = MXS_NAND_TIMEOUT_WRITE;

	/* Add DMA descriptor to wait for ready and execute the DMA chain */
	if (mxs_nand_wait_ready(mtd, timeout))
		return NAND_STATUS_FAIL;

	/* Issue NAND_CMD_STATUS command to read status */
	chip->cmdfunc(mtd, NAND_CMD_STATUS, -1, -1);

	/* Read and return status byte */
	return mxs_nand_read_byte(mtd);
}

/* Write command to NAND flash. This usually results in creating a chain of
   DMA descriptors which is executed at the end. */
static void mxs_nand_command(struct mtd_info *mtd, uint command, int column,
			     int page)
{
	struct nand_chip *chip = mtd->priv;
	struct mxs_nand_priv *priv = chip->priv;

	/* Simulate NAND_CMD_READOOB with NAND_CMD_READ0; NAND_CMD_READOOB is
	   only used in nand_block_bad() to read the BBM. So simply use the
	   command that will load the BBM correctly. */
	if (command == NAND_CMD_READOOB)
		command = NAND_CMD_READ0;
	/*
	 * The command sequence should be:
	 *   NAND_CMD_READ0 Col+Row -> NAND_CMD_READSTART -> Ready -> Read data
	 *	  {-> NAND_CMD_RNDOUT col -> NAND_CMD_RNDOUTSTART -> Read data}
	 *   NAND_CMD_SEQIN Col+Row -> Write data
	 *	  {-> NAND_CMD_RNDIN Col -> Write Data}
	 *	  -> NAND_CMD_PAGEPROG -> Ready
	 *   NAND_CMD_ERASE1 Row -> NAND_CMD_ERASE2 -> Ready
	 *   NAND_CMD_STATUS -> Read 1 data byte
	 *   NAND_CMD_READID 0x00/0x20 -> Read ID/Read ONFI ID
	 *   NAND_CMD_PARAM 0x00 -> Ready -> Read ONFI parameters
	 *   NAND_CMD_SET_FEATURES addr -> Write 4 data bytes -> Ready
	 *   NAND_CMD_GET_FEATURES addr -> Ready -> Read 4 data bytes
	 *   NAND_CMD_RESET -> Ready
	 */
	switch (command) {
	case NAND_CMD_RNDIN:
	case NAND_CMD_READ0:
	case NAND_CMD_READSTART:
	case NAND_CMD_RNDOUT:
	case NAND_CMD_SEQIN:
	case NAND_CMD_PAGEPROG:
	case NAND_CMD_ERASE1:
	case NAND_CMD_ERASE2:
		/* Add DMA descriptor with command + column and/or row */
		mxs_nand_add_cmd_desc(priv, command, column, page, -1);
		/* In case of NAND_CMD_PAGEPROG and NAND_CMD_ERASE2,
		   chip->waitfunc() is called later, so we need not wait now.
		   FIXME: In case of NAND_CMD_RNDIN we should have a delay of
		   t_CCS here so that the chip can switch columns */
		break;

	case NAND_CMD_PARAM:
	case NAND_CMD_GET_FEATURES:
		/* Add DMA descriptor with command and one address byte */
		mxs_nand_add_cmd_desc(priv, command, -1, -1, column);
		/* Add DMA descriptor to wait for ready and run DMA chain */
		mxs_nand_wait_ready(mtd, MXS_NAND_TIMEOUT_DATA);
		break;

	case NAND_CMD_RESET:
		/* Add DMA descriptor with command byte */
		mxs_nand_add_cmd_desc(priv, command, -1, -1, -1);
		/* Add DMA descriptor to wait for ready and run DMA chain */
		mxs_nand_wait_ready(mtd, MXS_NAND_TIMEOUT_RESET);
		break;

	case NAND_CMD_RNDOUTSTART:
	case NAND_CMD_STATUS:
		column = -1;
		/* Fall through to case NAND_CMD_READID */
	case NAND_CMD_READID:
	case NAND_CMD_SET_FEATURES:
		/* Add DMA descriptor with command and up to one byte */
		mxs_nand_add_cmd_desc(priv, command, -1, -1, column);
		/* According to the ONFI specification these commands never
		   set the busy signal so we need not wait for ready here.
		   FIXME: But in case of NAND_CMD_RNDOUTSTART we should have a
		   delay of t_CCS here so that the chip can switch columns.
		   FIXME: In case of NAND_CMD_SET_FEATURES we should have a
		   delay of t_ADL here so that the chip can switch mode. */
		break;

	default:
		printf("NAND_CMD 0x%02x not supported\n", command);
		break;
	}
}

/*
 * Read a page from NAND without ECC
 */
static int mxs_nand_read_page_raw(struct mtd_info *mtd, struct nand_chip *chip,
				  uint8_t *buf, int oob_required, int page)
{
	struct mxs_nand_priv *priv = chip->priv;
	int ret;

	/*
	 * A DMA descriptor for the first command byte NAND_CMD_READ0 and the
	 * column/row bytes was already created in nand_do_read_ops(). Now add
	 * a DMA descriptor for the second command byte NAND_CMD_READSTART and
	 * a DMA descriptor to wait for ready. Execute this DMA chain and
	 * check for timeout.
	 */
	chip->cmdfunc(mtd, NAND_CMD_READSTART, -1, -1);
	ret = mxs_nand_wait_ready(mtd, MXS_NAND_TIMEOUT_DATA);
	if (ret)
		return ret;

	/* Add DMA descriptor to read the whole page and execute DMA chain */
	mxs_nand_read_data_buf(mtd, 0, mtd->writesize + mtd->oobsize, 1);
	ret = mxs_nand_dma_go(priv);
	if (ret)
		return ret;

	/* Copy the main data to buf */
	mxs_nand_read_main_data(mtd, buf);

	if (oob_required) {
		/* Copy Bad Block Marker and User OOB section */
		memcpy(chip->oob_poi, data_buf, mtd->oobavail + 4);

		/* Copy the ECC data */
		mxs_nand_read_ecc_data(mtd, chip->oob_poi);
	} else {
		/* Simply clear OOB area */
		memset(chip->oob_poi, 0xFF, mtd->oobsize);
	}

	return 0;
}

/*
 * Write a page to NAND without ECC
 */
static int mxs_nand_write_page_raw(struct mtd_info *mtd, struct nand_chip *chip,
				   const uint8_t *buf, int oob_required)
{
	/* NAND_CMD_SEQIN for column 0 was already issued by the caller */

	/* We must write OOB data as it is interleaved with main data. But if
	   no OOB data is available, use 0xFF instead. This does not modify
	   the existing values. */
	if (!oob_required)
		memset(chip->oob_poi, 0xFF, mtd->oobsize);

	/* Copy the OOB data to data_buf[]. Please note that this has to be
	   done before the main data, because mxs_nand_write_ecc_data() will
	   use 1-bits in unused positions that must be overwritten by the
	   main data later. */
	mxs_nand_write_ecc_data(mtd, chip->oob_poi);

	/* Copy the main data to data_buf[] */
	mxs_nand_write_main_data(mtd, buf);

	/* Copy Bad Block Marker and User OOB section */
	memcpy(data_buf, chip->oob_poi, mtd->oobavail + 4);

	/* Add DMA descriptor to write data_buf to NAND flash */
	mxs_nand_write_data_buf(mtd, 0, mtd->writesize + mtd->oobsize);

	/* The actual programming will take place in mxs_nand_command() when
	   command NAND_CMD_PAGEPROG is sent */
	return 0;
}

/*
 * Read a page from NAND with ECC
 */
static int mxs_nand_read_page(struct mtd_info *mtd, struct nand_chip *chip,
			      uint8_t *buf, int oob_required, int page)
{
	struct mxs_nand_priv *priv = chip->priv;
	struct mxs_dma_desc *d;
	struct mxs_bch_regs *bch_regs = (struct mxs_bch_regs *)MXS_BCH_BASE;
	uint32_t corrected = 0;
	uint8_t *status;
	uint32_t status_offs;
	int i, ret;

#ifdef DEBUG
	/* Show DMA descriptors from now on. */
	showdesc = 1;
#endif

	/* Select the desired flash layout */
	writel(priv->bch_layout, &bch_regs->hw_bch_layoutselect);

	/*
	 * A DMA descriptor for the first command byte NAND_CMD_READ0 and the
	 * column/row bytes was already created in nand_do_read_ops(). Now add
	 * a DMA descriptor for the second command byte NAND_CMD_READSTART and
	 * a DMA descriptor to wait for ready. Execute this DMA chain and
	 * check for timeout.
	 */
	chip->cmdfunc(mtd, NAND_CMD_READSTART, -1, -1);
	ret = mxs_nand_wait_ready(mtd, MXS_NAND_TIMEOUT_DATA);
	if (ret)
		return ret;

	/* Add DMA descriptor to enable the BCH block and read */
	d = mxs_nand_get_dma_desc(priv);
	d->cmd.data =
		MXS_DMA_DESC_COMMAND_NO_DMAXFER |
		MXS_DMA_DESC_NAND_LOCK |
		MXS_DMA_DESC_WAIT4END |
		(6 << MXS_DMA_DESC_PIO_WORDS_OFFSET);

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

	mxs_nand_dma_desc_append(priv, d);

	/* Add DMA descriptor to disable the BCH block (wait-for-ready is a
	   NOP here) */
	d = mxs_nand_get_dma_desc(priv);
	d->cmd.data =
		MXS_DMA_DESC_COMMAND_NO_DMAXFER |
		MXS_DMA_DESC_NAND_LOCK |
		MXS_DMA_DESC_NAND_WAIT_4_READY |
		MXS_DMA_DESC_WAIT4END |
		(3 << MXS_DMA_DESC_PIO_WORDS_OFFSET);

	d->cmd.address = 0;

	d->cmd.pio_words[0] =
		GPMI_CTRL0_COMMAND_MODE_WAIT_FOR_READY |
		GPMI_CTRL0_WORD_LENGTH |
		(priv->cur_chip << GPMI_CTRL0_CS_OFFSET) |
		GPMI_CTRL0_ADDRESS_NAND_DATA;
	d->cmd.pio_words[1] = 0;
	d->cmd.pio_words[2] = 0;

	mxs_nand_dma_desc_append(priv, d);

	/* Add DMA descriptor to deassert the NAND lock and issue interrupt */
	d = mxs_nand_get_dma_desc(priv);
	d->cmd.data =
		MXS_DMA_DESC_COMMAND_NO_DMAXFER | MXS_DMA_DESC_IRQ |
		MXS_DMA_DESC_DEC_SEM;

	d->cmd.address = 0;

	mxs_nand_dma_desc_append(priv, d);

	/* Remove any COMPLETE_IRQ states from previous writes (we only check
	   COMPLETE_IRQ when reading); BCH has a pending state, too, so we may
	   have to clear it twice. We simply clear until it remains zero. */
	while (readl(&bch_regs->hw_bch_ctrl_reg) & BCH_CTRL_COMPLETE_IRQ)
		writel(BCH_CTRL_COMPLETE_IRQ, &bch_regs->hw_bch_ctrl_clr);

	/* Execute the DMA chain */
	ret = mxs_nand_dma_go(priv);
	if (ret) {
		printf("MXS NAND: DMA read error\n");
		return ret;
	}

	ret = mxs_nand_wait_for_bch_complete();
	if (ret) {
		printf("MXS NAND: BCH read timeout\n");
		return ret;
	}

	/*
	 * Loop over status bytes, accumulating ECC status. The status bytes
	 * are located at the first 32 bit boundary behind the auxiliary data.
	 *   0x00:  no errors
	 *   0xFF:  empty (all bytes 0xFF)
	 *   0xFE:  uncorrectable
	 *   other: number of bitflips
	 * Save the maximum number of bitflips of all chunks in variable ret.
	 *
	 * Remark: Please note that from the point of view of the BCH engine
	 * the OOB area at the beginning of the page is a separate chunk. So
	 * it will have its own status byte, even if ECC is set to ECC0. So we
	 * have to check one more status byte than we have main chunks or we
	 * would miss bitflips in the last main chunk. Hence the +1 in the
	 * loop below.
	 */
	status_offs = mtd->writesize + mtd->oobavail + 4;
	status_offs = (status_offs + 3) & ~3;
	status = data_buf + status_offs;

	/* Invalidate cache for data_buf[] */
	mxs_nand_inval_buf(data_buf,
			   mtd->writesize + status_offs + chip->ecc.steps + 1);
	ret = 0;
	for (i = 0; i < chip->ecc.steps + 1; i++) {
		if (status[i] == 0xfe) {
			mtd->ecc_stats.failed++;
			printf("Non-correctable error in page at 0x%08llx\n",
			       (loff_t)page << chip->page_shift);

			/* Note: buf and chip->oob_poi are unchanged here! */
			return 0;
		}
		if ((status[i] != 0x00) && (status[i] != 0xff)) {
			corrected += status[i];
			if (status[i] > ret)
				ret = status[i];
		}
	}

	mtd->ecc_stats.corrected += corrected;

	/* Copy the main data */
	memcpy(buf, data_buf, mtd->writesize);

	/* Copy the user OOB data if requested */
	if (oob_required) {
		memset(chip->oob_poi, 0xff, mtd->oobsize);
		memcpy(chip->oob_poi + 4, data_buf + mtd->writesize + 4,
		       mtd->oobavail);
	}

#ifdef CONFIG_NAND_REFRESH
	/* If requested, read refresh block number from the four bytes at the
	   beginning of the OOB area (that came from the beginningof the page)
	   and return it converted to an offset. The caller has to make sure
	   that this flag is not set in the first two pages of a block,
	   because there the Bad Block Marker is stored in this place. */
	if (mtd->extraflags & MTD_EXTRA_REFRESHOFFS) {
		u32 refresh;

		memcpy(&refresh, data_buf + mtd->writesize, 4);
		if (refresh == 0xFFFFFFFF)
			refresh = 0;
		mtd->extradata = refresh << chip->phys_erase_shift;
	}
#endif

	/* Return maximum number of bitflips across all chunks */
	return ret;
}

/*
 * Write a page to NAND with ECC.
 */
static int mxs_nand_write_page(struct mtd_info *mtd, struct nand_chip *chip,
			       const uint8_t *buf, int oob_required)
{
	struct mxs_nand_priv *priv = chip->priv;
	struct mxs_bch_regs *bch_regs = (struct mxs_bch_regs *)MXS_BCH_BASE;
	struct mxs_dma_desc *d;

	/* Select the desired flash layout */
	writel(priv->bch_layout, &bch_regs->hw_bch_layoutselect);

	/* Copy the main data to data_buf[] */
	memcpy(data_buf, buf, mtd->writesize);

	/* We must provide OOB data because it is written in one go with the
	   main data. If no OOB data is available, use 0xFF instead. This
	   does not modify the existing values. */
	memset(data_buf + mtd->writesize, 0xFF, mtd->oobavail + 4);
	if (oob_required)
		memcpy(data_buf + mtd->writesize + 4, chip->oob_poi + 4,
		       mtd->oobavail);

#ifdef CONFIG_NAND_REFRESH
	/* If requested, store refresh offset as block number in the four
	   bytes at the beginning of OOB area (that will go to the beginning
	   of the page). The caller has to make sure that this flag is not set
	   when writing to the first two pages of the block or the Bad Block
	   Marker may be set unintentionally. */
	if ((mtd->extraflags & MTD_EXTRA_REFRESHOFFS) && mtd->extradata) {
		u32 refresh;

		refresh = (u32)(mtd->extradata >> chip->phys_erase_shift);
		memcpy(data_buf + mtd->writesize, &refresh, 4);
	}
#endif

	/* Add DMA descriptor to enable BCH and write data */
	d = mxs_nand_get_dma_desc(priv);
	d->cmd.data =
		MXS_DMA_DESC_COMMAND_NO_DMAXFER |
		MXS_DMA_DESC_NAND_LOCK |
		MXS_DMA_DESC_WAIT4END |
		(6 << MXS_DMA_DESC_PIO_WORDS_OFFSET);

	d->cmd.address = 0;

	d->cmd.pio_words[0] =
		GPMI_CTRL0_COMMAND_MODE_WRITE |
		GPMI_CTRL0_LOCK_CS |
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

	mxs_nand_dma_desc_append(priv, d);

	/* Flush cache for page and OOB data */
	mxs_nand_flush_buf(data_buf, mtd->writesize + mtd->oobavail + 4);

	/* The actual programming will take place in mxs_nand_command() when
	   command NAND_CMD_PAGEPROG is sent. This also disables the BCH. */
	return 0;
}

/*
 * Read the OOB area, including the BBM and ECC info. Unfortunately the ECC
 * system of the i.MX6 CPU does not match the common view of NAND chips. So we
 * must compile a virtual OOB area by reading different parts of the page.
 *
 * See mxs_nand_read_ecc_data() for a description of the OOB area layout.
 */
static int mxs_nand_read_oob_raw(struct mtd_info *mtd,
				 struct nand_chip *chip, int page)
{
	return mxs_nand_do_read_oob(mtd, chip, page, 1);
}

/*
 * Read the user part of the OOB area (behind the BBM, before the ECC data)
 *
 * See mxs_nand_read_ecc_data() for a description of the OOB area layout.
 */
static int mxs_nand_read_oob(struct mtd_info *mtd, struct nand_chip *chip,
			     int page)
{
	return mxs_nand_do_read_oob(mtd, chip, page, 0);
}

/*
 * Write the OOB area, including the BBM and ECC info. Unfortunately the ECC
 * system of the i.MX6 CPU does not match the common view of NAND chips. So we
 * must store the (virtual) OOB area to different parts of the page.
 *
 * See mxs_nand_read_ecc_data() for a description of the OOB area layout.
 */
static int mxs_nand_write_oob_raw(struct mtd_info *mtd,
				  struct nand_chip *chip, int page)
{
	return mxs_nand_do_write_oob(mtd, chip, page, 1);
}

/*
 * Write the user part of the OOB area (behind the BBM, before the ECC data)
 *
 * See mxs_nand_read_ecc_data() for a description of the OOB area layout.
 */
static int mxs_nand_write_oob(struct mtd_info *mtd, struct nand_chip *chip,
			      int page)
{
	return mxs_nand_do_write_oob(mtd, chip, page, 0);
}

/*
 * Called before nand_scan_ident(). Initialize some basic NFC hardware to be
 * able to read the NAND ID and ONFI data to detect the block/page/OOB sizes.
 */
static int mxs_nand_init(struct mxs_nand_priv *priv)
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

	/* If timing0 value is 0, use the current setting */
	if (!priv->timing0)
		priv->timing0 = readl(&gpmi_regs->hw_gpmi_timing0);
	/* Reset the GPMI block. */
	mxs_reset_block(&gpmi_regs->hw_gpmi_ctrl0_reg);
	mxs_reset_block(&bch_regs->hw_bch_ctrl_reg);

	/*
	 * Choose NAND mode, set IRQ polarity, disable write protection and
	 * select BCH ECC.
	 */
	clrsetbits_le32(&gpmi_regs->hw_gpmi_ctrl1,
			GPMI_CTRL1_GPMI_MODE,
			GPMI_CTRL1_ATA_IRQRDY_POLARITY | GPMI_CTRL1_DECOUPLE_CS
			| GPMI_CTRL1_DEV_RESET | GPMI_CTRL1_BCH_MODE);

	/* Set timeout when waiting for ready to maximum */
	writel(0xFFFF0000, &gpmi_regs->hw_gpmi_timing1);

	/* Set GPMI timing */
	writel(priv->timing0, &gpmi_regs->hw_gpmi_timing0);

	return 0;
}

/*
 * Called after nand_scan_ident(). Now the flash geometry is known. However
 * mtd->oobavail is not set yet, so we have to pass the value as argument.
 */
static void mxs_nand_init_cont(struct mtd_info *mtd, uint32_t oobavail,
			       uint32_t index)
{
	struct nand_chip *chip = mtd->priv;
	struct mxs_nand_priv *priv = chip->priv;
	struct mxs_bch_regs *bch_regs = (struct mxs_bch_regs *)MXS_BCH_BASE;
	uint32_t layout0, layout1;

	/* Configure BCH and set NFC geometry */
	mxs_reset_block(&bch_regs->hw_bch_ctrl_reg);

	/* Configure the layout given by chip_select */
	layout0 =
		chip->ecc.steps << BCH_FLASHLAYOUT0_NBLOCKS_OFFSET |
		((oobavail + 4) << BCH_FLASHLAYOUT0_META_SIZE_OFFSET) |
		(0 << BCH_FLASHLAYOUT0_ECC0_OFFSET) |
		(0 << BCH_FLASHLAYOUT0_DATA0_SIZE_OFFSET);

	layout1 =
		((mtd->writesize + mtd->oobsize)
		       << BCH_FLASHLAYOUT1_PAGE_SIZE_OFFSET) |
		((mtd->ecc_strength >> 1) << BCH_FLASHLAYOUT1_ECCN_OFFSET) |
		(chip->ecc.size >> MXS_NAND_CHUNK_DATA_CHUNK_SIZE_SHIFT);	
	if (priv->gf == 14)
		layout1 |= BCH_FLASHLAYOUT1_GF13_0_GF14_1;

	switch (index) {
	case 0:
		writel(layout0, &bch_regs->hw_bch_flash0layout0);
		writel(layout1, &bch_regs->hw_bch_flash0layout1);
		break;

	case 1:
		writel(layout0, &bch_regs->hw_bch_flash1layout0);
		writel(layout1, &bch_regs->hw_bch_flash1layout1);
		break;

	case 2:
		writel(layout0, &bch_regs->hw_bch_flash2layout0);
		writel(layout1, &bch_regs->hw_bch_flash2layout1);
		break;

	case 3:
		writel(layout0, &bch_regs->hw_bch_flash3layout0);
		writel(layout1, &bch_regs->hw_bch_flash3layout1);
		break;
	}

	/* Use index as layout number for all chip selects */
	index |= index << 2;
	index |= index << 4;
	priv->bch_layout = index;

	/* For now set *all* chip selects to use layout 0; we will change this
	   to the desired layout number when reading/writing pages with ECC */
	writel(0, &bch_regs->hw_bch_layoutselect);

	/* Allow these many zero-bits in an empty page */
	writel(mtd->bitflip_threshold, &bch_regs->hw_bch_mode);

	/* Do not enable BCH complete interrupt, we will poll */
	//writel(BCH_CTRL_COMPLETE_IRQ_EN, &bch_regs->hw_bch_ctrl_set);
}

/*
 * Register an i.MX6 NAND device
 */
void mxs_nand_register(int nfc_hw_id,
		       const struct mxs_nand_fus_platform_data *pdata)
{
	static int index = 0;
	struct mxs_nand_priv *priv;
	struct nand_chip *chip;
	struct mtd_info *mtd;
	uint32_t i, oobavail;
	uint32_t ecc_strength, chunk_shift;

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

	priv->cmd_queue_len = 0;
	priv->desc_index = 0;
	priv->timing0 = pdata ? pdata->timing0 : 0;

	/* Setup all things required to detect the chip */
	chip->IO_ADDR_R = (void __iomem *)nfc_base_addresses[nfc_hw_id];
	chip->IO_ADDR_W = chip->IO_ADDR_R;
	chip->select_chip = mxs_nand_select_chip;
	chip->dev_ready = mxs_nand_device_ready;
	chip->cmdfunc = mxs_nand_command;
	chip->read_byte = mxs_nand_read_byte;
	chip->read_word = mxs_nand_read_word;
	chip->read_buf = mxs_nand_read_buf;
	chip->write_buf = mxs_nand_write_buf;
	chip->waitfunc = mxs_nand_waitfunc;
	chip->options = pdata ? pdata->options : 0;
	chip->options |= NAND_BBT_SCAN2NDPAGE | NAND_NO_SUBPAGE_WRITE;
	chip->badblockpos = 0;

	/* If this is the first call, init the GPMI/BCH/DMA system */
	if (!index) {
		/* Initialize the DMA descriptors */
		mxs_nand_init_dma_descs(priv);
		if (mxs_nand_init(priv))
			return;
	}

	/* Identify the device, set page and block sizes, etc. */
	if (nand_scan_ident(mtd, CONFIG_SYS_NAND_MAX_CHIPS, NULL)) {
		mtd->name = NULL;
		return;
	}

	/* Set skipped region and backup region */
	chunk_shift = 9;
	ecc_strength = 0;
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
		if (pdata->flags & MXS_NAND_CHUNK_1K)
			chunk_shift = 10;
		ecc_strength = pdata->ecc_strength;
	}
	priv->gf = (chunk_shift > 9) ? 14 : 13;
	if (!ecc_strength)
		ecc_strength = mxs_nand_get_ecc_strength(mtd, chunk_shift,
							 priv->gf);

	/* Set up ECC configuration and ECC layout */
	chip->ecc.layout = &fus_nfc_ecclayout;
	chip->ecc.mode = NAND_ECC_HW;
	chip->ecc.steps = mtd->writesize >> chunk_shift;
	chip->ecc.size = 1 << chunk_shift;
	chip->ecc.strength = ecc_strength;
	chip->ecc.bytes = ecc_strength * chip->ecc.steps * priv->gf / 8;
	oobavail = mtd->oobsize - chip->ecc.bytes - 4;
	chip->ecc.layout->oobfree[0].offset = 4;
	chip->ecc.layout->oobfree[0].length = oobavail;
	chip->ecc.layout->oobfree[1].length = 0; /* Sentinel */
	chip->ecc.layout->eccbytes = chip->ecc.bytes;
	for (i = 0; i < chip->ecc.bytes; i++)
		chip->ecc.layout->eccpos[i] = oobavail + 4 + i;
	mtd->ecc_strength = chip->ecc.strength;

	chip->ecc.read_page = mxs_nand_read_page;
	chip->ecc.write_page = mxs_nand_write_page;
	chip->ecc.read_oob = mxs_nand_read_oob;
	chip->ecc.write_oob = mxs_nand_write_oob;
	chip->ecc.read_page_raw = mxs_nand_read_page_raw;
	chip->ecc.write_page_raw = mxs_nand_write_page_raw;
	chip->ecc.read_oob_raw = mxs_nand_read_oob_raw;
	chip->ecc.write_oob_raw = mxs_nand_write_oob_raw;

	if (chip->ecc.strength >= 20)
		mtd->bitflip_threshold = chip->ecc.strength - 2;
	else
		mtd->bitflip_threshold = chip->ecc.strength - 1;

#ifdef CONFIG_SYS_NAND_ONFI_DETECTION
	if (chip->onfi_version) {
		/* Get address cycles from ONFI data:
		   [7:4] column cycles, [3:0] row cycles */
		priv->column_cycles = chip->onfi_params.addr_cycles >> 4;
		priv->row_cycles = chip->onfi_params.addr_cycles & 0x0F;
	} else
#endif
	{
		/* Use two column cycles and decide from the size whether we
		   need two or three row cycles */
		priv->column_cycles = 2;
		if (chip->chipsize > (mtd->writesize << 16))
			priv->row_cycles = 3;
		else
			priv->row_cycles = 2;
	}

	mxs_nand_init_cont(mtd, oobavail, index);

	if (nand_scan_tail(mtd)) {
		mtd->name = NULL;
		return;
	}

	nand_register(index++);
}
