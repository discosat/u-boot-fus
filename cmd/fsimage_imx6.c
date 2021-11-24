/*
 * Copyright 2021 F&S Elektronik Systeme GmbH
 * Hartmut Keller <keller@fs-net.de>
 *
 * Handle F&S NBOOT images.
 *
 * SPDX-License-Identifier:	GPL-2.0+
 *
 * See board/F+S/common/fs_image_common.c for a description of the NBoot file
 * format.
 */

#include <common.h>
#include <mmc.h>
#include <nand.h>
#include <asm/mach-imx/checkboot.h>
#include <asm/arch-mx6/sys_proto.h>
#include <asm/mach-imx/boot_mode.h>
#include <jffs2/jffs2.h>		/* struct mtd_device + part_info */
#include "../board/F+S/common/fs_board_common.h"	/* fs_board_*() */
#include "../board/F+S/common/fs_mmc_common.h"	/* get_mmc_boot_device() */

/* Structure to hold regions in NAND/eMMC for an image */
struct storage_info {
	loff_t start;		/* List of start addresses */
	size_t size;		/* *-size entry */
	unsigned int count;	/* Number of entries in start */
};

static u8 local_buffer[0x200] __aligned(ARCH_DMA_MINALIGN);

/* Return the F&S architecture */
const char *fs_image_get_arch(void)
{
	return CONFIG_SYS_BOARD;
}

/* Report any error and return failure or success */
static int report_return_code(int return_code)
{
	if (return_code) {
		printf("Error: %d\n", return_code);
		return CMD_RET_FAILURE;
	} else {
		return CMD_RET_SUCCESS;
	}
}

/* ------------- NAND handling --------------------------------------------- */

#ifdef CONFIG_CMD_NAND

#ifdef CONFIG_FS_UPDATE_SUPPORT
	const char *uboot_mtd_names[] = {"UBoot_A", "UBoot_B"};
#elif defined CONFIG_SYS_NAND_U_BOOT_OFFS_REDUND
	const char *uboot_mtd_names[] = {"UBoot", "UBootRed"};
#else
	const char *uboot_mtd_names[] = {"UBoot"};
#endif

static int fs_image_setup_storage_info(struct storage_info *si,
				        struct part_info *part)
{
	if (part->offset && part->size) {
		if (part->size < CONFIG_BOARD_SIZE_LIMIT) {
			printf("partition size < CONFIG_BOARD_SIZE_LIMIT(%d),"
				"abbort!\n",CONFIG_BOARD_SIZE_LIMIT);
			return -EFBIG;
		}
		si->start = part->offset;
		si->size = CONFIG_BOARD_SIZE_LIMIT;
		return 0;
	}
	else {
		printf("Offset/Size unknown!\n");
		return - EINVAL;
	}
}

/* Get storage info */
static int fs_image_get_storage_info(struct storage_info *si, bool second)
{
	struct mtd_device *dev;
	u8 part_num;
	struct part_info *part;
	const char *name;
	int i, err;

	mtdparts_init();

	/* get partition name */
	for (i = 0; i < ARRAY_SIZE(uboot_mtd_names); i++) {
		name = uboot_mtd_names[i];
		if (find_dev_and_part(name, &dev, &part_num, &part)) {
			printf("WARNING: MTD %s not found\n", name);
			return -EINVAL;
		}
		/* break after first partition or take second partition */
		if (!second)
			break;
	}

	/* setup storage info */
	err = fs_image_setup_storage_info(si, part);

	return err;
}

/* Save the image to the given NAND offset */
static int fs_image_save_image_to_nand(struct mtd_info *mtd, void *buf,
				       size_t size, loff_t offs, loff_t lim,
				       const char *type)
{
	int err = 0;

	printf("  Writing %s (size 0x%zx) to offset 0x%llx... ",
	       type, size, offs);

	// ### TODO: On write fails, we have to mark a block as bad and repeat

	/*
	 * Write everything of the image but the first page with the header.
	 * If this is interrupted (e.g. due to a power loss), then the image
	 * will not be seen as valid when loading because of the missing
	 * header. So there is no problem with half-written files.
	 */
	if (size > mtd->writesize) {
		size -= mtd->writesize;
		err = nand_write_skip_bad(mtd, offs + mtd->writesize, &size,
			NULL, lim - mtd->writesize, buf + mtd->writesize,
			 WITH_WR_VERIFY);
		size = mtd->writesize;
	}

	/*
	 * Finally write the page with the header; if this succeeds, then we
	 * know that the whole image is completely written. If this is
	 * interrupted, then loading will fail either because of a
	 * bad header or because of a bad ECC. So again this prevents loading
	 * files that are not fully written.
	 */
	if (!err) {
		err = nand_write_skip_bad(mtd, offs, &size, NULL,
					  mtd->writesize, buf, WITH_WR_VERIFY);
	}
	if (err)
		printf("Failed (%d)\n", err);
	else
		puts("Done\n");

	return err;
}

static int fs_image_erase_nand(struct mtd_info *mtd, size_t size, loff_t offs,
			loff_t lim, const char *type1, const char *type2)
{
	int err;
	struct nand_erase_options opts = {0};

	printf("  Erasing %s", type1);
	if (type2)
		printf("/%s", type2);
	printf(" (size 0x%zx) at offset 0x%llx... ", size, offs);

	opts.length = size;
	opts.lim = size;
	opts.quiet = 1;
	opts.offset = offs;
	err = nand_erase_opts(mtd, &opts);
	if (err)
		printf("Failed (%d)\n", err);
	else
		puts("Done\n");

	return err;
}

/* Save U-Boot to NAND */
static int fs_image_save_uboot_to_nand(unsigned long addr)
{
	struct mtd_info *mtd = get_nand_dev_by_index(0);
	struct storage_info si;
	int err;

	err = fs_image_get_storage_info(&si, false);
	if (err)
		return err;

	/* Actually save image */
	printf("Saving %s to NAND:\n", "U-Boot");
	err = fs_image_erase_nand(mtd, si.size, si.start,
				si.size, uboot_mtd_names[0], NULL);
	if (!err) {
		err = fs_image_save_image_to_nand(mtd, (void *) addr, si.size,
				si.start, si.size, uboot_mtd_names[0]);
		if (err)
			return err;
	}
	else {
		return err;
	}
#ifdef  CONFIG_SYS_NAND_U_BOOT_OFFS_REDUND
	err = fs_image_get_storage_info(&si, true);
	if (err)
		return err;

	/* Actually save image */
	printf("Saving %s redund to NAND:\n", "U-Boot");
	err = fs_image_erase_nand(mtd, si.size, si.start,
				si.size, uboot_mtd_names[1], NULL);
	if (!err) {
		err = fs_image_save_image_to_nand(mtd, (void *) addr, si.size,
				si.start, si.size, uboot_mtd_names[1]);
		if (err)
			return err;
	}
	else {
		return err;
	}
#endif
	return err;
}
#endif /* CONFIG_CMD_NAND */


/* ------------- MMC handling ---------------------------------------------- */

#ifdef CONFIG_CMD_MMC

/* Save the image to the given MMC offset */
static int fs_image_save_image_to_mmc(struct blk_desc *blk_desc, void *buf,
				      unsigned int offs, unsigned int size,
				      const char *type)
{
	lbaint_t blk = offs / blk_desc->blksz;
	lbaint_t count = (size + blk_desc->blksz -1) / blk_desc->blksz;
	unsigned long n;
	int err = 0;

	printf("  Writing %s (size 0x%x) to offset 0x%x (block 0x"
	       LBAF ")... ", type, size, offs, blk);

	/*
	 * Write everything of the image but the first block with the header.
	 * If this is interrupted (e.g. due to a power loss), then the image
	 * will not be seen as valid when loading because of the missing
	 * header. So there is no problem with half-written files.
	 */
	if (count > 1) {
		n = blk_dwrite(blk_desc, blk+1, count-1, buf+blk_desc->blksz);
		if (n < count - 1)
			n = (unsigned long)-EIO;
		if (IS_ERR_VALUE(n))
			err = (int)n;
	}
	/*
	 * Finally write the block with the header; if this succeeds, then we
	 * know that the whole image is completely written.
	 */
	if (!err) {
		n = blk_dwrite(blk_desc, blk, 1, buf);
		if (n < 1)
			n = (unsigned long)-EIO;
		if (IS_ERR_VALUE(n))
			err = (int)n;
	}

	if (err)
		printf("Failed (%d)\n", err);
	else
		puts("Done\n");

	return err;
}

/* Invalidate an image by overwriting the first block with zeroes */
static int fs_image_invalidate_mmc(struct blk_desc *blk_desc,
				   unsigned int offs, const char *type)
{
	lbaint_t blk = offs / blk_desc->blksz;
	unsigned long n;
	int err = 0;

	printf("  Invalidating %s at offset 0x%x (block 0x" LBAF ")... ",
	       type, offs, blk);
	memset(local_buffer, 0, blk_desc->blksz);
	n = blk_dwrite(blk_desc, blk, 1, (unsigned char *)local_buffer);
	if (n < 1)
		n = (unsigned long)-EIO;
	if (IS_ERR_VALUE(n)) {
		err = (int)n;
		printf("Failed! (%d)\n", err);
		return err;
	}

	puts("Done\n");
	return 0;
}

/* Save U-Boot to eMMC */
static int fs_image_save_uboot_to_mmc(unsigned long addr, int mmc_dev)
{
	struct mmc *mmc = find_mmc_device(mmc_dev);
	struct blk_desc *blk_desc;
	unsigned int cur_part;
	int err;

	if (!mmc) {
		printf("mmc%d not found\n", mmc_dev);
		return -ENODEV;
	}
	blk_desc = mmc_get_blk_desc(mmc);

	/* Switch to User partition (0) */
	cur_part = mmc->part_config & PART_ACCESS_MASK;
	if (cur_part != 0) {
		err = blk_dselect_hwpart(blk_desc, 0);
		if (err) {
			printf("Cannot switch to part 0 on mmc%d\n", mmc_dev);
			return err;
		}
	}

	/* Actually save U-Boot */
	printf("Saving U-Boot to mmc%d, part 0:\n", mmc_dev);
	err = fs_image_invalidate_mmc(blk_desc, CONFIG_SYS_MMC_U_BOOT_START,
					uboot_mtd_names[0]);
	if (!err) {
		err = fs_image_save_image_to_mmc(blk_desc, (void *) addr,
			CONFIG_SYS_MMC_U_BOOT_START, CONFIG_BOARD_SIZE_LIMIT,
			uboot_mtd_names[0]);
		if (err)
			return err;
	}
	else {
		return err;
	}

	/* Switch back the previous partition */
	if (cur_part != 0) {
		int tmperr = blk_dselect_hwpart(blk_desc, cur_part);
		if (tmperr) {
			printf("WARNING: Cannot switch back to part %d on"
				" mmc%d (%d)\n", cur_part, mmc_dev, tmperr);
		}
	}

	return err;
}
#endif /* CONFIG_CMD_MMC */


/* ------------- Command implementation ------------------------------------ */

/* Show the F&S architecture */
static int do_fsimage_arch(cmd_tbl_t *cmdtp, int flag, int argc,
			   char * const argv[])
{
	printf("%s\n", fs_image_get_arch());

	return report_return_code(0);
}

LOADER_TYPE GetImageType(u32 addr)
{
	if (IS_UBOOT(addr))
		return LOADER_UBOOT;

	if (IS_UBOOT_IVT(addr))
		return LOADER_UBOOT_IVT;

	if (IS_UIMAGE(addr) || IS_ZIMAGE(addr))
		return LOADER_KERNEL;

	if (IS_UIMAGE_IVT(addr) || IS_ZIMAGE_IVT(addr))
		return LOADER_KERNEL_IVT;

	if (IS_DEVTREE(addr))
		return LOADER_FDT;

	if (IS_DEVTREE_IVT(addr))
		return LOADER_FDT_IVT;

	return LOADER_NONE;
}

/* Save the F&S NBoot image to the boot device (NAND or MMC) */
static int do_fsimage_save(cmd_tbl_t *cmdtp, int flag, int argc,
			   char * const argv[])
{
	int err;
	int mmc_dev;
	unsigned long addr;
	LOADER_TYPE type;

	if (argc > 1)
		addr = parse_loadaddr(argv[1], NULL);
	else
		addr = get_loadaddr();

	type = GetImageType(addr);
	if (type != LOADER_UBOOT && type != LOADER_UBOOT_IVT) {
		printf("type %d not supported yet!\n", type);
		return report_return_code(-EINVAL);
	}

	switch (fs_board_get_boot_dev_from_fuses()) {
		case SD1_BOOT:
		case MMC1_BOOT:
		case SD2_BOOT:
		case MMC2_BOOT:
		case SD3_BOOT:
		case MMC3_BOOT:
		case SD4_BOOT:
		case MMC4_BOOT:
			mmc_dev = get_mmc_boot_device();
			err = fs_image_save_uboot_to_mmc(addr, mmc_dev);
			break;
		case NAND_BOOT:
			mmc_dev = -1;
			err = fs_image_save_uboot_to_nand(addr);
			break;
		default:
			err = -ENODEV;
	}

	return report_return_code(err);
}

/* Subcommands for "fsimage" */
static cmd_tbl_t cmd_fsimage_sub[] = {
	U_BOOT_CMD_MKENT(arch, 0, 1, do_fsimage_arch, "", ""),
	U_BOOT_CMD_MKENT(save, 2, 0, do_fsimage_save, "", ""),
};

static int do_fsimage(cmd_tbl_t *cmdtp, int flag, int argc,
		      char * const argv[])
{
	cmd_tbl_t *cp;

	if (argc < 2)
		return CMD_RET_USAGE;

	/* Drop argv[0] ("fsimage") */
	argc--;
	argv++;

	cp = find_cmd_tbl(argv[0], cmd_fsimage_sub,
			  ARRAY_SIZE(cmd_fsimage_sub));
	if (!cp)
		return CMD_RET_USAGE;
	if (flag == CMD_FLAG_REPEAT && !cp->repeatable)
		return CMD_RET_SUCCESS;

	return cp->cmd(cmdtp, flag, argc, argv);
}

U_BOOT_CMD(fsimage, 4, 1, do_fsimage,
	   "Handle F&S images, e.g. U-BOOT",
	   "arch\n"
	   "    - Show F&S architecture\n"
	   "fsimage save [addr]\n"
	   "    - Save the F&S image (BOARD-CFG, SPL, FIRMWARE)\n"
	   "\n"
	   "If no addr is given, use loadaddr.\n"
);
