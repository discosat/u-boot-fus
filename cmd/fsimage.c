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
 * be failsafe.
 *
 * The location, where each region is stored in flash, is given by the
 * nboot-info which is part of the BOARD-CFG. For now, handling of these
 * regions is rather simple, but in the future, only the relevant parts of the
 * FIRMWARE will be saved, so the NBOOT region will then consist of more, but
 * smaller sub-images.
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
	u32 start[2];			/* *-start entries */
	unsigned int size;		/* *-size entry */
#ifdef CONFIG_CMD_MMC
	u8 hwpart[2];			/* hwpart (in case of eMMC) */
#endif
	const char *type;		/* Name of storage region */
};

/* Storage info from the nboot-info of a BOARD-CFG in binary form */
struct nboot_info {
	struct storage_info spl;
	struct storage_info nboot;
	struct storage_info uboot;
	unsigned int board_cfg_size;
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

struct flash_info {
	enum boot_device boot_dev;	/* Device to boot from */
	const char *boot_dev_name;	/* Boot device as string */
#ifdef CONFIG_NAND_MXS
	struct mtd_info *mtd;		/* Handle to NAND */
#endif
#ifdef CONFIG_CMD_MMC
	struct blk_desc *blk_desc;	/* Handle to MMC block device */
	struct mmc *mmc;		/* Handle to MMC device */
	u8 boot_hwpart;			/* HW partition we boot from (0..2) */
	u8 old_hwpart;			/* Previous partition before command */
#endif
	u8 *temp;			/* Buffer for one NAND page/MMC block */
	unsigned int temp_size;		/* Size of temp buffer */
	unsigned int base_offs;		/* Offset where temp will be written */
	unsigned int write_pos;		/* Write position within temp */
	unsigned int bb_extra_offs;	/* Extra offset due to bad blocks */
};

union local_buf {
	struct fs_header_v1_0 fsh;	/* Space for one F&S header */
	u8 ivt[HAB_HEADER];		/* Space for IVT + boot_data */
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

/* Get the board-cfg-size from nboot-info */
static unsigned int fs_image_get_board_cfg_size(void *fdt, int offs)
{
	return fdt_getprop_u32_default_node(fdt, offs, 0, "board-cfg-size", 0);
}

static void fs_image_build_nboot_info_name(char *name, const char *prefix,
				const char *suffix)
{
	char c;

	/* Copy prefix in lowercase */
	do {
		c = *prefix++;
		if (c) {
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

/* Return start and size from nboot-info for entries of given type */
static int fs_image_get_si(void *fdt, int offs, unsigned int align,
			   struct storage_info *si, const char *type)
{
	char name[20];
	int len;
	const fdt32_t *start;

	fs_image_build_nboot_info_name(name, type, "-start");
	start = fdt_getprop(fdt, offs, name, &len);
	if (!start || !len || (len % sizeof(fdt32_t))) {
		printf("Missing or invalid entry %s in BOARD-CFG\n", name);
		return -EINVAL;
	}
	len /= sizeof(fdt32_t);

	if (len > 2)
		printf("Ignoring extra data in %s in BOARD-CFG\n", name);

	si->start[0] = fdt32_to_cpu(start[0]);
	si->start[1] = (len > 1) ? fdt32_to_cpu(start[1]) : si->start[0];
	si->hwpart[0] = 1;
#if defined(CONFIG_IMX8) || defined(CONFIG_IMX8MN) || defined(CONFIG_IMX8MP)
	si->hwpart[1] = 2;
#else
	si->hwpart[1] = 1;
#endif

	if (align && ((si->start[0] % align) || (si->start[1] % align))) {
		printf("Wrong alignment for %s in BOARD-CFG\n", name);
		return -EINVAL;
	}

	fs_image_build_nboot_info_name(name, type, "-size");
	si->size = fdt_getprop_u32_default_node(fdt, offs, 0, name, 0);
	if (!si->size) {
		printf("Missing or invalid entry for %s in BOARD-CFG\n", name);
		return -EINVAL;
	}

	si->type = type;

	return 0;
}

/* Get nboot-start and nboot-size values from nboot-info */
static int fs_image_get_nboot_si(void *fdt, int offs, unsigned int align,
				 struct storage_info *si)
{
	return fs_image_get_si(fdt, offs, align, si, "NBOOT");
}

/* Get spl-start and spl-size values from nboot-info */
static int fs_image_get_spl_si(void *fdt, int offs, unsigned int align,
			       struct storage_info *si)
{
	return fs_image_get_si(fdt, offs, align, si, "SPL");
}

/* Get uboot-start and uboot-size values from nboot-info */
static int fs_image_get_uboot_si(void *fdt, int offs, unsigned int align,
				 struct storage_info *si)
{
	int err;

	err = fs_image_get_si(fdt, offs, align, si, "UBOOT");
	si->type = "U-BOOT";		/* Have same name for region and sub */

	return err;
}

/* Return the BOARD-ID; id must have room for MAX_DESCR_LEN characters */
static int fs_image_get_board_id(char *id)
{
	struct fs_header_v1_0 *fsh = fs_image_get_cfg_addr();

	if (!fsh)
		return -ENOENT;

	memcpy(id, fsh->param.descr, MAX_DESCR_LEN);

	return 0;
}

/* Compute checksum for FCB or DBBT block */
static u32 fs_image_bcb_checksum(u8 *data, size_t size)
{
	u32 checksum = 0;

	while (size--)
		checksum += *data++;

	return ~checksum;
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

/* Return pointer to the header of the given sub-image or NULL if not found */
static struct fs_header_v1_0 *fs_image_find(struct fs_header_v1_0 *fsh,
					    const char *type, const char *descr)
{
	unsigned int size;
	unsigned int remaining;

	remaining = fs_image_get_size(fsh++, false);
	while (remaining > 0) {
		if (!fs_image_is_fs_image(fsh))
			break;
		size = fs_image_get_size(fsh, true);
		if (fs_image_match(fsh, type, descr))
			return fsh;
		fsh = (struct fs_header_v1_0 *)((void *)fsh + size);
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
static unsigned int fs_image_region_add_raw(
	struct region_info *ri, void *img, const char *type, const char *descr,
	unsigned int woffset, unsigned int flags, unsigned int size)
{
	struct sub_info *sub;

	if (woffset + size > ri->si->size) {
		printf("%s does not fit into target slot\n", type);
		return 0;
	}

	sub = &ri->sub[ri->count++];
	sub->type = type;
	sub->descr = descr;
	sub->img = img;
	sub->size = size;
	sub->offset = woffset;
	sub->flags = flags;

	debug("- %s(%s): 0x%08lx -> woffset 0x%08x size 0x%x\n", type, descr,
	      (unsigned long)img, woffset, size);

	return woffset + size;
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

	return fs_image_region_add_raw(ri, fsh, type, descr, woffset, flags,
				       size);
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
		puts("OK\n");
		break;
	case 1:
		puts("BAD BLOCKS\n");
		break;
	default:
		printf("FAILED (%d)\n", err);
		break;
	}
}

/* Show status after saving an image and return CMD_RET code */
static int fs_image_show_save_status(int err, const char *type)
{
	if (!err) {
		printf("Saving %s complete\n", type);
		return CMD_RET_SUCCESS;
	}

	printf("Saving %s incomplete or failed!\n\n"
	       "*** ATTENTION!\n"
	       "*** Do not switch off or restart the board before you have\n"
	       "*** installed a working %s version. Otherwise the board will\n"
	       "*** most probably fail to boot.\n", type, type);

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

/* Validate a signed image; Return 0: OK, <0: Error */
static int fs_image_validate_signed(struct ivt *ivt)
{
#ifdef CONFIG_FS_SECURE_BOOT
	struct boot_data *boot;
	void *self;
	u32 length;

	/* Check that boot data entry points directly behind IVT */
	if (ivt->boot != ivt->self + IVT_TOTAL_LENGTH) {
		puts("\nError: Invalid image, bad IVT, refusing to save\n");
		return -EINVAL;
	}

	boot = (struct boot_data *)(ivt + 1);
	self = (void *)ivt->self;
	length = boot_data->length;

	/* Copy to verification address and check signature */
	memcpy(self, ivt, length);
	if (imx_hab_authenticate_image(self, length, 0)) {
		puts("\nError: Invalid signature, refusing to save\n");
		return -EINVAL;
	}

	puts(" (signature OK)\n");

	return 0;
#else
	printf("\nError: Cannot authenticate, refusing to save\n"
	       "You need U-Boot with CONFIG_FS_SECURE_BOOT enabled for this\n");

	return -EINVAL;
#endif
}

/* Validate an image, either check signature or CRC32; 0: OK, <0: Error */
static int fs_image_validate(struct fs_header_v1_0 *fsh, const char *type,
			     const char *descr, ulong addr)
{
	u32 expected_cs;
	u32 computed_cs;
	u32 *pcs;
	struct ivt *ivt;
	unsigned int size;
	unsigned char *start;

	if (!fs_image_match(fsh, type, descr)) {
		printf("Error: No %s image for %s found at address 0x%lx\n",
		       type, descr, addr);
		return -EINVAL;
	}

	ivt = (struct ivt *)(fsh + 1);

	if (ivt->hdr.magic == IVT_HEADER_MAGIC) {
		printf("Found signed %s image", type);

		return fs_image_validate_signed(ivt);
	}

	printf("Found unsigned %s image", type);

#ifdef CONFIG_FS_SECURE_BOOT
	if (imx_hab_is_enabled) {
		puts("\nError: Board is closed, refusing to save unsigned"
		     " image\n");
		return -EINVAL;
	}
#endif

	if (!(fsh->info.flags & (FSH_FLAGS_SECURE | FSH_FLAGS_CRC32))) {
		/* No CRC32 provided, assume image is OK */
		puts(" (no checksum)\n");
		return 0;
	}

	if (fsh->info.flags & FSH_FLAGS_SECURE) {
		start = (unsigned char *)fsh;
		size = FSH_SIZE;
	} else {
		start = (unsigned char *)(fsh + 1);
		size = 0;
	}

	if (fsh->info.flags & FSH_FLAGS_CRC32)
		size += fs_image_get_size(fsh, false);

	/* CRC32 is in type[12..15]; temporarily set to 0 while computing */
	pcs = (u32 *)&fsh->type[12];
	expected_cs = *pcs;
	*pcs = 0;
	computed_cs = crc32(0, start, size);
	*pcs = expected_cs;
	if (computed_cs == expected_cs) {
		puts(" (checksum ok)\n");
		return 0;
	}

	printf("\nError: Bad CRC32 (found 0x%08x, expected 0x%08x)\n",
	       computed_cs, expected_cs);

	return -EINVAL;
}

/* Get image length from F&S header or IVT (e.g. in case of SPL) */
static int fs_image_get_size_from_fsh_or_ivt(struct sub_info *sub, size_t *size)
{
	if (sub->flags & SUB_HAS_FS_HEADER) {
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
	char old_id[MAX_DESCR_LEN + 1];
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
#ifdef CONFIG_FS_SECURE_BOOT
			if (imx_hab_is_enabled()) {
				puts("Error: Current board is %s and board is"
				     " closed\nRefusing to %s for %s\n",
				     old_id, action, id);
				return -EINVAL;
			}
#endif
			printf("Warning! Current board is %s but you will\n"
			       "%s for %s\n", old_id, action, id);
			if (!force && !fs_image_confirm()) {
				return 0; /* used_cfg == NULL in this case */
			}
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

	if (fs_image_is_fs_image((struct fs_header_v1_0 *)addr))
		return addr;

	printf("No F&S image found at 0x%lx", addr);

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


/* ------------- NAND handling --------------------------------------------- */

#ifdef CONFIG_CMD_NAND

#ifdef CONFIG_FS_UPDATE_SUPPORT
	const char *uboot_mtd_names[] = {"UBoot_A", "UBoot_B"};
#elif defined CONFIG_SYS_NAND_U_BOOT_OFFS_REDUND
	const char *uboot_mtd_names[] = {"UBoot", "UBootRed"};
#else
	const char *uboot_mtd_names[] = {"UBoot"};
#endif

/* Parse nboot-info for NAND settings and fill struct */
static int fs_image_get_nboot_info_nand(struct flash_info *fi, void *fdt,
					int offs, struct nboot_info *ni)
{
	int err;

	ni->board_cfg_size = fs_image_get_board_cfg_size(fdt, offs);

	err = fs_image_get_spl_si(fdt, offs, 0, &ni->spl);
	if (err)
		return err;

	err = fs_image_get_nboot_si(fdt, offs, 0, &ni->nboot);
	if (err)
		return err;

	err = fs_image_get_uboot_si(fdt, offs, 0, &ni->uboot);
	if (err)
		return err;

	return 0;
}

/* Issue warning if U-Boot MTD partitions do not match nboot-info */
static bool fs_image_check_uboot_mtd(struct storage_info *si, bool force)
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

/* Load the image of given type/descr from NAND flash at given offset */
static int fs_image_load_image_from_nand(struct flash_info *fi, int copy,
					 const struct storage_info *si,
					 struct sub_info *sub)
{
	int err;
	size_t size;
	loff_t offs = si->start[copy] + sub->offset;
	loff_t lim = si->size;
	u32 checksum;

#ifdef CONFIG_IMX8MM
	/* On i.MX8MM, SPL starts at offset 0x400 in the middle of the page */
	if (sub->flags & SUB_IS_SPL)
		offs += 0x400;
#endif

	printf("  Loading copy %d from NAND offset 0x%08llx... ", copy, offs);

	if (sub->flags & SUB_IS_FCB) {
		mxs_nand_mode_fcb_62bit(fi->mtd);
		size = fi->mtd->writesize; /* Different writesize here! */
	} else if ((sub->flags & SUB_IS_DBBT)
		   || (sub->flags & SUB_IS_DBBT_DATA)) {
		size = fi->mtd->writesize;
	} else {
		/* Load IVT (SPL) or FS_HEADER and get size from it */
		size = (sub->flags & SUB_HAS_FS_HEADER) ? FSH_SIZE : HAB_HEADER;

		err = nand_read_skip_bad(fi->mtd, offs, &size, NULL, lim,
					 sub->img);
		if (err)
			return err;

		err = fs_image_get_size_from_fsh_or_ivt(sub, &size);
		if (err)
			return err;
	}

	/* Load image itself */
	err = nand_read_skip_bad(fi->mtd, offs, &size, NULL, lim, sub->img);
	if (sub->flags & SUB_IS_FCB)
		mxs_nand_mode_normal(fi->mtd);
	if (err)
		return err;

	/* Check NAND specific images if corrupted */
	if (sub->flags & SUB_IS_FCB) {
		struct fcb_block *fcb = sub->img;

		if ((fcb->fingerprint != FCB_FINGERPRINT)
		    || (fcb->version != FCB_VERSION_1))
			return -ENOENT;

		checksum = fs_image_bcb_checksum(sub->img + 4,
						 sizeof(struct fcb_block) - 4);
		if (fcb->checksum != checksum)
			return -EILSEQ;
	} else if (sub->flags & SUB_IS_DBBT) {
		struct dbbt_block *dbbt = sub->img;

		if ((dbbt->fingerprint != DBBT_FINGERPRINT)
		    || (dbbt->version != DBBT_VERSION_1))
			return -ENOENT;

		if (dbbt->checksum != 0) {
			checksum = fs_image_bcb_checksum(
				sub->img + 4, sizeof(struct dbbt_block) - 4);
			if (dbbt->checksum != checksum)
				return -EILSEQ;
		}
	} else if (sub->flags & SUB_IS_DBBT_DATA) {
		u32 *dbbt_data = sub->img;
		u32 count = dbbt_data[1];

		/* Detection is vague, but enough to not accept empty pages */
		if (count > 32)
			return -ENOENT;
		if (dbbt_data[0] != 0) {
			checksum = fs_image_bcb_checksum(
				sub->img + 4, (count + 1) * sizeof(u32));
			if (dbbt_data[0] != checksum)
				return -EILSEQ;
		}
	}

	sub->size = size;

	return 0;
}

static int fs_image_load_image(struct flash_info *fi,
			       const struct storage_info *si,
			       struct sub_info *sub);


/* Temporarily load Boot Control Block (BCB) with FCB and DBBT */
static int fs_image_load_bcb(struct flash_info *fi, struct storage_info *spl,
			     void *tempaddr)
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

	sub.type = "DBBT";
	sub.offset = fcb->dbbt_start % pages_per_block * mtd->writesize;
	sub.flags = SUB_IS_DBBT;
	err = fs_image_load_image(fi, &bcb, &sub);
	if (err) {
		printf("  Warning! No Discovered Bad Block Table (DBBT)!\n");
		return err;
	}

	dbbt = tempaddr + FSH_SIZE;
	if (!dbbt->dbbtpages) {
		printf("  No Bad Blocks in DBBT recorded\n");
		return 0;
	}

	sub.type = "DTTB-DATA";
	sub.offset += 4 * mtd->writesize;
	sub.flags = SUB_IS_DBBT_DATA;
	err = fs_image_load_image(fi, &bcb, &sub);
	if (err) {
		printf("  Warning! No DBBT data found!\n");
		return err;
	}

	dbbt_data = tempaddr + FSH_SIZE;
	printf("  %d bad block(s) in DBBT recorded\n", dbbt_data[1]);

	return 0;
}

/* Erase given region */
static int fs_image_erase_nand(struct flash_info *fi, int copy,
			       const struct storage_info *si)
{
	loff_t offs = si->start[copy];
	size_t size = si->size;
	struct nand_erase_options opts = {0};
	int err;

	printf("  Erasing %s region (size 0x%zx) at offset 0x%llx... ",
	       si->type, size, offs);

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
static int fs_image_write_to_nand(struct flash_info *fi, unsigned int offs,
				  unsigned int size, unsigned int lim,
				  unsigned int flags, u8 *buf)
{
	int err;
	size_t wsize;
	size_t actual;
	loff_t woffs;
	loff_t maxsize;
	size_t bb_extra;

	wsize = size;

	/* FCB, DBBT and DBBT_DATA ignore any bad block offsets */
	if (flags & (SUB_IS_FCB | SUB_IS_DBBT | SUB_IS_DBBT_DATA)) {
		unsigned int block_offs = offs & ~(fi->mtd->erasesize - 1);

		if (nand_block_isbad(fi->mtd, block_offs)) {
			printf("Ignore bad block 0x%x\n", block_offs);
			return -EBADMSG;;
		}
	}

	woffs = offs + fi->bb_extra_offs;
	maxsize = lim - woffs;

	debug("  -> nand_write size 0x%x to offs 0x%llx maxsize 0x%llx\n",
	      size, woffs, maxsize);
	err = nand_write_skip_bad(fi->mtd, woffs, &wsize, &actual,
				  maxsize, buf, WITH_WR_VERIFY);
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

static int fs_image_flush_temp_nand(struct flash_info *fi, unsigned int lim,
				    unsigned int flags)
{
	int err;

	if (!fi->write_pos)
		return 0;

	/* Always write mtd->writesize bytes, this differs in FCB mode */
	if (flags & SUB_IS_FCB) {
		debug("  - Switch to 62bit ECC\n");
		mxs_nand_mode_fcb_62bit(fi->mtd);
	}
	debug("  - Flush temp size 0x%x pos 0x%x to offs 0x%x\n",
	      fi->mtd->writesize, fi->write_pos, fi->base_offs);
	err = fs_image_write_to_nand(fi, fi->base_offs, fi->mtd->writesize,
				     lim, flags, fi->temp);
	if (flags & SUB_IS_FCB) {
		debug("  - Switch back to normal ECC\n");
		mxs_nand_mode_normal(fi->mtd);
	}
	fi->write_pos = 0;
	memset(fi->temp, 0xff, fi->temp_size);

	return err;
}

/* Write one sub-image to nand */
static int fs_image_save_sub_to_nand(struct flash_info *fi, unsigned int offs,
				     unsigned int size, unsigned int lim,
				     u8 *buf, unsigned int flags)
{
	int err;
	unsigned int write_pos;
	unsigned int base_offs;
	unsigned int chunk_size;
	unsigned int chunk_mask = fi->temp_size - 1;
	unsigned int remaining = size;

	debug("\n");
	/*
	 * Step 1: If the temp buffer was used and we are not continuinig in
	 * the same page/block, write back the temp buffer first.
	 */
	base_offs = offs & ~chunk_mask;
	if (fi->write_pos && (base_offs != fi->base_offs)) {
		err = fs_image_flush_temp_nand(fi, lim, 0);
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
			debug("  - Copy start from 0x%lx size 0x%x to temp"
			      " pos 0x%x for offs 0x%x\n", (ulong)buf,
			      chunk_size, write_pos, base_offs);
			memcpy(fi->temp + write_pos, buf, chunk_size);
			err = fs_image_flush_temp_nand(fi, lim, flags);
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

		err = fs_image_write_to_nand(fi, offs, chunk_size, lim,
					     flags, buf);
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
		debug("  - Copy end from 0x%lx size 0x%x to temp pos 0x%x for"
		      " offs 0x%x\n", (ulong)buf, remaining, fi->write_pos,
		      fi->base_offs);
		memcpy(fi->temp + fi->write_pos, buf, remaining);
		fi->write_pos += remaining;

		if (flags & SUB_SYNC) {
			debug("  - SYNC\n");
			err = fs_image_flush_temp_nand(fi, lim, flags);
			if (flags & SUB_IS_FCB)
				mxs_nand_mode_normal(fi->mtd);
			if (err)
				return err;
		}
	}

	return 0;
}

/* Save the given region to NAND */
static int fs_image_save_region_to_nand(struct flash_info *fi, int copy,
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

	printf("  -- %s --\n", si->type);

repeat:
	err = fs_image_erase_nand(fi, copy, si);
	if (err)
		return err;

	/*
	 * Write region in two passes:
	 *
	 * Pass 1:
	 * Write everything of the image but the first page with the header.
	 * If this is interrupted (e.g. due to a power loss), then the image
	 * will not be seen as valid when loading because of the missing
	 * header. So there is no problem with half-written files.
	 *
	 * Pass 2:
	 * Write only the first page with the header; if this succeeds, then
	 * we know that the whole image is completely written. If this is
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
				/* Behind first page, i.e. done? */
				if (offset >= temp_size)
					break;

				/* Write only first part of image? */
				if (offset + size > temp_size) {
					size = temp_size - offset;
					action = "Completing";
				}
			}
			offset += si->start[copy];

			printf("  %s %s (size 0x%x) at offset 0x%x... ",
			       action, s->type, size, offset);

			err = fs_image_save_sub_to_nand(fi, offset, size,
							lim, buf, s->flags);
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

/* Save U-BOOT region to NAND */
static int fs_image_save_uboot_to_nand(struct flash_info *fi,
				       struct region_info *ri)
{
	int err;
	int copy;

	/* ### TODO: set copy depending on Set A or B (or redundant copy) */
	for (copy = 0; copy < 2; copy++) {
		printf("Saving copy %d to NAND:\n", copy);
		err = fs_image_save_region_to_nand(fi, copy, ri);
		if (err)
			return err;
	}

	return 0;
}

/* Save NBOOT and SPL region to NAND */
#define DBBT_DATA_BLOCKS 32
#define DBBT_DATA_ENTRIES (DBBT_DATA_BLOCKS - 8)
static int fs_image_save_nboot_to_nand(struct flash_info *fi,
				       struct region_info *nboot_ri,
				       struct region_info *spl_ri)
{
	int err;
	int copy;
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
	dbbt_data[0] = 0;

	/* Fill DBBT (Discovered Bad Block Table) */
	memset(&dbbt, 0, sizeof(struct dbbt_block));
	dbbt.fingerprint = DBBT_FINGERPRINT;
	dbbt.version = DBBT_VERSION_1;
	dbbt.checksum = 0;
	dbbt.dbbtpages = (bad_blocks > 0);

#ifdef CONFIG_IMX8MM
	/* On i.MX8MM, SPL starts on offset 0x400 in the middle of the page */
	spl_ri->sub[0].offset += 0x400;
#endif

	fs_image_region_create(&bcb_ri, &bcb_si, bcb_sub);
	if (!fs_image_region_add_raw(&bcb_ri, &fcb, "FCB", arch, 0,
				     SUB_IS_FCB | SUB_SYNC, sizeof(fcb)))
		return -ENOMEM;
	if (!fs_image_region_add_raw(&bcb_ri, &dbbt, "DBBT", arch,
				     4 * fi->mtd->writesize,
				     SUB_IS_DBBT | SUB_SYNC, sizeof(dbbt)))
		return -ENOMEM;
	if ((bad_blocks > 0)
	    && !fs_image_region_add_raw(&bcb_ri, dbbt_data, "DBBT-DATA", arch,
					8 * fi->mtd->writesize,
					SUB_IS_DBBT_DATA | SUB_SYNC,
					bad_blocks * 4 + 8))
		return -ENOMEM;

	/*
	 * Save sequence:
	 *  1. Invalidate NBOOT region. by erasing the region. This immediately
	 *     invalidates the F&S header of the BOARD-CFG so that this copy
	 *     will not be loaded anymore. If interrupted here, any copy of
	 *     (old) SPL will use the second (old) copy of NBOOT that matches
	 *     the old SPL.
	 *  2. Write all of NBOOT but the first page. If interrupted, the
	 *     BOARD-CFG ist still invalid and the same happens as in 1.
	 *  3. Write first page of NBOOT. This adds the F&S header and makes
	 *     NBOOT valid.
	 *  4. Erase SPL region. This immediately invalidates SPL (IVT).
	 *     If interrupted here, the second (old) copy of SPL will use the
	 *     second (old) copy of the BOARD-CFG that matches the old SPL.
	 *     This assumes that the second copy is tried first if booting from
	 *     the secondary SPL image.
	 *  5. Write all of SPL but the first page. If interrupted here, SPL is
	 *     still invalid and the same happens as in 4.
	 *  6. Write the first page of SPL. This adds the IVT and makes SPL
	 *     valid. However BCB may still be pointing somewhere else.
	 *  7. Erase BCB region. If interrupted here, the same happens as in 4.
	 *  8. Write DBBT and DBBT-DATA. If interrupted here, the same happens
	 *     as in 4.
	 *  9. Write FCB. This validates the BCB. If interrupted after that,
	 *     the first (new) copy of SPL will use the first (new) copy of
	 *     NBOOT that matches the new SPL.
	 * 10. Update the second copies in the same sequence.
	 *
	 * Remark:
	 * - There is a small risk that an old SPL will load a new NBOOT copy,
	 *   e.g. if interrupted between 3. and 4. However this is only a real
	 *   problem if new NBOOT and old SPL differ significantly. Typically
	 *   an old SPL can also handle new NBOOT and vice versa.
	 *
	 * ### TODO:
	 * - Step 4 assumes that the second copy is tried first if booting
	 *   from the secondary SPL image, but currently this is not true on
	 *   i.MX8MN/MP/X because we cannot detect this on these architectures.
	 */
	for (copy = 0; copy < 2; copy++) {
		printf("Saving copy %d to NAND:\n", copy);
		err = fs_image_save_region_to_nand(fi, copy, nboot_ri);
		if (err)
			return err;

		err = fs_image_save_region_to_nand(fi, copy, spl_ri);
		if (err)
			return err;

		err = fs_image_save_region_to_nand(fi, copy, &bcb_ri);
		if (err)
			return err;
	}

	return 0;
}

#endif /* CONFIG_CMD_NAND */


/* ------------- MMC handling ---------------------------------------------- */

#ifdef CONFIG_CMD_MMC

static int fs_image_set_hwpart(struct flash_info *fi, int copy,
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
	u8 first = fi->boot_hwpart;
	u8 second = first ? (3 - first) : first;

	ni->board_cfg_size = fs_image_get_board_cfg_size(fdt, offs);
#ifdef CONFIG_IMX8MM
	/* On fsimx8mm, everything is in same boot hwpart */
	second = first;
#endif

	/* Get SPL storage info */
	err = fs_image_get_spl_si(fdt, offs, 0, &ni->spl);
	if (err)
		return err;
	ni->spl.hwpart[0] = first;
	ni->spl.hwpart[1] = second;

	/* Get NBoot storage info */
	err = fs_image_get_nboot_si(fdt, offs, 0, &ni->nboot);
	if (err)
		return err;
	ni->nboot.hwpart[0] = first;
	ni->nboot.hwpart[1] = second;

	/* Get U-Boot storage info */
	err = fs_image_get_uboot_si(fdt, offs, 0, &ni->uboot);
	if (err)
		return err;
	ni->uboot.hwpart[0] = 0;
	ni->uboot.hwpart[1] = 0;

#ifndef CONFIG_IMX8MM
	/* SPL always starts on sector 0 in boot1/2 hwpart */
	ni->spl.start[0] = 0;
	ni->spl.start[1] = 0;

	/* Both NBoot copies start on same sector in boot1/2 */
	ni->nboot.start[1] = ni->nboot.start[0];
#endif

	return 0;
}

/* Load the image of given type/descr from eMMC at given offset */
static int fs_image_load_image_from_mmc(struct flash_info *fi, int copy,
					const struct storage_info *si,
					struct sub_info *sub)
{
	size_t size;
	unsigned long n;
	unsigned int offs = si->start[copy] + sub->offset;
	lbaint_t blk = offs / fi->blk_desc->blksz;
	unsigned int hwpart = si->hwpart[copy];
	lbaint_t count;
	int err;

	err = fs_image_set_hwpart(fi, copy, si);
	if (err)
		return err;

	printf("  Loading copy %d from mmc%d hwpart %d offset 0x%x (block 0x"
	       LBAF ")... ", copy, fi->blk_desc->devnum, hwpart, offs, blk);

	/* Read F&S header or IVT */
	n = blk_dread(fi->blk_desc, blk, 1, sub->img);
	if (IS_ERR_VALUE(n))
		return (int)n;
	if (n < 1)
		return -EIO;

	/* Get image size */
	err = fs_image_get_size_from_fsh_or_ivt(sub, &size);
	if (err)
		return err;

	/* Load whole image incl. header */
	count = (size + fi->blk_desc->blksz - 1) / fi->blk_desc->blksz;
	n = blk_dread(fi->blk_desc, blk, count, sub->img);
	if (IS_ERR_VALUE(n))
		return (int)n;
	if (n < count)
		return -EIO;

	sub->size = size;

	return 0;
}

#ifdef CONFIG_IMX8MM
/* Write Secondary Image table for redundant SPL */
static int fs_image_write_secondary_table(struct flash_info *fi,
					  struct storage_info *si)
{
	unsigned int blksz = fi->blk_desc->blksz;
	lbaint_t blk;
	unsigned long n;
	unsigned int offset;
	int err;

	/*
	 * Write Secondary Image Table for redundant SPL; the first_sector
	 * entry is counted relative to 0x8000 (0x40 blocks) and also the two
	 * skipped blocks for MBR and Secondary Image Table must be included,
	 * even if empty, in the secondary case, which results in subtracting
	 * 0x42 blocks.
	 *
	 * The table itself is located one block before primary SPL.
	 */
	memset(&local_buffer.block, 0, blksz);
	local_buffer.secondary.tag = 0x00112233;
	local_buffer.secondary.first_sector = si->start[1] / blksz - 0x42;

	offset = si->start[0] - blksz;
	blk = offset / blksz;

	printf("Writing SECONDARY-SPL-INFO (size 0x200) at offset 0x%x (block"
	       " 0x" LBAF ")... ",  offset, blk);

	n = blk_dwrite(fi->blk_desc, blk, 1, &local_buffer.block);
	if (n < 1)
		err = -EIO;
	else if (IS_ERR_VALUE(n))
		err = (int)n;
	else
		err = 0;

	fs_image_show_sub_status(err);

	return err;
}

#else

/* If booting from User space of MMC, check the fused secondary image offset */
static int fs_image_check_secondary_offset(struct flash_info *fi,
					   const struct storage_info *si,
					   bool force)
{
	u32 offset_fuses = fs_board_get_secondary_offset();
	u32 offset_nboot = si->start[1];

	if (!fi->boot_hwpart && (offset_fuses != offset_nboot)) {
		printf("Secondary Image Offset in fuses is 0x%08x but new NBOOT"
		       " wants 0x%08x.\nSecond copy (backup) will not boot"
		       " in case of errors!\n", offset_fuses, offset_nboot);
		if (!force && !fs_image_confirm())
			return -EINVAL;
	}

	return 0;
}
#endif

/* Invalidate an image by overwriting the first block with zeroes */
static int fs_image_invalidate_mmc(struct flash_info *fi, int copy,
				   const struct storage_info *si)
{
	unsigned int offs = si->start[copy];
	lbaint_t blk = offs / fi->blk_desc->blksz;
	unsigned long n;
	int err = 0;

	err = fs_image_set_hwpart(fi, copy, si);
	if (err)
		return err;

	printf("  Invalidating %s (size 0x%x) at offset 0x%x (block 0x" LBAF
	       ")... ", si->type, si->size, offs, blk);

	memset(&local_buffer.block, 0, fi->blk_desc->blksz);
	n = blk_dwrite(fi->blk_desc, blk, 1, &local_buffer.block);
	if (n < 1)
		err = -EIO;
	else if (IS_ERR_VALUE(n))
		err = (int)n;
	else
		err = 0;

	fs_image_show_sub_status(err);

	return err;
}

/* Save the given region to MMC */
static int fs_image_save_region_to_mmc(struct flash_info *fi, int copy,
				       struct region_info *ri)
{
	unsigned long blksz = fi->blk_desc->blksz;
	unsigned int offset;
	unsigned int size;
	struct sub_info *s;
	struct storage_info *si = ri->si;
	lbaint_t blk;
	lbaint_t blk_count;
	void *buf;
	unsigned long n;
	int err;
	bool pass2, complete;

	err = fs_image_set_hwpart(fi, copy, si);
	if (err)
		return err;

	printf("  -- %s (hwpart %d) --\n", si->type, si->hwpart[copy]);

	err = fs_image_invalidate_mmc(fi, copy, si);
	if (err)
		return err;

	/*
	 * Write region in two passes:
	 *
	 * Pass 1:
	 * Write everything of the image but the first block with the header.
	 * If this is interrupted (e.g. due to a power loss), then the image
	 * will not be seen as valid when loading because of the missing
	 * header. So there is no problem with half-written files.
	 *
	 * Pass 2:
	 * Write only the first block with the header; if this succeeds,
	 * then we know that the whole image is completely written.
	 */
	pass2 = false;
	do {
		complete = 0;
		for (s = ri->sub; s < ri->sub + ri->count; s++) {
			offset = s->offset;
			size = s->size;
			buf = s->img;
			if (!pass2) {
				/* Skip image completely? */
				if (offset + size < blksz)
					continue;

				/* Skip only first part of image? */
				if (offset < blksz) {
					size -= blksz - offset;
					buf += blksz - offset;
					offset = blksz;
				}
			} else {
				/* Behind first block, i.e. done? */
				if (offset >= blksz)
					break;

				/* Write only first part of image? */
				if (offset + size > blksz) {
					size = blksz - offset;
					complete = true;
				}
			}

			offset += si->start[copy];
			blk = offset / blksz;
			blk_count = (size + blksz - 1) / blksz;

			printf("  %sting %s (size 0x%x) at offset 0x%x (block"
			       " 0x" LBAF ")... ", complete ? "Comple" : "Wri",
			       s->type, size, offset, blk);

			n = blk_dwrite(fi->blk_desc, blk, blk_count, buf);
			if (n < blk_count)
				err = -EIO;
			else if (IS_ERR_VALUE(n))
				err = (int)n;
			else
				err = 0;
			fs_image_show_sub_status(err);
			if (err)
				return err;
		}
		pass2 = !pass2;
	} while (pass2);

	return 0;
}

/* Save U-BOOT region to MMC */
static int fs_image_save_uboot_to_mmc(struct flash_info *fi,
				      struct region_info *ri)
{
	int err;
	int copy;

	/* ### TODO: set copy depending on Set A or B (or redundant copy) */
	for (copy = 0; copy < 2; copy++) {
		printf("Saving copy %d to mmc%d:\n", copy,
		       fi->blk_desc->devnum);
		err = fs_image_save_region_to_mmc(fi, copy, ri);
		if (err)
			return err;
	}

	return 0;
}

/* Save NBOOT and SPL region to MMC */
static int fs_image_save_nboot_to_mmc(struct flash_info *fi,
				      struct region_info *nboot_ri,
				      struct region_info *spl_ri)
{
	int err;
	int copy;

	/*
	 * Save sequence:
	 * 1. Invalidate NBOOT region by overwriting the first block. This
	 *    immediately invalidates the F&S header of the BOARD-CFG so that
	 *    this copy will not be loaded anymore. If interrupted here, any
	 *    copy of (old) SPL will use the second (old) copy of NBOOT that
	 *    matches the old SPL.
	 * 2. Write all of NBOOT but the first block. If interrupted, the
	 *    BOARD-CFG ist still invalid and the same happens as in 1.
	 * 3. Write first block of NBOOT. This adds the F&S header and makes
	 *    NBOOT valid.
	 * 4. Invalidate SPL region. This immediately invalidates SPL (IVT).
	 *    If interrupted here, the second (old) copy of SPL will use the
	 *    second (old) copy of the BOARD-CFG that matches the old SPL.
	 *    This assumes that the second copy is tried first if booting from
	 *    the secondary SPL image.
	 * 5. Write all of SPL but the first block. If interrupted here, SPL
	 *    is still invalid and the same happens as in 4.
	 * 6. Write the first block of SPL. This adds the IVT and makes SPL
	 *    valid. If interrupted after that, the first (new) copy of SPL
	 *    will use the first (new) copy of NBOOT that matches the new SPL.
	 *    If for some reason the first copy of SPL can not be loaded, the
	 *    system boots the old version from the second copy.
	 * 7. Update the second copies in the same sequence.
	 * 8. On i.MX8MM: Update the information block for the secondary SPL.
	 *
	 * Remark:
	 * - There is a small risk that an old SPL will load a new NBOOT copy,
	 *   e.g. if interrupted between 3. and 4. However this is only a real
	 *   problem if new NBOOT and old SPL differ significantly. Typically
	 *   an old SPL can also handle new NBOOT and vice versa.
	 *
	 * ### TODO:
	 * - Step 4 assumes that the second copy is tried first if booting
	 *   from the secondary SPL image, but currently this is not true on
	 *   i.MX8MN/MP/X because we cannot detect this on these architectures.
	 */
	for (copy = 0; copy < 2; copy++) {
		printf("Saving copy %d to mmc%d:\n", copy,
		       fi->blk_desc->devnum);
		err = fs_image_save_region_to_mmc(fi, copy, nboot_ri);
		if (err)
			return err;

		err = fs_image_save_region_to_mmc(fi, copy, spl_ri);
		if (err)
			return err;

#ifdef CONFIG_IMX8MM
		if (copy == 1) {
			/* Write Secondary Image Table */
			err = fs_image_write_secondary_table(fi, spl_ri->si);
			if (err)
				return err;
		}
#endif
	}

	return 0;
}
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
		fi->temp_size = fi->mtd->writesize;
		fi->temp = malloc(fi->temp_size);
		if (!fi->temp) {
			puts("Cannot allocate page buffer\n");
			return -ENOMEM;
		}
		memset(fi->temp, 0xff, fi->temp_size);
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
		fi->blk_desc = mmc_get_blk_desc(fi->mmc);
		fi->old_hwpart = fi->blk_desc->hwpart;
		fi->boot_hwpart = (fi->mmc->part_config >>3) & PART_ACCESS_MASK;
		if (fi->boot_hwpart > 2)
			fi->boot_hwpart = 0;
		break;
#endif

	default:
		printf("Cannot handle %s boot device\n", fi->boot_dev_name);
		return -ENODEV;
	}

	return 0;
}

/* Clean up flash info */
static void fs_image_put_flash_info(struct flash_info *fi)
{
	/* Free the temp buffer */
	if (fi->temp)
		free(fi->temp);

	switch (fi->boot_dev) {
#ifdef CONFIG_NAND_MXS
	case NAND_BOOT:
		/* Nothing to be done in case of NAND */
		break;
#endif

#ifdef CONFIG_MMC
	case MMC1_BOOT:
	case MMC2_BOOT:
	case MMC3_BOOT:
		if (blk_dselect_hwpart(fi->blk_desc, fi->old_hwpart))
			printf("Cannot switch back to original hwpart %d\n",
			       fi->old_hwpart);
		break;
#endif

	default:
		break;
	}
}

static int fs_image_get_nboot_info(struct flash_info *fi, void *fdt,
				   struct nboot_info *ni)
{
	int offs = fs_image_get_nboot_info_offs(fdt);

	if (offs < 0) {
		puts("Cannot find nboot-info in BOARD-CFG\n");
		return -ENOENT;
	}

	switch (fi->boot_dev) {
#ifdef CONFIG_NAND_MXS
	case NAND_BOOT:
		return fs_image_get_nboot_info_nand(fi, fdt, offs, ni);
#endif

#ifdef CONFIG_MMC
	case MMC1_BOOT:
	case MMC2_BOOT:
	case MMC3_BOOT:
		return fs_image_get_nboot_info_mmc(fi, fdt, offs, ni);
#endif

	default:
		return -ENODEV;
	}
}

static void fs_image_set_header(struct fs_header_v1_0 *fsh, const char *type,
				const char *descr, unsigned int size)
{
	u8 padsize = 0;

	padsize = size % 16;
	if (padsize)
		padsize = 16 - padsize;

	memset(fsh, 0, FSH_SIZE);
	fsh->info.magic[0] = 'F';
	fsh->info.magic[1] = 'S';
	fsh->info.magic[2] = 'L';
	fsh->info.magic[3] = 'X';
	fsh->info.file_size_low = size;
	fsh->info.flags = FSH_FLAGS_DESCR;
	fsh->info.padsize = padsize;
	fsh->info.version = 0x10;
	strncpy(fsh->type, type, sizeof(fsh->type));
	strncpy(fsh->param.descr, descr, sizeof(fsh->param.descr));

	//### TODO: Compute checksum if needed
}

static int fs_image_load_one_copy(struct flash_info *fi, int copy,
				  const struct storage_info *si,
				  struct sub_info *sub)
{
	int err;

	sub->size = 0;

	switch (fi->boot_dev) {
#ifdef CONFIG_NAND_MXS
	case NAND_BOOT:
		err = fs_image_load_image_from_nand(fi, copy, si, sub);
		break;
#endif
#ifdef CONFIG_CMD_MMC
	case MMC1_BOOT:
	case MMC2_BOOT:
	case MMC3_BOOT:
		err = fs_image_load_image_from_mmc(fi, copy, si, sub);
		break;
#endif
	default:
		err = -EINVAL;
		break;
	}

	if (!err && (sub->flags & SUB_HAS_FS_HEADER)) {
		//### TODO: Check CRC32 if given
	}

	if (!err)
		printf("Size 0x%x ", sub->size);
	fs_image_show_sub_status(err);

	return err;
}

static int fs_image_load_image(struct flash_info *fi,
			       const struct storage_info *si,
			       struct sub_info *sub)
{
	struct fs_header_v1_0 *fsh;
	void *copy0, *copy1;
	unsigned int size0 = 0;
	int err;

	printf("Loading %s\n", sub->type);

	/* Add room for FS header if image has none */
	fsh = sub->img;
	if (!(sub->flags & SUB_HAS_FS_HEADER))
		sub->img += FSH_SIZE;

	copy0 = sub->img;
	err = fs_image_load_one_copy(fi, 0, si, sub);
	size0 = sub->size;
	sub->img += size0;

	copy1 = sub->img;
	err = fs_image_load_one_copy(fi, 1, si, sub);
	if (err && (copy0 == copy1)) {
		printf("Error, cannot load %s\n", sub->type);
		return -ENOENT;
	} else if (err || (copy0 == copy1)) {
		printf("  Warning! One copy corrupted! You should save NBoot"
		       " again to fix this.\n");
	}

	if (!err) {
		if (copy0 == copy1)
			size0 = sub->size;
		else if ((size0 != sub->size) || memcmp(copy0, copy1, size0))
			printf("  Warning! Images differ, taking copy 0\n");
	}

	/* Fill FS header */
	sub->img = copy0 + size0;
	sub->size = size0;
	if (!(sub->flags & SUB_HAS_FS_HEADER))
		fs_image_set_header(fsh, sub->type, sub->descr, sub->size);

	return 0;
}

/* Handle fsimage save if loaded image is a U-Boot image */
static int fsimage_save_uboot(ulong addr, bool force)
{
	void *fdt;
	struct sub_info sub;
	struct region_info ri;
	struct flash_info fi;
	struct nboot_info ni;
	int ret;
	struct fs_header_v1_0 *fsh = (struct fs_header_v1_0 *)addr;

	fdt = fs_image_get_cfg_fdt();
	if (fs_image_get_flash_info(&fi, fdt)
	    || fs_image_get_nboot_info(&fi, fdt, &ni))
		return CMD_RET_FAILURE;

	fs_image_region_create(&ri, &ni.uboot, &sub);
	if (!fs_image_region_add(&ri, fsh, "U-BOOT", fs_image_get_arch(),
				0, /*###SUB_HAS_FS_HEADER |*/ SUB_SYNC))
		return CMD_RET_FAILURE;

	if (fs_image_validate(fsh, sub.type, sub.descr, addr))
		return CMD_RET_FAILURE;

	switch (fi.boot_dev) {
#ifdef CONFIG_NAND_MXS
	case NAND_BOOT:
		/* Ask for confirmation if UBoot MTD partitions do not match */
		if (fs_image_check_uboot_mtd(&ni.uboot, force))
			return CMD_RET_FAILURE;

		ret = fs_image_save_uboot_to_nand(&fi, &ri);
		break;
#endif

#ifdef CONFIG_MMC
	case MMC1_BOOT:
	case MMC2_BOOT:
	case MMC3_BOOT:
		/* Switch back to original hwpart when done */
		ret = fs_image_save_uboot_to_mmc(&fi, &ri);
		break;
#endif

	default:
		ret = -EINVAL;
		break;
	}

	fs_image_put_flash_info(&fi);

	return fs_image_show_save_status(ret, "U-BOOT");
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
	char id[MAX_DESCR_LEN + 1];

	if (fs_image_get_board_id(id))
		return CMD_RET_FAILURE;

	id[MAX_DESCR_LEN] = '\0';
	printf("%s\n", id);

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

	printf("offset   size     type (description)\n");
	printf("------------------------------------------------------------"
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
	struct fs_header_v1_0 *fsh_nboot, *fsh_board_info;
	struct flash_info fi;
	struct nboot_info ni;

	if ((argc > 1) && (argv[1][0] == '-')) {
		if (strcmp(argv[1], "-f"))
			return CMD_RET_USAGE;
		force = true;
		argv++;
		argc--;
	}

	if (argc > 1)
		addr = parse_loadaddr(argv[1], NULL);
	else
		addr = get_loadaddr();

	/* Ask for confirmation if there is already an F&S image at addr */
	fsh_nboot = (struct fs_header_v1_0 *)addr;
	if (fs_image_is_fs_image(fsh_nboot)) {
		printf("Warning! This will overwrite F&S image at 0x%lx\n",
		       addr);
		if (force && !fs_image_confirm())
			return CMD_RET_FAILURE;
	}

	/* Invalidate any old image */
	memset(fsh_nboot->info.magic, 0, 4);

	fdt = fs_image_get_cfg_fdt();
	if (fs_image_get_flash_info(&fi, fdt)
	    || fs_image_get_nboot_info(&fi, fdt, &ni))
		return CMD_RET_FAILURE;

#ifdef CONFIG_NAND_MXS
	/* Temporarily load BCB behind NBOOT F&S header */
	if (fi.boot_dev == NAND_BOOT) {
		if (fs_image_load_bcb(&fi, &ni.spl, fsh_nboot + 1))
			return CMD_RET_FAILURE;
	}
#endif

	/* Load SPL behind the NBOOT F&S header that is filled later */
	sub.type = "SPL";
	sub.descr = fs_image_get_arch();
	sub.img = fsh_nboot + 1;
	sub.offset = 0;
	sub.flags = SUB_IS_SPL;
	if (fs_image_load_image(&fi, &ni.spl, &sub))
		return CMD_RET_FAILURE;

	/* Load BOARD_CFG and create BOARD-CONFIGS header */
	fsh_board_info = sub.img;
	sub.type = "BOARD-CFG";
	sub.descr = NULL;
	sub.img = fsh_board_info + 1;
	sub.flags = SUB_HAS_FS_HEADER;
	if (fs_image_load_image(&fi, &ni.nboot, &sub))
		return CMD_RET_FAILURE;
	sub.descr = fs_image_get_arch();
	fs_image_set_header(fsh_board_info, "BOARD-CONFIGS", sub.descr,
			    sub.size);

	/* Load FIRMWARE */
	sub.type = "FIRMWARE";
	sub.offset = ni.board_cfg_size;
	if (fs_image_load_image(&fi, &ni.nboot, &sub))
		return CMD_RET_FAILURE;

	/* Fill overall NBOOT header */
	fs_image_set_header(fsh_nboot, "NBOOT", sub.descr,
			    sub.img - (void *)(fsh_nboot + 1));

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
	struct region_info nboot_ri, spl_ri;
	struct sub_info nboot_sub[2], spl_sub;
	const char *arch = fs_image_get_arch();
	void *fdt;
	struct nboot_info ni;
	struct flash_info fi;
	int ret;
	unsigned long addr;
	bool force = false;
	unsigned int woffset;

	if ((argc > 1) && (argv[1][0] == '-')) {
		if (strcmp(argv[1], "-f"))
			return CMD_RET_USAGE;
		force = true;
		argv++;
		argc--;
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

	/* Prepare subimages for NBOOT region: BOARD-CFG + FIRMWARE */
	fs_image_region_create(&nboot_ri, &ni.nboot, nboot_sub);
	woffset = fs_image_region_add(&nboot_ri, cfg_fsh, "BOARD-CFG",
				      arch, 0, SUB_HAS_FS_HEADER | SUB_SYNC);
	if (!woffset)
		return CMD_RET_FAILURE;
	if (ni.board_cfg_size)
		woffset = ni.board_cfg_size;
	woffset = fs_image_region_find_add(&nboot_ri, nboot_fsh, "FIRMWARE",
					   arch, woffset,
					   SUB_HAS_FS_HEADER | SUB_SYNC);
	if (!woffset)
		return CMD_RET_FAILURE;

	/* Prepare subimage for SPL */
	fs_image_region_create(&spl_ri, &ni.spl, &spl_sub);
	woffset = fs_image_region_find_add(&spl_ri, nboot_fsh, "SPL",
					   arch, 0, SUB_IS_SPL | SUB_SYNC);
	if (!woffset)
		return CMD_RET_FAILURE;

	/* Found all sub-images, let's go and save NBoot */
	switch (fi.boot_dev) {
#ifdef CONFIG_NAND_MXS
	case NAND_BOOT:
		/* Issue warning if UBoot MTD partitions are missing/differ */
		fs_image_check_uboot_mtd(&ni.uboot, true); /* ### needed? */

		ret = fs_image_save_nboot_to_nand(&fi, &nboot_ri, &spl_ri);
		break;
#endif

#ifdef CONFIG_MMC
	case MMC1_BOOT:
	case MMC2_BOOT:
	case MMC3_BOOT:
#ifndef CONFIG_IMX8MM
		ret = fs_image_check_secondary_offset(&fi, &ni.spl, force);
		if (ret)
			break;
#endif
		ret = fs_image_save_nboot_to_mmc(&fi, &nboot_ri, &spl_ri);
		break;
#endif

	default:
		ret = -EINVAL;
		break;
	}

	fs_image_put_flash_info(&fi);

	if (!ret) {
		/* Success: Activate new BOARD-CFG by copying it to OCRAM */
		puts("Activating new BOARD-CFG... ");
		memcpy(fs_image_get_cfg_addr(), cfg_fsh,
		       fs_image_get_size(cfg_fsh, true));
		fs_image_show_sub_status(0);
	}

	return fs_image_show_save_status(ret, "NBOOT");
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
	 * it is still valid and not compromised in some way.
	 */
	if (!fs_image_cfg_is_valid()) {
		printf("Error: BOARD-CFG in OCRAM at 0x%lx damaged\n",
		       (ulong)found_cfg);
		return CMD_RET_FAILURE;
	}

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
	   "fsimage load [-f] [<addr>]\n"
	   "    - Load the current NBoot to <addr> for inspection\n"
	   "fsimage save [-f] [<addr>]\n"
	   "    - Save the F&S image at the right place (U-Boot, NBoot)\n"
	   "fsimage fuse [-f] [<addr> | stored]\n"
	   "    - Program fuses according to the current BOARD-CFG.\n"
	   "      WARNING: This is a one time option and cannot be undone.\n"
	   "\n"
	   "If no addr is given, use loadaddr. Using -f forces the command to\n"
	   "continue without showing any confirmation queries. This is meant\n"
	   "for non-interactive installation procedures.\n"
);
