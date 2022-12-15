// SPDX-License-Identifier: GPL-2.0

#ifndef __UBOOT__
#include <malloc.h>
#include <linux/device.h>
#include <linux/kernel.h>
#endif
#include <linux/mtd/spinand.h>

#define SPINAND_MFR_ESMT		0xC8

#define F50XXG41XX_STATUS_ECC_MASK					(3 << 4)
#define F50XXG41XX_STATUS_ECC_NO_BITFLIPS			(0 << 4)
#define F50XXG41XX_STATUS_ECC_1_BITFLIP				(1 << 4)
#define F50XXG41XX_STATUS_ECC_2_OR_MORE_BITFLIPS	(2 << 4)
#define F50XXG41XX_STATUS_ECC_UNCOR_ERROR			(3 << 4)

		//SPINAND_PAGE_READ_FROM_CACHE_QUADIO_OP(0, 2, NULL, 0),
		//SPINAND_PAGE_READ_FROM_CACHE_DUALIO_OP(0, 1, NULL, 0),

static SPINAND_OP_VARIANTS(read_cache_variants,
		SPINAND_PAGE_READ_FROM_CACHE_QUADIO_OP(0, 4, NULL, 0),
		SPINAND_PAGE_READ_FROM_CACHE_X4_OP(0, 1, NULL, 0),
		SPINAND_PAGE_READ_FROM_CACHE_DUALIO_OP(0, 2, NULL, 0),
		SPINAND_PAGE_READ_FROM_CACHE_X2_OP(0, 1, NULL, 0),
		SPINAND_PAGE_READ_FROM_CACHE_OP(true, 0, 1, NULL, 0),
		SPINAND_PAGE_READ_FROM_CACHE_OP(false, 0, 1, NULL, 0));

static SPINAND_OP_VARIANTS(write_cache_variants,
		SPINAND_PROG_LOAD_X4(true, 0, NULL, 0),
		SPINAND_PROG_LOAD(true, 0, NULL, 0));

static SPINAND_OP_VARIANTS(update_cache_variants,
		SPINAND_PROG_LOAD_X4(false, 0, NULL, 0),
		SPINAND_PROG_LOAD(false, 0, NULL, 0));

static int f50l2g41lb_ooblayout_ecc(struct mtd_info *mtd, int section,
				  struct mtd_oob_region *region)
{
	if (section > 3)
		return -ERANGE;

	region->offset = (0x10 * section) + 0x8;
	region->length = 0x8;

	return 0;
}

static int f50l2g41lb_ooblayout_free(struct mtd_info *mtd, int section,
				   struct mtd_oob_region *region)
{
	if (section > 3)
		return -ERANGE;

	region->offset = (0x10 * section) + 2;
	region->length = 0x6;

	return 0;
}

static const struct mtd_ooblayout_ops f50l2g41lb_ooblayout = {
	.ecc = f50l2g41lb_ooblayout_ecc,
	.rfree = f50l2g41lb_ooblayout_free,
};

static int f50l2g41lb_ecc_get_status(struct spinand_device *spinand,
					 u8 status)
{
	struct nand_device *nand = spinand_to_nand(spinand);

	switch (status & STATUS_ECC_MASK) {
	case STATUS_ECC_NO_BITFLIPS:
		return 0;

	case F50XXG41XX_STATUS_ECC_1_BITFLIP:
		return 1;

	case F50XXG41XX_STATUS_ECC_2_OR_MORE_BITFLIPS:
		/*
		 * We have no way to know exactly how many bitflips have been
		 * fixed, so let's return the maximum possible value so that
		 * wear-leveling layers move the data immediately.
		 */
		return nand->eccreq.strength;

	case F50XXG41XX_STATUS_ECC_UNCOR_ERROR:
		return -EBADMSG;

	default:
		break;
	}

	return -EINVAL;
}

static int f50l2g41lb_select_target(struct spinand_device *spinand,
				  unsigned int target)
{
	struct spi_mem_op op = SPI_MEM_OP(SPI_MEM_OP_CMD(0xc2, 1),
					  SPI_MEM_OP_NO_ADDR,
					  SPI_MEM_OP_NO_DUMMY,
					  SPI_MEM_OP_DATA_OUT(1,
							spinand->scratchbuf,
							1));

	*spinand->scratchbuf = target;
	return spi_mem_exec_op(spinand->slave, &op);
}

static const struct spinand_info esmt_spinand_table[] = {
	SPINAND_INFO("f50l2g41lb", 0x0A,
		     NAND_MEMORG(1, 2048, 64, 64, 1024, 1, 1, 2),
		     NAND_ECCREQ(1, 512),
		     SPINAND_INFO_OP_VARIANTS(&read_cache_variants,
					      &write_cache_variants,
					      &update_cache_variants),
		     0,
		     SPINAND_ECCINFO(&f50l2g41lb_ooblayout, f50l2g41lb_ecc_get_status),
		     SPINAND_SELECT_TARGET(f50l2g41lb_select_target)),
};

/**
 * esmt_spinand_detect - initialize device related part in spinand_device
 * struct if it is a ESMT device.
 * @spinand: SPI NAND device structure
 */
static int esmt_spinand_detect(struct spinand_device *spinand)
{
	u8 *id = spinand->id.data;
	int ret;

	/*
	 * ESMT SPI NAND read ID need a dummy byte,
	 * so the first byte in raw_id is dummy.
	 */
	if (id[1] != SPINAND_MFR_ESMT)
		return 0;

	ret = spinand_match_and_init(spinand, esmt_spinand_table,
				     ARRAY_SIZE(esmt_spinand_table), id[2]);
	if (ret)
		return ret;

	return 1;
}

static const struct spinand_manufacturer_ops esmt_spinand_manuf_ops = {
	.detect = esmt_spinand_detect,
};

const struct spinand_manufacturer esmt_spinand_manufacturer = {
	.id = SPINAND_MFR_ESMT,
	.name = "ESMT",
	.ops = &esmt_spinand_manuf_ops,
};
