/*
 * (C) Copyright 2012 F&S Elektronik Systeme GmbH
 * Hartmut Keller, F&S Elektronik Systeme GmbH, keller@fs-net.de
 *
 * Implementation for S5P
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

#include <asm/io.h>
#include <asm/errno.h>
#include <asm/arch/cpu.h>		  /* samsung_get_base_nfcon */

/* OOB layout for NAND flashes with 512 byte pages and 16 bytes OOB */
/* 1-bit ECC: ECC is in bytes 8..11 in OOB, bad block marker is in byte 5 */
static struct nand_ecclayout s5p_layout_ecc1_oob16 = {
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
static struct nand_ecclayout s5p_layout_ecc8_oob16 = {
	.eccbytes = 13,
	.eccpos = {0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12},
	.oobfree = {
		{.offset = 13,
		 . length = 3}		  /* Behind ECC */
	}
};

/* OOB layout for NAND flashes with 2048 byte pages and 64 bytes OOB */
/* 1-bit ECC:  We compute 4 bytes ECC for each 512 bytes of the page; ECC
   is in bytes 16..31 in OOB, bad block marker in byte 0, but two bytes are
   checked; so our first free byte is at offset 2. */
static struct nand_ecclayout s5p_layout_ecc1_oob64 = {
	.eccbytes = 16,
	.eccpos = {16, 17, 18, 19, 20, 21, 22, 23,
		   24, 25, 26, 27, 28, 29, 30, 31},
	.oobfree = {
		{.offset = 2,		  /* Between bad block marker and ECC */
		 .length = 14},
		{.offset = 32,		  /* Behind ECC */
		 .length = 32}}
};

/* 8-bit ECC: ECC is 13 bytes for each 512 bytes of data, in bytes 0..51 in
   OOB. This means the bad block marker in byte 0 can't be used! However if
   there are more than 8 bad bits in the page, the flash is in serious trouble
   anyway. */
static struct nand_ecclayout s5p_layout_ecc8_oob64 = {
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

#ifdef CONFIG_NAND_SPL
#define printf(arg...) do {} while (0)
#endif

#define NFCONF_ECC_1BIT		(0<<23)
#define NFCONF_ECC_OFF		(1<<23)
#define NFCONF_ECC_4BIT		(2<<23)
#define NFCONF_ECC_MASK		(3<<23)

#define NFCONT_ECC_ENC		(1<<18)
#define NFCONT_LOCK		(1<<16)
#define NFCONT_MECCLOCK		(1<<7)
#define NFCONT_SECCLOCK		(1<<6)
#define NFCONT_INITMECC		(1<<5)
#define NFCONT_INITSECC		(1<<4)
#define NFCONT_INITECC		(NFCONT_INITMECC | NFCONT_INITSECC)
#define NFCONT_CS_ALT		(1<<2)
#define NFCONT_CS		(1<<1)
#define NFCONT_ENABLE		(1<<0)

#define NFSTAT_ECCENCDONE	(1<<7)
#define NFSTAT_ECCDECDONE	(1<<6)
#define NFSTAT_RnB		(1<<0)

#define NFESTAT0_ECCBUSY	(1<<31)

struct s5p_nfcon {			  /* Offset */
	volatile unsigned int nfconf;	  /* 0x00 */
	volatile unsigned int nfcont;	  /* 0x04 */
	volatile unsigned int nfcmmd;	  /* 0x08 */
	volatile unsigned int nfaddr;	  /* 0x0c */
	volatile unsigned int nfdata;	  /* 0x10 */
	volatile unsigned int nfmeccd0;	  /* 0x14 */
	volatile unsigned int nfmeccd1;	  /* 0x18 */
	volatile unsigned int nfseccd;	  /* 0x1c */
	volatile unsigned int nfsblk;	  /* 0x20 */
	volatile unsigned int nfeblk;	  /* 0x24 */
	volatile unsigned int nfstat;	  /* 0x28 */
	volatile unsigned int nfeccerr0;  /* 0x2c */
	volatile unsigned int nfeccerr1;  /* 0x30 */
	volatile unsigned int nfmecc0;	  /* 0x34 */
	volatile unsigned int nfmecc1;	  /* 0x38 */
	volatile unsigned int nfsecc;	  /* 0x3c */
	volatile unsigned int nfmlcbitpt; /* 0x40 */
};


/* Nand flash definition values by jsgood */
#ifdef S5P_NAND_DEBUG
/*
 * Function to print out oob buffer for debugging
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
#endif /* S5P_NAND_DEBUG */

static void s5p_nand_select_chip(struct mtd_info *mtd, int chip)
{
	struct s5p_nfcon *const nfcon =
		(struct s5p_nfcon *)samsung_get_base_nfcon();
	unsigned int ctrl;

	ctrl = readl(&nfcon->nfcont);

	switch (chip) {
	case -1:
		ctrl |= ((1<<1) | (1<<2) | (1<<22) | (1<<23));
		break;
	case 0:
		ctrl &= ~(1<<1);
		break;
	case 1:
		ctrl &= ~(1<<2);
		break;
	case 2:
		ctrl &= ~(1<<22);
		break;
	case 3:
		ctrl &= ~(1<<23);
		break;
	default:
		return;
	}

	writel(ctrl, &nfcon->nfcont);
}

/*
 * Hardware specific access to control-lines function
 */
static void s5p_nand_hwcontrol(struct mtd_info *mtd, int cmd, unsigned int ctrl)
{
	struct nand_chip *this = mtd->priv;
	struct s5p_nfcon *const nfcon =
		(struct s5p_nfcon *)samsung_get_base_nfcon();

	if (ctrl & NAND_CTRL_CHANGE) {
		if (ctrl & NAND_CLE)
			this->IO_ADDR_W = (void __iomem *)&nfcon->nfcmmd;
		else if (ctrl & NAND_ALE)
			this->IO_ADDR_W = (void __iomem *)&nfcon->nfaddr;
		else
			this->IO_ADDR_W = (void __iomem *)&nfcon->nfdata;
#if 0 //###
		if (ctrl & NAND_NCE)
			s5p_nand_select_chip(mtd, *(int *)this->priv);
		else
			s5p_nand_select_chip(mtd, -1);
#endif //###
	}

	if (cmd != NAND_CMD_NONE)
		writeb(cmd, this->IO_ADDR_W);
}

/*
 * Function for checking device ready pin
 * Written by jsgood
 */
static int s5p_nand_device_ready(struct mtd_info *mtdinfo)
{
	struct s5p_nfcon *const nfcon =
		(struct s5p_nfcon *)samsung_get_base_nfcon();

	return !!(readl(&nfcon->nfstat) & NFSTAT_RnB);
}

#ifdef CONFIG_SYS_S5P_NAND_HWECC
/*
 * This function is called before encoding ecc codes to ready ecc engine.
 */
static void s5p_nand_enable_hwecc(struct mtd_info *mtd, int mode)
{
	u_long nfcont, nfconf;
	struct s5p_nfcon *const nfcon =
		(struct s5p_nfcon *)samsung_get_base_nfcon();

	/* Set for 1-bit ECC */
	nfconf = readl(&nfcon->nfconf);
	nfconf &= ~NFCONF_ECC_MASK;
	nfconf |= NFCONF_ECC_1BIT;
	writel(nfconf, &nfcon->nfconf);

	/* Initialize & unlock */
	nfcont = readl(&nfcon->nfcont);
	nfcont |= NFCONT_INITECC;
	nfcont &= ~NFCONT_MECCLOCK;

	if (mode == NAND_ECC_WRITE)
		nfcont |= NFCONT_ECC_ENC;
	else if (mode == NAND_ECC_READ)
		nfcont &= ~NFCONT_ECC_ENC;

	writel(nfcont, &nfcon->nfcont);
}

/*
 * This function is called immediately after encoding ecc codes.
 * This function returns encoded ecc codes.
 * Written by jsgood
 */
static int s5p_nand_calculate_ecc(struct mtd_info *mtd, const u_char *dat,
				  u_char *ecc_code)
{
	u_long nfcont, nfmecc0;
	struct s5p_nfcon *const nfcon =
		(struct s5p_nfcon *)samsung_get_base_nfcon();

	/* Lock */
	nfcont = readl(&nfcon->nfcont);
	nfcont |= NFCONT_MECCLOCK;
	writel(nfcont, &nfcon->nfcont);

	nfmecc0 = readl(&nfcon->nfmecc0);

	ecc_code[0] = nfmecc0 & 0xff;
	ecc_code[1] = (nfmecc0 >> 8) & 0xff;
	ecc_code[2] = (nfmecc0 >> 16) & 0xff;
	ecc_code[3] = (nfmecc0 >> 24);

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
static int s5p_nand_correct_data(struct mtd_info *mtd, u_char *dat,
				 u_char *read_ecc, u_char *calc_ecc)
{
	int ret = -1;
	u_long nfeccerr0, nfmeccdata0, nfmeccdata1, err_byte_addr;
	u_char err_type, repaired;
	struct s5p_nfcon *const nfcon =
		(struct s5p_nfcon *)samsung_get_base_nfcon();

	/* SLC: Write ecc to compare */
	nfmeccdata0 = (read_ecc[1] << 16) | read_ecc[0];
	nfmeccdata1 = (read_ecc[3] << 16) | read_ecc[2];
	writel(nfmeccdata0, &nfcon->nfmeccd0);
	writel(nfmeccdata1, &nfcon->nfmeccd1);

	/* Read ecc status */
	nfeccerr0 = readl(&nfcon->nfeccerr0);
	err_type = nfeccerr0 & 0x3;

	switch (err_type) {
	case 0: /* No error */
		ret = 0;
		break;

	case 1:
		/*
		 * 1 bit error (Correctable)
		 * (nfeccerr0 >> 7) & 0x7ff	:error byte number
		 * (nfeccerr0 >> 4) & 0x7	:error bit number
		 */
		err_byte_addr = (nfeccerr0 >> 7) & 0x7ff;
		repaired = dat[err_byte_addr] ^ (1 << ((nfeccerr0 >> 4) & 0x7));

		printf("S5P NAND: 1 bit error detected at byte %ld. "
		       "Correcting from 0x%02x to 0x%02x...OK\n",
		       err_byte_addr, dat[err_byte_addr], repaired);

		dat[err_byte_addr] = repaired;

		ret = 1;
		break;

	case 2: /* Multiple error */
	case 3: /* ECC area error */
		printf("S5P NAND: ECC uncorrectable error detected. "
		       "Not correctable.\n");
		ret = -1;
		break;
	}

	return ret;
}
#endif /* CONFIG_SYS_S5P_NAND_HWECC */

#if 0 //###
static int cur_ecc_mode = 0;

static void s5p_nand_wait_ecc_busy_8bit(void)
{
	while (readl(NF8ECCERR0) & NFESTAT0_ECCBUSY) {}
}

void s5p_nand_enable_hwecc_8bit(struct mtd_info *mtd, int mode)
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

int s5p_nand_calculate_ecc_8bit(struct mtd_info *mtd, const u_char *dat,
				u_char *ecc_code)
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

int s5p_nand_correct_data_8bit(struct mtd_info *mtd, u_char *dat,
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

void s5p_nand_write_page_8bit(struct mtd_info *mtd, struct nand_chip *chip,
			      const uint8_t *buf)
{
	int i, eccsize = 512;
	int eccbytes = 13;
	int eccsteps = mtd->writesize / eccsize;
	uint8_t *ecc_calc = chip->buffers->ecccalc;
	uint8_t *p = (uint8_t *) buf;

	for (i = 0; eccsteps; eccsteps--, i += eccbytes, p += eccsize) {
		s5p_nand_enable_hwecc_8bit(mtd, NAND_ECC_WRITE);
		chip->write_buf(mtd, p, eccsize);
		s5p_nand_calculate_ecc_8bit(mtd, p, &ecc_calc[i]);
	}

	for (i = 0; i < eccbytes * (mtd->writesize / eccsize); i++)
		chip->oob_poi[i] = ecc_calc[i];

	chip->write_buf(mtd, chip->oob_poi, mtd->oobsize);
}

int s5p_nand_read_page_8bit(struct mtd_info *mtd, struct nand_chip *chip,
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
		s5p_nand_enable_hwecc_8bit(mtd, NAND_ECC_READ);

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
		s5p_nand_calculate_ecc_8bit(mtd, 0, 0);
		stat = s5p_nand_correct_data_8bit(mtd, p, 0, 0);
		if (stat == -1)
			mtd->ecc_stats.failed++;
		p += eccsize;
	} while (++i < eccsteps);

	/* Read remaining bytes of OOB */
	chip->read_buf(mtd, chip->oob_poi + eccsteps*eccbytes,
		       mtd->oobsize - eccsteps*eccbytes);

	return 0;
}
#endif

void s5p_nand_write_page_8bit(struct mtd_info *mtd, struct nand_chip *chip,
			      const uint8_t *buf)
{
	puts("### s5p_nand_write_page_8bit() not yet implemented\n");
}

int s5p_nand_read_page_8bit(struct mtd_info *mtd, struct nand_chip *chip,
			    uint8_t *buf, int page)
{
	puts("### s5p_nand_read_page_8bit() not yet implemented\n");
	return -1;
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
	unsigned int nfcont;
	struct s5p_nfcon *const nfcon =
		(struct s5p_nfcon *)samsung_get_base_nfcon();

	s5p_nand_select_chip(NULL, -1);

	/* Disable software write protection (lock) and enable controller */
	nfcont = readl(&nfcon->nfcont);
	nfcont &= ~NFCONT_LOCK;
	nfcont |= NFCONT_ENABLE;
	writel(nfcont, &nfcon->nfcont);

	chip->IO_ADDR_R		= (void __iomem *)&nfcon->nfdata;
	chip->IO_ADDR_W		= (void __iomem *)&nfcon->nfdata;
	chip->cmd_ctrl		= s5p_nand_hwcontrol;
	chip->dev_ready		= s5p_nand_device_ready;
	chip->select_chip	= s5p_nand_select_chip;
	chip->options		= 0;
#ifdef CONFIG_NAND_SPL
	chip->read_byte		= nand_read_byte;
	chip->write_buf		= nand_write_buf;
	chip->read_buf		= nand_read_buf;
#endif

	return 0;
}

static int __board_nand_setup_s5p(struct mtd_info *mtd,
					struct nand_chip *chip, int id)
{
	return 0;
}

int board_nand_setup_s5p(struct mtd_info *mtd, struct nand_chip *chip, int id)
	__attribute__((weak, alias("__board_nand_setup_s5p")));

/* The s5p nand driver is capable of using up to 4 chips per NAND device
   (CONFIG_SYS_NAND_MAX_CHIPS) that are selected with s5p_nand_select_chip().
   If more than one flash device (with up to 4 chips each) is required, the
   chip addressing must be implemented externally (board-specific). */
int board_nand_setup(struct mtd_info *mtd, struct nand_chip *chip, int id)
{
	int ret;

	/* Despite the name, board_nand_setup() is basically arch-specific, so
	   first set things that are really board-specific */
	ret = board_nand_setup_s5p(mtd, chip, id);
	if (ret)
		return ret;

#ifdef CONFIG_SYS_S5P_NAND_HWECC
	if (chip->ecc.mode == -8) {
		/* Use 8-bit ECC */
		if (mtd->oobsize == 16)
			chip->ecc.layout = &s5p_layout_ecc8_oob16;
		else
			chip->ecc.layout = &s5p_layout_ecc8_oob64;
		chip->ecc.size = 512;
		chip->ecc.bytes = 13;
		chip->ecc.read_page = s5p_nand_read_page_8bit;
		chip->ecc.write_page = s5p_nand_write_page_8bit;
	} else {
		/* By default use 1-bit ECC, this will work up to 2048 bytes */
		if (mtd->oobsize == 16)
			chip->ecc.layout = &s5p_layout_ecc1_oob16;
		else
			chip->ecc.layout = &s5p_layout_ecc1_oob64;
		chip->ecc.size = 512;
		chip->ecc.bytes = 4;
#if 0
		/* The default functions nand_read_page_hwecc_oob_first() and
		   nand_write_page_hwecc() from nand_base.c, that get
		   automatically selected if we don't set anything different
		   here, happen to work for our 1-bit ECC case. So we don't
		   need our own versions here. */
		chip->ecc.read_page = s5p_nand_read_page_1bit;
		chip->ecc.write_page = s5p_nand_write_page_1bit;
#endif
	}
	chip->ecc.hwctl		= s5p_nand_enable_hwecc;
	chip->ecc.calculate	= s5p_nand_calculate_ecc;
	chip->ecc.correct	= s5p_nand_correct_data;

	chip->ecc.mode		= NAND_ECC_HW_OOB_FIRST;
#else
	chip->ecc.mode		= NAND_ECC_SOFT;
#endif /* ! CONFIG_SYS_S5P_NAND_HWECC */

	return 0;
}
