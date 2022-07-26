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
#include <command.h>
#include <mmc.h>
#include <nand.h>
#include <console.h>			/* confirm_yesno() */
#include <jffs2/jffs2.h>		/* struct mtd_device + part_info */
#include <fuse.h>			/* fuse_read() */

#include "../board/F+S/common/fs_board_common.h"	/* fs_board_*() */
#include "../board/F+S/common/fs_image_common.h"	/* fs_image_*() */

struct img_info {
	struct storage_info si;		/* Info where to store image */
	void *img;			/* Pointer to image */
	const char *type;		/* "BOARD-CFG", "FIRMWARE", "SPL" */
	const char *descr;		/* e.g. board architecture */
	unsigned int size;		/* Size of image */
};

union local_buf {
	struct fs_header_v1_0 fsh;	/* Space for one F&S header */
#ifdef CONFIG_CMD_MMC
	struct info_table {		/* Info table for secondary SPL (MMC) */
		u32 chip_num;		/* unused, set to 0 */
		u32 drive_type;		/* unused, set to 0 */
		u32 tag;		/* 0x00112233 */
		u32 first_sector;	/* Start sector of secondary SPL */
		u32 sector_count;	/* unused, set to 0 */
	} secondary;
	u8 block[0x200];		/* Space for one MMC sector */
#endif
};

static union local_buf local_buffer;


/* ------------- Common helper function ------------------------------------ */

/* Check if board configuration in OCRAM is OK and return the address */
static void *fs_image_get_cfg_addr_check(bool with_fs_header)
{
	struct fs_header_v1_0 *fsh = fs_image_get_cfg_addr(true);
	const char *type = "BOARD-CFG";

	if (!fs_image_match(fsh, type, NULL)) {
		printf("%s in OCRAM damaged\n", type);
		return NULL;
	}

	if (!with_fs_header)
		fsh++;

	return fsh;
}

/* Return the BOARD-ID; id must have room for MAX_DESCR_LEN characters */
static int fs_image_get_board_id(char *id)
{
	struct fs_header_v1_0 *fsh = fs_image_get_cfg_addr_check(true);

	if (!fsh)
		return -ENOENT;

	memcpy(id, fsh->param.descr, MAX_DESCR_LEN);

	return 0;
}

static bool fs_image_fits(struct img_info *img)
{
	if (img->size > img->si.size) {
		printf("%s with size 0x%x does not fit into slot of size"
		       " 0x%x\n", img->type, img->size, img->si.size);
		return false;
	}
	return true;
}

/* Load FIRMWARE and SPL info, check alignment and verify that images fit */
static int fs_image_get_nboot_storage(void *fdt, int offs, unsigned int align,
				      unsigned int align_blk_size,
				      struct img_info img[3])
{
	int err;
	unsigned int cfg_size;
	int i;

	err = fs_image_get_board_cfg_size(fdt, offs, align_blk_size, &cfg_size);
	if (err)
		return err;
	err = fs_image_get_nboot_info(fdt, offs, align, &img[1].si);
	if (err)
		return err;
	err = fs_image_get_spl_info(fdt, offs, align, &img[2].si);
	if (err)
		return err;

	/* Use start addresses for FIRMWARE also for BOARD-CFG */
	img[0].si.start = img[1].si.start;
	img[0].si.size = cfg_size;

	/*
	 * The FIRMWARE region is actually smaller. We cannot move the FIRMWARE
	 * start because start points directly into the FDT and is read-only.
	 */
	img[1].si.size -= cfg_size;

	/* Make sure that all images fit into their regions */
	for (i = 0; i < 3; i++) {
		if (!fs_image_fits(&img[i]))
			return -ENOMEM;
	}

	return 0;
}

static void fs_image_parse_image(unsigned long addr, unsigned int offs,
				 int level, unsigned int remaining)
{
	struct fs_header_v1_0 *fsh = (struct fs_header_v1_0 *)(addr + offs);
	bool had_sub_image = false;
	unsigned int size;
	char info[MAX_DESCR_LEN + 1];
	int i;


	/* Show info for this image */
	printf("%08x %08x", offs, remaining);
	for (i = 0; i < level; i++)
		putc(' ');
	if (fsh->type[0]) {
		memcpy(info, fsh->type, MAX_TYPE_LEN);
		info[MAX_TYPE_LEN] = '\0';
		printf(" %s", info);
	}
	if ((fsh->info.flags & FSH_FLAGS_DESCR) && fsh->param.descr[0]) {
		memcpy(info, fsh->param.descr, MAX_DESCR_LEN);
		info[MAX_DESCR_LEN] = '\0';
		printf(" (%s)", info);
	}
	puts("\n");
	offs += FSH_SIZE;
	level++;

	/* Handle subimages */
	while (remaining > 0) {
		fsh = (struct fs_header_v1_0 *)(addr + offs);
		if (fs_image_is_fs_image(fsh)) {
			had_sub_image = true;
			size = fs_image_get_size(fsh, false);
			fs_image_parse_image(addr, offs, level, size);
			size += FSH_SIZE;
		} else {
			size = remaining;
			if (had_sub_image) {
				printf("%08x %08x", offs, size);
				for (i = 0; i < level; i++)
					putc(' ');
				puts(" [unkown data]\n");
			}
		}
		offs += size;
		remaining -= size;
	}
}

/*
 * Return the header of the given sub-image withing an F&S image or NULL if not
 * found. If img is not NULL, also fill in the struct with or without header.
 */
struct fs_header_v1_0 *fs_image_find(struct fs_header_v1_0 *fsh,
				     const char *type, const char *descr,
				     struct img_info *img, bool with_header)
{
	unsigned int size;
	unsigned int remaining;

	remaining = fs_image_get_size(fsh++, false);
	while (remaining > 0) {
		if (!fs_image_is_fs_image(fsh))
			break;
		size = fs_image_get_size(fsh, true);
		if (fs_image_match(fsh, type, descr)) {
			if (img) {
				img->type = type;
				img->descr = descr;
				if (with_header) {
					img->img = fsh;
					img->size = size;
				} else {
					img->img = fsh + 1;
					img->size = size - FSH_SIZE;
				}
			}
			return fsh;
		}
		fsh = (struct fs_header_v1_0 *)((void *)fsh + size);
		remaining -= size;
	}

	printf("No %s found for %s\n", type, descr);

	return NULL;
}

static int fs_image_confirm(void)
{
	puts("Are you sure? [y/N] ");
	return confirm_yesno();
}

/* Check boot device */
static int fs_image_check_bootdev(void *fdt, const char **used_boot_dev)
{
	int offs;
	const char *boot_dev;
	enum boot_device boot_dev_cfg;
	enum boot_device boot_dev_fuses;

	/* Check for valid boot device */
	offs = fs_image_get_cfg_offs(fdt);
	boot_dev = fdt_getprop(fdt, offs, "boot-dev", NULL);
	boot_dev_cfg = fs_board_get_boot_dev_from_name(boot_dev);
	if (boot_dev_cfg == UNKNOWN_BOOT) {
		printf("Unknown boot device %s in BOARD-CFG\n", boot_dev);
		return -EINVAL;
	}

	*used_boot_dev = boot_dev;
	boot_dev_fuses = fs_board_get_boot_dev_from_fuses();
	if (boot_dev_fuses != boot_dev_cfg) {
		if (boot_dev_fuses != USB_BOOT) {
			printf("Error: New BOARD-CFG wants to boot from %s but"
			       " board is already fused\nfor %s. Rejecting"
			       " to save this configuration.\n", boot_dev,
			       fs_board_get_name_from_boot_dev(boot_dev_fuses));
			return -EINVAL;
		}
		return 1;
	}

	return 0;
}

/*
 * Get pointer to BOARD-CFG image that is to be used and to NBOOT part
 * Returns: <0: error; 0: aborted by user; >0: proceed to save
 */
static int fs_image_find_board_cfg(unsigned long addr, bool force,
				   struct fs_header_v1_0 **used_cfg,
				   struct fs_header_v1_0 **nboot)
{
	struct fs_header_v1_0 *fsh = (struct fs_header_v1_0 *)addr;
	char id[MAX_DESCR_LEN + 1];
	char old_id[MAX_DESCR_LEN + 1];
	void *fdt;
	struct fs_header_v1_0 *cfg;
	const char *arch = fs_image_get_arch();
	unsigned int size, remaining;
	const char *nboot_version;
	int err;

	*used_cfg = NULL;

	if (!fs_image_is_fs_image(fsh)) {
		printf("No F&S image found at address 0x%lx\n", addr);
		return -ENOENT;
	}

	/* Use current BOARD-ID for saving */
	err = fs_image_get_board_id(id);
	if (err)
		return err;
	id[MAX_DESCR_LEN] = '\0';

	/* In case of an NBoot image with prepended BOARD-ID, use this ID */
	if (fs_image_match(fsh, "BOARD-ID", NULL)) {
		memcpy(old_id, id, MAX_DESCR_LEN + 1);
		memcpy(id, fsh->param.descr, MAX_DESCR_LEN);
		if (strcmp(id, old_id)) {
			printf("Warning! Current board is %s but you want to\n"
			       "save for %s\n", old_id, id);
			if (!force && !fs_image_confirm()) {
				puts("Aborted by user, nothing changed.");
				return 0; /* used_cfg == NULL in this case */
			}
		}
		fsh++;
	}

	if (!fs_image_match(fsh, "NBOOT", arch)) {
		printf("No NBOOT image for %s found at address 0x%lx\n",
		       arch, addr);
		return -EINVAL;
	}

	/* Look for BOARD-CONFIGS subimage and search for matching BOARD-CFG */
	cfg = fs_image_find(fsh, "BOARD-CONFIGS", arch, NULL, true);
	if (!cfg) {
		printf("No BOARD-CONFIGS found for arch %s\n", arch);
		return -ENOENT;
	}

	remaining = fs_image_get_size(cfg++, false);
	fs_image_set_board_id_compare(id);
	while (1) {
		if (!remaining || !fs_image_is_fs_image(cfg)) {
			printf("No BOARD-CFG found for BOARD-ID %s\n", id);
			return -ENOENT;
		}
		if (fs_image_match_board_id(cfg, "BOARD-CFG"))
			break;
		size = fs_image_get_size(cfg, true);
		remaining -= size;
		cfg = (struct fs_header_v1_0 *)((void *)cfg + size);
	}

	/* Get and show NBoot version as noted in BOARD-CFG */
	fdt = (void *)(cfg + 1);
	nboot_version = fs_image_get_nboot_version(fdt);
	if (!nboot_version) {
		puts("Unknown NBOOT version, rejecting to save\n");
		return -EINVAL;
	}
	printf("Found NBOOT version %s\n", nboot_version);

	*used_cfg = cfg;
	if (nboot)
		*nboot = fsh;

	return 1;			/* Proceed */
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

/* Load the image of given type/descr from NAND flash at given offset */
static int fs_image_load_image_from_nand(struct mtd_info *mtd,
					 unsigned int offs, unsigned int lim,
					 struct img_info *img)
{
	size_t size;
	int err;

	/* Read F&S header */
	size = FSH_SIZE;
	err = nand_read_skip_bad(mtd, offs, &size, NULL, lim,
				 (u_char *)&local_buffer.fsh);
	if (err)
		return err;

	/* Check type */
	if (!fs_image_match(&local_buffer.fsh, img->type, img->descr))
		return -ENOENT;

	/* Load whole image incl. header */
	size = fs_image_get_size(&local_buffer.fsh, true);
	err = nand_read_skip_bad(mtd, offs, &size, NULL, lim, img->img);
	if (err)
		return err;

	img->size = size;

	return 0;
}

/* Load the FIRMWARE from NAND to addr */
static int fs_image_load_firmware_from_nand(void *fdt, struct img_info *img)
{
	unsigned int start;
	int err;
	int i;
	struct mtd_info *mtd = get_nand_dev_by_index(0);
	int offs = fs_image_get_info_offs(fdt);
	unsigned int cfg_size;

	err = fs_image_get_board_cfg_size(fdt, offs, mtd->writesize, &cfg_size);
	if (err)
		return err;

	err = fs_image_get_nboot_info(fdt, offs, mtd->erasesize, &img->si);
	if (err)
		return err;

	for (i = 0; i < img->si.count; i++) {
		start = fdt32_to_cpu(img->si.start[i]);
		printf("Trying copy %d from NAND at 0x%x\n", i, start);
		err = fs_image_load_image_from_nand(
			mtd, start + cfg_size, img->si.size - cfg_size, img);
		if (!err)
			break;
	}

	if (err)
		printf("Loading %s failed (%d)\n", img->type, err);

	return err;
}

/* Save the image to the given NAND offset */
static int fs_image_save_image_to_nand(struct mtd_info *mtd, void *buf,
				       size_t size, loff_t offs, loff_t lim,
				       const char *type)
{
	int err = 0;

	printf("  Writing %s (size 0x%zx) to offset 0x%llx...",
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
					  NULL, lim - mtd->writesize,
					  buf + mtd->writesize, WITH_WR_VERIFY);
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

/* Issue warning if U-Boot MTD partitions do not match nboot-info */
static bool fs_image_check_uboot_mtd(struct storage_info *si)
{
	struct mtd_device *dev;
	u8 part_num;
	struct part_info *part;
	const char *name;
	int i;
	u64 start;
	bool warning = false;

	mtdparts_init();

	for (i = 0; i < ARRAY_SIZE(uboot_mtd_names); i++) {
		name = uboot_mtd_names[i];
		if (find_dev_and_part(name, &dev, &part_num, &part)) {
			printf("WARNING: MTD %s not found\n", name);
			warning = true;
		} else {
			start = fdt32_to_cpu(si->start[i]);
			if (part->offset != start) {
				printf("WARNING: MTD %s starts on 0x%llx but"
				       " should start on 0x%llx.\n",
				       name, part->offset, start);
				warning = true;
			}
			if (part->size != (u64)(si->size)) {
				printf("WARNING: MTD %s has size 0x%llx but"
				       " should have size 0x%llx.\n",
				       name, part->offset, start);
				warning = true;
			}
		}
	}
	return warning;
}

/* Save NBoot to NAND (BOARD-CFG, FIRMWARE; ### TODO: also SPL) */
static int fs_image_save_nboot_to_nand(void *fdt, struct img_info img[3])
{
	struct mtd_info *mtd = get_nand_dev_by_index(0);
	int offs = fs_image_get_info_offs(fdt);
	unsigned int size, cfg_size;
	int err, lasterr;
	int i;
	int success = 0;
	struct storage_info si;
	u64 start;

	err = fs_image_get_nboot_storage(fdt, offs, mtd->erasesize,
					 mtd->writesize, img);
	if (err)
		return err;

	/* Issue warning if UBoot MTD partitions are missing or differ */
	err = fs_image_get_uboot_info(fdt, offs, mtd->erasesize, &si);
	if (err)
		return err;
	fs_image_check_uboot_mtd(&si);

	cfg_size = img[0].si.size;
	size = cfg_size + img[1].si.size;

	lasterr = 0;
	for (i = 0; i < img[1].si.count; i++) {
		printf("Saving copy %d to NAND:\n", i);

		/* Erase BOARD-CFG/FIRMWARE */
		start = fdt32_to_cpu(img[1].si.start[i]);
		err = fs_image_erase_nand(mtd, size, start, size,
					  img[0].type, img[1].type);

		/*
		 * Write FIRMWARE first. We assume that the FIRMWARE starts
		 * within the same NAND block as the BOARD-CFG; so we do not
		 * have to care about bad blocks inbetween. If the first block
		 * is bad, we will skip it here in the same way as later when
		 * writing the BOARD-CFG.
		 */
		if (!err) {
			err = fs_image_save_image_to_nand(
				mtd, img[1].img, img[1].size, start+cfg_size,
				size-cfg_size, img[1].type);
		}

		/* Write BOARD-CFG */
		if (!err) {
			err = fs_image_save_image_to_nand(
				mtd, img[0].img, img[0].size, start,
				size, img[0].type);
		}

		/* ### TODO: Erase/Write SPL incl. FCB/DBBT to img[2].si here */
		if (err)
			lasterr = err;
		else
			success++;
	}

	if (success < i) {
		printf("WARNING: Only %d of %d copies were successful\n",
		       success, i);
	}

	//### TODO: Write SPL incl. FCB/DBBT to img[2].si
	puts("WARNING: Writing SPL skipped, use kobs to write it!\n");

	/* Set the new BOARD-CFG as current */
	if (success)
		memcpy(fs_image_get_cfg_addr(true), img[0].img, img[0].size);

	return lasterr;
}

/* Save U-Boot to NAND */
static int fs_image_save_uboot_to_nand(void *fdt, struct img_info *img,
				       int copy, bool force)
{
	struct mtd_info *mtd = get_nand_dev_by_index(0);
	int offs = fs_image_get_info_offs(fdt);
	int err;
	u64 start;

	/* Issue warning if UBoot MTD partitions are missing or differ */
	err = fs_image_get_uboot_info(fdt, offs, mtd->erasesize, &img->si);
	if (err)
		return err;

	/* Ask for confirmation in case of warning */
	if (fs_image_check_uboot_mtd(&img->si)) {
		if (!force) {
			printf("Saving %s to offset 0x%x with size 0x%x. ",
			       img->type, fdt32_to_cpu(img->si.start[copy]),
			       img->si.size);
			if (!fs_image_confirm())
				return -EINVAL;
		}
	}

	/* Actually save image */
	printf("Saving %s to NAND:\n", img->type);
	start = fdt32_to_cpu(img->si.start[copy]);
	err = fs_image_erase_nand(mtd, img->si.size, start, img->si.size,
				  img->type, NULL);
	if (!err) {
		err = fs_image_save_image_to_nand(
		     mtd, img->img, img->size, start, img->si.size, img->type);
	}

	return err;
}
#endif /* CONFIG_CMD_NAND */


/* ------------- MMC handling ---------------------------------------------- */

#ifdef CONFIG_CMD_MMC
/* Load the image of given type/descr from eMMC at given offset */
static int fs_image_load_image_from_mmc(struct blk_desc *blk_desc,
					unsigned int offs, struct img_info *img)
{
	size_t size;
	unsigned long n;
	lbaint_t blk = offs / blk_desc->blksz;
	lbaint_t count;

	/* Read F&S header */
	n = blk_dread(blk_desc, blk, 1, &local_buffer.block);
	if (IS_ERR_VALUE(n))
		return (int)n;
	if (n < 1)
		return -EIO;

	/* Check type */
	if (!fs_image_match(&local_buffer.fsh, img->type, img->descr))
		return -ENOENT;

	/* Load whole image incl. header */
	size = fs_image_get_size(&local_buffer.fsh, true);
	count = (size + blk_desc->blksz - 1) / blk_desc->blksz;
	n = blk_dread(blk_desc, blk, count, img->img);
	if (IS_ERR_VALUE(n))
		return (int)n;
	if (n < count)
		return -EIO;

	img->size = size;

	return 0;
}

/* Load the FIRMWARE from eMMC to addr */
static int fs_image_load_firmware_from_mmc(void *fdt, struct img_info *img,
					   int mmc_dev)
{
	unsigned int start;
	int err;
	int i;
	struct mmc *mmc = find_mmc_device(mmc_dev);
	struct blk_desc *blk_desc;
	int offs = fs_image_get_info_offs(fdt);
	unsigned int cfg_size;
	unsigned int cur_part, boot_part;

	if (!mmc) {
		printf("mmc%d not found\n", mmc_dev);
		return -ENODEV;
	}
	blk_desc = mmc_get_blk_desc(mmc);

	err = fs_image_get_board_cfg_size(fdt, offs, blk_desc->blksz,
					  &cfg_size);
	if (err)
		return err;

	err = fs_image_get_nboot_info(fdt, offs, blk_desc->blksz, &img->si);
	if (err)
		return err;

	cur_part = mmc->part_config & PART_ACCESS_MASK;
	boot_part = (mmc->part_config >> 3) & PART_ACCESS_MASK;
	if (boot_part == 7)
		boot_part = 0;
	if (cur_part != boot_part) {
		err = blk_dselect_hwpart(blk_desc, boot_part);
		if (err) {
			printf("Cannot switch to part %d on mmc%d\n",
			       boot_part, mmc_dev);
			return err;
		}
	}
	for (i = 0; i < img->si.count; i++) {
		start = fdt32_to_cpu(img->si.start[i]);
		printf("Trying copy %d at mmc%d, part %d, offset 0x%x\n",
		       i, mmc_dev, boot_part, start);
		err = fs_image_load_image_from_mmc(blk_desc,
						   start + cfg_size, img);
		if (!err)
			break;
	}
	if (cur_part != boot_part) {
		int tmperr = blk_dselect_hwpart(blk_desc, cur_part);
		if (tmperr) {
			printf("Cannot switch back to part %d on mmc%d (%d)\n",
			       cur_part, mmc_dev, tmperr);
		}
	}

	return err;
}

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
	memset(&local_buffer.block, 0, blk_desc->blksz);
	n = blk_dwrite(blk_desc, blk, 1, &local_buffer.block);
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

/* Save NBoot to eMMC (BOARD-CFG, FIRMWARE, SPL) */
static int fs_image_save_nboot_to_mmc(void *fdt, struct img_info img[3],
				      int mmc_dev)
{
	struct mmc *mmc = find_mmc_device(mmc_dev);
	struct blk_desc *blk_desc;
	int offs = fs_image_get_info_offs(fdt);
	int err, lasterr;
	unsigned int i, count;
	int success = 0;
	unsigned int cur_part, boot_part;
	unsigned long start;

	if (!mmc) {
		printf("mmc%d not found\n", mmc_dev);
		return -ENODEV;
	}
	blk_desc = mmc_get_blk_desc(mmc);

	err = fs_image_get_nboot_storage(fdt, offs, blk_desc->blksz,
					 blk_desc->blksz, img);
	if (err)
		return err;

	/* Determine the boot partition */
	boot_part = (mmc->part_config >> 3) & PART_ACCESS_MASK;
	if (boot_part == 7)
		boot_part = 0;

#if defined(CONFIG_IMX8) || defined(CONFIG_IMX8MN)
	/* If booting from User space, check the fused secondary image offset */
	if (!boot_part && img[2].si.count > 1) {
		u32 secondary_offset_fuses = fs_board_get_secondary_offset();
		u32 secondary_offset_nboot = fdt32_to_cpu(img[2].si.start[1]);

		if (secondary_offset_fuses != secondary_offset_nboot) {
			printf("Secondary Image Offset in fuses is at 0x%08x,"
			       " NBOOT wants 0x%08x\n"
			       "Fix this first (e.g. burn fuses)!\n",
			       secondary_offset_fuses, secondary_offset_nboot);
			return -EINVAL;
		}
	}
#endif

	/*
	 * Save sequence:
	 * 1. Invalidate BOARD-CFG by overwriting the 1st block (F&S header).
	 *    If interrupted here, any copy of (old) SPL will use the second
	 *    (old) copy of the BOARD-CFG that matches the old SPL.
	 * 2. Write FIRMWARE. If interrupted here, the same happens as in 2.
	 * 3. Write BOARD-CFG.
	 * 4. Invalidate SPL by overwriting the 1st block (IVT). If
	 *    interrupted here, the second (old) copy of SPL will use the
	 *    second (old) copy of the BOARD-CFG that matches the old SPL.
	 * 5. Write SPL. If this is written, then the new SPL is valid. If
	 *    interrupted now, the first (new) copy of SPL will use the first
	 *    (new) copy of the BOARD-CFG that matches the new SPL. If the
	 *    first copy of SPL can not be loaded for some reason, the second
	 *    copy is loaded and also the second copy of BOARD-CFG, i.e. the
	 *    system boots the old version.
	 * 6. Update the second copies in the same sequence.
	 * 7. Update the information block for the secondary SPL.
	 *
	 * There is a small risk that SPL will load a BOARD-CFG copy that does
	 * not match, e.g. if interrupted between 3. and 4. or in 5. when the
	 * second copy of the BOARD-CFG is damaged and the first is loaded.
	 * However this is only a problem if the BOARD-CFG has changed
	 * significantly. Typically an old SPL can also handle a new BOARD-CFG
	 * and vice versa.
	 *
	 * ### TODO: Actually make sure that the second copy of BOARD-CFG is
         *     loaded first when the system booted from the secondary SPL,
         *     i.e. implement fs_image_get_start_index().
	 */
	lasterr = 0;
	cur_part = mmc->part_config & PART_ACCESS_MASK;
	count = max(img[1].si.count, img[2].si.count);
	if (count > 2)
		count = 2;		/* Ignore any extra values */

	for (i = 0; i < count; i++) {
#if defined(CONFIG_IMX8) || defined(CONFIG_IMX8MN)
		/* Switch to other boot partition */
		if (boot_part && (i > 0))
			boot_part = 3 - boot_part;
#endif
		/* Switch to the partition that we boot from */
		if ((mmc->part_config & PART_ACCESS_MASK) != boot_part) {
			err = blk_dselect_hwpart(blk_desc, boot_part);
			if (err) {
				printf("Cannot switch to part %d on mmc%d\n",
				       boot_part, mmc_dev);
				return err;
			}
		}
		printf("Saving copy %d to mmc%d, part %d:\n",
		       i, mmc_dev, boot_part);

		err = 0;
		if (i < img[1].si.count) {
			/* Invalidate BOARD-CFG/FIRMWARE */
#if defined(CONFIG_IMX8) || defined(CONFIG_IMX8MN)
			/* Only use first entry when writing to boot part */
			if (boot_part)
				start = fdt32_to_cpu(img[1].si.start[0]);
			else
#endif
				start = fdt32_to_cpu(img[1].si.start[i]);
			err = fs_image_invalidate_mmc(blk_desc, start,
						      img[0].type);
			if (!err) {
				/* Write FIRMWARE */
				err = fs_image_save_image_to_mmc(
					blk_desc, img[1].img,
					start + img[0].si.size,
					img[1].size, img[1].type);
			}
			if (!err) {
				/* Write BOARD-CFG */
				err = fs_image_save_image_to_mmc(
					blk_desc, img[0].img, start,
					img[0].size, img[0].type);
			}
		}
		if (!err && (i < img[1].si.count)) {
			/* Invalidate SPL */
#if defined(CONFIG_IMX8) || defined(CONFIG_IMX8MN)
			if (boot_part)
				start = 0; /* Always 0 in boot part */
			else
#endif
				start = fdt32_to_cpu(img[2].si.start[i]);
			err = fs_image_invalidate_mmc(blk_desc, start,
						      img[2].type);
			if (!err) {
				/* Write SPL */
				err = fs_image_save_image_to_mmc(
					blk_desc, img[2].img, start,
					img[2].size, img[2].type);
			}
#if defined(CONFIG_IMX8) || defined(CONFIG_IMX8MN)
			if (!err && (i == 1)) {
				/*
				 * Write Secondary Image Table for redundant
				 * SPL; the first_sector entry is counted
				 * relative to 0x8000 (0x40 blocks) and also
				 * the two skipped blocks for MBR and Secondary
				 * Image Table must be included, even if empty,
				 * in the secondary case, which results in
				 * subtracting 0x42 blocks.
				 */
				memset(&local_buffer.block, 0, blk_desc->blksz);
				local_buffer.secondary.tag = 0x00112233;
				local_buffer.secondary.first_sector =
					start / blk_desc->blksz - 0x42;
				/* This table is one block before primary SPL */
				start = fdt32_to_cpu(img[2].si.start[0]);
				start -= blk_desc->blksz;
				err = fs_image_save_image_to_mmc(
					blk_desc, &local_buffer.block, start,
					blk_desc->blksz, "SECONDARY-SPL-INFO");
			}
#endif
		}

		if (err)
			lasterr = err;
		else
			success++;
	}

	if (success < i) {
		printf("WARNING: Only %d of %d copies were successful\n",
		       success, i);
	}

	/* Switch back the previous partition */
	if (cur_part != boot_part) {
		int tmperr = blk_dselect_hwpart(blk_desc, cur_part);
		if (tmperr) {
			printf("WARNING: Cannot switch back to part %d on"
			       " mmc%d (%d)\n", cur_part, mmc_dev, tmperr);
		}
	}

	/* Set the new BOARD-CFG as current */
	if (success)
		memcpy(fs_image_get_cfg_addr(true), img[0].img, img[0].size);

	return lasterr;
}

/* Save U-Boot to eMMC */
static int fs_image_save_uboot_to_mmc(void *fdt, struct img_info *img,
				       int copy, int mmc_dev)
{
	struct mmc *mmc = find_mmc_device(mmc_dev);
	struct blk_desc *blk_desc;
	int offs = fs_image_get_info_offs(fdt);
	int err;
	unsigned int cur_part;
	unsigned long start;

	if (!mmc) {
		printf("mmc%d not found\n", mmc_dev);
		return -ENODEV;
	}
	blk_desc = mmc_get_blk_desc(mmc);

	err = fs_image_get_uboot_info(fdt, offs, blk_desc->blksz, &img->si);
	if (err)
		return err;

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
	start = fdt32_to_cpu(img->si.start[copy]);
	printf("Saving %s to mmc%d, part 0:\n", img->type, mmc_dev);
	err = fs_image_invalidate_mmc(blk_desc, start, img->type);
	if (!err)
		err = fs_image_save_image_to_mmc(blk_desc, img->img, start,
						 img->size, img->type);

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

static int fsimage_save_uboot(struct fs_header_v1_0 *fsh, bool force)
{
	void *fdt;
	const char *arch = fs_image_get_arch();
	const char *boot_dev;
	struct img_info img;
	int ret;
	int copy;

	img.type = "U-BOOT";
	img.descr = arch;
	img.img = fsh + 1;
	img.size = fs_image_get_size(fsh, false);
	if (!fs_image_match(fsh, img.type, img.descr)) {
		printf("%s image not valid for %s\n", img.type, img.descr);
		return 1;
	}

	fdt = fs_image_get_cfg_addr_check(false);
	if (!fdt)
		return 1;
	ret = fs_image_check_bootdev(fdt, &boot_dev);
	if (ret < 0)
		return 1;

	/* ### TODO: set copy depending on Set A or B (or redundant copy) */
	copy = 0;
	switch (fs_board_get_boot_dev_from_name(boot_dev)) {
#ifdef CONFIG_NAND_MXS
	case NAND_BOOT:
		ret = fs_image_save_uboot_to_nand(fdt, &img, copy, force);
		break;
#endif

#ifdef CONFIG_MMC
	case MMC1_BOOT:
		ret = fs_image_save_uboot_to_mmc(fdt, &img, copy, 0);
		break;
	case MMC3_BOOT:
		ret = fs_image_save_uboot_to_mmc(fdt, &img, copy, 2);
		break;
#endif

	default:
		printf("Saving %s to boot device %s not available\n",
		       img.type, boot_dev);
		ret = -EINVAL;
		break;
	}

	printf("Saving %s %s\n", img.type,
	       ret ? "incomplete or failed" : "complete");

	return ret ? 1 : 0;
}

/* ------------- Command implementation ------------------------------------ */

/* Show the F&S architecture */
static int do_fsimage_arch(cmd_tbl_t *cmdtp, int flag, int argc,
			   char * const argv[])
{
	printf("%s\n", fs_image_get_arch());

	return 0;
}

/* Show the current BOARD-ID */
static int do_fsimage_boardid(cmd_tbl_t *cmdtp, int flag, int argc,
			      char * const argv[])
{
	char id[MAX_DESCR_LEN + 1];

	if (fs_image_get_board_id(id))
		return 1;

	id[MAX_DESCR_LEN] = '\0';
	printf("%s\n", id);

	return 0;
}

#ifdef CONFIG_CMD_FDT
/* Print FDT content of current BOARD-CFG */
static int do_fsimage_boardcfg(cmd_tbl_t *cmdtp, int flag, int argc,
			       char * const argv[])
{
	void *fdt = fs_image_get_cfg_addr_check(false);

	if (!fdt)
		return 1;

	printf("FDT part of BOARD-CFG located at 0x%lx\n", (ulong)fdt);

	//return fdt_print(fdt, "/", NULL, 5);
	return 0;
}
#endif

/* Load the FIRMWARE image from the boot device (NAND or MMC) to DRAM */
static int do_fsimage_firmware(cmd_tbl_t *cmdtp, int flag, int argc,
			       char * const argv[])
{
	void *fdt;
	struct img_info img;
	int offs;
	const char *boot_dev;
	enum boot_device boot_dev_cfg;
	int ret = 0;
	unsigned long addr;

	if (argc > 1)
		addr = simple_strtoul(argv[1], NULL, 16);
	else
		addr = image_load_addr;

	fdt = fs_image_get_cfg_addr_check(false);
	if (!fdt)
		return 1;

	/* Load a FIRMWARE image */
	img.type = "FIRMWARE";
	img.descr = fs_image_get_arch();
	img.img = (void *)addr;

	offs = fs_image_get_cfg_offs(fdt);
	boot_dev = fdt_getprop(fdt, offs, "boot-dev", NULL);
	boot_dev_cfg = fs_board_get_boot_dev_from_name(boot_dev);

	switch (boot_dev_cfg) {
#ifdef CONFIG_NAND_MXS
	case NAND_BOOT:
		ret = fs_image_load_firmware_from_nand(fdt, &img);
		break;
#endif

#ifdef CONFIG_MMC
	case MMC3_BOOT:
		ret = fs_image_load_firmware_from_mmc(fdt, &img, 2);
		break;
#endif

	default:
		printf("Cannot handle boot device %s\n", boot_dev);
		return 1;
	}

	if (ret == -ENOENT)
		printf("No valid %s found.\n", img.type);
	else if (ret)
		printf("Loading %s failed (%d)\n", img.type, ret);
	else {
		/* Set parameters for loaded file */
		env_set_hex("fileaddr", addr);
		env_set_hex("filesize", img.size);
		printf("%s with size 0x%x loaded to address 0x%lx\n",
		       img.type, img.size, addr);
	}

	return ret ? 1 : 0;
}

/* List contents of an F&S image */
static int do_fsimage_list(cmd_tbl_t *cmdtp, int flag, int argc,
			   char * const argv[])
{
	unsigned long addr;
	struct fs_header_v1_0 *fsh;

	if (argc > 1)
		addr = simple_strtoul(argv[1], NULL, 16);
	else
		addr = image_load_addr;

	fsh = (struct fs_header_v1_0 *)addr;
	if (!fs_image_is_fs_image(fsh)) {
		printf("No F&S image found at addr 0x%lx\n", addr);
		return 1;
	}

	printf("offset   size     type (description)\n");
	printf("------------------------------------------------------------"
	       "-------------------\n");

	fs_image_parse_image(addr, 0, 0, fs_image_get_size(fsh, false));

	return 0;
}

/* Save the F&S NBoot image to the boot device (NAND or MMC) */
static int do_fsimage_save(cmd_tbl_t *cmdtp, int flag, int argc,
			   char * const argv[])
{
	struct fs_header_v1_0 *cfg;
	struct fs_header_v1_0 *nboot;
	struct img_info img[3];
	const char *arch = fs_image_get_arch();
	const char *boot_dev;
	void *fdt;
	int ret;
	unsigned long addr;
	bool force = false;
	const char *type = "NBOOT";

	if ((argc > 1) && (argv[1][0] == '-')) {
		if (strcmp(argv[1], "-f"))
			return CMD_RET_USAGE;
		force = true;
		argv++;
		argc--;
	}
	if (argc > 1)
		addr = simple_strtoul(argv[1], NULL, 16);
	else
		addr = image_load_addr;

	/* If this is an U-Boot image, handle separately */
	if (fs_image_match((void *)addr, "U-BOOT", NULL))
		return fsimage_save_uboot((void *)addr, force);

	ret = fs_image_find_board_cfg(addr, force, &cfg, &nboot);
	if (ret <= 0)
		return 1;

	fdt = (void *)(cfg + 1);
	ret = fs_image_check_bootdev(fdt, &boot_dev);
	if (ret < 0)
		return 1;
	if (ret > 0) {
		printf("Warning! Boot fuses not yet set, remember to burn"
		       " them for %s\n", boot_dev);
	}

	/* Prepare img[0]: BOARD-CFG */
	img[0].type = "BOARD-CFG";
	img[0].descr = arch;
	img[0].img = cfg;
	img[0].size = fs_image_get_size(cfg, true);

	/* Prepare img[1]: FIRMWARE */
	if (!fs_image_find(nboot, "FIRMWARE", arch, &img[1], true))
		return 1;

	/* Prepare img[2]: SPL */
	if (!fs_image_find(nboot, "SPL", arch, &img[2], false))
		return 1;

	/* Found all sub-images, let's go and save NBoot */
	switch (fs_board_get_boot_dev_from_name(boot_dev)) {
#ifdef CONFIG_NAND_MXS
	case NAND_BOOT:
		ret = fs_image_save_nboot_to_nand(fdt, img);
		break;
#endif

#ifdef CONFIG_MMC
	case MMC1_BOOT:
		ret = fs_image_save_nboot_to_mmc(fdt, img, 0);
		break;
	case MMC3_BOOT:
		ret = fs_image_save_nboot_to_mmc(fdt, img, 2);
		break;
#endif

	default:
		printf("Saving %s to boot device %s not available\n",
		       type, boot_dev);
		ret = -EINVAL;
		break;
	}

	printf("Saving %s %s\n", type,
	       ret ? "incomplete or failed" : "complete");

	return ret ? 1 : 0;
}

/* Burn the fuses according to the NBoot in DRAM */
static int do_fsimage_fuse(cmd_tbl_t *cmdtp, int flag, int argc,
			   char * const argv[])
{
	struct fs_header_v1_0 *cfg;
	void *fdt;
	int offs;
	int ret;
	const char *boot_dev;
	unsigned int cur_val, fuse_val, fuse_mask, fuse_bw;
	const fdt32_t *fvals, *fmasks, *fbws;
	int i, len, len2, len3;
	unsigned long addr;
	bool force = false;

	if ((argc > 1) && (argv[1][0] == '-')) {
		if (strcmp(argv[1], "-f"))
			return CMD_RET_USAGE;
		force = true;
		argv++;
		argc--;
	}
	if (argc > 1)
		addr = simple_strtoul(argv[1], NULL, 16);
	else
		addr = image_load_addr;

	ret = fs_image_find_board_cfg(addr, force, &cfg, NULL);
	if (ret <= 0)
		return 1;

	fdt = (void *)(cfg + 1);
	ret = fs_image_check_bootdev(fdt, &boot_dev);
	if (ret < 0)
		return 1;

	/* No contradictions, do an in-depth check */
	offs = fs_image_get_cfg_offs(fdt);

	fbws = fdt_getprop(fdt, offs, "fuse-bankword", &len);
	fmasks = fdt_getprop(fdt, offs, "fuse-mask", &len2);
	fvals = fdt_getprop(fdt, offs, "fuse-value", &len3);
	if (!fbws || !fmasks || !fvals || (len != len2) || (len2 != len3)
	    || !len || (len % sizeof(fdt32_t) != 0)) {
		printf("Invalid or missing fuse value settings for boot"
		       " device %s\n", boot_dev);
		return 1;
	}
	len /= sizeof(fdt32_t);

	puts("\n"
	     "Fuse settings (with respect to booting):\n"
	     "\n"
	     "  Bank Word Value      -> Target\n"
	     "  ----------------------------------------------\n");
	ret = 0;
	for (i = 0; i < len; i++) {
		fuse_bw = fdt32_to_cpu(fbws[i]);
		fuse_mask = fdt32_to_cpu(fmasks[i]);
		fuse_val = fdt32_to_cpu(fvals[i]);
		fuse_read(fuse_bw >> 16, fuse_bw & 0xffff, &cur_val);
		cur_val &= fuse_mask;

		printf("  0x%02x 0x%02x 0x%08x -> 0x%08x", fuse_bw >> 16,
		       fuse_bw & 0xffff, cur_val, fuse_val);
		if (cur_val == fuse_val)
			puts(" (unchanged)");
		else if (cur_val & ~fuse_val) {
			ret |= 1;
			puts(" (impossible)");
		} else
			ret |= 2;	/* Need change */
		puts("\n");
	}
	puts("\n");
	if (!ret) {
		printf("Fuses already set correctly to boot from %s\n",
		       boot_dev);
		return 0;
	}
	if (ret & 1) {
		printf("Error: New settings for boot device %s would need to"
		       " clear fuse bits which\nis impossible. Rejecting to"
		       " save this configuration.\n", boot_dev);
		return 1;
	}
	if (!force) {
		puts("The fuses will be changed to the above settings. This is"
		     " a write once option\nand can not be undone. ");
		if (!fs_image_confirm())
			return 0;
	}

	/* Now there is no way back... actually burn the fuses */
	for (i = 0; i < len; i++) {
		fuse_bw = fdt32_to_cpu(fbws[i]);
		fuse_val = fdt32_to_cpu(fvals[i]);
#if defined(CONFIG_IMX8)
		fuse_mask = fdt32_to_cpu(fmasks[i]);
		fuse_read(fuse_bw >> 16, fuse_bw & 0xffff, &cur_val);
		cur_val &= fuse_mask;
#else
		cur_val = 0;
#endif
		if (cur_val != fuse_val) {
			ret = fuse_prog(fuse_bw >> 16, fuse_bw & 0xffff, fuse_val);
			if (ret) {
				printf("Error: Fuse programming failed for bank 0x%x,"
					   " word 0x%x, value 0x%08x (%d)\n",
					   fuse_bw >> 16, fuse_bw & 0xffff, fuse_val, ret);
				return 1;
			}
		}
	}

	printf("Fuses programmed for boot device %s\n", boot_dev);

	return 0;
}

/* Subcommands for "fsimage" */
static cmd_tbl_t cmd_fsimage_sub[] = {
	U_BOOT_CMD_MKENT(arch, 0, 1, do_fsimage_arch, "", ""),
	U_BOOT_CMD_MKENT(board-id, 0, 1, do_fsimage_boardid, "", ""),
#ifdef CONFIG_CMD_FDT
	U_BOOT_CMD_MKENT(board-cfg, 0, 1, do_fsimage_boardcfg, "", ""),
#endif
	U_BOOT_CMD_MKENT(firmware, 1, 1, do_fsimage_firmware, "", ""),
	U_BOOT_CMD_MKENT(list, 1, 1, do_fsimage_list, "", ""),
	U_BOOT_CMD_MKENT(save, 2, 0, do_fsimage_save, "", ""),
	U_BOOT_CMD_MKENT(fuse, 2, 0, do_fsimage_fuse, "", ""),
};

static int do_fsimage(cmd_tbl_t *cmdtp, int flag, int argc, char * const argv[])
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
	if (flag == CMD_FLAG_REPEAT && !cmd_is_repeatable(cp))
		return CMD_RET_SUCCESS;

	return cp->cmd(cmdtp, flag, argc, argv);
}

U_BOOT_CMD(fsimage, 4, 1, do_fsimage,
	   "Handle F&S images, e.g. NBOOT",
	   "arch\n"
	   "    - Show F&S architecture\n"
	   "fsimage board-id\n"
	   "    - Show current BOARD-ID\n"
#ifdef CONFIG_CMD_FDT
	   "fsimage board-cfg [addr]\n"
	   "    - List contents of current BOARD-CFG\n"
#endif
	   "fsimage firmware [addr]\n"
	   "    - Load the current FIRMWARE to addr for inspection\n"
	   "fsimage list [addr]\n"
	   "    - List the content of the F&S image at addr\n"
	   "fsimage save [-f] [addr]\n"
	   "    - Save the F&S image (BOARD-CFG, SPL, FIRMWARE)\n"
	   "fsimage fuse [-f]\n"
	   "    - Program fuses according to the current BOARD-CFG.\n"
	   "      WARNING: This is a one time option and cannot be undone.\n"
	   "\n"
	   "If no addr is given, use loadaddr. Using -f forces the command to\n"
	   "continue without showing any confirmation queries. This is meant\n"
	   "for non-interactive installation procedures.\n"
);
