/*
 * (C) Copyright 2006 DENX Software Engineering
 *
 * Implementation for U-Boot 1.1.6 by Samsung
 *
 * (C) Copyright 2008
 * Guennadi Liakhovetki, DENX Software Engineering, <lg@denx.de>
 *
 * See file CREDITS for list of people who contributed to this
 * project.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of
 * the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston,
 * MA 02111-1307 USA
 */

#include <common.h>

#include <nand.h>
#include <linux/mtd/nand.h>

#include <asm/arch/s3c64xx-regs.h>

#include <asm/io.h>
#include <asm/errno.h>

/* OOB layout for NAND flashes with 512 byte pages and 16 bytes OOB */
/* 1-bit ECC: ECC is in bytes 8..11 in OOB, bad block marker is in byte 5 */
static struct nand_ecclayout s3c_layout_ecc1_oob16 = {
	.eccbytes = 4,
	.eccpos = {8, 9, 10, 11},
	.oobfree = {
		{.offset = 0,
		 . length = 5},		  /* Before bad block marker */
		{.offset = 6,
		 . length = 2},		  /* Between bad block marker and ECC */
		{.offset = 12,
		 . length = 4},		  /* Behind ECC */
	}
};

/* 8-bit ECC: ECC is in bytes 0..12 in OOB. This means the bad block marker in
   byte 5 can't be used! However if there are more than 8 bad bits in the
   page, the flash is in serious trouble anyway. */
static struct nand_ecclayout s3c_layout_ecc8_oob16 = {
	.eccbytes = 13,
	.eccpos = {0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12},
	.oobfree = {
		{.offset = 13,
		 . length = 3}		  /* Behind ECC */
	}
};

/* OOB layout for NAND flashes with 2048 byte pages and 64 bytes OOB */
/* 1-bit ECC: ECC is in bytes 1..4 in OOB, bad block marker is in byte 0 */
static struct nand_ecclayout s3c_layout_ecc1_oob64 = {
	.eccbytes = 4,
	.eccpos = {1, 2, 3, 4},
	.oobfree = {
		{.offset = 5,		  /* Behind bad block marker and ECC */
		 .length = 59}}
};

/* 8-bit ECC: ECC is 13 bytes for each 512 bytes of data, in bytes 0..51 in
   OOB. This means the bad block marker in byte 0 can't be used! However if
   there are more than 8 bad bits in the page, the flash is in serious trouble
   anyway. */
static struct nand_ecclayout s3c_layout_ecc8_oob64 = {
	.eccbytes = 52,
	.eccpos = {0, 1, 2, 3, 4, 5, 6, 7, 8, 9,
		   10, 11, 12, 13, 14, 15, 16, 17, 18, 19,
		   20, 21, 22, 23, 24, 25, 26, 27, 28, 29,
		   30, 31, 32, 33, 34, 35, 36, 37, 38, 39,
		   40, 41, 42, 43, 44, 45, 46, 47, 48, 49,
		   50, 51},
	.oobfree = {
		{.offset = 52,
		 . length = 12}		  /* Behind ECC */
	}
};

static int cur_ecc_mode = 0;

#ifdef CONFIG_NAND_SPL
#define printf(arg...) do {} while (0)
#endif

/* Nand flash definition values by jsgood */
#ifdef S3C_NAND_DEBUG
/*
 * Function to print out oob buffer for debugging
 * Written by jsgood
 */
static void print_oob(const char *header, struct mtd_info *mtd)
{
	int i;
	struct nand_chip *chip = mtd->priv;

	printf("%s:\t", header);

	for (i = 0; i < 64; i++)
		printf("%02x ", chip->oob_poi[i]);

	printf("\n");
}
#endif /* S3C_NAND_DEBUG */

static void s3c_nand_select_chip(struct mtd_info *mtd, int chip)
{
	int ctrl = readl(NFCONT);

	switch (chip) {
	case -1:
		ctrl |= 6;
		break;
	case 0:
		ctrl &= ~2;
		break;
	case 1:
		ctrl &= ~4;
		break;
	default:
		return;
	}

	writel(ctrl, NFCONT);
}

/*
 * Hardware specific access to control-lines function
 * Written by jsgood
 */
static void s3c_nand_hwcontrol(struct mtd_info *mtd, int cmd, unsigned int ctrl)
{
	struct nand_chip *this = mtd->priv;

	if (ctrl & NAND_CTRL_CHANGE) {
		if (ctrl & NAND_CLE)
			this->IO_ADDR_W = (void __iomem *)NFCMMD;
		else if (ctrl & NAND_ALE)
			this->IO_ADDR_W = (void __iomem *)NFADDR;
		else
			this->IO_ADDR_W = (void __iomem *)NFDATA;
#if 0 //###
		if (ctrl & NAND_NCE)
			s3c_nand_select_chip(mtd, *(int *)this->priv);
		else
			s3c_nand_select_chip(mtd, -1);
#endif //###
	}

	if (cmd != NAND_CMD_NONE)
		writeb(cmd, this->IO_ADDR_W);
}

/*
 * Function for checking device ready pin
 * Written by jsgood
 */
static int s3c_nand_device_ready(struct mtd_info *mtdinfo)
{
	return !!(readl(NFSTAT) & NFSTAT_RnB);
}

#ifdef CONFIG_SYS_S3C_NAND_HWECC
/*
 * This function is called before encoding ecc codes to ready ecc engine.
 * Written by jsgood
 */
static void s3c_nand_enable_hwecc(struct mtd_info *mtd, int mode)
{
	u_long nfcont, nfconf;

	/*
	 * The original driver used 4-bit ECC for "new" MLC chips, i.e., for
	 * those with non-zero ID[3][3:2], which anyway only holds for ST
	 * (Numonyx) chips
	 */
	nfconf = readl(NFCONF) & ~NFCONF_ECC_4BIT;

	writel(nfconf, NFCONF);

	/* Initialize & unlock */
	nfcont = readl(NFCONT);
	nfcont |= NFCONT_INITECC;
	nfcont &= ~NFCONT_MECCLOCK;

	if (mode == NAND_ECC_WRITE)
		nfcont |= NFCONT_ECC_ENC;
	else if (mode == NAND_ECC_READ)
		nfcont &= ~NFCONT_ECC_ENC;

	writel(nfcont, NFCONT);
}

/*
 * This function is called immediately after encoding ecc codes.
 * This function returns encoded ecc codes.
 * Written by jsgood
 */
static int s3c_nand_calculate_ecc(struct mtd_info *mtd, const u_char *dat,
				  u_char *ecc_code)
{
	u_long nfcont, nfmecc0;

	/* Lock */
	nfcont = readl(NFCONT);
	nfcont |= NFCONT_MECCLOCK;
	writel(nfcont, NFCONT);

	nfmecc0 = readl(NFMECC0);

	ecc_code[0] = nfmecc0 & 0xff;
	ecc_code[1] = (nfmecc0 >> 8) & 0xff;
	ecc_code[2] = (nfmecc0 >> 16) & 0xff;
	ecc_code[3] = (nfmecc0 >> 24) & 0xff;

	return 0;
}

/*
 * This function determines whether read data is good or not.
 * If SLC, must write ecc codes to controller before reading status bit.
 * If MLC, status bit is already set, so only reading is needed.
 * If status bit is good, return 0.
 * If correctable errors occured, do that.
 * If uncorrectable errors occured, return -1.
 * Written by jsgood
 */
static int s3c_nand_correct_data(struct mtd_info *mtd, u_char *dat,
				 u_char *read_ecc, u_char *calc_ecc)
{
	int ret = -1;
	u_long nfestat0, nfmeccdata0, nfmeccdata1, err_byte_addr;
	u_char err_type, repaired;

	/* SLC: Write ecc to compare */
	nfmeccdata0 = (calc_ecc[1] << 16) | calc_ecc[0];
	nfmeccdata1 = (calc_ecc[3] << 16) | calc_ecc[2];
	writel(nfmeccdata0, NFMECCDATA0);
	writel(nfmeccdata1, NFMECCDATA1);

	/* Read ecc status */
	nfestat0 = readl(NFESTAT0);
	err_type = nfestat0 & 0x3;

	switch (err_type) {
	case 0: /* No error */
		ret = 0;
		break;

	case 1:
		/*
		 * 1 bit error (Correctable)
		 * (nfestat0 >> 7) & 0x7ff	:error byte number
		 * (nfestat0 >> 4) & 0x7	:error bit number
		 */
		err_byte_addr = (nfestat0 >> 7) & 0x7ff;
		repaired = dat[err_byte_addr] ^ (1 << ((nfestat0 >> 4) & 0x7));

		printf("S3C NAND: 1 bit error detected at byte %ld. "
		       "Correcting from 0x%02x to 0x%02x...OK\n",
		       err_byte_addr, dat[err_byte_addr], repaired);

		dat[err_byte_addr] = repaired;

		ret = 1;
		break;

	case 2: /* Multiple error */
	case 3: /* ECC area error */
		printf("S3C NAND: ECC uncorrectable error detected. "
		       "Not correctable.\n");
		ret = -1;
		break;
	}

	return ret;
}
#endif /* CONFIG_SYS_S3C_NAND_HWECC */

void s3c_nand_enable_hwecc_8bit(struct mtd_info *mtd, int mode)
{
	u_long nfcont, nfconf;

	cur_ecc_mode = mode;

	/* 8 bit selection */
	nfconf = readl(NFCONF);

	nfconf &= ~(0x3 << 23);
	nfconf |= (0x1 << 23);
	
	writel(nfconf, NFCONF);

	/* Initialize & unlock */
	nfcont = readl(NFCONT);
	nfcont |= NFCONT_INITECC;
	nfcont &= ~NFCONT_MECCLOCK;

	if (mode == NAND_ECC_WRITE)
		nfcont |= NFCONT_ECC_ENC;
	else if (mode == NAND_ECC_READ)
		nfcont &= ~NFCONT_ECC_ENC;

	writel(nfcont, NFCONT);
}

int s3c_nand_calculate_ecc_8bit(struct mtd_info *mtd, const u_char *dat, u_char *ecc_code)
{
	u_long nfcont, nfm8ecc0, nfm8ecc1, nfm8ecc2, nfm8ecc3;

	/* Lock */
	nfcont = readl(NFCONT);
	nfcont |= NFCONT_MECCLOCK;
	writel(nfcont, NFCONT);

	if (cur_ecc_mode == NAND_ECC_READ)
		while (!(readl(NFSTAT) & NFSTAT_ECCDECDONE))
			/* nothing */;
	else {
		while (!(readl(NFSTAT) & NFSTAT_ECCENCDONE))
			/* nothing */;
		
		nfm8ecc0 = readl(NFM8ECC0);
		nfm8ecc1 = readl(NFM8ECC1);
		nfm8ecc2 = readl(NFM8ECC2);
		nfm8ecc3 = readl(NFM8ECC3);

		ecc_code[0] = nfm8ecc0 & 0xff;
		ecc_code[1] = (nfm8ecc0 >> 8) & 0xff;
		ecc_code[2] = (nfm8ecc0 >> 16) & 0xff;
		ecc_code[3] = (nfm8ecc0 >> 24) & 0xff;			
		ecc_code[4] = nfm8ecc1 & 0xff;
		ecc_code[5] = (nfm8ecc1 >> 8) & 0xff;
		ecc_code[6] = (nfm8ecc1 >> 16) & 0xff;
		ecc_code[7] = (nfm8ecc1 >> 24) & 0xff;
		ecc_code[8] = nfm8ecc2 & 0xff;
		ecc_code[9] = (nfm8ecc2 >> 8) & 0xff;
		ecc_code[10] = (nfm8ecc2 >> 16) & 0xff;
		ecc_code[11] = (nfm8ecc2 >> 24) & 0xff;
		ecc_code[12] = nfm8ecc3 & 0xff;
	}
	
	return 0;
}

int s3c_nand_correct_data_8bit(struct mtd_info *mtd, u_char *dat,
			       u_char *read_ecc, u_char *calc_ecc)
{
	int ret = -1;
	u_long nf8eccerr0, nf8eccerr1, nf8eccerr2, nfmlc8bitpt0, nfmlc8bitpt1;
	u_char err_type;

	while (readl(NF8ECCERR0) & NFESTAT0_ECCBUSY)
		;

	nf8eccerr0 = readl(NF8ECCERR0);
	nf8eccerr1 = readl(NF8ECCERR1);
	nf8eccerr2 = readl(NF8ECCERR2);
	nfmlc8bitpt0 = readl(NFMLC8BITPT0);
	nfmlc8bitpt1 = readl(NFMLC8BITPT1);
	
	err_type = (nf8eccerr0 >> 25) & 0xf;

	/* No error, If free page (all 0xff) */
	/* While testing, it was found that NFECCERR0[29] bit is set even if
         * the page contents were not zero. So this code is commented */
	/*if ((nf8eccerr0 >> 29) & 0x1)
		err_type = 0;*/

	switch (err_type) {
	case 9: /* Uncorrectable */
		printk("s3c-nand (8bit): ECC uncorrectable error detected\n");
		ret = -1;
		break;

	case 8: /* 8 bit error (Correctable) */
		dat[(nf8eccerr2 >> 22) & 0x3ff] ^= ((nfmlc8bitpt1 >> 24) & 0xff);

	case 7: /* 7 bit error (Correctable) */
		dat[(nf8eccerr2 >> 11) & 0x3ff] ^= ((nfmlc8bitpt1 >> 16) & 0xff);

	case 6: /* 6 bit error (Correctable) */
		dat[nf8eccerr2 & 0x3ff] ^= ((nfmlc8bitpt1 >> 8) & 0xff);

	case 5: /* 5 bit error (Correctable) */
		dat[(nf8eccerr1 >> 22) & 0x3ff] ^= (nfmlc8bitpt1 & 0xff);

	case 4: /* 4 bit error (Correctable) */
		dat[(nf8eccerr1 >> 11) & 0x3ff] ^= ((nfmlc8bitpt0 >> 24) & 0xff);

	case 3: /* 3 bit error (Correctable) */
		dat[nf8eccerr1 & 0x3ff] ^= ((nfmlc8bitpt0 >> 16) & 0xff);

	case 2: /* 2 bit error (Correctable) */
		dat[(nf8eccerr0 >> 15) & 0x3ff] ^= ((nfmlc8bitpt0 >> 8) & 0xff);

	case 1: /* 1 bit error (Correctable) */
		dat[nf8eccerr0 & 0x3ff] ^= (nfmlc8bitpt0 & 0xff);
		printk("s3c-nand (8-bit): %d bit(s) error detected, corrected successfully\n", err_type);
		ret = err_type;
		break;

	case 0: /* No error */
		ret = 0;
		break;
	}

	return ret;
}

void s3c_nand_write_page_8bit(struct mtd_info *mtd, struct nand_chip *chip,
			      const uint8_t *buf)
{
	int i, eccsize = 512;
	int eccbytes = 13;
	int eccsteps = mtd->writesize / eccsize;
	uint8_t *ecc_calc = chip->buffers->ecccalc;
	uint8_t *p = (uint8_t *) buf;

	for (i = 0; eccsteps; eccsteps--, i += eccbytes, p += eccsize) {
		s3c_nand_enable_hwecc_8bit(mtd, NAND_ECC_WRITE);
		chip->write_buf(mtd, p, eccsize);
		s3c_nand_calculate_ecc_8bit(mtd, p, &ecc_calc[i]);
	}

	for (i = 0; i < eccbytes * (mtd->writesize / eccsize); i++)
		chip->oob_poi[i] = ecc_calc[i];

	chip->write_buf(mtd, chip->oob_poi, mtd->oobsize);
}

int s3c_nand_read_page_8bit(struct mtd_info *mtd, struct nand_chip *chip,
			    uint8_t *buf, int page)
{
	int i, stat, eccsize = 512;
	int eccbytes = 13;
	int eccsteps = mtd->writesize / eccsize;
	int col = 0;
	uint8_t *p = buf;

        i = 0;
	do {
		/* Seek to main data */
		if (mtd->writesize > 512) {
			col = i * eccsize;
			chip->cmdfunc(mtd, NAND_CMD_RNDOUT, col, -1);
		}
		s3c_nand_enable_hwecc_8bit(mtd, NAND_ECC_READ);

		/* Read 512 bytes of main data */
		chip->read_buf(mtd, p, eccsize);

		/* Seek to corresponding ECC part in OOB */
		if (mtd->writesize > 512) {
			col = mtd->writesize + i * eccbytes;
			chip->cmdfunc(mtd, NAND_CMD_RNDOUT, col, -1);
		}

		/* Read 13 bytes of ECC data */
		chip->read_buf(mtd, chip->oob_poi + i * eccbytes, eccbytes);

		/* Calculate ECC and correct any errors */
		s3c_nand_calculate_ecc_8bit(mtd, 0, 0);
		stat = s3c_nand_correct_data_8bit(mtd, p, 0, 0);
		if (stat == -1)
			mtd->ecc_stats.failed++;
		p += eccsize;
	} while (++i < eccsteps);

	/* Read remaining bytes of OOB */
	chip->read_buf(mtd, chip->oob_poi + eccsteps*eccbytes,
		       mtd->oobsize - eccsteps*eccbytes);

	return 0;
}

/*
 * Board-specific NAND initialization. The following members of the
 * argument are board-specific (per include/linux/mtd/nand.h):
 * - IO_ADDR_R?: address to read the 8 I/O lines of the flash device
 * - IO_ADDR_W?: address to write the 8 I/O lines of the flash device
 * - hwcontrol: hardwarespecific function for accesing control-lines
 * - dev_ready: hardwarespecific function for  accesing device ready/busy line
 * - enable_hwecc?: function to enable (reset)  hardware ecc generator. Must
 *   only be provided if a hardware ECC is available
 * - eccmode: mode of ecc, see defines
 * - chip_delay: chip dependent delay for transfering data from array to
 *   read regs (tR)
 * - options: various chip options. They can partly be set to inform
 *   nand_scan about special functionality. See the defines for further
 *   explanation
 * Members with a "?" were not set in the merged testing-NAND branch,
 * so they are not set here either.
 */
int board_nand_init(struct nand_chip *chip)
{
	NFCONT_REG = (NFCONT_REG & ~NFCONT_WP) | NFCONT_ENABLE | 0x6;

	s3c_nand_select_chip(NULL, -1);

	chip->IO_ADDR_R		= (void __iomem *)NFDATA;
	chip->IO_ADDR_W		= (void __iomem *)NFDATA;
	chip->cmd_ctrl		= s3c_nand_hwcontrol;
	chip->dev_ready		= s3c_nand_device_ready;
	chip->select_chip	= s3c_nand_select_chip;
	chip->options		= 0;
#ifdef CONFIG_NAND_SPL
	chip->read_byte		= nand_read_byte;
	chip->write_buf		= nand_write_buf;
	chip->read_buf		= nand_read_buf;
#endif

	return 0;
}

static int __board_nand_setup_s3c(struct mtd_info *mtd,
					struct nand_chip *chip, int id)
{
	return 0;
}

int board_nand_setup_s3c(struct mtd_info *mtd, struct nand_chip *chip, int id)
	__attribute__((weak, alias("__board_nand_setup_s3c")));

/* The s3c64xx nand driver is capable of using up to 2 chips per NAND device
   (CONFIG_SYS_NAND_MAX_CHIPS) that are selected with s3c_nand_select_chip().
   If more than one flash device (with up to 2 chips each) is required, the
   chip addressing must be implemented externally (board-specific). */
int board_nand_setup(struct mtd_info *mtd, struct nand_chip *chip, int id)
{
	int ret;

	/* Despite the name, board_nand_setup() is basically arch-specific, so
	   first set things that are really board-specific */
	ret = board_nand_setup_s3c(mtd, chip, id);
	if (ret)
		return ret;

#ifdef CONFIG_SYS_S3C_NAND_HWECC
	if (chip->ecc.mode == -8) {
		/* Use 8-bit ECC */
		if (mtd->oobsize == 16)
			chip->ecc.layout = &s3c_layout_ecc8_oob16;
		else
			chip->ecc.layout = &s3c_layout_ecc8_oob64;
		chip->ecc.size = 512;
		chip->ecc.bytes = 13;
		chip->ecc.read_page = s3c_nand_read_page_8bit;
		chip->ecc.write_page = s3c_nand_write_page_8bit;
	} else {
		/* By default use 1-bit ECC, this will work up to 2048 bytes */
		if (mtd->oobsize == 16)
			chip->ecc.layout = &s3c_layout_ecc1_oob16;
		else
			chip->ecc.layout = &s3c_layout_ecc1_oob64;
		chip->ecc.size = mtd->writesize; /* 512 or 2048 */
		chip->ecc.bytes = 4;
#if 0
		/* The default functions nand_read_page_hwecc() and
		   nand_write_page_hwecc() from nand_base.c, that get
		   automatically selected if we don't set anything different
		   here, happen to work if eccsteps is 1, which is the case if
		   we don't have pages with more than 2048 bytes. If this
		   situation changes in the future, we have to implement our
		   own 1-bit ECC functions and set them here. */
		chip->ecc.read_page = s3c_nand_read_page_1bit;
		chip->ecc.write_page = s3c_nand_write_page_1bit;
#endif
	}
	chip->ecc.hwctl		= s3c_nand_enable_hwecc;
	chip->ecc.calculate	= s3c_nand_calculate_ecc;
	chip->ecc.correct	= s3c_nand_correct_data;

	chip->ecc.mode		= NAND_ECC_HW;
#else
	chip->ecc.mode		= NAND_ECC_SOFT;
#endif /* ! CONFIG_SYS_S3C_NAND_HWECC */

	return 0;
}
