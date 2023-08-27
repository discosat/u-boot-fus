// SPDX-License-Identifier:	GPL-2.0+
/*
 * (C) Copyright 2021 F&S Elektronik Systeme GmbH
 * Hartmut Keller <keller@fs-net.de>
 *
 * Handle F&S nboot.fs and uboot.fs images.
 *
 * See board/F+S/common/fs_image_common.c for a description of the NBoot file
 * format.
 *
 * When saving NBoot, different parts of the nboot.fs image, so-called
 * sub-images, are stored at different places in flash memory. In addition,
 * some administrative information needs to be stored. In NAND flash we need
 * the Boot Control Block (BCB), consisting of several smaller parts called
 * FCB, DBBT and DBBT-DATA. And in eMMC, some CPUs expect a Secondary Image
 * Table. Both are needed by the ROM loader to locate and load the primary and
 * secondary copy of the SPL bootloader.
 *
 * In case of NAND, this looks like this:
 *
 *       auto-generated          NAND
 *       +------------+          +------------------+
 *       | FCB        |          | FCB Copy 0       | \
 *       | DBBT       |----+---->| DBBT Copy 0      |  |
 *       | DBBT-DATA  |    |     | DBBT-DATA Copy 0 |  |
 *       +------------+    |     +------------------+  | BCB region
 *                         |     | FCB Copy 1       |  |
 *                         +---->| DBBT Copy 1      |  |
 *                               | DBBT-DATA Copy 1 | /
 *                               +------------------+
 *                               | ...              |
 *                               +------------------+
 *                    +--------->| SPL Copy 0       | \
 *                    |          +------------------+  | SPL region
 *                    +--------->| SPL Copy 1       | /
 *                    |          +------------------+
 *   RAM (nboot.fs)   |          | ...              |
 *   +------------+   |          +------------------+
 *   | SPL        |---+  +------>| BOARD-CFG Copy 0 | \
 *   |------------|      |  +--->| FIRMWARE Copy 0  |  |
 *   | BOARD-CFG  |------+  |    +------------------+  | NBOOT region
 *   |------------|      +--|--->| BOARD-CFG Copy 1 |  |
 *   | FIRMWARE   |---------+--->| FIRMWARE Copy 1  | /
 *   +------------+              +------------------+
 *
 * So we have to deal with different flash regions, one for BCB, one for SPL
 * and one for NBOOT. We store two copies of each sub-image in each region to
 * be failsafe. The location, where each region is stored in flash, is given
 * by the nboot-info which is part of the BOARD-CFG. Please note that the
 * FIRMWARE part actually consists of even more sub-images when saving, we
 * just want to keep this example as simple as possible.
 *
 * We use several structs to define the relationship between the sub-images in
 * RAM and the regions in flash memory.
 *
 * A region is built by a struct region_info (ri). It consists of two parts:
 *
 *  1. The place in flash, given by struct storage_info (si). It defines the
 *     two start addresses in flash (start[0..1], one for each copy) and the
 *     common size. In case of eMMC, it also tells the hardware partition
 *     (0: user area, 1: boot1, 2: boot2). This depends on which partition the
 *     eMMC is configured to boot from.
 *  2. A group of sub-images given by struct sub_info (sub). Each sub defines
 *     the source address of the corresponding sub-image in RAM (img),
 *     the size of the sub-image and the target in the flash region, relative
 *     to its start (offset).
 *
 * To deal with the above NAND setup, we need three regions:
 *
 *                       1. ri for BCB
 *                       +------------------+
 *                       | si:              |        NAND
 *                       | start[0]         |------->+------------------+Offset
 *                       | start[1]         |----+  {| FCB Copy 0       |0x0000
 *                       | size             |--+-|--{| DBBT Copy 0      |0x2000
 *                       |------------------|  | |  {| DBBT-DATA Copy 0 |0x4000
 *                       | sub[0]: FCB      |  | +-->|------------------|
 *                       | offset = 0x0000  |  |    {| FCB Copy 1       |0x0000
 *                  +----| img              |  +----{| DBBT Copy 1      |0x2000
 *                  | +--| size             |       {| DBBT-DATA Copy 1 |0x4000
 * auto-generated   | |  |------------------|        |------------------|
 * +------------+<--+ |  | sub[1]: DBBT     |        | ...              |
 * | FCB        |}----+  | offset = 0x2000  |        |                  |
 * |------------|<-------| img              |        |                  |
 * | DBBT       |}-------| size             |        |                  |
 * |------------|<----+  |------------------|        |                  |
 * | DBBT-DATA  |}--+ |  | sub[2]: DBBT-DATA|        |                  |
 * +------------+   | |  | offset = 0x4000  |        |                  |
 *                  | +--| img              |        |                  |
 *                  +----| size             |        |                  |
 *                       +------------------+        |                  |
 *                                                   |                  |
 *                       2. ri for SPL               |                  |
 *                       +------------------+        |                  |
 *                       | si:              |  +---->|------------------|
 *                       | start[0]         |--+ +--{| SPL Copy 0       |0x0000
 *                       | start[1]         |----|-->|------------------|
 *                       | size             |----+--{| SPL Copy 1       |0x0000
 *                       |------------------|        |------------------|
 *                       | sub[0]: SPL      |        | ...              |
 *                       | offset = 0x0000  |        |                  |
 *                  +----| img              |        |                  |
 *                  | +--| size             |        |                  |
 *                  | |  +------------------+        |                  |
 *                  | |                              |                  |
 *                  | |  3. ri for NBOOT             |                  |
 *                  | |  +------------------+        |                  |
 *                  | |  | si:              |        |                  |
 *                  | |  | start[0]         |------->|------------------|
 *                  | |  | start[1]         |----+  {| BOARD-CFG Copy 0 |0x0000
 *                  | |  | size             |--+-|--{| FIRMWARE Copy 0  |0x2000
 * RAM (nboot.fs)   | |  |------------------|  | +-->|------------------|
 * +------------+<--+ |  | sub[0]: BOARD-CFG|  |    {| BOARD-CFG Copy 1 |0x0000
 * | SPL        |}----+  | offset = 0x0000  |  +----{| FIRMWARE Copy 1  |0x2000
 * |------------|<-------| img              |        +------------------+
 * | BOARD-CFG  |}-------| size             |
 * |------------|<----+  |------------------|
 * | FIRMWARE   |}--+ |  | sub[1]: FIRMWARE |
 * +------------+   | |  | offset = 0x2000  |
 *                  + +--| img              |
 *                  +----| size             |
 *                       +------------------+
 *
 * Please note that the two copies of a region are not necessarily stored
 * consecutively. There are typically gaps between them to have space for
 * bigger images in the future. And they may even interleave with other
 * regions, e.g. SPL copy 0 followed by NBOOT copy 0 followed by SPL copy 1
 * followed by NBOOT copy 1. In eMMC, the two copies are typically on the same
 * offset, but in different boot partitions.
 *
 * To handle flash access (loading and saving) as generic as possible, there
 * is a struct flash_info (fi) that holds all flash specific settings.
 * Especially, there is the ops part, that defines a set of access functions
 * that differ between NAND and eMMC. When the code requires a flash specific
 * procedure, it calls such an ops function. As a result, the remaining code
 * can be kept common for both types of flash memory.
 */

#include <common.h>
#include <command.h>
#include <mmc.h>
#include <nand.h>
#include <mxs_nand.h>			/* mxs_nand_mode_fcb_62bit(), ... */
#include <console.h>			/* confirm_yesno() */
#include <jffs2/jffs2.h>		/* struct mtd_device + part_info */
#include <fuse.h>			/* fuse_read() */
#include <image.h>			/* parse_loadaddr() */
#include <u-boot/crc.h>			/* crc32() */

#include "../board/F+S/common/fs_board_common.h"	/* fs_board_*() */
#include "../board/F+S/common/fs_image_common.h"	/* fs_image_*() */

#include <asm/mach-imx/hab.h>
#include <asm/mach-imx/checkboot.h>
#include <asm/mach-imx/imx-nandbcb.h>

/* Structure to hold regions in NAND/eMMC for an image, taken from nboot-info */
struct storage_info {
	unsigned int start[2];			/* *-start entries */
	unsigned int size;		/* *-size entry */
#ifdef CONFIG_CMD_MMC
	u8 hwpart[2];			/* hwpart (in case of eMMC) */
#endif
	const char *type;		/* Name of storage region */
};

/* Storage info from the nboot-info of a BOARD-CFG in binary form */
#define NI_SUPPORT_CRC32       BIT(0)	/* Support CRC32 in F&S headers */
#define NI_SAVE_BOARD_ID       BIT(1)	/* Save the board-rev. in BOARD-CFG */
#define NI_UBOOT_WITH_FSH      BIT(2)	/* Save U-Boot with F&S Header */
#define NI_UBOOT_EMMC_BOOTPART BIT(3)	/* On eMMC when booting from boot part,
					   also save U-Boot in boot part */
#define NI_EMMC_BOTH_BOOTPARTS BIT(4)	/* On eMMC when booting from boot part,
					   use both boot partitions, one for
					   each copy */
struct nboot_info {
	unsigned int flags;		/* See NI_* above */
	unsigned int board_cfg_size;
	struct storage_info spl;
	struct storage_info nboot;
	struct storage_info uboot;
	struct storage_info env;
#ifdef CONFIG_NAND_MXS
	unsigned int env_used;		/* From env-size entry, region size
					   is from env-range */
#endif
};

#define SUB_SYNC          BIT(0)	/* After writing image, flush temp */
#define SUB_HAS_FS_HEADER BIT(1)	/* Image has an F&S header in flash */
#define SUB_IS_SPL        BIT(2)	/* SPL: has IVT, may beed offset */
#ifdef CONFIG_NAND_MXS
#define SUB_IS_FCB        BIT(3)	/* FCB: needs other ECC */
#define SUB_IS_DBBT       BIT(4)
#define SUB_IS_DBBT_DATA  BIT(5)
#endif
struct sub_info {
	void *img;			/* Pointer to image */
	const char *type;		/* "BOARD-CFG", "FIRMWARE", "SPL" */
	const char *descr;		/* e.g. board architecture */
	unsigned int size;		/* Size of image */
	unsigned int offset;		/* Offset of image within si */
	unsigned int flags;		/* See SUB_* above */
};

struct region_info {
	struct storage_info *si;	/* Region information */
	struct sub_info *sub;		/* Pointer to subimages */
	int count;			/* Number of subimages */
};

/* Access functions that differ between NAND and MMC */
struct flash_info;
struct flash_ops {
	bool (*check_for_uboot)(struct storage_info *si, bool force);
	bool (*check_for_nboot)(struct flash_info *fi, struct storage_info *si,
				bool force);
	int (*get_nboot_info)(struct flash_info *fi, void *fdt,
			      int offs, struct nboot_info *ni);
	int (*read)(struct flash_info *fi, uint offs, uint size, uint lim,
		    uint flags, u8 *buf);
	int (*load_image)(struct flash_info *fi, int copy,
			  const struct storage_info *si, struct sub_info *sub);
	int (*load_extra)(struct flash_info *fi, struct storage_info *spl,
			  void *tempaddr);
	int (*invalidate)(struct flash_info *fi, int copy,
			  const struct storage_info *si);
	int (*write)(struct flash_info *fi, uint offs, uint size, uint lim,
		     uint flags, u8 *buf);
	int (*prepare_region)(struct flash_info *fi, int copy,
			      struct storage_info *si);
	int (*save_nboot)(struct flash_info *fi, struct region_info *nboot_ri,
			  struct region_info *spl_ri);
	void (*get_flash)(struct flash_info *fi);
	void (*put_flash)(struct flash_info *fi);
};

struct flash_info {
#ifdef CONFIG_NAND_MXS
	struct mtd_info *mtd;		/* Handle to NAND */
#endif
#ifdef CONFIG_CMD_MMC
	struct mmc *mmc;		/* Handle to MMC device */
	struct blk_desc *blk_desc;	/* Handle to MMC block device */
	u8 boot_hwpart;			/* HW partition we boot from (0..2) */
	u8 old_hwpart;			/* Previous partition before command */
#endif
	char devname[6];		/* Name of device (NAND, mmc<n>) */
	u8 *temp;			/* Buffer for one NAND page/MMC block */
	unsigned int temp_size;		/* Size of temp buffer */
	unsigned int base_offs;		/* Offset where temp will be written */
	unsigned int write_pos;		/* temp contains data up to this pos */
	unsigned int bb_extra_offs;	/* Extra offset due to bad blocks */
	u8 temp_fill;			/* Default value for temp buffer */
	enum boot_device boot_dev;	/* Device to boot from */
	const char *boot_dev_name;	/* Boot device as string */
	struct flash_ops *ops;		/* Access functions for NAND/MMC */
};

/* Info table for secondary SPL (MMC) */
struct info_table {
	u32 chip_num;			/* unused, set to 0 */
	u32 drive_type;			/* unused, set to 0 */
	u32 tag;			/* 0x00112233 */
	u32 first_sector;		/* Start sector of secondary SPL */
	u32 sector_count;		/* unused, set to 0 */
};

/* Struct that is big enough for all headers that we may want to load */
union any_header {
	struct fs_header_v1_0 fsh;
	u8 ivt[HAB_HEADER];
	struct fdt_header fdt;
};

/* Argument of option -e in fsimage save */
static unsigned int early_support_index;

/* ------------- Common helper function ------------------------------------ */

/* Build lowercase nboot-info property name from upper-case region name */
static void fs_image_build_nboot_info_name(char *name, const char *prefix,
				const char *suffix)
{
	char c;

	/* Copy prefix in lowercase, drop hyphens from region name */
	do {
		c = *prefix++;
		if (c && (c != '-')) {
			if ((c >= 'A') && (c <= 'Z'))
				c += 'a' - 'A';
			*name++ = c;
		}
	} while (c);

	/* Append suffix */
	do {
		c = *suffix++;
		*name++ = c;
	} while (c);
}

/* Get start[0..1] and size for a storage info */
static int fs_image_get_si(void *fdt, int offs, uint align, const char *type,
			   struct storage_info *si)
{
	int err;
	char name[20];

	si->type = type;

	/* Create the property name for nboot-info and get value */
	fs_image_build_nboot_info_name(name, type, "-start");

	err = fs_image_get_fdt_val(fdt, offs, name, align, 2, si->start);
	if (err)
		return err;

	/* Create the property name for nboot-info and get value */
	fs_image_build_nboot_info_name(name, type, "-size");

	return fs_image_get_fdt_val(fdt, offs, name, align, 1, &si->size);
}

static int fs_image_get_nboot_info(struct flash_info *fi, void *fdt,
				   struct nboot_info *ni)
{
	int offs = fs_image_get_nboot_info_offs(fdt);

	if (offs < 0) {
		puts("Cannot find nboot-info in BOARD-CFG\n");
		return -ENOENT;
	}

	memset(ni, 0, sizeof(*ni));

	/* Parse generic NBoot capablities here */
	ni->board_cfg_size = fdt_getprop_u32_default_node(fdt, offs, 0,
							  "board-cfg-size", 0);
	if (fdt_getprop(fdt, offs, "support-crc32", NULL))
		ni->flags |= NI_SUPPORT_CRC32;
	if (fdt_getprop(fdt, offs, "save-board-id", NULL))
		ni->flags |= NI_SAVE_BOARD_ID;
	if (fdt_getprop(fdt, offs, "uboot-with-fsh", NULL))
		ni->flags |= NI_UBOOT_WITH_FSH;
	if (fdt_getprop(fdt, offs, "uboot-emmc-bootpart", NULL))
		ni->flags |= NI_UBOOT_EMMC_BOOTPART;
#ifdef CONFIG_IMX8MM
	/* Have a flag that not everything is in one boot partition */
	if (fdt_getprop(fdt, offs, "emmc-both-bootparts", NULL))
		ni->flags |= NI_EMMC_BOTH_BOOTPARTS;
#else
	/* This has always been the default on all other architectures */
	ni->flags |= NI_EMMC_BOTH_BOOTPARTS;
#endif

	/* Parse flash specific settings individually */
	return fi->ops->get_nboot_info(fi, fdt, offs, ni);
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

/* Update size, flags and padsize, calculate CRC32 if requested */
static void fs_image_update_header(struct fs_header_v1_0 *fsh,
				   uint size, uint fsh_flags)
{
	u8 padsize = 0;

	padsize = size % 16;
	if (padsize)
		padsize = 16 - padsize;

	fsh->info.file_size_low = size;
	fsh->info.padsize = padsize;
	fsh->info.flags = fsh_flags | FSH_FLAGS_DESCR;

	if (fsh_flags & (FSH_FLAGS_CRC32 | FSH_FLAGS_SECURE)) {
		unsigned char *crc32_start = (unsigned char *)fsh;
		unsigned int crc32_size = 0;
		u32 *pcs = (u32 *)&fsh->type[12];

		*pcs = 0;
		if (fsh_flags & FSH_FLAGS_SECURE)
			crc32_size += FSH_SIZE;
		else
			crc32_start += FSH_SIZE;

		if (fsh_flags & FSH_FLAGS_CRC32)
			crc32_size += size;

		*pcs = crc32(0, crc32_start, crc32_size);
		debug("- Setting CRC32 for %s to 0x%08x\n", fsh->type, *pcs);
	}
}

/* Set all fields of the F&S header */
static void fs_image_set_header(struct fs_header_v1_0 *fsh, const char *type,
				const char *descr, uint size, uint fsh_flags)
{
	/* Set basic members */
	memset(fsh, 0, FSH_SIZE);
	fsh->info.magic[0] = 'F';
	fsh->info.magic[1] = 'S';
	fsh->info.magic[2] = 'L';
	fsh->info.magic[3] = 'X';
	fsh->info.version = 0x10;
	strncpy(fsh->type, type, sizeof(fsh->type));
	strncpy(fsh->param.descr, descr, sizeof(fsh->param.descr));

	/* Set size, flags and padsize, calculate CRC32 if requested */
	fs_image_update_header(fsh, size, fsh_flags);
}

/* Return pointer to the header of the given sub-image or NULL if not found */
static struct fs_header_v1_0 *fs_image_find(struct fs_header_v1_0 *fsh,
					    const char *type, const char *descr)
{
	unsigned int size;
	unsigned int remaining;
	struct fs_header_v1_0 *temp;

	remaining = fs_image_get_size(fsh++, false);
	while (remaining > 0) {
		if (fs_image_match(fsh, type, descr))
			return fsh;
		if (!fs_image_is_fs_image(fsh))
			break;

		/* Search recursively */
		temp = fs_image_find(fsh, type, descr);
		if (temp)
			return temp;

		/* Go to next sub-image */
		size = fs_image_get_size(fsh, true);
		fsh = (void *)fsh + size;
		remaining -= size;
	}

	return NULL;
}

static void fs_image_region_create(struct region_info *ri,
				   struct storage_info *si,
				   struct sub_info *sub)
{
	ri->si = si;
	ri->sub = sub;
	ri->count = 0;
}


/*
 * Add a subimage with any format to the region. Return offset for next
 * subimage or 0 in case of error.
 */
static void fs_image_region_add_raw(
	struct region_info *ri, void *img, const char *type, const char *descr,
	unsigned int woffset, unsigned int flags, unsigned int size)
{
	struct sub_info *sub;

	sub = &ri->sub[ri->count++];
	sub->type = type;
	sub->descr = descr;
	sub->img = img;
	sub->size = size;
	sub->offset = woffset;
	sub->flags = flags;

	debug("- %s(%s): 0x%08lx -> offset 0x%x size 0x%x\n", type, descr,
	      (unsigned long)img, woffset, size);
}

/*
 * Add a subimage with given data to the region. Return offset for next
 * subimage or 0 in case of error.
 */
static unsigned int fs_image_region_add(
	struct region_info *ri, struct fs_header_v1_0 *fsh, const char *type,
	const char *descr, unsigned int woffset, unsigned int flags)
{
	unsigned int size;

	size = fs_image_get_size(fsh, true);
	if (!(flags & SUB_HAS_FS_HEADER)) {
		fsh++;
		size -= FSH_SIZE;
	}

	if (woffset + size > ri->si->size) {
		printf("%s does not fit into target slot\n", type);
		return 0;
	}

	fs_image_region_add_raw(ri, fsh, type, descr, woffset, flags, size);

	return woffset + size;
}

/*
 * Add a single F&S header with given data to the region. Return offset for
 * next subimage or 0 in case of error.
 */
static unsigned int fs_image_region_add_fsh(
	struct region_info *ri, struct fs_header_v1_0 *fsh, const char *type,
	const char *descr, unsigned int woffset)
{
	fs_image_set_header(fsh, type, descr, 0, 0);

	return fs_image_region_add(ri, fsh, type, descr, woffset,
				   SUB_HAS_FS_HEADER);
}

/*
 * Search the subimage with given type/descr and add it to the region. Return
 * offset for next image or 0 in case of error.
 */
static unsigned int fs_image_region_find_add(
	struct region_info *ri, struct fs_header_v1_0 *fsh, const char *type,
	const char *descr, unsigned int woffset, unsigned int flags)
{
	fsh = fs_image_find(fsh, type, descr);
	if (!fsh) {
		printf("No %s found for %s\n", type, descr);
		return 0;
	}

	return fs_image_region_add(ri, fsh, type, descr, woffset, flags);
}

/* Show status after handling a subimage */
static void fs_image_show_sub_status(int err)
{
	switch (err) {
	case 0:
		puts(" OK\n");
		break;
	case 1:
		puts(" BAD BLOCKS\n");
		break;
	default:
		printf(" FAILED (%d)\n", err);
		break;
	}
}

/* Show status after saving an image and return CMD_RET code */
static int fs_image_show_save_status(int failed, const char *type)
{
	printf("\nSaving %s ", type);

	/* Each bit identifies a copy that failed */
	if (!failed) {
		puts("complete\n");
		return CMD_RET_SUCCESS;
	}

	if (failed != 3) {
		puts("incomplete!\n\n"
		     "*** WARNING! One copy failed, the system is unstable!\n");
		return CMD_RET_SUCCESS;
	}

	printf("\nFAILED!\n\n"
	       "*** ATTENTION!\n"
	       "*** Do not switch off or restart the board before you have\n"
	       "*** installed a working %s version. Otherwise the board will\n"
	       "*** most probably fail to boot.\n", type);

	return CMD_RET_FAILURE;
}

static int fs_image_confirm(void)
{
	int yes;

	puts("Are you sure? [y/N] ");
	yes = confirm_yesno();
	if (!yes)
		puts("Aborted by user, nothing was changed\n");

	return yes;
}

/* Determine first copy to modify depending on which SPL copy we booted */
static int fs_image_get_start_copy(void)
{
	int start_copy;

	if (fs_board_get_cfg_info()->flags & CI_FLAGS_SECONDARY)
		start_copy = 0;
	else
		start_copy = 1;

	printf("Booted from %s SPL, so starting with copy %d\n",
	       start_copy ? "Primary" : "Secondary", start_copy);

	return start_copy;
}

static int fs_image_get_boot_dev(void *fdt, enum boot_device *boot_dev,
				 const char **boot_dev_name)
{
	int offs;
	const char *boot_dev_prop;

	offs = fs_image_get_board_cfg_offs(fdt);
	if (offs < 0) {
		puts("Cannot find BOARD-CFG\n");
		return -ENOENT;
	}
	boot_dev_prop = fdt_getprop(fdt, offs, "boot-dev", NULL);
	if (boot_dev_prop < 0) {
		puts("Cannot find boot-dev in BOARD-CFG\n");
		return -ENOENT;
	}
	*boot_dev = fs_board_get_boot_dev_from_name(boot_dev_prop);
	if (*boot_dev == UNKNOWN_BOOT) {
		printf("Unknown boot device %s in BOARD-CFG\n", boot_dev_prop);
		return -EINVAL;
	}

	*boot_dev_name = fs_board_get_name_from_boot_dev(*boot_dev);

	return 0;
}

/* Check boot device; Return 0: OK, 1: Not fused yet, <0: Error */
static int fs_image_check_boot_dev_fuses(enum boot_device boot_dev,
					 const char *action)
{
	enum boot_device boot_dev_fuses;

	boot_dev_fuses = fs_board_get_boot_dev_from_fuses();
	if (boot_dev_fuses == boot_dev)
		return 0;		/* Match, no change */

	if (boot_dev_fuses == USB_BOOT)
		return 1;		/* Not fused yet */

	printf("Error: New BOARD-CFG wants to boot from %s but board is\n"
	       "already fused for %s. Refusing to %s this configuration.\n",
	       fs_board_get_name_from_boot_dev(boot_dev),
	       fs_board_get_name_from_boot_dev(boot_dev_fuses), action);

	return -EINVAL;
}

/* Check CRC32 from image and all sub-images */
static int fs_image_check_all_crc32(struct fs_header_v1_0 *fsh)
{
	unsigned int size;
	unsigned int remaining;
	int err;

	debug("  - %s", fsh->type);
	err = fs_image_check_crc32(fsh);
	switch (err) {
	case 0:
		debug(" (no CRC32)\n");
		break;
	case 1:
		debug(" (CRC32 header only ok)\n");
		break;
	case 2:
		debug(" (CRC32 image only ok)\n");
		break;
	case 3:
		debug(" (CRC32 header+image ok)\n");
		break;
	default:
		puts(" BAD CRC32");
		return err;
	}

	remaining = fs_image_get_size(fsh++, false);
	while (remaining > 0) {
		if (!fs_image_is_fs_image(fsh))
			break;

		/* Check recursively */
		err = fs_image_check_all_crc32(fsh);
		if (err)
			return err;

		/* Go to next sub-image */
		size = fs_image_get_size(fsh, true);
		fsh = (void *)fsh + size;
		remaining -= size;
	}

	return 0;
}

/* Validate a signed image; Return 0: OK, <0: Error */
static int fs_image_validate_signed(struct fs_header_v1_0 *fsh)
{
	struct fs_header_v1_0 *validate_addr;
	u32 size;

	validate_addr = fs_image_get_ivt_info(fsh, &size);
	if (!validate_addr || !size) {
		puts("Error: Bad IVT, validation impossible\n");
		return -EINVAL;
	}

	/* Copy to verification address and check signature */
	debug("Copy 0x%x bytes from 0x%08lx to validation address 0x%08lx\n",
	      size, (ulong)fsh, (ulong)validate_addr);
	memcpy(validate_addr, fsh, size);
	if (fs_image_is_valid_signature(validate_addr)) {
		puts("Error: Invalid signature, refusing to save\n");
		return -EILSEQ;
	}

	puts("Signature OK\n");

	return 0;
}

/* Validate an image, either check signature or CRC32; 0: OK, <0: Error */
static int fs_image_validate(struct fs_header_v1_0 *fsh, const char *type,
			     const char *descr, ulong addr)
{
	int err;

	if (!fs_image_match(fsh, type, descr)) {
		printf("Error: No %s image for %s found at address 0x%lx\n",
		       type, descr, addr);
		return -EINVAL;
	}

	if (fs_image_is_signed(fsh)) {
		printf("Found signed %s image at 0x%08lx\n", type, addr);

		return fs_image_validate_signed(fsh);
	}

	printf("Found unsigned %s image at 0x%08lx ", type, addr);

#ifdef CONFIG_FS_SECURE_BOOT
	if (imx_hab_is_enabled()) {
		puts("\nError: Board is closed, refusing to save unsigned"
		     " image\n");
		return -EINVAL;
	}
#endif

	err = fs_image_check_crc32(fsh);
	switch (err) {
	case 0:
		puts("(no CRC32)\n");
		break;
	case 1:
		puts("(CRC32 header only ok)\n");
		break;
	case 2:
		puts("(CRC32 image only ok)\n");
		break;
	case 3:
		puts("(CRC32 header+image ok)\n");
		break;
	default:
		puts("- Error: BAD CRC32\n");
		return err;
	}

	return 0;
}

/* Get the full size of a FIT image, including all external images */
static int fs_image_get_size_from_fit(struct sub_info *sub, uint *size)
{
	void *fit = sub->img;
	unsigned int fit_size = ALIGN(fit_get_size(fit), 4);
	int images;
	int node;
	int offs;
	int img_size;
	unsigned int maxsize = fit_size;
	const void *dummy_data;
	size_t dummy_size;
	int err;

	images = fdt_path_offset(fit, FIT_IMAGES_PATH);
	if (images < 0)
		return -ENOENT;

	/* Parse all images to find the last one (with highest offset) */
	fdt_for_each_subnode(node, fit, images) {
		/* If image data is embedded, this will not increase size */
		if (!fit_image_get_data(fit, node, &dummy_data, &dummy_size))
			continue;
		/*
		 * If image data is external (given by data_position or
		 * data_offset, look for end of image data and keep highest
		 * value.
		 */
		if (fit_image_get_data_position(fit, node, &offs)) {
			if (fit_image_get_data_offset(fit, node, &offs))
				return -ENOENT;
			offs += fit_size;
		}
		err =  fit_image_get_data_size(fit, node, &img_size);
		if (err < 0)
			return -ENOENT;
		img_size = ALIGN(img_size, 4);
		offs += img_size;
		if ((unsigned int)offs > maxsize)
			maxsize = (unsigned int)offs;
	}

	*size = maxsize;

	return 0;
}

/* Get image length from F&S header or IVT (SPL, signed U-Boot) */
static int fs_image_get_size_from_fsh_or_ivt(struct sub_info *sub, uint *size)
{
	if (fs_image_is_fs_image(sub->img)) {
		struct fs_header_v1_0 *fsh = sub->img;

		/* Check image type and get image size from F&S header */
		if (!fs_image_match(fsh, sub->type, sub->descr))
			return -ENOENT;
		*size = fs_image_get_size(fsh, true);
	} else {
		struct ivt *ivt = sub->img;
		struct boot_data *boot_data;

		/* Get image size from IVT/boot_data */
		if ((ivt->hdr.magic != IVT_HEADER_MAGIC)
		    || (ivt->boot != ivt->self + IVT_TOTAL_LENGTH))
			return -ENOENT;
		boot_data = (struct boot_data *)(ivt + 1);
		*size = boot_data->length;
	}

	return 0;
}

/*
 * Get pointer to BOARD-CFG image that is to be used and to NBOOT part
 * Returns: <0: error; 0: aborted by user; 1: same ID; 2: new ID
 */
static int fs_image_find_board_cfg(unsigned long addr, bool force,
				   const char *action,
				   struct fs_header_v1_0 **used_cfg,
				   struct fs_header_v1_0 **nboot)
{
	struct fs_header_v1_0 *fsh = (struct fs_header_v1_0 *)addr;
	char id[MAX_DESCR_LEN + 1];
	void *fdt;
	struct fs_header_v1_0 *cfg;
	const char *arch = fs_image_get_arch();
	unsigned int size, remaining;
	const char *nboot_version;
	int err;
	int ret = 1;

	*used_cfg = NULL;

	if (!fs_image_is_fs_image(fsh)) {
		printf("No F&S image found at address 0x%lx\n", addr);
		return -ENOENT;
	}

	/* In case of an NBoot image with prepended BOARD-ID, use this ID */
	if (fs_image_match(fsh, "BOARD-ID", NULL)) {
		const char *old_id = fs_image_get_board_id();
		char new_id[MAX_DESCR_LEN + 1];

		memcpy(new_id, fsh->param.descr, MAX_DESCR_LEN);
		new_id[MAX_DESCR_LEN] = '\0';
		if (strcmp(new_id, old_id)) {
#ifdef CONFIG_FS_SECURE_BOOT
			if (imx_hab_is_enabled()) {
				printf("Error: Current board is %s and board"
				       " is closed\nRefusing to %s for %s\n",
				       old_id, action, new_id);
				return -EINVAL;
			}
#endif
			printf("Warning! Current board is %s but you will\n"
			       "%s for %s\n", old_id, action, new_id);
			if (!force && !fs_image_confirm()) {
				return 0; /* used_cfg == NULL in this case */
			}

			/* Set this BOARD-ID as compare_id */
			fs_image_set_compare_id(fsh->param.descr);
			ret = 2;
		}
		fsh++;
	}

	/* Authenticate signature or check CRC32 */
	err = fs_image_validate(fsh, "NBOOT", arch, addr);
	if (err)
		return err;

	/* Look for BOARD-INFO subimage and search for matching BOARD-CFG */
	cfg = fs_image_find(fsh, "BOARD-INFO", arch);
	if (!cfg) {
		/* Fall back to BOARD-CONFIGS for old NBoot variants */
		cfg = fs_image_find(fsh, "BOARD-CONFIGS", arch);
		if (!cfg) {
			printf("No BOARD-INFO/CONFIGS found for %s\n", arch);
			return -ENOENT;
		}
	}

	remaining = fs_image_get_size(cfg++, false);
	while (1) {
		if (!remaining || !fs_image_is_fs_image(cfg)) {
			printf("No BOARD-CFG found for BOARD-ID %s\n", id);
			return -ENOENT;
		}
		if (fs_image_match_board_id(cfg))
			break;
		size = fs_image_get_size(cfg, true);
		remaining -= size;
		cfg = (struct fs_header_v1_0 *)((void *)cfg + size);
	}

	/* Get and show NBoot version as noted in BOARD-CFG */
	fdt = fs_image_find_cfg_fdt(cfg);

	nboot_version = fs_image_get_nboot_version(fdt);
	if (!nboot_version) {
		printf("Unknown NBOOT version, refusing to %s\n", action);
		return -EINVAL;
	}
	printf("Found NBOOT version %s\n", nboot_version);

	*used_cfg = cfg;
	if (nboot)
		*nboot = fsh;

	return ret;			/* Proceed */
}

/* Get addr for image; 0 if "stored", <0: Error */
static unsigned long fs_image_get_loadaddr(int argc, char * const argv[],
					     bool use_stored_if_empty)
{
	unsigned long addr;

	if (argc > 1) {
		if (!strncmp(argv[1], "stored", strlen(argv[1])))
			return 0;

		addr = parse_loadaddr(argv[1], NULL);
		use_stored_if_empty = false;
	} else
		addr = get_loadaddr();

	if (fs_image_match((void *)addr, "NBOOT", fs_image_get_arch()))
		return addr;

	printf("No F&S NBoot image found at 0x%lx", addr);

	if (argc > 1) {
		putc('\n');
		return -ENOENT;
	}

	if (!use_stored_if_empty) {
		puts(", use 'stored' to refer to stored NBoot\n");
		return -ENOENT;
	}

	puts(", switching to stored NBoot\n\n");

	return 0;
}

/* Invalidate the temp buffer read cache */
static void fs_image_drop_temp(struct flash_info *fi)
{
	fi->write_pos = 0;
	fi->bb_extra_offs = 0;
	memset(fi->temp, fi->temp_fill, fi->temp_size);
}

static int fs_image_fill_temp(struct flash_info *fi, uint base_offs, uint lim,
			      uint flags)
{
	int err;

	fi->write_pos = 0;

	debug("  - Fill temp from offs 0x%x\n", base_offs);
	err = fi->ops->read(fi, base_offs, fi->temp_size, lim, flags, fi->temp);
	if (err)
		return err;

	fi->base_offs = base_offs;
	fi->write_pos = fi->temp_size;

	return 0;
}

static int fs_image_load_sub(struct flash_info *fi, uint offs, uint size,
			     uint lim, uint flags, u8 *buf)
{
	int err;
	unsigned int read_pos;
	unsigned int base_offs;
	unsigned int chunk_size;
	unsigned int chunk_mask = fi->temp_size - 1;
	unsigned int remaining = size;

	/*
	 * Step 1: If reading starts in the middle of a page/block and we do
	 * not have this page/block cached in the temp buffer yet, load the
	 * page/block to the temp buffer.
	 */
	base_offs = offs & ~chunk_mask;
	read_pos = offs & chunk_mask;
	if ((read_pos) && fi->write_pos && (base_offs != fi->base_offs)) {
		err = fs_image_fill_temp(fi, base_offs, lim, flags);
		if (err)
			return err;
	}

	/*
	 * Step 2: If the start of the data is already cached in the
	 * temp/buffer, take it from there.
	 */
	if (fi->write_pos && (base_offs == fi->base_offs)) {
		chunk_size = fi->temp_size - read_pos;
		if (chunk_size > remaining)
			chunk_size = remaining;
		debug("  - Copy leading bytes from temp pos 0x%x size 0x%x"
		      " to 0x%lx\n", read_pos, chunk_size, (ulong)buf);
		memcpy(buf, fi->temp + read_pos, chunk_size);
		buf += chunk_size;
		offs += chunk_size;
		remaining -= chunk_size;
	}

	/*
	 * Step 3: Read the middle part consisting of full pages/blocks.
	 */
	chunk_size = remaining & ~chunk_mask;
	if (chunk_size) {
		debug("  - Read from offs 0x%x lim 0x%x size 0x%x to 0x%lx\n",
		      offs, lim, chunk_size, (ulong)buf);
		err = fi->ops->read(fi, offs, chunk_size, lim, flags, buf);
		if (err)
			return err;
		buf += chunk_size;
		offs += chunk_size;
		remaining -= chunk_size;
	}

	/*
	 * Step 4: Read the last page/block, from which we only need a part
	 * of, to the temp buffer and take the remaining bytes from there.
	 */
	if (remaining) {
		base_offs = offs & ~chunk_mask;
		err = fs_image_fill_temp(fi, base_offs, lim, flags);
		if (err)
			return err;

		read_pos = offs & chunk_mask;
		debug("  - Copy trailing bytes from temp pos 0x%x size 0x%x to"
		      " 0x%lx\n", read_pos, remaining, (ulong)buf);
		memcpy(buf, fi->temp + read_pos, remaining);
	}

	return 0;
}

static int fs_image_load_image(struct flash_info *fi,
			       const struct storage_info *si,
			       struct sub_info *sub)
{
	struct fs_header_v1_0 *fsh;
	void *copy0, *copy1;
	unsigned int size0 = 0;
	int err;

	printf("Loading %s from %s\n", sub->type, fi->devname);

	/* Add room for FS header if image has none */
	fsh = sub->img;
	if (!(sub->flags & SUB_HAS_FS_HEADER))
		sub->img += FSH_SIZE;

	/* Load first copy; on error, sub->size is 0, i.e. copy1 == copy0 */
	copy0 = sub->img;
	err = fi->ops->load_image(fi, 0, si, sub);
	fs_image_show_sub_status(err);
	size0 = sub->size;
	sub->img += size0;

	/* Load second copy; this overwrites first copy if it had an error */
	copy1 = sub->img;
	err = fi->ops->load_image(fi, 1, si, sub);
	fs_image_show_sub_status(err);
	if (err && (copy0 == copy1)) {
		printf("  Error, cannot load %s\n", sub->type);
		return -ENOENT;
	} else if (err || (copy0 == copy1)) {
		printf("  Warning! One copy corrupted! Saving NBoot again may"
		       " fix this.\n");
	}

	if (!err) {
		if (copy0 == copy1)
			size0 = sub->size;
		else if ((size0 != sub->size) || memcmp(copy0, copy1, size0))
			printf("  Warning! Images differ, taking copy 0\n");
	}

	/* Align the size to 16 Bytes, pad with 0 and fill FS header */
	sub->size = ALIGN(size0, 16);
	sub->img = copy0 + sub->size;
	memset(copy0 + size0, 0, sub->size - size0);
	if (!(sub->flags & SUB_HAS_FS_HEADER))
		fs_image_set_header(fsh, sub->type, sub->descr, size0, 0);

	return 0;
}

/* Load U-Boot to given address */
static int fs_image_load_uboot(struct flash_info *fi, struct nboot_info *ni,
			       void *addr)
{
	struct sub_info sub;
	int err;

	/*
	 * Assume no F&S header, so fs_image_load_image() creates a new one.
	 * If the image actually has a header (new U-BOOT versions are stored
	 * with header), it is used for CRC32 checking, then removed, and the
	 * own new header is used instead. We create a new CRC32 here anyway,
	 * so the old header content does not matter.
	 */
	sub.type = "U-BOOT";
	sub.descr = fs_image_get_arch();
	sub.img = addr;
	sub.offset = 0;
	sub.flags = 0;

	err = fs_image_load_image(fi, &ni->uboot, &sub);
	if (err)
		return err;

	/* Compute CRC32 if it was missing */
	debug("  ");
	fs_image_update_header((void *)addr, sub.size,
			       FSH_FLAGS_CRC32 | FSH_FLAGS_SECURE);

	return 0;
}

/* Flush the temp buffer to flash */
static int fs_image_flush_temp(struct flash_info *fi, uint lim, uint flags)
{
	int err;

	if (!fi->write_pos)
		return 0;

	debug("  - Flush temp (filled to pos 0x%x) to offs 0x%x\n",
	      fi->write_pos, fi->base_offs);
	err = fi->ops->write(fi, fi->base_offs, fi->temp_size, lim, flags,
			     fi->temp);
	fi->write_pos = 0;
	memset(fi->temp, fi->temp_fill, fi->temp_size);

	return err;
}

/* Write one sub-image to flash */
static int fs_image_save_sub(struct flash_info *fi, uint offs, uint size,
			     uint lim, uint flags, u8 *buf)
{
	int err;
	unsigned int write_pos;
	unsigned int base_offs;
	unsigned int chunk_size;
	unsigned int chunk_mask = fi->temp_size - 1;
	unsigned int remaining = size;

	debug("\n");
	/*
	 * Step 1: If the temp buffer was used and we are not continuing in
	 * the same page/block, write back the temp buffer first.
	 */
	base_offs = offs & ~chunk_mask;
	if (fi->write_pos && (base_offs != fi->base_offs)) {
		err = fs_image_flush_temp(fi, lim, 0);
		if (err)
			return err;
	}

	/*
	 * Step 2: Handle the beginning of the sub-image if it does not start
	 * on a page/block boundary. Write the beginning up to the next
	 * page/block boundary in the temp buffer and write back the temp
	 * buffer. If the data is very small so that it does not fill the temp
	 * buffer completely, then this is handled below in Step 4 instead.
	 */
	write_pos = offs & chunk_mask;
	if (write_pos) {
		chunk_size = fi->temp_size - write_pos;
		if (remaining >= chunk_size) {
			fi->base_offs = base_offs;
			fi->write_pos = write_pos + chunk_size;
			debug("  - Copy leading bytes from 0x%lx size 0x%x"
			      " to temp pos 0x%x\n", (ulong)buf, chunk_size,
			      write_pos);
			memcpy(fi->temp + write_pos, buf, chunk_size);
			err = fs_image_flush_temp(fi, lim, flags);
			if (err)
				return err;
			buf += chunk_size;
			offs += chunk_size;
			remaining -= chunk_size;
		}
	}

	/*
	 * Step 3: Write the middle part consisting of full pages/blocks. If
	 * SUB_SYNC is given, we can even write all of the remaining part.
	 */
	chunk_size = remaining & ~chunk_mask;
	if (chunk_size) {
		if (flags & SUB_SYNC) {
			debug("  - SYNC\n");
			chunk_size = remaining;
		}
		debug("  - Write from 0x%lx size 0x%x to offs 0x%x lim 0x%x\n",
		      (ulong)buf, chunk_size, offs, lim);

		err = fi->ops->write(fi, offs, chunk_size, lim, flags, buf);
		if (err)
			return err;
		buf += chunk_size;
		offs += chunk_size;
		remaining -= chunk_size;
	}

	/*
	 * Step 4: Put the remaining part, which does not fill a full
	 * page/block anymore, in the temp buffer. If SUB_SYNC is not given,
	 * this will be written in one of the next sub-images. The last image
	 * must have SUB_SYNC set.
	 */
	if (remaining) {
		fi->base_offs = offs & ~chunk_mask;
		debug("  - Copy trailing bytes from 0x%lx size 0x%x to temp"
		      " pos 0x%x\n", (ulong)buf, remaining, fi->write_pos);
		memcpy(fi->temp + fi->write_pos, buf, remaining);
		fi->write_pos += remaining;

		if (flags & SUB_SYNC) {
			debug("  - SYNC\n");
			err = fs_image_flush_temp(fi, lim, flags);
			if (err)
				return err;
		}
	}

	return 0;
}

/* Save the given region to flash */
static int fs_image_save_region(struct flash_info *fi, int copy,
				struct region_info *ri)
{
	void *buf;
	int err;
	struct sub_info *s;
	struct storage_info *si = ri->si;
	unsigned int lim = si->start[copy] + si->size;
	unsigned int offset;
	unsigned int size;
	unsigned int temp_size = fi->temp_size;
	bool pass2;
	const char *action;

	err = fi->ops->prepare_region(fi, copy, si);
	if (err)
		return err;

repeat:
	err = fi->ops->invalidate(fi, copy, si);
	if (err)
		return err;

	/*
	 * Write region in two passes:
	 *
	 * Pass 1:
	 * Write everything of the image but the first page/block with the
	 * header. If this is interrupted (e.g. due to a power loss), then the
	 * image will not be seen as valid when loading because of the missing
	 * header. So there is no problem with half-written files.
	 *
	 * Pass 2:
	 * Write only the first page/block with the header; if this succeeds,
	 * then we know that the whole image is completely written. If this is
	 * interrupted, then loading will fail either because of a bad header
	 * or because of a bad ECC. So again this prevents loading files that
	 * are not fully written.
	 */
	pass2 = false;
	do {
		fi->bb_extra_offs = 0;
		debug("  - Pass %d\n", pass2 ? 2 : 1);
		action = "Writing";
		for (s = ri->sub; s < ri->sub + ri->count; s++) {
			offset = s->offset;
			size = s->size;
			buf = s->img;

			if (!pass2) {
				/* Skip image completely? */
				if (offset + size < temp_size)
					continue;

				/* Skip only first part of image? */
				if (offset < temp_size) {
					size -= temp_size - offset;
					buf += temp_size - offset;
					offset = temp_size;
				}
			} else {
				/* Behind first page/block, i.e. done? */
				if (offset >= temp_size)
					break;

				/* Write only first part of image? */
				if (offset + size > temp_size) {
					size = temp_size - offset;
					action = "Completing";
				}
			}
			offset += si->start[copy];

			printf("  %s %s at offset 0x%08x size 0x%x...",
			       action, s->type, offset, size);

			err = fs_image_save_sub(fi, offset, size, lim,
						s->flags, buf);
			fs_image_show_sub_status(err);
			if (err == 1) {
				/* We had new bad blocks when writing */
				printf("Repeating copy %d\n", copy);
				goto repeat;
			}
			if (err)
				return err;
		}
		pass2 = !pass2;
	} while (pass2);

	return 0;
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

/* Check if any prerequisites for saving U-Boot are not met */
static bool fs_image_check_for_uboot_nand(struct storage_info *si, bool force)
{
	struct mtd_device *dev;
	u8 part_num;
	struct part_info *part;
	const char *name;
	int i;
	u64 start;
	bool warning = false;

	/* Issue warning if U-Boot MTD partitions do not match nboot-info */
	mtdparts_init();

	for (i = 0; i < ARRAY_SIZE(uboot_mtd_names); i++) {
		name = uboot_mtd_names[i];
		if (find_dev_and_part(name, &dev, &part_num, &part)) {
			printf("WARNING: MTD %s not found\n", name);
			warning = true;
		} else {
			start = si->start[i];
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

	return warning && !force && !fs_image_confirm();
}

/* Check if any prerequisites for storing NBoot are not met */
static bool fs_image_check_for_nboot_nand(struct flash_info *fi,
					  struct storage_info *si, bool force)
{
	/* Nothing to be done in case of NAND */

	return false;
}

/* Parse nboot-info for NAND settings and fill struct */
static int fs_image_get_nboot_info_nand(struct flash_info *fi, void *fdt,
					int offs, struct nboot_info *ni)
{
	int err;
	unsigned int align = fi->mtd->erasesize;

	err = fs_image_get_si(fdt, offs, align, "SPL", &ni->spl);
	if (err)
		return err;

	err = fs_image_get_si(fdt, offs, align, "NBOOT", &ni->nboot);
	if (err)
		return err;

	err = fs_image_get_si(fdt, offs, align, "U-BOOT", &ni->uboot);
	if (err)
		return err;

	/*
	 * The size of the environment region is actually given by env-range
	 * entry, from which env-size bytes are then used.
	 */
	ni->env.type = "ENV";
	err = fs_image_get_fdt_val(fdt, offs, "env-start", align,
				   2, ni->env.start);
	if (!err) {
		err = fs_image_get_fdt_val(fdt, offs, "env-range", align,
					   1, &ni->env.size);
		if (err)
			return err;
		/* The size only has to be aligned to pages */
		align = fi->mtd->writesize;
		err = fs_image_get_fdt_val(fdt, offs, "env-size", align,
					   1, &ni->env_used);
	} else if (err == -ENOENT) {
		/*
		 * No env data found in nboot-info, fall back to some known
		 * values. Use option -e to select index.
		 */
		err = fs_image_get_known_env_nand(early_support_index,
						  ni->env.start, &ni->env_used);
		ni->env.size = CONFIG_ENV_NAND_RANGE;;
	}
	if (err)
		return err;

	debug("- nboot-info@0x%lx (nand): board-cfg-size=0x%08x (%s layout)\n",
	      (ulong)fdt, ni->board_cfg_size, ni->board_cfg_size ? "old":"new");
	debug("- spl:   start=0x%08x/0x%08x size=0x%08x\n",
	      ni->spl.start[0], ni->spl.start[1], ni->spl.size);
	debug("- nboot: start=0x%08x/0x%08x size=0x%08x\n",
	      ni->nboot.start[0], ni->nboot.start[1], ni->nboot.size);
	debug("- uboot: start=0x%08x/0x%08x size=0x%08x\n",
	      ni->uboot.start[0], ni->uboot.start[1], ni->uboot.size);
	debug("- env:   start=0x%08x/0x%08x size=0x%08x env_used=0x%08x\n",
	      ni->env.start[0], ni->env.start[1], ni->env.size, ni->env_used);

	return 0;
}

/* Compute checksum for FCB or DBBT block */
static u32 fs_image_bcb_checksum(void *data, size_t size)
{
	u32 checksum = 0;
	u8 *p = data;

	while (size--)
		checksum += *p++;

	return ~checksum;
}

/* Check if checksum for FCB or DBBT block is correct */
static int fs_image_check_bcb_checksum(void *data, size_t size,
					u32 expected, bool accept_zero)
{
	u32 checksum;

	debug("  -");
	if (accept_zero && (expected == 0)) {
		debug(" Checksum 0 OK\n");
		return 0;
	}

	checksum = fs_image_bcb_checksum(data, size);
	if (checksum == expected) {
		debug(" Checksum OK\n");
		return 0;
	}

	puts(" BAD CHECKSUM");
	debug(" (got 0x%08x, expected 0x%08x)\n", checksum, expected);

	return -EILSEQ;
}

/* Load some data from offset with given size */
static int fs_image_read_nand(struct flash_info *fi, uint offs, uint size,
			      uint lim, uint flags, u8 *buf)
{
	int err;
	size_t rsize;
	size_t actual;
	loff_t roffs;
	loff_t maxsize;
	size_t bb_extra;

	/* FCB, DBBT and DBBT_DATA sub-images ignore any bad block offsets */
	if (flags & (SUB_IS_FCB | SUB_IS_DBBT | SUB_IS_DBBT_DATA)) {
		unsigned int block_offs = offs & ~(fi->mtd->erasesize - 1);

		if (nand_block_isbad(fi->mtd, block_offs)) {
			puts(" BAD BLOCK!");
			return -EBADMSG;;
		}
		if (flags & SUB_IS_FCB) {
			debug("  - Switch to 62bit ECC\n");
			mxs_nand_mode_fcb_62bit(fi->mtd);
		}
	}

	rsize = size;
	roffs = offs + fi->bb_extra_offs;
	maxsize = lim - roffs;

	debug("  -> nand_read from offs 0x%llx size 0x%x maxsize 0x%llx\n",
	     roffs, size, maxsize);
	err = nand_read_skip_bad(fi->mtd, roffs, &rsize, &actual,
				  maxsize, buf);

	if (flags & SUB_IS_FCB) {
		debug("  - Switch back to normal ECC\n");
		mxs_nand_mode_normal(fi->mtd);
	}

	bb_extra = actual - rsize;
	if (bb_extra) {
		debug("  - Adding 0x%lx to bad block offset\n", bb_extra);
		fi->bb_extra_offs += bb_extra;
	}

	return err;
}

/* Load the image of given type/descr from NAND flash at given offset */
static int fs_image_load_image_nand(struct flash_info *fi, int copy,
				    const struct storage_info *si,
				    struct sub_info *sub)
{
	int err;
	unsigned int size;
	unsigned int offs = si->start[copy] + sub->offset;
	unsigned int lim = si->start[copy] + si->size;
	size_t cs_size;

	sub->size = 0;

	printf("  Loading copy %d from offset 0x%08x", copy, offs);
	debug("\n");

#ifdef CONFIG_IMX8MM
	/* On i.MX8MM, SPL starts at offset 0x400 in the middle of the page */
	if (sub->flags & SUB_IS_SPL)
		offs += 0x400;
#endif

	/* Clear the temp buffer (read cache) */
	fs_image_drop_temp(fi);

	/* Determine sub-image size */
	if (sub->flags & SUB_IS_FCB) {
		size = sizeof(struct fcb_block);
	} else if ((sub->flags & SUB_IS_DBBT)
		   || (sub->flags & SUB_IS_DBBT_DATA)) {
		size = fi->mtd->writesize;
	} else {
		/* Load header (IVT, FIT or FS_HEADER) and get size from it */
		size = sizeof(union any_header);
		err = fs_image_load_sub(fi, offs, size, lim, 0, sub->img);
		if (err)
			return err;

		if (fdt_magic(sub->img) == FDT_MAGIC) {
			/* Read the FDT part of the FIT image to get size */
			size = fdt_totalsize(sub->img);
			err = fs_image_load_sub(fi, offs, size, lim, 0,
						sub->img);
			if (!err)
				err = fs_image_get_size_from_fit(sub, &size);
		} else {
			err = fs_image_get_size_from_fsh_or_ivt(sub, &size);
#ifdef CONFIG_IMX8MM
			/* Remove offset */
			if (sub->flags & SUB_IS_SPL)
				size -= 0x400;
#endif
		}
		if (err)
			return err;
	}

	printf(" size 0x%x...", size);
	debug("\n");

	/* Load image itself */
	err = fs_image_load_sub(fi, offs, size, lim, sub->flags, sub->img);
	if (err)
		return err;

	/* Check if image is corrupted */
	if (sub->flags & SUB_IS_FCB) {
		struct fcb_block *fcb = sub->img;

		if ((fcb->fingerprint != FCB_FINGERPRINT)
		    || (fcb->version != FCB_VERSION_1))
			return -ENOENT;

		cs_size = sizeof(struct fcb_block) - 4;
		err = fs_image_check_bcb_checksum(sub->img + 4, cs_size,
						  fcb->checksum, false);
	} else if (sub->flags & SUB_IS_DBBT) {
		struct dbbt_block *dbbt = sub->img;

		if ((dbbt->fingerprint != DBBT_FINGERPRINT)
		    || (dbbt->version != DBBT_VERSION_1))
			return -ENOENT;

		/* NXP's kobs writes checksum as 0, accept that */
		cs_size = sizeof(struct dbbt_block) - 4;
		err = fs_image_check_bcb_checksum(sub->img + 4, cs_size,
						  dbbt->checksum, true);
	} else if (sub->flags & SUB_IS_DBBT_DATA) {
		u32 *dbbt_data = sub->img;
		u32 count = dbbt_data[1];

		/* Detection is vague, but enough to not accept empty pages */
		if (count > 32)
			return -ENOENT;

		/* NXP's kobs writes checksum as 0, accept that */
		cs_size = (count + 1) * sizeof(u32);
		err = fs_image_check_bcb_checksum(sub->img + 4, cs_size,
						  dbbt_data[0], true);
	} else if (sub->flags & SUB_HAS_FS_HEADER) {
		err = fs_image_check_all_crc32(sub->img);
	} else if (fs_image_is_fs_image(sub->img)) {
		/*
		 * We found an F&S header on an image that may or may not have
		 * one (e.g. U-BOOT). Check the CRC32, but then remove the
		 * header, because the caller has already inserted an empty
		 * header before sub->img and will fill it after we return.
		 */
		err = fs_image_check_all_crc32(sub->img);
		size -= FSH_SIZE;
		memmove(sub->img, sub->img + FSH_SIZE, size);
	}
	if (err < 0)
		return err;

	sub->size = size;

	return 0;
}

/* Temporarily load Boot Control Block (BCB) with FCB and DBBT */
static int fs_image_load_extra_nand(struct flash_info *fi,
				    struct storage_info *spl, void *tempaddr)
{
	struct fcb_block *fcb;
	struct dbbt_block *dbbt;
	u32 *dbbt_data;
	struct sub_info sub;
	u32 start;
	struct storage_info bcb;
	unsigned int pages_per_block;
	struct mtd_info *mtd = fi->mtd;
	int copy;
	int err;

	/* Load Firmware Configuration Block (FCB) */
	bcb.start[0] = 0;
	bcb.start[1] = mtd->erasesize;
	bcb.size = mtd->erasesize;

	sub.type = "FCB";
	sub.descr = fs_image_get_arch();
	sub.img = tempaddr;
	sub.offset = 0;
	sub.flags = SUB_IS_FCB;
	err = fs_image_load_image(fi, &bcb, &sub);
	if (err)
		return err;

	fcb = tempaddr + FSH_SIZE;
	for (copy = 0; copy < 2; copy++) {
		start = copy ? fcb->fw2_start : fcb->fw1_start;
		start *= mtd->writesize;
		if (start != spl->start[copy]) {
			printf("  Warning! SPL copy %d is on offset 0x%08x,"
			       " should be on 0x%08x\n",
			       copy, start, spl->start[copy]);
			spl->start[copy] = start;
		}
	}

	/* Load Discovered Bad Block Table (DBBT) */
	pages_per_block = mtd->erasesize / mtd->writesize;
	start = fcb->dbbt_start / pages_per_block * mtd->erasesize;
	bcb.start[0] = start;
	bcb.start[1] = start + mtd->erasesize;
	bcb.size = mtd->erasesize;

	/* Do not fail on DBBT/DBBT-DATA, output is for information only */
	tempaddr = sub.img;
	sub.type = "DBBT";
	sub.offset = fcb->dbbt_start % pages_per_block * mtd->writesize;
	sub.flags = SUB_IS_DBBT;
	err = fs_image_load_image(fi, &bcb, &sub);
	if (err) {
		printf("  Warning! No Discovered Bad Block Table (DBBT)!\n");
		return 0;		/* Don't fail, DBBT is not important */
	}

	dbbt = tempaddr + FSH_SIZE;
	if (!dbbt->dbbtpages) {
		printf("  No Bad Blocks in DBBT recorded\n");
		return 0;
	}

	tempaddr = sub.img;
	sub.type = "DBBT-DATA";
	sub.offset += 4 * mtd->writesize;
	sub.flags = SUB_IS_DBBT_DATA;
	err = fs_image_load_image(fi, &bcb, &sub);
	if (err) {
		printf("  Warning! No DBBT data found!\n");
		return 0;		/* Don't fail, DBBT is not important */
	}

	dbbt_data = tempaddr + FSH_SIZE;
	printf("  %d bad block(s) in DBBT recorded\n", dbbt_data[1]);

	return 0;
}

/* Erase given region */
static int fs_image_invalidate_nand(struct flash_info *fi, int copy,
			       const struct storage_info *si)
{
	loff_t offs = si->start[copy];
	size_t size = si->size;
	struct nand_erase_options opts = {0};
	int err;

	printf("  Erasing %s region at offset 0x%llx size 0x%zx...",
	       si->type, offs, size);
	debug("\n");

	opts.length = size;
	opts.lim = size;
	opts.quiet = 1;
	opts.offset = offs;

	/* This automatically marks blocks bad if they cannot be erased */
	err = nand_erase_opts(fi->mtd, &opts);

	fs_image_show_sub_status(err);

	return err;
}

/* Save some data (only full pages) to NAND; return 1 if new bad block */
static int fs_image_write_nand(struct flash_info *fi, uint offs, uint size,
			       uint lim, uint flags, u8 *buf)
{
	int err;
	size_t wsize;
	size_t actual;
	loff_t woffs;
	loff_t maxsize;
	size_t bb_extra;

	/* FCB, DBBT and DBBT_DATA sub-images ignore any bad block offsets */
	if (flags & (SUB_IS_FCB | SUB_IS_DBBT | SUB_IS_DBBT_DATA)) {
		unsigned int block_offs = offs & ~(fi->mtd->erasesize - 1);

		if (nand_block_isbad(fi->mtd, block_offs)) {
			puts(" BAD BLOCK!");
			return -EBADMSG;;
		}
		if (flags & SUB_IS_FCB) {
			debug("  - Switch to 62bit ECC\n");
			mxs_nand_mode_fcb_62bit(fi->mtd);
			size = fi->mtd->writesize;
		}
	}

	wsize = size;
	woffs = offs + fi->bb_extra_offs;
	maxsize = lim - woffs;

	debug("  -> nand_write to offs 0x%llx size 0x%x maxsize 0x%llx\n",
	      woffs, size, maxsize);
	err = nand_write_skip_bad(fi->mtd, woffs, &wsize, &actual,
				  maxsize, buf, WITH_WR_VERIFY);
	if (flags & SUB_IS_FCB) {
		debug("  - Switch back to normal ECC\n");
		mxs_nand_mode_normal(fi->mtd);
	}
	if (err) {
		/*
		 * ### TODO:
		 * Handle new bad blocks and return 1. Actually written bytes
		 * are in wsize, i.e. this is kind of a pointer to the bad
		 * page, but nevertheless difficult to handle in case of bad
		 * blocks. Also difficult to say if the call failed because of
		 * a real write failure (where the block should be marked bad)
		 * or because of some other minor error. For example if wsize
		 * is 0 after return, this could be because the image does not
		 * fit because of bad blocks, or there was a real write error
		 * right in the first page. Maybe check for -EIO?
		 *
		 * Or should we implement our own writing loop that writes
		 * block by block? This seems like re-inventing the wheel.
		 */
		return err;
	}

	bb_extra = actual - wsize;
	if (bb_extra) {
		debug("  - Adding 0x%lx to bad block offset\n", bb_extra);
		fi->bb_extra_offs += bb_extra;
	}

	return 0;
}

/* Show region info */
static int fs_image_prepare_region_nand(struct flash_info *fi, int copy,
					struct storage_info *si)
{
	printf("  -- %s --\n", si->type);

	return 0;
}

/* Save NBOOT and SPL region to NAND */
#define DBBT_DATA_BLOCKS 32
#define DBBT_DATA_ENTRIES (DBBT_DATA_BLOCKS - 8)
static int fs_image_save_nboot_nand(struct flash_info *fi,
				    struct region_info *nboot_ri,
				    struct region_info *spl_ri)
{
	int failed;
	int copy, start_copy;
	u32 i, bad_blocks;
	struct storage_info bcb_si;
	struct region_info bcb_ri;
	struct sub_info bcb_sub[3];
	struct fcb_block fcb;
	struct dbbt_block dbbt;
	u32 dbbt_data[DBBT_DATA_ENTRIES + 2];
	struct nand_chip *chip = mtd_to_nand(fi->mtd);
	struct mxs_nand_info *nand_info = nand_get_controller_data(chip);
	struct mxs_nand_layout mxs_layout;
	const char *arch = fs_image_get_arch();

	/*
	 * On NAND we have an additional region called BCB (Boot Control
	 * Block). This consists of:
	 *
	 * 1. FCB (Firmware Configuration Block)
	 * One FCB is stored in the first page of the first n blocks in NAND.
	 * It is a struct that tells the ROM Loader which NAND settings and
	 * ECC to use to access further images. It also says where the DBBT and
	 * the two Firmware copies (a.k.a. SPL) are located. The FCB itself is
	 * loaded by the ROM Loader with safe NAND timings and a very high
	 * ECC: 8 chunks with 128 Bytes with 62-Bit ECC each, which is 1024
	 * bytes main data in total, and additional 32 Bytes metadata in the
	 * first chunk. This allows for up to 496 bit errors within those 1056
	 * Bytes.
	 *
	 * 2. DBBT (Discovered Bad Block Table)
	 * The page number of the first DBBT is given by the FCB. The next
	 * DBBT is in the next block at the same page offset. NXPs kobs tool
	 * locates the DBBT copies in the n blocks after the FCB copies, but we
	 * are using the same blocks as for FCB, but in the 4th page. The DBBT
	 * is a struct that tells the ROM Loader how many DBBT-DATA pages
	 * there are. If there are no bad blocks in the boot area, no
	 * DBBT-DATA pages are required.
	 *
	 * 3. DBBT-DATA
	 * This entry starts 4 pages behind DBBT and is only necessary, if
	 * there are bad blocks in the boot area. It tells the ROM Loader how
	 * many of those blocks are bad, and then lists the numbers of those
	 * blocks. It helps the ROM Loader so that it does not need to read
	 * all the Bad Block Markers. We consider the NBoot MTD partition to
	 * be our boot area. It consists of 32 blocks, but at least 8 of them
	 * need to be intact to have at least one copy of BCB and SPL. So we
	 * only prepare an array for at most 24 bad block entries.
	 *
	 * The number n of FCB and DBBT copies is given by OTP fuses and can
	 * be read from BOOT_SEARCH_COUNT (0x470[6:5] on i.MX8MM, 0x1B0[7:8]
	 * on i.MX8X) or NAND_FCB_SEARCH_COUNT (0x4A0[14:13] on i.MX8MN/MP)
	 * respectively. n can be 2, 4 or 8. We do not change the fuses and
	 * therefore assume two copies like with all our other images. But our
	 * layout has room for up to four copies, so we can change this in the
	 * future if we find this useful. Please note that the ROM Loader does
	 * *not* skip bad blocks for FCB/DBBT, so if a block is bad there,
	 * this copy simply does not exist, which reduces the number of
	 * available copies.
	 */
	bcb_si.type = "BCB";
	bcb_si.start[0] = 0x0;
	bcb_si.start[1] = fi->mtd->erasesize;
	bcb_si.size = fi->mtd->erasesize;

	/* Fill FCB (Firmware Configuration Block) */
	memset(&fcb, 0, sizeof(struct fcb_block));
	mxs_nand_get_layout(fi->mtd, &mxs_layout);

	fcb.fingerprint = FCB_FINGERPRINT;
	fcb.version = FCB_VERSION_1;

	fcb.datasetup = 80;
	fcb.datahold = 60;
	fcb.addr_setup = 25;
	fcb.dsample_time = 6;

	fcb.pagesize = fi->mtd->writesize;
	fcb.oob_pagesize = fcb.pagesize + fi->mtd->oobsize;
	fcb.sectors = fi->mtd->erasesize / fcb.pagesize;

	fcb.meta_size = mxs_layout.meta_size;
	fcb.nr_blocks = mxs_layout.nblocks;
	fcb.ecc_nr = mxs_layout.data0_size;
	fcb.ecc_level = mxs_layout.ecc0;
	fcb.ecc_size = mxs_layout.datan_size;
	fcb.ecc_type = mxs_layout.eccn;
	fcb.bchtype = mxs_layout.gf_len;

	/* DBBT search area starts in first block at page 4 */
	fcb.dbbt_start = 4;

	fcb.bb_byte = nand_info->bch_geometry.block_mark_byte_offset;
	fcb.bb_start_bit = nand_info->bch_geometry.block_mark_bit_offset;

	fcb.phy_offset = fcb.pagesize;

	fcb.disbbm = 0;

	fcb.fw1_start = spl_ri->si->start[0] / fcb.pagesize;
	fcb.fw2_start = spl_ri->si->start[1] / fcb.pagesize;
	fcb.fw1_pages = (spl_ri->sub[0].size + fcb.pagesize - 1) / fcb.pagesize;
	fcb.fw2_pages = fcb.fw1_pages;

	fcb.checksum = fs_image_bcb_checksum((void *)&fcb.fingerprint,
					     sizeof(fcb) - 4);

	/* Fill DBBT-DATA */
	memset(&dbbt_data[2], 0xFF, DBBT_DATA_ENTRIES * 4);
	bad_blocks = 0;
	for (i = 0; i < DBBT_DATA_BLOCKS; i++) {
		unsigned int offs = i * fi->mtd->erasesize;

		if (mtd_block_isbad(fi->mtd, offs)) {
			debug("- Found bad block 0x%x\n", offs);
			dbbt_data[2 + bad_blocks] = i;
			if (++bad_blocks >= DBBT_DATA_ENTRIES)
				break;
		}
	}
	debug("- Total %u bad block(s)\n", bad_blocks);
	dbbt_data[1] = bad_blocks;
	dbbt_data[0] = fs_image_bcb_checksum(&dbbt_data[1], bad_blocks * 4 + 4);

	/* Fill DBBT (Discovered Bad Block Table) */
	memset(&dbbt, 0, sizeof(struct dbbt_block));
	dbbt.fingerprint = DBBT_FINGERPRINT;
	dbbt.version = DBBT_VERSION_1;
	dbbt.dbbtpages = (bad_blocks > 0);
	dbbt.checksum = fs_image_bcb_checksum(&dbbt.fingerprint,
					      sizeof(dbbt) - 4);

#ifdef CONFIG_IMX8MM
	/* On i.MX8MM, SPL starts on offset 0x400 in the middle of the page */
	spl_ri->sub[0].offset += 0x400;
#endif

	fs_image_region_create(&bcb_ri, &bcb_si, bcb_sub);
	fs_image_region_add_raw(&bcb_ri, &fcb, "FCB", arch, 0,
				SUB_IS_FCB | SUB_SYNC, sizeof(fcb));
	fs_image_region_add_raw(&bcb_ri, &dbbt, "DBBT", arch,
				4 * fi->mtd->writesize,
				SUB_IS_DBBT | SUB_SYNC, sizeof(dbbt));
	if (bad_blocks > 0) {
		fs_image_region_add_raw(&bcb_ri, dbbt_data, "DBBT-DATA",
					arch, 8 * fi->mtd->writesize,
					SUB_IS_DBBT_DATA | SUB_SYNC,
					bad_blocks * 4 + 8);
	}

	/*
	 * When saving NBoot, start with "the other" copy first, i.e. if
	 * running form Primary SPL, update the Secondary copy first and if
	 * running from Secondary SPL, update the Primary copy first. The
	 * reason is that the current copy is apparently working, but the
	 * other copy may very well be broken. So it makes sense to first
	 * update the broken version and repair it by doing so, before
	 * touching the working version.
	 *
	 * For example if currently running on the Secondary copy, this means
	 * that the Primary copy is damaged. So if the Secondary copy was
	 * updated first and this failed for some reason, then both copies
	 * would be non-functional and the board would be bricked. But if the
	 * damaged Primary copy is updated first and this succeeds, then the
	 * Primary copy is repaired and provides a working fallback when
	 * writing the Secondary copy afterwards.
	 *
	 * Start with the "other" copy:
	 *
	 *  1. Invalidate the "other" NBOOT region by erasing the region. This
	 *     immediately invalidates the F&S header of the BOARD-CFG so that
	 *     this copy will definitely not be loaded anymore.
	 *  2. Write all of the "other" NBOOT but the first block. If
	 *     interrupted, the BOARD-CFG ist still invalid and will not be
	 *     loaded.
	 *  3. Write first block of the "other" NBOOT. This adds the F&S
	 *     header and makes NBOOT valid.
	 *  4. Erase the "other" SPL region. This immediately invalidates SPL
	 *     (IVT) so that this copy will definitely not be loaded anymore.
	 *  5. Write all of the "other" SPL but the first block. If
	 *     interrupted, SPL is still invalid and will not be loaded.
	 *  6. Write the first block of the "other" SPL. This adds the IVT and
	 *     makes SPL valid.
	 *  7. Erase the "other" BCB region.
	 *  8. Write DBBT and DBBT-DATA of the "other" BCB.
	 *  9. Write FCB of the "other" BCB. This validates the BCB.
	 *
	 * If interrupted somewhere in steps 1 to 6, the "current" copy is
	 * still available and will continue to boot. After step 6, the
	 * "other" copy is fully functional. So if the "other" copy is the
	 * Primary copy, it will be booted after Step 6 again. (Unless the
	 * start address of new SPL has changed so that the old BCB entries do
	 * not point to it anymore. In that case, the Primary copy will be
	 * started after step 9 again.)
	 *
	 * 10. Update the "current" NBOOT in the same sequence.
	 * 11. Update the "current" SPL in the same sequence.
	 * 12. Update the "current" BCB in the same sequence.
	 *
	 * The worst case happens if interrupted in step 10 and if "current"
	 * is the Primary copy. Then the "current" (=Primary) but still old SPL
	 * will boot, but fails to load the "current" (=Primary) NBOOT,
	 * because it is invalid right now. So it will fall back to load the
	 * "other" (=Secondary) NBOOT, which is the new version already. This
	 * may or may not work, depending on how compatible the old and new
	 * versions are.
	 *
	 * If interrupted in step 11, the "other" (=Secondary) copy is loaded,
	 * which is the new version already. This is OK. If interrupted in
	 * step 12, the "current" (=Primary) copy is loaded, but then SPL and
	 * NBOOT are also both the new version, so this is OK, too.
	 *
	 * ### TODO:
	 * The sequence above assumes that SPL can detect correctly from which
	 * copy it was booting. Currently this is not true on i.MX8MN/MP/X.
	 */
	failed = 0;
	start_copy = fs_image_get_start_copy();
	copy = start_copy;
	do {
		printf("\nSaving copy %d to %s:\n", copy, fi->devname);
		if (fs_image_save_region(fi, copy, nboot_ri))
			failed |= BIT(copy);

		if (fs_image_save_region(fi, copy, spl_ri))
			failed |= BIT(copy);

		if (fs_image_save_region(fi, copy, &bcb_ri))
			failed |= BIT(copy);
		copy = 1 - copy;
	} while (copy != start_copy);

	return failed;
}

static void fs_image_get_flash_nand(struct flash_info *fi)
{
	/* Temporary buffer is for one page */
	fi->temp_size = fi->mtd->writesize;
	fi->temp_fill = 0xff;

	/* Set device name */
	strcpy(fi->devname, "NAND");
}

static void fs_image_put_flash_nand(struct flash_info *fi)
{
	/* Nothing to be done in case of NAND */
}

struct flash_ops flash_ops_nand = {
	.check_for_uboot = fs_image_check_for_uboot_nand,
	.check_for_nboot = fs_image_check_for_nboot_nand,
	.get_nboot_info = fs_image_get_nboot_info_nand,
	.read = fs_image_read_nand,
	.load_image = fs_image_load_image_nand,
	.load_extra = fs_image_load_extra_nand,
	.invalidate = fs_image_invalidate_nand,
	.write = fs_image_write_nand,
	.prepare_region = fs_image_prepare_region_nand,
	.save_nboot = fs_image_save_nboot_nand,
	.get_flash = fs_image_get_flash_nand,
	.put_flash = fs_image_put_flash_nand,
};

#endif /* CONFIG_CMD_NAND */


/* ------------- MMC handling ---------------------------------------------- */

#ifdef CONFIG_CMD_MMC

/* Check if any prerequisites for storing U-Boot are not met */
static bool fs_image_check_for_uboot_mmc(struct storage_info *si, bool force)
{
	/* Nothing to be done in case of MMC */
	return false;
}

/* Check if any prerequisites for storing NBoot are not met */
static bool fs_image_check_for_nboot_mmc(struct flash_info *fi,
					 struct storage_info *si, bool force)
{
#ifndef CONFIG_IMX8MM
	u32 offset_fuses = fs_board_get_secondary_offset();
	u32 offset_nboot = si->start[1];

	/*
	 * If booting from User space of eMMC, check the fused secondary image
	 * matches the required offset
	 */
	if (!fi->boot_hwpart && (offset_fuses != offset_nboot)) {
		printf("Secondary Image Offset in fuses is 0x%08x but new NBOOT"
		       " wants 0x%08x.\nSecond copy (backup) will not boot"
		       " in case of errors!\n", offset_fuses, offset_nboot);
		if (!force && !fs_image_confirm())
			return true;
	}
#endif

	return false;
}

static int fs_image_set_hwpart_mmc(struct flash_info *fi, int copy,
				   const struct storage_info *si)
{
	int err;
	unsigned int hwpart = si->hwpart[copy];

	err = blk_dselect_hwpart(fi->blk_desc, hwpart);
	if (err)
		printf("  Cannot switch to hwpart %d on mmc%d for %s (%d)\n",
		       hwpart, fi->blk_desc->devnum, si->type, err);

	return err;
}

/* Parse nboot-info for MMC settings and fill struct */
static int fs_image_get_nboot_info_mmc(struct flash_info *fi, void *fdt,
				       int offs, struct nboot_info *ni)
{
	int err;
	unsigned int align = fi->blk_desc->blksz;
	u8 first = fi->boot_hwpart;
	u8 second = first ? (3 - first) : first;

	/* Pre 2023.08, everything was in the same boot hwpart on fsimx8mm */
	if (!(ni->flags & NI_EMMC_BOTH_BOOTPARTS))
		second = first;

	/* Get SPL storage info */
	err = fs_image_get_si(fdt, offs, align, "SPL", &ni->spl);
	if (err)
		return err;
	ni->spl.hwpart[0] = first;
	ni->spl.hwpart[1] = second;

	/* Get NBoot storage info */
	err = fs_image_get_si(fdt, offs, align, "NBOOT", &ni->nboot);
	if (err)
		return err;
	ni->nboot.hwpart[0] = first;
	ni->nboot.hwpart[1] = second;

	/* Get U-Boot storage info */
	err = fs_image_get_si(fdt, offs, align, "U-BOOT", &ni->uboot);
	if (err)
		return err;
	if (ni->flags & NI_UBOOT_EMMC_BOOTPART) {
		ni->uboot.hwpart[0] = first;
		ni->uboot.hwpart[1] = second;
	} else {
		ni->uboot.hwpart[0] = 0;
		ni->uboot.hwpart[1] = 0;
	}

#ifndef CONFIG_IMX8MM
	if (first) {
		/* SPL always starts on sector 0 in boot1/2 hwpart */
		ni->spl.start[0] = 0;
		ni->spl.start[1] = 0;

		/* Both NBoot copies start on same sector in boot1/2 */
		ni->nboot.start[1] = ni->nboot.start[0];
	}
#endif

	/* Get env info from nboot-info */
	err = fs_image_get_si(fdt, offs, align, "ENV", &ni->env);
	if (err == -ENOENT) {
		/*
		 * No env data found in nboot-info, fall back to some known
		 * values. Use option -e to select index.
		 */
		err = fs_image_get_known_env_mmc(early_support_index,
						 ni->env.start, &ni->env.size);
	}
	if (err)
		return err;

	/*
	 * In the past, both environment copies were on the same hwpart. But
	 * if the two addresses are equal, they are obviously on different
	 * hwparts.
	 */
	ni->env.hwpart[0] = first;
	ni->env.hwpart[1] = first;
	if (first && (ni->env.start[0] == ni->env.start[1]))
		ni->env.hwpart[1] = second;

	debug("- nboot-info@0x%lx (%s): board-cfg-size=0x%08x (%s layout)\n",
	      (ulong)fdt, first ? "emmc-boot" : "sd-user", ni->board_cfg_size,
	      ni->board_cfg_size ? "old" : "new");
	debug("- spl:   start=%d:0x%08x/%d:0x%08x size=0x%08x\n",
	      ni->spl.hwpart[0], ni->spl.start[0],
	      ni->spl.hwpart[1], ni->spl.start[1], ni->spl.size);
	debug("- nboot: start=%d:0x%08x/%d:0x%08x size=0x%08x\n",
	      ni->nboot.hwpart[0], ni->nboot.start[0],
	      ni->nboot.hwpart[1], ni->nboot.start[1], ni->nboot.size);
	debug("- uboot: start=%d:0x%08x/%d:0x%08x size=0x%08x\n",
	      ni->uboot.hwpart[0], ni->uboot.start[0],
	      ni->uboot.hwpart[1], ni->uboot.start[1], ni->uboot.size);
	debug("- env:   start=%d:0x%08x/%d:0x%08x size=0x%08x\n",
	      ni->env.hwpart[0], ni->env.start[0],
	      ni->env.hwpart[1], ni->env.start[1], ni->env.size);

	return 0;
}

/* Read image at offset with given size */
static int fs_image_read_mmc(struct flash_info *fi, uint offs, uint size,
			     uint lim, uint flags, u8 *buf)
{
	unsigned long count;
	unsigned long blksz = fi->blk_desc->blksz;
	lbaint_t blk = offs / blksz;
	lbaint_t blk_count = (size + blksz - 1) / blksz;;

	debug("  -> mmc_read from offs 0x%x (block 0x" LBAF ") size 0x%x\n",
	      offs, blk, size);

	count = blk_dread(fi->blk_desc, blk, blk_count, buf);
	if (count < blk_count)
		return -EIO;
	else if (IS_ERR_VALUE(count))
		return (int)count;

	return 0;
}

/* Load the image of given type/descr from eMMC at given offset */
static int fs_image_load_image_mmc(struct flash_info *fi, int copy,
				   const struct storage_info *si,
				   struct sub_info *sub)
{
	unsigned int size;
	unsigned int offs = si->start[copy] + sub->offset;
	unsigned int lim = si->start[copy] + si->size;
	unsigned int hwpart = si->hwpart[copy];
	int err;

	sub->size = 0;

	printf("  Loading copy %d from hwpart %d offset 0x%08x", copy, hwpart,
	       offs);
	debug("\n");

	/* Clear the temp buffer (read cache) */
	fs_image_drop_temp(fi);

	err = fs_image_set_hwpart_mmc(fi, copy, si);
	if (err)
		return err;

	/* Read F&S header, IVT or FIT header */
	size = sizeof(union any_header);
	err = fs_image_load_sub(fi, offs, size, lim, 0, sub->img);
	if (err)
		return err;

	/* Get image size */
	if (fdt_magic(sub->img) == FDT_MAGIC) {
		/* Read the FDT part of the FIT image to get size */
		size = fdt_totalsize(sub->img);
		err = fs_image_load_sub(fi, offs, size, lim, 0, sub->img);
		if (!err)
			err = fs_image_get_size_from_fit(sub, &size);
	} else {
		err = fs_image_get_size_from_fsh_or_ivt(sub, &size);
#ifdef CONFIG_IMX8MM
		/* Remove virtual IVT offset */
		if (sub->flags & SUB_IS_SPL)
			size -= 0x400;
#endif
	}
	if (err)
		return err;

	printf(" size 0x%x...", size);
	debug("\n");

	/* Load whole image incl. header */
	err = fs_image_load_sub(fi, offs, size, lim, sub->flags, sub->img);
	if (err)
		return err;

	if (sub->flags & SUB_HAS_FS_HEADER) {
		err = fs_image_check_all_crc32(sub->img);
		if (err < 0)
			return err;
	} else if (fs_image_is_fs_image(sub->img)) {
		/*
		 * We found an F&S header on an image that may or may not have
		 * one (e.g. U-BOOT). Check the CRC32, but then remove the
		 * header, because the caller has already inserted an empty
		 * header before sub->img and will fill it after we return.
		 */
		err = fs_image_check_all_crc32(sub->img);
		size -= FSH_SIZE;
		memmove(sub->img, sub->img + FSH_SIZE, size);
	}

	sub->size = size;

	return 0;
}

/* Temporarily load Secondary Image Table */
static int fs_image_load_extra_mmc(struct flash_info *fi,
				   struct storage_info *spl, void *tempaddr)
{
#ifdef CONFIG_IMX8MM
	unsigned int blksz = fi->blk_desc->blksz;
	unsigned int offs;
	unsigned int lim;
	int err;
	struct info_table *secondary;

	/* Do nothing if not running from User area of eMMC */
	if (spl->hwpart[1] != 0)
		return 0;

	/* Read Secondary Image Table, which is one block before primary SPL */
	lim = spl->start[0];
	offs = lim - blksz;

	printf("Loading SECONDARY-SPL-INFO from NAND\n"
	       "  Loading only copy from offset 0x%08x size 0x%x...",
	       offs, blksz);

	err = fi->ops->read(fi, offs, blksz, lim, 0, fi->temp);
	secondary = (struct info_table *)(fi->temp);
	if (!err && (secondary->tag != 0x00112233))
		err = -ENOENT;

	fs_image_show_sub_status(err);
	if (err)
		return 0;		/* Don't fail, rely on nboot-info */

	/*
	 * The first_sector entry is counted relative to 0x8000 (0x40 blocks)
	 * and also the two skipped blocks for MBR and Secondary Image Table
	 * must be included, even if empty in the secondary case, which
	 * results in 0x42 blocks that have to be added.
	 */
	offs = (secondary->first_sector + 0x42) * blksz;
	if (offs != spl->start[1]) {
		printf("  Warning! SPL copy 1 is on offset 0x%08x, should be"
		       " on 0x%08x\n", offs, spl->start[1]);
		spl->start[1] = offs;
	}
	memset(fi->temp, fi->temp_fill, fi->temp_size);
#endif

	return 0;
}

/* Invalidate an image by overwriting the first block with zeroes */
static int fs_image_invalidate_mmc(struct flash_info *fi, int copy,
				   const struct storage_info *si)
{
	unsigned int offs = si->start[copy];
	unsigned int lim = offs + si->size;
	int err;

	printf("  Invalidating %s at offset 0x%08x size 0x%x...",
	       si->type, offs, si->size);
	debug("\n");

	memset(fi->temp, 0, fi->temp_size);
	err = fi->ops->write(fi, offs, fi->temp_size, lim, 0, fi->temp);

	fs_image_show_sub_status(err);

	return err;
}

/* Save some data (only full pages) to NAND; return 1 if new bad block */
static int fs_image_write_mmc(struct flash_info *fi, uint offs, uint size,
			      uint lim, uint flags, u8 *buf)
{
	unsigned long count;
	unsigned long blksz = fi->blk_desc->blksz;
	lbaint_t blk = offs / blksz;
	lbaint_t blk_count = (size + blksz - 1) / blksz;;

	/* Bad block handling is done by eMMC controller */
	debug("  -> mmc_write to offs 0x%x (block 0x" LBAF ") size 0x%x\n",
	      offs, blk, size);

	count = blk_dwrite(fi->blk_desc, blk, blk_count, buf);
	if (count < blk_count)
		return -EIO;
	else if (IS_ERR_VALUE(count))
		return (int)count;

	return 0;
}

/* Switch to partition where reion is located and show region info */
static int fs_image_prepare_region_mmc(struct flash_info *fi, int copy,
				       struct storage_info *si)
{
	int err;

	err = fs_image_set_hwpart_mmc(fi, copy, si);
	if (err)
		return err;

	printf("  -- %s (hwpart %d) --\n", si->type, si->hwpart[copy]);

	return 0;
}

#ifdef CONFIG_IMX8MM
/* Write Secondary Image table for redundant SPL */
static int fs_image_write_secondary_table(struct flash_info *fi, int copy,
					  struct storage_info *si)
{
	unsigned int blksz = fi->blk_desc->blksz;
	unsigned int offs;
	unsigned int lim;
	int err;
	struct info_table *secondary;

	/* Do nothing if not second copy or not in User area of eMMC */
	if ((copy != 1) || (si->hwpart[1] != 0))
		return 0;

	/*
	 * Write Secondary Image Table for redundant SPL; the first_sector
	 * entry is counted relative to 0x8000 (0x40 blocks) and also the two
	 * skipped blocks for MBR and Secondary Image Table must be included,
	 * even if empty in the secondary case, which results in subtracting
	 * 0x42 blocks.
	 *
	 * The table itself is located one block before primary SPL.
	 */
	secondary = (struct info_table *)(fi->temp);
	secondary->tag = 0x00112233;
	secondary->first_sector = si->start[1] / blksz - 0x42;

	lim = si->start[0];
	offs = lim - blksz;

	printf("  Writing SECONDARY-SPL-INFO at offset 0x%08x size 0x200...",
	       offs);

	err = fi->ops->write(fi, offs, blksz, lim, 0, fi->temp);
	memset(fi->temp, fi->temp_fill, fi->temp_size);

	fs_image_show_sub_status(err);

	return err;
}

#endif

/* Save NBOOT and SPL region to MMC */
static int fs_image_save_nboot_mmc(struct flash_info *fi,
				   struct region_info *nboot_ri,
				   struct region_info *spl_ri)
{
	int failed;
	int copy, start_copy;

	/*
	 * When saving NBoot, start with "the other" copy first, i.e. if
	 * running form Primary SPL, update the Secondary copy first and if
	 * running from Secondary SPL, update the Primary copy first. The
	 * reason is that the current copy is apparently working, but the
	 * other copy may very well be broken. So it makes sense to first
	 * update the broken version and repair it by doing so, before
	 * touching the working version.
	 *
	 * For example if currently running on the Secondary copy, this means
	 * that the Primary copy is damaged. So if the Secondary copy was
	 * updated first and this failed for some reason, then both copies
	 * would be non-functional and the board would be bricked. But if the
	 * damaged Primary copy is updated first and this succeeds, the
	 * Primary copy is repaired and provides a working fallback when
	 * writing the Secondary copy afterwards.
	 *
	 * Start with the "other" copy:
	 *
	 *  1. Invalidate the "other" NBOOT region by overwriting the first
	 *     block. This immediately invalidates the F&S header of the
	 *     BOARD-CFG so that this copy will definitely not be loaded
	 *     anymore.
	 *  2. Write all of the "other" NBOOT but the first block. If
	 *     interrupted, the BOARD-CFG ist still invalid and will not be
	 *     loaded.
	 *  3. Write first block of the "other" NBOOT. This adds the F&S
	 *     header and makes NBOOT valid.
	 *  4. Invalidate the "other" SPL region. This immediately invalidates
	 *     SPL (IVT) so that this copy will definitely not be loaded
	 *     anymore.
	 *  5. Write all of the "other" SPL but the first block. If
	 *     interrupted, SPL is still invalid and will not be loaded.
	 *  6. Write the first block of the "other" SPL. This adds the IVT and
	 *     makes SPL valid.
	 *  7. On i.MX8MM: If booting from User partition and the "other" copy
	 *     is the Secondary copy, update the information block for the
	 *     secondary SPL.
	 *
	 * If interrupted somewhere in steps 1 to 7, the "current" copy is
	 * still available and will continue to boot. After step 6, the
	 * "other" copy is fully functional. So if the "other" copy is the
	 * Primary copy, it will be booted after Step 6 again.
	 *
	 *  8. Update the "current" NBOOT in the same sequence.
	 *  9. Update the "current" SPL in the same sequence.
	 * 10. On i.MX8MM: If booting from User partition and the "current" copy
	 *     is the Secondary copy: Update the information block for the
	 *     secondary SPL.
	 *
	 * The worst case happens if interrupted in step 8 and if "current" is
	 * the Primary copy. Then the "current" (=Primary) but still old SPL
	 * will boot, but fails to load the "current" (=Primary) NBOOT,
	 * because it is invalid right now. So it will fall back to load the
	 * "other" (=Secondary) NBOOT, which is the new version already. This
	 * may or may not work, depending on how compatible the old and new
	 * versions are.
	 *
	 * If interrupted in step 9, the "other" (=Secondary) copy is loaded,
	 * which is the new version already. This is OK.
	 *
	 * ### TODO:
	 * The sequence above assumes that SPL can detect correctly from which
	 * copy it was booting. Currently this is not true on i.MX8MN/MP/X.
	 */
	failed = 0;
	start_copy = fs_image_get_start_copy();
	copy = start_copy;
	do {
		printf("\nSaving copy %d to %s:\n", copy, fi->devname);
		if (fs_image_save_region(fi, copy, nboot_ri))
			failed |= BIT(copy);

		if (fs_image_save_region(fi, copy, spl_ri))
			failed |= BIT(copy);

#ifdef CONFIG_IMX8MM
		/* Write Secondary Image Table */
		if (fs_image_write_secondary_table(fi, copy, spl_ri->si))
			failed = BIT(copy);
#endif
		copy = 1 - copy;
	} while (copy != start_copy);

	return failed;
}

static void fs_image_get_flash_mmc(struct flash_info *fi)
{
	/* Determine hwpart (when command starts) and boot hwpart */
	fi->blk_desc = mmc_get_blk_desc(fi->mmc);
	fi->old_hwpart = fi->blk_desc->hwpart;
	fi->boot_hwpart = (fi->mmc->part_config >>3) & PART_ACCESS_MASK;
	if (fi->boot_hwpart > 2)
		fi->boot_hwpart = 0;

	/* Temporary buffer is for one block */
	fi->temp_size = fi->blk_desc->blksz;

	/* Set device name */
	sprintf(fi->devname, "mmc%d", fi->blk_desc->devnum);
}

static void fs_image_put_flash_mmc(struct flash_info *fi)
{
	if (blk_dselect_hwpart(fi->blk_desc, fi->old_hwpart)) {
		printf("Cannot switch back to original hwpart %d\n",
		       fi->old_hwpart);
	}
}

struct flash_ops flash_ops_mmc = {
	.check_for_uboot = fs_image_check_for_uboot_mmc,
	.check_for_nboot = fs_image_check_for_nboot_mmc,
	.get_nboot_info = fs_image_get_nboot_info_mmc,
	.read = fs_image_read_mmc,
	.load_image = fs_image_load_image_mmc,
	.load_extra = fs_image_load_extra_mmc,
	.invalidate = fs_image_invalidate_mmc,
	.write = fs_image_write_mmc,
	.prepare_region = fs_image_prepare_region_mmc,
	.save_nboot = fs_image_save_nboot_mmc,
	.get_flash = fs_image_get_flash_mmc,
	.put_flash = fs_image_put_flash_mmc,
};

#endif /* CONFIG_CMD_MMC */

/* ------------- Generic Flash Handling ------------------------------------ */

/* Get flash information for given boot device */
static int fs_image_get_flash_info(struct flash_info *fi, void *fdt)
{
	int err;

	memset(fi, 0, sizeof(struct flash_info));

	err = fs_image_get_boot_dev(fdt, &fi->boot_dev, &fi->boot_dev_name);
	if (err)
		return err;

	/* Prepare flash information from where to load */
	switch (fi->boot_dev) {
#ifdef CONFIG_NAND_MXS
	case NAND_BOOT:
		fi->mtd = get_nand_dev_by_index(0);
		if (!fi->mtd) {
			puts("NAND not found\n");
			return -ENODEV;
		}
		fi->ops = &flash_ops_nand;
		break;
#endif

#ifdef CONFIG_MMC
	case MMC1_BOOT:
	case MMC2_BOOT:
	case MMC3_BOOT:
		fi->mmc = find_mmc_device(fi->boot_dev - MMC1_BOOT);
		if (!fi->mmc) {
			printf("mmc%d not found\n", fi->boot_dev - MMC1_BOOT);
			return -ENODEV;
		}
		fi->ops = &flash_ops_mmc;
		break;
#endif

	default:
		printf("Cannot handle %s boot device\n", fi->boot_dev_name);
		return -ENODEV;
		break;
	}

	fi->ops->get_flash(fi);

	fi->temp = malloc(fi->temp_size);
	if (!fi->temp) {
		puts("Cannot allocate temp buffer\n");
		return -ENOMEM;
	}
	memset(fi->temp, fi->temp_fill, fi->temp_size);

	return 0;
}

/* Clean up flash info */
static void fs_image_put_flash_info(struct flash_info *fi)
{
	fi->ops->put_flash(fi);
	free(fi->temp);
}

/* Handle fsimage save if loaded image is a U-Boot image */
static int fsimage_save_uboot(ulong addr, bool force)
{
	void *fdt;
	struct sub_info sub;
	struct region_info ri;
	struct flash_info fi;
	struct nboot_info ni;
	int failed;
	int copy, start_copy;
	unsigned int flags;
	struct fs_header_v1_0 *fsh = (struct fs_header_v1_0 *)addr;

	fdt = fs_image_get_cfg_fdt();
	if (fs_image_get_flash_info(&fi, fdt)
	    || fs_image_get_nboot_info(&fi, fdt, &ni))
		return CMD_RET_FAILURE;

	fs_image_region_create(&ri, &ni.uboot, &sub);
	flags = SUB_SYNC;
	if (ni.flags & NI_UBOOT_WITH_FSH)
		flags |= SUB_HAS_FS_HEADER; /* Save with F&S header */
	if (!fs_image_region_add(&ri, fsh, "U-BOOT", fs_image_get_arch(),
				0, flags))
		return CMD_RET_FAILURE;

	if (fs_image_validate(fsh, sub.type, sub.descr, addr))
		return CMD_RET_FAILURE;

	/* Check if all prerequisites for U-Boot are valid */
	if (fi.ops->check_for_uboot(&ni.uboot, force))
		return CMD_RET_FAILURE;

	/* ### TODO: set copy depending on Set A or B (or redundant copy) */
	failed = 0;
	start_copy = fs_image_get_start_copy();
	copy = start_copy;
	do {
		printf("\nSaving copy %d to %s:\n", copy, fi.devname);
		if (fs_image_save_region(&fi, copy, &ri))
			failed |= BIT(copy);
		copy = 1 - copy;
	} while (copy != start_copy);

	fs_image_put_flash_info(&fi);

	return fs_image_show_save_status(failed, "U-Boot");
}

/* ------------- Command implementation ------------------------------------ */

/* Show the F&S architecture */
static int do_fsimage_arch(struct cmd_tbl *cmdtp, int flag, int argc,
			   char * const argv[])
{
	printf("%s\n", fs_image_get_arch());

	return CMD_RET_SUCCESS;
}

/* Show the current BOARD-ID */
static int do_fsimage_boardid(struct cmd_tbl *cmdtp, int flag, int argc,
			      char * const argv[])
{
	printf("%s\n", fs_image_get_board_id());

	return CMD_RET_SUCCESS;
}

#ifdef CONFIG_CMD_FDT
/* Print FDT content of current BOARD-CFG */
static int do_fsimage_boardcfg(struct cmd_tbl *cmdtp, int flag, int argc,
			       char * const argv[])
{
	unsigned long addr;
	int ret;
	void *fdt = fs_image_get_cfg_fdt();
	struct fs_header_v1_0 *cfg;

	addr = fs_image_get_loadaddr(argc, argv, true);
	if (IS_ERR_VALUE(addr))
		return CMD_RET_USAGE;

	if (addr) {
		ret = fs_image_find_board_cfg(addr, true, "show", &cfg, NULL);
		if (ret <= 0)
			return CMD_RET_FAILURE;
	} else
		cfg = fs_image_get_cfg_addr();

	fdt = fs_image_find_cfg_fdt(cfg);
	if (!fdt)
		return CMD_RET_FAILURE;

	printf("FDT part of BOARD-CFG located at 0x%lx\n", (ulong)fdt);

	return fdt_print(fdt, "/", NULL, 5);
}
#endif

/* List contents of an F&S image */
static int do_fsimage_list(struct cmd_tbl *cmdtp, int flag, int argc,
			   char * const argv[])
{
	unsigned long addr;
	struct fs_header_v1_0 *fsh;

	if (argc > 1)
		addr = parse_loadaddr(argv[1], NULL);
	else
		addr = get_loadaddr();

	fsh = (struct fs_header_v1_0 *)addr;
	if (!fs_image_is_fs_image(fsh)) {
		printf("No F&S image found at addr 0x%lx\n", addr);
		return CMD_RET_FAILURE;
	}
	printf("Content of F&S image at addr 0x%lx\n\n", addr);

	puts("offset   size     type (description)\n"
	     "------------------------------------------------------------"
	     "-------------------\n");

	fs_image_parse_image(addr, 0, 0, fs_image_get_size(fsh, false));

	return CMD_RET_SUCCESS;
}

/* Load NBOOT and SPL regions from the boot device (NAND or MMC) to DRAM,
   create minimal NBoot image that could be saved again */
static int do_fsimage_load(struct cmd_tbl *cmdtp, int flag, int argc,
			   char * const argv[])
{
	void *fdt;
	struct sub_info sub;
	unsigned long addr;
	bool force;
	bool load_uboot = false;
	struct fs_header_v1_0 *nboot_fsh, *board_info_fsh, *board_cfg_fsh;
	struct flash_info fi;
	struct nboot_info ni;

	early_support_index = 0;
	if ((argc > 1) && (argv[1][0] == '-')) {
		if (strcmp(argv[1], "-f"))
			return CMD_RET_USAGE;
		force = true;
		argv++;
		argc--;
	}

	if (argc > 1) {
		size_t len = strlen(argv[1]);

		if (!strncmp(argv[1], "uboot", len)) {
			load_uboot = true;
			argv++;
			argc--;
		} else if (!strncmp(argv[1], "nboot", len)) {
			/* Accept "nboot", too, but it is the default anyway */
			argv++;
			argc--;
		}
	}

	if (argc > 1)
		addr = parse_loadaddr(argv[1], NULL);
	else
		addr = get_loadaddr();

	/* Ask for confirmation if there is already an F&S image at addr */
	nboot_fsh = (struct fs_header_v1_0 *)addr;
	if (fs_image_is_fs_image(nboot_fsh)) {
		printf("Warning! This will overwrite F&S image at RAM address"
		       " 0x%lx\n", addr);
		if (force && !fs_image_confirm())
			return CMD_RET_FAILURE;
	}

	/* Invalidate any old image */
	memset(nboot_fsh->info.magic, 0, 4);

	fdt = fs_image_get_cfg_fdt();
	if (fs_image_get_flash_info(&fi, fdt)
	    || fs_image_get_nboot_info(&fi, fdt, &ni))
		return CMD_RET_FAILURE;

	if (load_uboot) {
		int err;

		err = fs_image_load_uboot(&fi, &ni, (void *)addr);
		if (err)
			return CMD_RET_FAILURE;

		printf("U-Boot successfully loaded to 0x%lx\n", addr);

		return CMD_RET_SUCCESS;
	}

	/* Load flash specific stuff (NAND: BCB, MMC: Secondary Image Table) */
	if (fi.ops->load_extra(&fi, &ni.spl, nboot_fsh + 1))
		return CMD_RET_FAILURE;

	/* Load SPL behind the NBOOT F&S header that is filled later */
	sub.type = "SPL";
	sub.descr = fs_image_get_arch();
	sub.img = nboot_fsh + 1;
	sub.offset = 0;
	sub.flags = SUB_IS_SPL;
	if (fs_image_load_image(&fi, &ni.spl, &sub))
		return CMD_RET_FAILURE;

	/* Load BOARD_CFG */
	board_info_fsh = sub.img;
	sub.type = "BOARD-CFG";
	sub.descr = NULL;
	board_cfg_fsh = board_info_fsh + 1;
	sub.img = board_cfg_fsh;
	sub.flags = SUB_HAS_FS_HEADER;
	if (fs_image_load_image(&fi, &ni.nboot, &sub))
		return CMD_RET_FAILURE;

	/* If set, remove BOARD-ID rev (in file_size_high) and update CRC32 */
	if (board_cfg_fsh->info.file_size_high) {
		board_cfg_fsh->info.file_size_high = 0;
		debug("  ");
		fs_image_update_header(board_cfg_fsh,
				       fs_image_get_size(board_cfg_fsh, false),
				       board_cfg_fsh->info.flags);
	}

	/* Create BOARD-INFO header */
	sub.descr = fs_image_get_arch();
	if (ni.flags & NI_SUPPORT_CRC32)
		sub.type = "BOARD-INFO";
	else
		sub.type = "BOARD-CONFIGS";
	fs_image_set_header(board_info_fsh, sub.type, sub.descr, sub.size, 0);

	/* Load FIRMWARE */
	sub.type = "FIRMWARE";
	sub.offset = ni.board_cfg_size ? ni.board_cfg_size : sub.size;
	if (fs_image_load_image(&fi, &ni.nboot, &sub))
		return CMD_RET_FAILURE;

	/* Fill overall NBOOT header */
	debug("  ");
	fs_image_set_header(nboot_fsh, "NBOOT", sub.descr,
			    sub.img - (void *)(nboot_fsh + 1),
			    FSH_FLAGS_CRC32 | FSH_FLAGS_SECURE);

	fs_image_put_flash_info(&fi);

	printf("NBoot successfully loaded to 0x%lx\n", addr);

	return CMD_RET_SUCCESS;
}

/* Save the F&S NBoot image to the boot device (NAND or MMC) */
static int do_fsimage_save(struct cmd_tbl *cmdtp, int flag, int argc,
			   char * const argv[])
{
	struct fs_header_v1_0 *cfg_fsh;
	struct fs_header_v1_0 *nboot_fsh;
	struct fs_header_v1_0 firmware_fsh, dram_info_fsh, dram_type_fsh;
	struct fs_header_v1_0 cfg_fsh_bak;
	unsigned int firmware_start, dram_info_start, dram_type_start;
	struct region_info nboot_ri, spl_ri;
	struct sub_info nboot_sub[8], spl_sub;
	const char *arch = fs_image_get_arch();
	const char *type;
	unsigned int flags;
	void *fdt;
	int board_cfg_offs;
	const char *dram_type;
	const char *dram_timing;
	struct nboot_info ni;
	struct flash_info fi;
	int ret;
	int failed;
	unsigned long addr;
	bool force = false;
	unsigned int woffset;

	early_support_index = 0;
	while ((argc > 1) && (argv[1][0] == '-')) {
		if (!strcmp(argv[1], "-e")) {
			if (argc <= 2) {
				puts("Missing argument for option -e\n");
				return CMD_RET_USAGE;
			}
			early_support_index = simple_strtoul(argv[2], NULL, 0);
			argv += 2;
			argc -= 2;
		} else if (!strcmp(argv[1], "-f")) {
			force = true;
			argv++;
			argc--;
		} else
			return CMD_RET_USAGE;
	}

	if (argc > 1)
		addr = parse_loadaddr(argv[1], NULL);
	else
		addr = get_loadaddr();

	/* If this is an U-Boot image, handle separately */
	if (fs_image_match((void *)addr, "U-BOOT", NULL))
		return fsimage_save_uboot(addr, force);

	/* Handle NBoot image */
	ret = fs_image_find_board_cfg(addr, force, "save",
				      &cfg_fsh, &nboot_fsh);
	if (ret <= 0)
		return CMD_RET_FAILURE;

	fdt = fs_image_find_cfg_fdt(cfg_fsh);

	board_cfg_offs = fs_image_get_board_cfg_offs(fdt);
	dram_type = fdt_getprop(fdt, board_cfg_offs, "dram-type", NULL);
	dram_timing = fdt_getprop(fdt, board_cfg_offs, "dram-timing", NULL);
	if (!dram_type || !dram_timing) {
		puts("Error: No dram-type and/or dram-timing in BOARD-CFG\n");
		return CMD_RET_FAILURE;
	}

	if (fs_image_get_flash_info(&fi, fdt)
	    || fs_image_get_nboot_info(&fi, fdt, &ni))
		return CMD_RET_FAILURE;

	ret = fs_image_check_boot_dev_fuses(fi.boot_dev, "save");
	if (ret < 0)
		return CMD_RET_FAILURE;
	if (ret > 0) {
		printf("Warning! Boot fuses not yet set, remember to burn"
		       " them for %s\n", fi.boot_dev_name);
	}

	/* Prepare subimages for NBOOT region */
	fs_image_region_create(&nboot_ri, &ni.nboot, nboot_sub);

	/* Sub 0: BOARD-CFG */
	flags = SUB_HAS_FS_HEADER;
	if (ni.board_cfg_size)
		flags |= SUB_SYNC;
	woffset = fs_image_region_add(&nboot_ri, cfg_fsh, "BOARD-CFG",
				      arch, 0, flags);
	if (!woffset)
		return CMD_RET_FAILURE;

	/* Sub 1: FIRMWARE header */
	if (ni.board_cfg_size)
		woffset = ni.board_cfg_size;
	woffset = fs_image_region_add_fsh(&nboot_ri, &firmware_fsh, "FIRMWARE",
					  arch, woffset);
	if (!woffset)
		return CMD_RET_FAILURE;
	firmware_start = woffset;

	/* Sub 2: DRAM-INFO/SETTINGS header */
	if (ni.flags & NI_SUPPORT_CRC32)
		type = "DRAM-INFO";
	else
		type = "DRAM-SETTINGS";
	woffset = fs_image_region_add_fsh(&nboot_ri, &dram_info_fsh, type,
					  arch, woffset);
	if (!woffset)
		return CMD_RET_FAILURE;
	dram_info_start = woffset;

	/* Sub 3: DRAM-TYPE header */
	woffset = fs_image_region_add_fsh(&nboot_ri, &dram_type_fsh,
					  "DRAM-TYPE", dram_type, woffset);
	if (!woffset)
		return CMD_RET_FAILURE;
	dram_type_start = woffset;


	/* Sub 4: DRAM-FW image */
	flags = SUB_HAS_FS_HEADER;
	woffset = fs_image_region_find_add(&nboot_ri, nboot_fsh, "DRAM-FW",
					   dram_type, woffset, flags);
	if (!woffset)
		return CMD_RET_FAILURE;

	/* Sub 5: DRAM-TIMING image */
	woffset = fs_image_region_find_add(&nboot_ri, nboot_fsh, "DRAM-TIMING",
					   dram_timing, woffset, flags);
	if (!woffset)
		return CMD_RET_FAILURE;

	/* Update size and CRC32 (header only) for DRAM-TYPE and DRAM-INFO */
	fs_image_update_header(&dram_type_fsh, woffset - dram_type_start,
			       FSH_FLAGS_SECURE);
	/* "DRAM-SETTINGS" is too long, cannot write CRC32 in this case */
	fs_image_update_header(&dram_info_fsh, woffset - dram_info_start,
			 (ni.flags & NI_SUPPORT_CRC32) ? FSH_FLAGS_SECURE : 0);

	/* Sub 6: ATF image */
	type = "ATF";
#ifdef CONFIG_IMX_OPTEE
	woffset = fs_image_region_find_add(&nboot_ri, nboot_fsh, type, arch,
					   woffset, flags);
	if (!woffset)
		return CMD_RET_FAILURE;

	/* Sub 7: TEE image */
	type = "TEE";
#endif
	/* Last image, set SUB_SYNC */
	woffset = fs_image_region_find_add(&nboot_ri, nboot_fsh, type, arch,
					   woffset, flags | SUB_SYNC);
	if (!woffset)
		return CMD_RET_FAILURE;

	/* Update size and CRC32 (header only) for FIRMWARE */
	fs_image_update_header(&firmware_fsh, woffset - firmware_start,
			       FSH_FLAGS_SECURE);

	/* Prepare SPL region: sub 0: SPL */
	fs_image_region_create(&spl_ri, &ni.spl, &spl_sub);
	woffset = fs_image_region_find_add(&spl_ri, nboot_fsh, "SPL",
					   arch, 0, SUB_IS_SPL | SUB_SYNC);
	if (!woffset)
		return CMD_RET_FAILURE;

	/* Issue warning if UBoot MTD partitions are missing/differ
	   ### TODO: only call if U-Boot is moved */
	fi.ops->check_for_uboot(&ni.uboot, true);

	if (fi.ops->check_for_nboot(&fi, &ni.spl, force))
		return CMD_RET_FAILURE;

	/* Temporarily set BOARD-ID board revision and update CRC32 */
	if (ni.flags & NI_SAVE_BOARD_ID) {
		cfg_fsh_bak = *cfg_fsh;
		fs_image_board_cfg_set_board_rev(cfg_fsh);
	}

	/* Found all sub-images, everything is prepared, go and save NBoot */
	failed = fi.ops->save_nboot(&fi, &nboot_ri, &spl_ri);

	fs_image_put_flash_info(&fi);

	ret = fs_image_show_save_status(failed, "NBoot");

	if (ret == CMD_RET_SUCCESS) {
		/* Success: Activate new BOARD-CFG by copying it to OCRAM */
		memcpy(fs_image_get_cfg_addr(), cfg_fsh,
		       fs_image_get_size(cfg_fsh, true));
		puts("New BOARD-CFG is now active\n");
	}

	/* Restore BOARD-CFG header to previous content */
	if (ni.flags & NI_SAVE_BOARD_ID)
		*cfg_fsh = cfg_fsh_bak;

	return ret;
}

/* Burn the fuses according to the NBoot in DRAM */
static int do_fsimage_fuse(struct cmd_tbl *cmdtp, int flag, int argc,
			   char * const argv[])
{
	struct fs_header_v1_0 *cfg_fsh;
	void *fdt;
	int offs;
	int ret;
	enum boot_device boot_dev;
	const char *boot_dev_name;
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

	addr = fs_image_get_loadaddr(argc, argv, false);
	if (IS_ERR_VALUE(addr))
		return CMD_RET_USAGE;

	if (addr) {
		ret = fs_image_find_board_cfg(addr, force, "fuse",
					      &cfg_fsh, NULL);
		if (ret <= 0)
			return CMD_RET_FAILURE;
	} else
		cfg_fsh = fs_image_get_cfg_addr();

	fdt = fs_image_find_cfg_fdt(cfg_fsh);
	if (fs_image_get_boot_dev(fdt, &boot_dev, &boot_dev_name)
	    || fs_image_check_boot_dev_fuses(boot_dev, "fuse"))
		return CMD_RET_FAILURE;

	/* No contradictions, do an in-depth check */
	offs = fs_image_get_board_cfg_offs(fdt);
	if (offs < 0) {
		puts("Cannot find BOARD-CFG\n");
		return CMD_RET_FAILURE;
	}

	fbws = fdt_getprop(fdt, offs, "fuse-bankword", &len);
	fmasks = fdt_getprop(fdt, offs, "fuse-mask", &len2);
	fvals = fdt_getprop(fdt, offs, "fuse-value", &len3);
	if (!fbws || !fmasks || !fvals || (len != len2) || (len2 != len3)
	    || !len || (len % sizeof(fdt32_t) != 0)) {
		printf("Invalid or missing fuse value settings for boot"
		       " device %s\n", boot_dev_name);
		return CMD_RET_FAILURE;
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
		       boot_dev_name);
		return CMD_RET_SUCCESS;
	}
	if (ret & 1) {
		printf("Error: New settings for boot device %s would need to"
		       " clear fuse bits which\nis impossible. Refusing to"
		       " save this configuration.\n", boot_dev_name);
		return CMD_RET_FAILURE;
	}
	if (!force) {
		puts("The fuses will be changed to the above settings. This is"
		     " a write once option\nand cannot be undone. ");
		if (!fs_image_confirm())
			return CMD_RET_FAILURE;
	}

	/* Now there is no way back... actually burn the fuses */
	for (i = 0; i < len; i++) {
		fuse_bw = fdt32_to_cpu(fbws[i]);
		fuse_val = fdt32_to_cpu(fvals[i]);
		fuse_mask = fdt32_to_cpu(fmasks[i]);
		fuse_read(fuse_bw >> 16, fuse_bw & 0xffff, &cur_val);
		cur_val &= fuse_mask;
		if (cur_val == fuse_val)
			continue;	/* Skip unchanged values */
		ret = fuse_prog(fuse_bw >> 16, fuse_bw & 0xffff, fuse_val);
		if (ret) {
			printf("Error: Fuse programming failed for bank 0x%x,"
			       " word 0x%x, value 0x%08x (%d)\n",
			       fuse_bw >> 16, fuse_bw & 0xffff, fuse_val, ret);
			return CMD_RET_FAILURE;
		}
	}

	printf("Fuses programmed for boot device %s\n", boot_dev_name);

	return CMD_RET_SUCCESS;
}

/* Subcommands for "fsimage" */
static struct cmd_tbl cmd_fsimage_sub[] = {
	U_BOOT_CMD_MKENT(arch, 0, 1, do_fsimage_arch, "", ""),
	U_BOOT_CMD_MKENT(board-id, 0, 1, do_fsimage_boardid, "", ""),
#ifdef CONFIG_CMD_FDT
	U_BOOT_CMD_MKENT(board-cfg, 1, 1, do_fsimage_boardcfg, "", ""),
#endif
	U_BOOT_CMD_MKENT(list, 1, 1, do_fsimage_list, "", ""),
	U_BOOT_CMD_MKENT(load, 2, 1, do_fsimage_load, "", ""),
	U_BOOT_CMD_MKENT(save, 2, 0, do_fsimage_save, "", ""),
	U_BOOT_CMD_MKENT(fuse, 2, 0, do_fsimage_fuse, "", ""),
};

static int do_fsimage(struct cmd_tbl *cmdtp, int flag, int argc,
		      char * const argv[])
{
	struct cmd_tbl *cp;
	void *found_cfg;
	void *expected_cfg;

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

	/*
	 * All fsimage commands will access the BOARD-CFG in OCRAM. Make sure
	 * it is still valid and not compromised in any way.
	 */
	if (!fs_image_is_ocram_cfg_valid()) {
		printf("Error: BOARD-CFG in OCRAM at 0x%lx damaged\n",
		       (ulong)fs_image_get_cfg_addr());
		return CMD_RET_FAILURE;
	}

	/*
	 * Set the current board_id name and the compare_id that is used in
	 * fs_image_find_board_cfg().
	 */
	fs_image_set_board_id_from_cfg();

	found_cfg = fs_image_get_cfg_addr();
	expected_cfg = fs_image_get_regular_cfg_addr();
	if (found_cfg != expected_cfg) {
		printf("\n"
		       "*** Warning!\n"
		       "*** BOARD-CFG found at 0x%lx, expected at 0x%lx\n"
		       "*** Installed NBoot and U-Boot are not compatible!\n"
		       "\n", (ulong)found_cfg, (ulong)expected_cfg);
	}

	return cp->cmd(cmdtp, flag, argc, argv);
}

U_BOOT_CMD(fsimage, 4, 1, do_fsimage,
	   "Handle F&S board configuration and F&S images, e.g. U-Boot, NBOOT",
	   "arch\n"
	   "    - Show F&S architecture\n"
	   "fsimage board-id\n"
	   "    - Show current BOARD-ID\n"
#ifdef CONFIG_CMD_FDT
	   "fsimage board-cfg [<addr> | stored]\n"
	   "    - List contents of current BOARD-CFG\n"
#endif
	   "fsimage list [<addr>]\n"
	   "    - List the content of the F&S image at <addr>\n"
	   "fsimage load [-f] [uboot | nboot] [<addr>]\n"
	   "    - Verify the current NBoot or U-Boot and load to <addr>\n"
	   "fsimage save [-f] [-e <n>] [<addr>]\n"
	   "    - Save the F&S image at the right place (NBoot, U-Boot)\n"
	   "fsimage fuse [-f] [<addr> | stored]\n"
	   "    - Program fuses according to the current BOARD-CFG.\n"
	   "      WARNING: This is a one time option and cannot be undone.\n"
	   "\n"
	   "If no addr is given, use loadaddr. Using -f forces the command to\n"
	   "continue without showing any confirmation queries. This is meant\n"
	   "for non-interactive installation procedures. Option -e supports\n"
	   "handling early NBoot versions. If the environment is not found\n"
	   "when updating from a pre 2023.08 NBoot version, try increasing\n"
	   "<n> until it works. Be careful when storing such an old NBoot,\n"
	   "you need to know the right <n> or you will lose the environment.\n"
);
