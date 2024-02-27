/*
 * Copyright 2021 F&S Elektronik Systeme GmbH
 *
 * Hartmut Keller, F&S Elektronik Systeme GmbH, keller@fs-net.de
 *
 * SPDX-License-Identifier:	GPL-2.0+
 *
 * F&S image processing
 *
 * When the SPL starts it needs to know what board it is running on. Usually
 * this information is read from NAND or eMMC. It then selects the correct DDR
 * RAM settings, initializes DRAM and then loads the ATF (and probably other
 * images like opTEE).
 *
 * Originally all these parts are all separate files: DRAM timings, DRAM
 * training firmware, the SPL code itself, the ATF code, board configurations,
 * etc. By using F&S headers, these files are all combined in one image that
 * is called NBoot.
 *
 * However if the board is empty (after having been assembled), or if the
 * configuration does not work for some reason, it is necessary to use the SDP
 * Serial Download Protocol to provide the configuration information.
 * Unfortunately the NBoot file is too big to fit in any available OCRAM and
 * TCM memory. But splitting NBoot in smaller parts and downloading separately
 * one after the other is also very uncomfortable and error prone. So the idea
 * was to interpret the data *while* it is downloaded. We call this
 * "streaming" the NBoot configuration file. Every time a meaningful part is
 * fully received, it is used to do the next part of the initialization.
 *
 * This detection is done by looking at the F&S headers. Each header gives
 * information for the next part of data. The following is a general view of
 * the NBoot structure.
 *
 *   +--------------------------------------------+
 *   | BOARD-ID (id) (optional)                   | CRC32 header
 *   |   +----------------------------------------+
 *   |   | NBOOT (arch)                           | Signed / CRC32 header+image
 *   |   |   +------------------------------------+
 *   |   |   | SPL (arch)                         | Signed / CRC32 header+image
 *   |   |   +------------------------------------+
 *   |   |   | BOARD-INFO (arch)                  | CRC32 header
 *   |   |   |   +--------------------------------+
 *   |   |   |   | BOARD-CFG (id)                 | Signed / CRC32 header+image
 *   |   |   |   +--------------------------------+
 *   |   |   |   | BOARD-CFG (id)                 | Signed / CRC32 header+image
 *   |   |   |   +--------------------------------+
 *   |   |   |   | ...                            |
 *   |   |   |---+--------------------------------+
 *   |   |   | FIRMWARE (arch)                    | CRC32 header
 *   |   |   |   +--------------------------------+
 *   |   |   |   | DRAM-INFO (arch)               | CRC32 header
 *   |   |   |   |   +----------------------------+
 *   |   |   |   |   | DRAM-TYPE (ddr3l)          | CRC32 header
 *   |   |   |   |   |   +------------------------+
 *   |   |   |   |   |   | DRAM-FW (ddr3l)        | Signed / CRC32 header+image
 *   |   |   |   |   |   +------------------------+
 *   |   |   |   |   |   | DRAM-TIMING (ram-chip) | Signed / CRC32 header+image
 *   |   |   |   |   |   +------------------------+
 *   |   |   |   |   |   | DRAM-TIMING (ram-chip) | Signed / CRC32 header+image
 *   |   |   |   |   |   +------------------------+
 *   |   |   |   |   |   | ...                    |
 *   |   |   |   |   +---+------------------------+
 *   |   |   |   |   | DRAM-TYPE (ddr4)           | CRC32 header
 *   |   |   |   |   |   +------------------------+
 *   |   |   |   |   |   | DRAM-FW (ddr4)         | Signed / CRC32 header+image
 *   |   |   |   |   |   +------------------------+
 *   |   |   |   |   |   | DRAM-TIMING (ram-chip) | Signed / CRC32 header+image
 *   |   |   |   |   |   +------------------------+
 *   |   |   |   |   |   | DRAM-TIMING (ram-chip) | Signed / CRC32 header+image
 *   |   |   |   |   |   +------------------------+
 *   |   |   |   |   |   | ...                    |
 *   |   |   |   |   +---+------------------------+
 *   |   |   |   |   | DRAM-TYPE (lpddr4)         | CRC32 header
 *   |   |   |   |   |   +------------------------+
 *   |   |   |   |   |   | DRAM-FW (lpddr4)       | Signed / CRC32 header+image
 *   |   |   |   |   |   +------------------------+
 *   |   |   |   |   |   | DRAM-TIMING (ram-chip) | Signed / CRC32 header+image
 *   |   |   |   |   |   +------------------------+
 *   |   |   |   |   |   | DRAM-TIMING (ram-chip) | Signed / CRC32 header+image
 *   |   |   |   |   |   +------------------------+
 *   |   |   |   |   |   | ...                    |
 *   |   |   |   +---+---+------------------------+
 *   |   |   |   | ATF (arch)                     | Signed / CRC32 header+image
 *   |   |   |   +--------------------------------+
 *   |   |   |   | TEE (arch) (optional)          | Signed / CRC32 header+image
 *   |   |   +---+--------------------------------+
 *   |   |   | EXTRAS                             |
 *   |   |   |   +--------------------------------+
 *   |   |   |   | BASH-SCRIPT (addfsheader.sh)   |
 *   |   |   |   +--------------------------------+
 *   |   |   |   | BASH-SCRIPT (fsimage.sh)       |
 *   +---+---+---+--------------------------------+
 *
 * Comments
 *
 * - The BOARD-ID is only present when the board is empty; from a customers
 *   view, NBoot has no such BOARD-ID prepended.
 * - The BASH-SCRIPT fsimage.sh can be used to extract the individual parts
 *   from an NBOOT image. It should be the last part in NBoot, so that it can
 *   easily be found and extracted with some grep/tail commands.
 *
 * Only a subset from such an NBoot image needs to be stored on the board:
 *
 * - Exactly one BOARD-CFG is stored; it identifies the board.
 * - SPL is stored in a special way so that the ROM loader can execute it.
 * - The FIRMWARE section is stored for DRAM settings, ATF and TEE. In fact
 *   from the DRAM settings only the specific setting for the specific board
 *   would be needed, but it may be too complicated to extract this part only
 *   when saving the data.
 *
 * When NBoot is downloaded via USB, then data is coming in automatically. We
 * have no way of triggering the next part of data. So we need a state machine
 * to interpret the stream data and do the necessary initializations at the
 * time when the data is available.
 *
 * When loading the data from NAND or eMMC, this could theoretically be done
 * differently, because here we can actively read the necessary parts. But
 * as we do not want to implement two versions of the interpreter, we try to
 * imitate the USB data flow for NAND and eMMC, too. So in the end we can use
 * the same state machine for all configuration scenarios.
 *
 * When the "download" is started, we get a list of jobs that have to be done.
 * For example:
 *
 * 1. Load board configuration
 * 2. Initialize DRAM
 * 3. Load ATF
 *
 * The state machine then always loads the next F&S header and decides from its
 * type what to do next. If a specific kind of initialization is in the job
 * list, it loads the image part and performs the necessary initialization
 * steps. Otherwise this image part is skipped.
 *
 * Not all parts of an NBoot image must be present. For example as only the
 * FIRMWARE part is stored on the board, the state machine can only perform the
 * last two steps. This has to be taken care of by the caller. If a previous
 * step is missing for some specific job, the job can also not be done. For
 * example a TEE file can only be loaded to DRAM, so DRAM initialization must
 * be done before that.
 */

#include <common.h>
#include <fdt_support.h>		/* fdt_getprop_u32_default_node() */
#include <spl.h>
#include <mmc.h>
#include <nand.h>
#include <sdp.h>
#include <asm/sections.h>
#include <u-boot/crc.h>			/* crc32() */

#include "fs_board_common.h"		/* fs_board_*() */
#include "fs_dram_common.h"		/* fs_dram_init_common() */
#include "fs_image_common.h"		/* Own interface */

#include <asm/mach-imx/checkboot.h>
#ifdef CONFIG_FS_SECURE_BOOT
#include <asm/mach-imx/hab.h>
#include <stdbool.h>
#include <hang.h>
#endif

/* Structure to handle board name and revision separately */
struct bnr {
	char name[MAX_DESCR_LEN];
	unsigned int rev;
};

static struct bnr compare_bnr;		/* Used for BOARD-ID comparisons */
static char board_id[MAX_DESCR_LEN + 1]; /* Current board-id */

struct env_info {
	unsigned int start[2];
	unsigned int size;
};


/* ------------- Functions in SPL and U-Boot ------------------------------- */

/* Return the F&S architecture */
const char *fs_image_get_arch(void)
{
	return CONFIG_SYS_BOARD;
}

/* Check if this is an F&S image */
bool fs_image_is_fs_image(const struct fs_header_v1_0 *fsh)
{
	return !strncmp(fsh->info.magic, "FSLX", sizeof(fsh->info.magic));
}

/* Return the intended address of the board configuration in OCRAM */
void *fs_image_get_regular_cfg_addr(void)
{
	return (void*)CONFIG_FUS_BOARDCFG_ADDR;
}

/* Return the real address of the board configuration in OCRAM */
void *fs_image_get_cfg_addr(void)
{
	void *cfg;

	if (!gd->board_cfg)
		gd->board_cfg = (ulong)fs_image_get_regular_cfg_addr();

	cfg = (void *)gd->board_cfg;

	return cfg;
}

/* Return the fdt part of the given board configuration */
void *fs_image_find_cfg_fdt(struct fs_header_v1_0 *fsh)
{
	void *fdt = fsh + 1;

	if (fs_image_is_signed(fsh))
		fdt += HAB_HEADER;

	return fdt;
}

/* Return the fdt part of the board configuration in OCRAM */
void *fs_image_get_cfg_fdt(void)
{
	return fs_image_find_cfg_fdt(fs_image_get_cfg_addr());
}

/* Return the address of the /nboot-info node */
int fs_image_get_nboot_info_offs(void *fdt)
{
	return fdt_path_offset(fdt, "/nboot-info");
}

/* Return the address of the /board-cfg node */
int fs_image_get_board_cfg_offs(void *fdt)
{
	return fdt_path_offset(fdt, "/board-cfg");
}

/* Return pointer to string with NBoot version */
const char *fs_image_get_nboot_version(void *fdt)
{
	int offs;

	if (!fdt)
		fdt = fs_image_get_cfg_fdt();

	offs = fs_image_get_nboot_info_offs(fdt);
	return fdt_getprop(fdt, offs, "version", NULL);
}

/* Read the image size (incl. padding) from an F&S header */
unsigned int fs_image_get_size(const struct fs_header_v1_0 *fsh,
			       bool with_fs_header)
{
	/* We ignore the high word, boot images are definitely < 4GB */
	return fsh->info.file_size_low + (with_fs_header ? FSH_SIZE : 0);
}

/* Check image magic, type and descr; return true on match */
bool fs_image_match(const struct fs_header_v1_0 *fsh,
		    const char *type, const char *descr)
{
	if (!type)
		return false;

	if (!fs_image_is_fs_image(fsh))
		return false;

	if (strncmp(fsh->type, type, MAX_TYPE_LEN))
		return false;

	if (descr) {
		if (!(fsh->info.flags & FSH_FLAGS_DESCR))
			return false;
		if (strncmp(fsh->param.descr, descr, MAX_DESCR_LEN))
			return false;
	}

	return true;
}

static void fs_image_get_board_name_rev(const char id[MAX_DESCR_LEN],
					struct bnr *bnr)
{
	char c;
	int i;
	int rev = -1;

	/* Copy string and look for rightmost '.' */
	bnr->rev = 0;
	i = 0;
	do {
		c = id[i];
		bnr->name[i] = c;
		if (!c)
			break;
		if (c == '.')
			rev = i;
	} while (++i < sizeof(bnr->name));

	/* No revision found, assume 0 */
	if (rev < 0)
		return;

	bnr->name[rev] = '\0';
	while (++rev < i) {
		char c = bnr->name[rev];

		if ((c < '0') || (c > '9'))
			break;
		bnr->rev = bnr->rev * 10 + c - '0';
	}
}

/* Check if ID of the given BOARD-CFG matches the compare_id */
bool fs_image_match_board_id(struct fs_header_v1_0 *cfg_fsh)
{
	struct bnr bnr;

	/* Compare magic and type */
	if (!fs_image_match(cfg_fsh, "BOARD-CFG", NULL))
		return false;

	/* A config must include a description, this is the board ID */
	if (!(cfg_fsh->info.flags & FSH_FLAGS_DESCR))
		return false;

	/* Split board ID of the config we look at into name and rev */
	fs_image_get_board_name_rev(cfg_fsh->param.descr, &bnr);

	/*
	 * Compare with name and rev of the BOARD-ID we are looking for (in
	 * compare_bnr). In the new layout, the BOARD-CFG does not have a
	 * revision anymore, so here bnr.rev is 0 and is accepted for any
	 * BOARD-ID of this board type.
	 */
	if (strncmp(bnr.name, compare_bnr.name, sizeof(bnr.name)))
		return false;
	if (bnr.rev > compare_bnr.rev)
		return false;

	return true;
}

/* Read property from board-rev subnode or board-cfg main node */
const void *fs_image_getprop(const void *fdt, int cfg_offs, int rev_offs,
			     const char *name, int *lenp)
{
	const void *prop;

	if (rev_offs) {
		prop = fdt_getprop(fdt, rev_offs, name, lenp);
		if (prop)
			return prop;
	}

	return fdt_getprop(fdt, cfg_offs, name, lenp);
}

/* Read u32 property from board-rev subnode or board-cfg main node */
u32 fs_image_getprop_u32(const void *fdt, int cfg_offs, int rev_offs,
			 int cell, const char *name, const u32 dflt)
{
	const void *prop;
	int len;

	if (rev_offs) {
		prop = fdt_getprop(fdt, rev_offs, name, &len);
		if (prop)
			return fdt_getprop_u32_default_node(fdt, rev_offs,
							    cell, name, dflt);
	}

	return fdt_getprop_u32_default_node(fdt, cfg_offs, cell, name, dflt);
}

/* Check if the F&S image is signed (followed by an IVT) */
bool fs_image_is_signed(struct fs_header_v1_0 *fsh)
{
	struct ivt *ivt = (struct ivt *)(fsh + 1);

	return (ivt->hdr.magic == IVT_HEADER_MAGIC);
}

/* Check IVT integrity of F&S image and return size and validation address */
void *fs_image_get_ivt_info(struct fs_header_v1_0 *fsh, u32 *size)
{
	struct ivt *ivt = (struct ivt *)(fsh + 1);
	struct boot_data *boot;

	/*
	 * Layout of a signed F&S image (SPL differs, but does not matter here)
	 *
	 * Offset    Header-Entry    Size                     Content
	 * -----------------------------------------------------------------
	 * 0x00      boot->start     0x40 (FSH-SIZE)          F&S header
	 * 0x40      ivt->self       0x20 (IVT_TOTAL_LENGTH)  IVT
	 * 0x60      ivt->boot       0x20                     boot_data
	 * 0x80      ivt->entry      n                        Image data
	 * 0x80+n    ivt->csf        m                        CSF
	 * -----------------------------------------------------------------
	 *           boot->length <- total size: n+m+0x80
	 *
	 * Remark: The size of IVT and boot_data together is HAB_HEADER
	 */
	if ((ivt->boot != ivt->self + IVT_TOTAL_LENGTH)
	    || (ivt->entry != ivt->self + HAB_HEADER))
		return NULL;

	boot = (struct boot_data *)(ivt + 1);
	*size = boot->length;

	return (void *)(ulong)(boot->start);
}

/* Validate a signed image; it has to be at the validation address */
bool fs_image_is_valid_signature(struct fs_header_v1_0 *fsh)
{
	struct ivt *ivt = (struct ivt *)(fsh + 1);
	void *start;
	u32 size;

	/* Check IVT integrity */
	start = fs_image_get_ivt_info(fsh, &size);
	if (!start || (start != fsh) || ((ulong)(ivt->self) != (ulong)ivt))
		return false;

#ifdef CONFIG_FS_SECURE_BOOT
	{
		u16 flags;
		u32 file_size_high;
		u32 *pcs = (u32 *)&fsh->type[12];
		u32 crc32;
		int err;

		/*
		 * The signature was created without board revision in
		 * file_size_high and without CRC32, so clear these values
		 * temporarily.
		 */
		file_size_high = fsh->info.file_size_high;
		fsh->info.file_size_high = 0;
		flags = fsh->info.flags;
		fsh->info.flags &= ~(FSH_FLAGS_SECURE | FSH_FLAGS_CRC32);
		crc32 = *pcs;
		*pcs = 0;

		/* Check signature */
		err = imx_hab_authenticate_image((u32)(ulong)fsh,
				fs_image_get_size(fsh, true), FSH_SIZE);

		/* Bring back the saved values */
		fsh->info.file_size_high = file_size_high;
		fsh->info.flags = flags;
		*pcs = crc32;

		if (err)
			return false;
	}
#endif

	return true;
}

/*
 * Check CRC32; return: 0: No CRC32, >0: CRC32 OK, <0: error (CRC32 failed)
 * If CRC32 is OK, the result also gives information about the type of CRC32:
 * 1: CRC32 was Header only (only FSH_FLAGS_SECURE set)
 * 2: CRC32 was Image only (only FSH_FLAGS_CRC32 set)
 * 3: CRC32 was Header+Image (both FSH_FLAGS_SECURE and FSH_FLAGS_CRC32 set)
 */
int fs_image_check_crc32(const struct fs_header_v1_0 *fsh)
{
	u32 expected_cs;
	u32 computed_cs;
	u32 *pcs;
	unsigned int size;
	unsigned char *start;
	int ret = 0;

	if (!(fsh->info.flags & (FSH_FLAGS_SECURE | FSH_FLAGS_CRC32)))
		return 0;		/* No CRC32 */

	if (fsh->info.flags & FSH_FLAGS_SECURE) {
		start = (unsigned char *)fsh;
		size = FSH_SIZE;
		ret |= 1;
	} else {
		start = (unsigned char *)(fsh + 1);
		size = 0;
	}

	if (fsh->info.flags & FSH_FLAGS_CRC32) {
		size += fs_image_get_size(fsh, false);
		ret |= 2;
	}

	/* CRC32 is in type[12..15]; temporarily set to 0 while computing */
	pcs = (u32 *)&fsh->type[12];
	expected_cs = *pcs;
	*pcs = 0;
	computed_cs = crc32(0, start, size);
	*pcs = expected_cs;
	if (computed_cs != expected_cs)
		return -EILSEQ;

	return ret;
}

/* Add the board revision as BOARD-ID to the given BOARD-CFG and update CRC32 */
void fs_image_board_cfg_set_board_rev(struct fs_header_v1_0 *cfg_fsh)
{
	u32 *pcs = (u32 *)&cfg_fsh->type[12];

	/* Set compare_id rev in file_size_high, compute new CRC32 */
	cfg_fsh->info.file_size_high = compare_bnr.rev;

	cfg_fsh->info.flags |= FSH_FLAGS_SECURE | FSH_FLAGS_CRC32;
	*pcs = 0;
	*pcs = crc32(0, (uchar *)cfg_fsh, fs_image_get_size(cfg_fsh, true));
}

/* Return the current BOARD-ID */
const char *fs_image_get_board_id(void)
{
	return board_id;
}

/* Store current compare_id as board_id */
static void fs_image_set_board_id(void)
{
	char c;
	int i;

	/* Copy base name */
	for (i = 0; i < MAX_DESCR_LEN; i++) {
		c = compare_bnr.name[i];
		if (!c)
			break;
		board_id[i] = c;
	}

	/* Add board revision */
	snprintf(&board_id[i], MAX_DESCR_LEN - i, ".%03d", compare_bnr.rev);
	board_id[MAX_DESCR_LEN] = '\0';
}

/* Set the compare_id that will be used in fs_image_match_board_id() */
void fs_image_set_compare_id(const char id[MAX_DESCR_LEN])
{
	fs_image_get_board_name_rev(id, &compare_bnr);
}

/* Get the board-rev from BOARD-ID (in compare-id) */
unsigned int fs_image_get_board_rev(void)
{
	return compare_bnr.rev;
}

/* Get the BOARD-ID from the BOARD-CFG in OCRAM */
static void _get_board_id_from_cfg(struct bnr *bnr)
{
	struct fs_header_v1_0 *cfg_fsh = fs_image_get_cfg_addr();
	unsigned int rev;

	/* Take base ID from descr; in old layout, this includes the rev */
	fs_image_get_board_name_rev(cfg_fsh->param.descr, bnr);

	/* The BOARD-ID rev is stored in file_size_high, 0 for old layout */
	rev = cfg_fsh->info.file_size_high;
	if (rev) {
		debug("Taking BOARD-ID rev from BOARD-CFG: %d\n", rev);
		bnr->rev = rev;
	}
}

/* Set the board_id and compare_id from the BOARD-CFG */
void fs_image_set_board_id_from_cfg(void)
{
	_get_board_id_from_cfg(&compare_bnr);

	fs_image_set_board_id();
}

/*
 * Find board_rev and return board-cfg subnode matching the given id_rev, 0 if
 * no subnode was found.
 */
static int _get_board_rev_subnode(const void *fdt, int offs, uint id_rev)
{
	int subnode;
	int rev_subnode = 0;
	int rev = 0;
	unsigned int temp;

	subnode = fdt_first_subnode(fdt, offs);
	while (subnode >= 0) {
		temp = fdt_getprop_u32_default_node(fdt, subnode, 0,
						    "board-rev", 100);
		if ((temp > rev) && (temp <= id_rev)) {
			rev = temp;
			rev_subnode = subnode;
			if (rev == id_rev)
				break;
		}
		subnode = fdt_next_subnode(fdt, subnode);
	}

	/* If no subnode was found, try the board-cfg node itself */
	if (!rev_subnode) {
		rev = fdt_getprop_u32_default_node(fdt, offs, 0,
						   "board-rev", 100);
	}

	debug("BOARD-ID rev=%u, BOARD-CFG rev=%u\n", id_rev, rev);

	return rev_subnode;
}

/*
 * Find board_rev of BOARD-ID (in compare-id) and return board-cfg subnode
 * matching it. The compare-id has to be set before this call.
 */
int fs_image_get_board_rev_subnode(const void *fdt, int offs)
{
	return _get_board_rev_subnode(fdt, offs, compare_bnr.rev);
}

/*
 * In the f-phase of U-Boot, when there are no variables available, we cannot
 * call fs_image_get_board_rev_subnode() because it uses the compare-id. We
 * have to determine the BOARD-ID directly from the BOARD-CFG in OCRAM. Also
 * return the board-rev (from the BOARD-ID) in this case.
 */
int fs_image_get_board_rev_subnode_f(const void *fdt, int offs, uint *board_rev)
{
	struct bnr bnr;

	/* Get the BOARD-ID from the BOARD-CFG in OCRAM */
	_get_board_id_from_cfg(&bnr);
	if (board_rev)
		*board_rev = bnr.rev;

	return _get_board_rev_subnode(fdt, offs, bnr.rev);
}

/*
 * Make sure that BOARD-CFG in OCRAM is valid. This function is called early
 * in the boot_f phase of U-Boot, and therefore must not access any variables.
 */
bool fs_image_is_ocram_cfg_valid(void)
{
	struct fs_header_v1_0 *cfg_fsh = fs_image_get_cfg_addr();
	int err;
	const char *type = "BOARD-CFG";

	if (!fs_image_match(cfg_fsh, type, NULL))
		return false;

	/*
	 * Check the additional CRC32. The BOARD-CFG in OCRAM also holds the
	 * BOARD-ID, which is not covered by the signature, but it is covered
	 * by the CRC32. So also do the check in case of Secure Boot.
	 *
	 * The BOARD-ID is given by the board-revision that is stored (as
	 * number) in unused entry file_size_high and is typically between 100
	 * and 999. This is the only part that may differ, the base name is
	 * always the same as of the ID of the BOARD-CFG itself (in descr).
	 */
	err = fs_image_check_crc32(cfg_fsh);
	if (err < 0)
		return false;

	/* Handle signed image */
	if (fs_image_is_signed(cfg_fsh))
		return fs_image_is_valid_signature(cfg_fsh);

	/* Handle unsigned image */
#ifdef CONFIG_FS_SECURE_BOOT
	if (imx_hab_is_enabled()) {
		printf("\nError: Refusing unsigned %s on closed board\n", type);
		return false;
	}
#endif

	return true;
}


/* ------------- Functions only in U-Boot, not SPL ------------------------- */

#ifndef CONFIG_SPL_BUILD

/*
 * Return if currently running from Secondary SPL. This function is called
 * early in boot_f phase of U-Boot and must not access any variables.
 */
bool fs_image_is_secondary(void)
{
	struct fs_header_v1_0 *fsh = fs_image_get_cfg_addr();
	u8 *size = (u8 *)&fsh->info.file_size_low;

	/*
	 * We know that a BOARD-CFG is smaller than 64KiB. So only the first
	 * two bytes of the file_size are actually used. Especially the 8th
	 * byte is definitely 0. SPL uses this byte to indicate if it was
	 * running from Primary (0) or Secondary SPL (<>0). Take this info and
	 * reset the byte to 0 before validating the BOARD-CFG.
	 */
	if (size[7]) {
		size[7] = 0;
		return true;
	}

	return false;
}

bool fs_image_is_secondary_uboot(void)
{
	struct fs_header_v1_0 *fsh = fs_image_get_cfg_addr();
	u8 *size = (u8 *)&fsh->info.file_size_low;

	/*
	 * Similar to the SPL, we use the "file_size_high" field of the
	 * BOARD-CFG as an indicator that we booted the UBoot from the
	 * secondary partition.
	 */
	if (size[6]) {
		size[6] = 0;
		return true;
	}

	printf("UBoot is secondary\n");
	return false;
}

/*
 * Return address of board configuration in OCRAM; search for it if not
 * available at expected place. This function is called early in boot_f phase
 * of U-Boot and must not access any variables. Use global data instead.
 */
bool fs_image_find_cfg_in_ocram(void)
{
	struct fs_header_v1_0 *fsh;
	const char *type = "BOARD-CFG";

	/* Try expected location first */
	fsh = fs_image_get_regular_cfg_addr();
	if (fs_image_match(fsh, type, NULL))
		return true;

	/*
	 * Search it from beginning of OCRAM.
	 *
	 * To avoid having to search this location over and over again, save a
	 * pointer to it in global data.
	 */
	fsh = (struct fs_header_v1_0 *)CONFIG_SYS_OCRAM_BASE;
	do {
		if (fs_image_match(fsh, type, NULL)) {
			gd->board_cfg = (ulong)fsh;
			return true;
		}
		fsh++;
	} while ((ulong)fsh < (CONFIG_SYS_OCRAM_BASE + CONFIG_SYS_OCRAM_SIZE));

	return false;
}

static int fs_image_fdt_err(const char *name, const char *reason, int err)
{
	printf("Entry %s in BOARD-CFG %s\n", name, reason);

	return err;
}

/* Get count values from given device tree property and check alignment */
int fs_image_get_fdt_val(void *fdt, int offs, const char *name, uint align,
			 int count, uint *val)
{
	int len;
	const fdt32_t *start;
	int i;

	start = fdt_getprop(fdt, offs, name, &len);
	if (!start)
		return fs_image_fdt_err(name, "missing", -ENOENT);

	/* Be pedantic, if nboot-info values are wrong, nothing will work */
	if (!len || (len % sizeof(fdt32_t)))
		return fs_image_fdt_err(name, "invalid", -EINVAL);
	len /= sizeof(fdt32_t);
	if (len > count)
		return fs_image_fdt_err(name, "has too many values", -EINVAL);

	/* Fetch all available values */
	for (i = 0; i < len; i++) {
		val[i] = fdt32_to_cpu(start[i]);
		if (align && (val[i] % align))
			return fs_image_fdt_err(name, "not aligned", -EINVAL);
	}

	/* If we have less than count values, repeat the last value */
	for (i = len; i < count; i++)
		val[i] = val[len - 1];

	return 0;
}

#ifdef CONFIG_NAND_MXS
static const struct env_info fs_image_known_env_nand[] = {
	{
		.start = {0x480000, 0x4c0000},
		.size = 0x4000
	},
};

int fs_image_get_known_env_nand(uint index, uint start[2], uint *size)
{
	const struct env_info *env_info;

	printf("Using env location fallback #%d for old NBoot\n", index);
	if (index > ARRAY_SIZE(fs_image_known_env_nand)) {
		puts("No such fallback for env location\n");
		return -EINVAL;
	}

	env_info = &fs_image_known_env_nand[index];

	start[0] = env_info->start[0];
	start[1] = env_info->start[1];
	if (size)
		*size = env_info->size;

	return 0;
}

#endif

#ifdef CONFIG_MMC
/*
 * List of know environment positions before it was moved to nboot-info.
 *
 * Remarks:
 * - Old versions of PicoCoreMX8MN, where no redundand environment was used
 *   and the environment was on 0x400000, are not supported.
 * - efusMX8X still uses no redundand environment, updating from such a version
 *   is not implemented yet. (### TODO ###)
 */
#ifdef CONFIG_IMX8MM
static const struct env_info fs_image_known_env_mmc[] = {
	{
		.start = {0x100000, 0x104000},
		.size = 0x4000
	},
	{
		.start = {0x100000, 0x104000},
		.size = 0x2000
	},
};
#elif defined CONFIG_IMX8MN
static const struct env_info fs_image_known_env_mmc[] = {
	{
		.start = {0x138000, 0x13c000},
		.size = 0x4000
	},
};
#elif defined CONFIG_IMX8MP
static const struct env_info fs_image_known_env_mmc[] = {
	{
		.start = {0x138000, 0x13c000},
		.size = 0x4000
	},
	{
		.start = {0x100000, 0x104000},
		.size = 0x4000
	},
};
#elif defined CONFIG_IMX8X
static const struct env_info fs_image_known_env_mmc[] = {
	{
		.start = {0x200000, 0x200000},
		.size = 0x2000
	},
};
#else
static const struct env_info fs_image_known_env_mmc[0];
#endif

int fs_image_get_known_env_mmc(uint index, uint start[2], uint *size)
{
	const struct env_info *env_info;

	printf("Using env location fallback #%d for old NBoot\n", index);
	if (index > ARRAY_SIZE(fs_image_known_env_mmc)) {
		puts("No such fallback for env location\n");
		return -EINVAL;
	}

	env_info = &fs_image_known_env_mmc[index];

	start[0] = env_info->start[0];
	start[1] = env_info->start[1];
	if (size)
		*size = env_info->size;

	return 0;
}

#endif /* CONFIG_MMC */

#endif /* !CONFIG_SPL_BUILD */

/* ------------- Functions only in SPL, not U-Boot ------------------------- */

#ifdef CONFIG_SPL_BUILD

/* Jobs to do when streaming image data */
#define FSIMG_JOB_CFG BIT(0)
#define FSIMG_JOB_DRAM BIT(1)
#define FSIMG_JOB_ATF BIT(2)
#define FSIMG_JOB_TEE BIT(3)
#ifdef CONFIG_IMX_OPTEE
#define FSIMG_FW_JOBS (FSIMG_JOB_DRAM | FSIMG_JOB_ATF | FSIMG_JOB_TEE)
#else
#define FSIMG_FW_JOBS (FSIMG_JOB_DRAM | FSIMG_JOB_ATF)
#endif

/* Load mode */
enum fsimg_mode {
	FSIMG_MODE_HEADER,		/* Loading F&S header */
	FSIMG_MODE_IMAGE,		/* Loading F&S image */
	FSIMG_MODE_SKIP,		/* Skipping data */
	FSIMG_MODE_DONE,		/* F&S image done */
};

static enum fsimg_state {
	FSIMG_STATE_ANY,
	FSIMG_STATE_BOARD_CFG,
	FSIMG_STATE_DRAM,
	FSIMG_STATE_DRAM_TYPE,
	FSIMG_STATE_DRAM_FW,
	FSIMG_STATE_DRAM_TIMING,
	FSIMG_STATE_ATF,
	FSIMG_STATE_TEE,
} state;

static enum fsimg_mode mode;
static unsigned int count;
static void *addr;
static unsigned int jobs;
static int nest_level;
static const char *ram_type;
static const char *ram_timing;
static basic_init_t basic_init_callback;
static const char *layout_name;
static struct fs_header_v1_0 one_fsh;	/* Buffer for one F&S header */
static void *validate_addr = NULL;
static void *final_addr;
static bool keep_fs_header;

#define MAX_NEST_LEVEL 8

static struct fsimg {
	unsigned int size;		/* Size of the main image data */
	unsigned int remaining;		/* Remaining bytes in this level */
} fsimg_stack[MAX_NEST_LEVEL];

struct flash_info_spl {
	unsigned int offs[2];		/* Offset in flash for each copy */
	const char *layout;		/* Name of the layout */
#ifdef CONFIG_CMD_MMC
	struct mmc *mmc;		/* Handle to MMC device */
	struct blk_desc *blk_desc;	/* Handle to MMC block device */
	u8 hwpart[2];			/* hwpart for each copy */
#endif
	/* Function to load a chunk of data from any offset */
	int (*load)(uint32_t offs, uint size, void *buf);

	/* Function to set the hwpart for given copy */
	int (*set_hwpart)(struct flash_info_spl *fi, int copy);
};

/* Mark BOARD_CFG to tell U-Boot that we are running on Secondary SPL */
void fs_image_mark_secondary(void)
{
	struct fs_header_v1_0 *fsh = fs_image_get_cfg_addr();
	u8 *size = (u8 *)&fsh->info.file_size_low;

	/*
	 * We know that a BOARD-CFG is smaller than 64KiB. So only the first
	 * two bytes of the file_size are actually used. Especially the 8th
	 * byte is definitely 0. SPL uses this byte to pass the info if
	 * running from Primary (0) or Secondary SPL (<>0) to U-Boot. Please
	 * note that this info is not included in the CRC32 and signature, so
	 * U-Boot has to reset this byte to 0 before validating the BOARD-CFG.
	 */
	size[7] = 0xff;
}

/* Mark BOARD_CFG to tell U-Boot that we are running on Secondary UBoot */
void fs_image_mark_secondary_uboot(void)
{
	struct fs_header_v1_0 *fsh = fs_image_get_cfg_addr();
	u8 *size = (u8 *)&fsh->info.file_size_low;

	/*
	 * Like with fs_image_mark_secondary() we use a field in the BOARD-CFG
	 * to indicate that we boot an UBoot from the secondary partition.
	 */
	size[6] = 0xff;
}

/* Relocate dram_timing_info structure and initialize DRAM */
static int fs_image_init_dram(void)
{
	unsigned long *p;

	/* Before we can init DRAM, we have to init the board config */
	basic_init_callback(layout_name);

	/* The image starts with a pointer to the dram_timing variable */
	p = (unsigned long *)CONFIG_SPL_DRAM_TIMING_ADDR;

	return !fs_dram_init_common(p);
}

/* State machine: Load next header in sub-image */
static void fs_image_next_header(enum fsimg_state new_state)
{
	struct fsimg *fsimg = &fsimg_stack[nest_level];

	state = new_state;
	count = FSH_SIZE;
	if (count > fsimg->remaining) {
		/* Image too small, no room for F&S header, skip */
		count = fsimg->remaining;
		fsimg->remaining = 0;
		mode = FSIMG_MODE_SKIP;
	} else {
		/* Load F&S header */
		addr = &one_fsh;
		fsimg->remaining -= count;
		mode = FSIMG_MODE_HEADER;
	}
}

/* State machine: Enter a new sub-image with given size and load the header */
static void fs_image_enter(unsigned int size, enum fsimg_state new_state)
{
	fsimg_stack[nest_level].remaining -= size;
	fsimg_stack[++nest_level].remaining = size;
	fsimg_stack[nest_level].size = size;
	fs_image_next_header(new_state);
}

/* State machine: Load data of given size to given address, go to new state */
static void fs_image_copy(void *final, void *validate, unsigned int size,
			  bool keep)
{
	fsimg_stack[nest_level].remaining -= size;
	count = size;
	mode = FSIMG_MODE_IMAGE;
	keep_fs_header = keep;

#ifndef CONFIG_FS_SECURE_BOOT
	/* No CRC32 active in image, copy directly to final address */
	if (!(one_fsh.info.flags & (FSH_FLAGS_SECURE | FSH_FLAGS_CRC32))) {
		if (keep) {
			memcpy(final, &one_fsh, FSH_SIZE);
			final += FSH_SIZE;
		}
		validate_addr = NULL;
		debug("Loading 0x%x bytes directly to 0x%08lx\n", size,
		      (ulong)final);
		return;
	}
#endif

	/*
	 * The image is either signed or has CRC32. Load image to validation
	 * address first and copy to final address later after validation.
	 */
	final_addr = final;
	if (!validate)
		validate = final;
	validate_addr = validate;
	memcpy(validate, &one_fsh, FSH_SIZE);
	validate += FSH_SIZE;
	addr = validate;
	debug("Loading 0x%x bytes to temp 0x%08lx for validation\n",
	      size, (ulong)addr);
}

/* State machine: Skip data of given size */
static void fs_image_skip(unsigned int size)
{
	debug("%d: skip=0x%x, state=0x%x\n", nest_level, size, state);
	fsimg_stack[nest_level].remaining -= size;
	count = size;
	mode = FSIMG_MODE_SKIP;
}

/* State machine: Get the next FIRMWARE job */
static enum fsimg_state fs_image_get_fw_state(void)
{
	if (jobs & FSIMG_JOB_DRAM)
		return FSIMG_STATE_DRAM;
	if (jobs & FSIMG_JOB_ATF)
		return FSIMG_STATE_ATF;
	if (jobs & FSIMG_JOB_TEE)
		return FSIMG_STATE_TEE;

	return FSIMG_STATE_ANY;
}

/* State machine: Switch to next FIRMWARE state or skip remaining images */
static void fs_image_next_fw(void)
{
	enum fsimg_state next = fs_image_get_fw_state();

	if (next != FSIMG_STATE_ANY)
		fs_image_next_header(next);
	else {
		state = next;
		fs_image_skip(fsimg_stack[nest_level].remaining);
	}
}

/* Return if header matches and CRC32 over header is OK */
static int fs_image_match_check(const struct fs_header_v1_0 *fsh,
				const char *type, const char *descr)
{
	if (!fs_image_match(fsh, type, descr))
		return false;

	debug("Got %s (%s), CRC32 header only", type, descr);
	if ((fsh->info.flags & FSH_FLAGS_CRC32)
	    || (fs_image_check_crc32(fsh) < 0)) {
		printf("\nError: CRC32 of %s (%s) FAILED\n", type, descr);
		return false;
	}

	debug(" OK\n");
	return true;
}

static bool fs_image_validate_spl(const char *type, const char *descr)
{
	struct fs_header_v1_0 *fsh = validate_addr;
	unsigned int size;


	debug("Got %s (%s), ", type, descr);

#ifndef CONFIG_FS_SECURE_BOOT
	/* Image without CRC32 that was loaded directly to the final address */
	if (!fsh) {
		debug("no CRC32 check\n");
		return true;
	}
#endif

	size = fs_image_get_size(fsh, true);
	if (fs_image_is_signed(fsh)){
		debug("signed\n");
		if (!fs_image_is_valid_signature(fsh)) {
			printf("Error: Authentication of %s (%s) FAILED!\n",
			       type, descr);
			return false;
		}
		debug("Authentication OK\n");
		if (!keep_fs_header) {
			struct ivt *ivt = (struct ivt *)(fsh + 1);

			size = ivt->csf - ivt->self - HAB_HEADER;
			validate_addr += FSH_SIZE + HAB_HEADER;
		}
	} else {
		debug("unsigned");
#ifdef CONFIG_FS_SECURE_BOOT
		if (imx_hab_is_enabled()) {
			printf("\nError: Refusing unsigned %s on closed board\n",
			       type);
			return false;
		}
#endif

		switch (fs_image_check_crc32(validate_addr)) {
		case 0:
			debug(", no CRC32 (OK)\n");
			break;
			case 1:
				debug(", CRC32 header only OK\n");
				break;
		case 2:
			debug(", CRC32 image only OK\n");
			break;
		case 3:
			debug(", CRC32 header+image OK\n");
			break;
		default:
			printf("\nError: CRC32 of %s (%s) FAILED!\n",
			       type, descr);
			return false;
		}

		if (!keep_fs_header) {
			size -= FSH_SIZE;
			validate_addr += FSH_SIZE;
		}
	}

	/* Copy validated image to the final address */
	debug("Copy %s from temp 0x%08lx to final 0x%08lx size 0x%x\n", type,
	      (ulong)validate_addr, (ulong)final_addr, size);
	memcpy(final_addr, validate_addr, size);

	return true;
}

/* State machine: Loading the F&S header of the (sub-)image is done */
static void fs_image_handle_header(void)
{
	struct fsimg *fsimg = &fsimg_stack[nest_level];
	unsigned int size;
	const char *arch;
	bool handled = false;

	/* Check if magic is correct */
	if (!fs_image_is_fs_image(&one_fsh)) {
		/* This is no F&S header; skip remaining image */
		fs_image_skip(fsimg->remaining);
		return;
	}

	/* Get the image size (incuding padding) */
	size = fs_image_get_size(&one_fsh, false);

	/* Fill in size on topmost level, if we did not know it (NAND, MMC) */
	if (!nest_level && !fsimg->remaining)
		fsimg->remaining = size;

	debug("%d: Found %s, size=0x%x remaining=0x%x state=%d\n",
	      nest_level, one_fsh.type, size, fsimg->remaining, state);

	arch = fs_image_get_arch();
	switch (state) {
	case FSIMG_STATE_ANY:
		if (fs_image_match_check(&one_fsh, "BOARD-ID", NULL)) {
			/* Save ID and add job to load BOARD-CFG */
			fs_image_set_compare_id(one_fsh.param.descr);
			fs_image_set_board_id();
			jobs |= FSIMG_JOB_CFG;
			fs_image_enter(size, state);
			handled = true;
		} else if (fs_image_match(&one_fsh, "NBOOT", arch)) {
			/*
			 * NBOOT has a CRC32/signature that covers the whole
			 * image. in the state machine, we can only check
			 * validate single sub-images, do not validate this
			 * header. Simply enter the image, no further action.
			 */
			fs_image_enter(size, state);
			handled = true;
		} else if (fs_image_match_check(&one_fsh, "BOARD-INFO", arch)) {
			fs_image_enter(size, FSIMG_STATE_BOARD_CFG);
			handled = true;
		} else if (!(jobs & FSIMG_JOB_CFG)
			   && fs_image_match_check(&one_fsh, "FIRMWARE", arch)) {
			enum fsimg_state next = fs_image_get_fw_state();

			if (next != FSIMG_STATE_ANY) {
				fs_image_enter(size, next);
				handled = true;
			}
		}
		break;

	case FSIMG_STATE_BOARD_CFG:
		if (fs_image_match_board_id(&one_fsh)) {
			fs_image_copy(fs_image_get_cfg_addr(), NULL, size, true);
			handled = true;
		}
		break;

	case FSIMG_STATE_DRAM:
		/* Get DRAM type and DRAM timing from BOARD-CFG */
		if (fs_image_match_check(&one_fsh, "DRAM-INFO", arch)) {
			void *fdt = fs_image_get_cfg_fdt();
			int offs = fs_image_get_board_cfg_offs(fdt);
			int rev_offs;

			rev_offs = fs_image_get_board_rev_subnode(fdt, offs);

			ram_type = fs_image_getprop(fdt, offs, rev_offs,
						    "dram-type", NULL);
			ram_timing = fs_image_getprop(fdt, offs, rev_offs,
						      "dram-timing", NULL);

			debug("Looking for: %s, %s\n", ram_type, ram_timing);

			fs_image_enter(size, FSIMG_STATE_DRAM_TYPE);
			handled = true;
		}
		break;

	case FSIMG_STATE_DRAM_TYPE:
		if (fs_image_match_check(&one_fsh, "DRAM-TYPE", ram_type)) {
#ifdef CONFIG_IMX8
			fs_image_enter(size, FSIMG_STATE_DRAM_TIMING);
#else
			fs_image_enter(size, FSIMG_STATE_DRAM_FW);
#endif
			handled = true;
		}
		break;

	case FSIMG_STATE_DRAM_FW:
		/* Load DDR training firmware behind SPL code */
		if (fs_image_match(&one_fsh, "DRAM-FW", ram_type)) {
			fs_image_copy(&_end, (void *)CONFIG_SPL_ATF_ADDR,
				      size, false);
			handled = true;
		}
		break;

	case FSIMG_STATE_DRAM_TIMING:
		if (fs_image_match(&one_fsh, "DRAM-TIMING", ram_timing)) {
			fs_image_copy((void *)CONFIG_SPL_DRAM_TIMING_ADDR,
				      (void *)CONFIG_SPL_ATF_ADDR, size, false);
			handled = true;
		}
		break;

	case FSIMG_STATE_ATF:
		if (fs_image_match(&one_fsh, "ATF", arch)) {
			fs_image_copy((void *)CONFIG_SPL_ATF_ADDR, NULL,
				      size, false);
			handled = true;
		}
		break;

	case FSIMG_STATE_TEE:
		if (fs_image_match(&one_fsh, "TEE", arch)) {
			fs_image_copy((void *)CONFIG_SPL_TEE_ADDR, NULL,
				      size, false);
			handled = true;
		}
		break;
	}

	/* Skip the image if it could not be handled */
	if (!handled)
		fs_image_skip(size);
}

/* State machine: Loading the data part of a sub-image is complete */
static void fs_image_handle_image(void)
{
	struct fsimg *fsimg = &fsimg_stack[nest_level];

	switch (state) {
	case FSIMG_STATE_ANY:
	case FSIMG_STATE_DRAM:
	case FSIMG_STATE_DRAM_TYPE:
		/* Should not happen, we do not load these images */
		break;

	case FSIMG_STATE_BOARD_CFG:
		/* If BOARD-CFG is OK, set BOARD-ID and mark job as done */
		if (fs_image_validate_spl("BOARD-CFG", board_id)) {
			fs_image_board_cfg_set_board_rev(
				fs_image_get_cfg_addr());
			jobs &= ~FSIMG_JOB_CFG;
		}

		/* Skip remaining configs */
		fs_image_skip(fsimg->remaining);
		break;

	case FSIMG_STATE_DRAM_FW:
		/* If DRAM training firmware is OK, look for DRAM timing next */
		if (fs_image_validate_spl("DRAM-FW", ram_type))
			fs_image_next_header(FSIMG_STATE_DRAM_TIMING);
		else
			fs_image_skip(fsimg->remaining);
		break;

	case FSIMG_STATE_DRAM_TIMING:
		/* If DRAM timing is OK, start DRAM and mark job as done */
		if (fs_image_validate_spl("DRAM-TIMING", ram_timing)) {
			if (fs_image_init_dram()) {
				debug("Init DDR succeeded\n");
				jobs &= ~FSIMG_JOB_DRAM;
			} else
				debug("Init DDR failed\n");
		}

		/* Skip remaining DRAM timings */
		fs_image_skip(fsimg->remaining);
		break;

	case FSIMG_STATE_ATF:
		/* If ATF is OK, marks job as done */
		if (fs_image_validate_spl("ATF", fs_image_get_arch()))
			jobs &= ~FSIMG_JOB_ATF;
		fs_image_next_fw();
		break;

	case FSIMG_STATE_TEE:
		/* If TEE is OK, mark job as done */
		if (fs_image_validate_spl("TEE", fs_image_get_arch()))
			jobs &= ~FSIMG_JOB_TEE;
		fs_image_next_fw();
		break;
	}
}

/* State machine: Skipping a part of a sub-image is complete */
static void fs_image_handle_skip(void)
{
	struct fsimg *fsimg = &fsimg_stack[nest_level];

	if (fsimg->remaining) {
		debug("%d: skip: remaining=0x%x state=0x%x\n",
		      nest_level, fsimg->remaining, state);
		fs_image_next_header(state);
		return;
	}

	if (!nest_level) {
		mode = FSIMG_MODE_DONE;
		return;
	}

	nest_level--;
	fsimg--;

	switch (state) {
	case FSIMG_STATE_ANY:
		fs_image_next_header(state);
		break;

	case FSIMG_STATE_BOARD_CFG:
		fs_image_next_header(FSIMG_STATE_ANY);
		break;

	case FSIMG_STATE_DRAM_TYPE:
		fs_image_next_fw();
		break;

	case FSIMG_STATE_DRAM_FW:
	case FSIMG_STATE_DRAM_TIMING:
		state = FSIMG_STATE_DRAM_TYPE;
		fs_image_skip(fsimg->remaining);
		break;

	case FSIMG_STATE_DRAM:
	case FSIMG_STATE_ATF:
	case FSIMG_STATE_TEE:
		fs_image_next_header(FSIMG_STATE_ANY);
		break;
	}
}

/* State machine: Handle the next part of the image when data is loaded */
static void fs_image_handle(void)
{
	switch (mode) {
	case FSIMG_MODE_HEADER:
		fs_image_handle_header();
		break;

	case FSIMG_MODE_IMAGE:
		fs_image_handle_image();
		break;

	case FSIMG_MODE_SKIP:
		fs_image_handle_skip();
		break;

	case FSIMG_MODE_DONE:
		/* Should not happen, caller has to drop all incoming data
		   when mode is FSIMG_MODE_DONE */
		break;
	}
}

/* Start state machine for a new F&S image */
static void fs_image_start(unsigned int size, unsigned int jobs_todo,
			   basic_init_t basic_init, const char *layout)
{
	jobs = jobs_todo;
	basic_init_callback = basic_init;
	layout_name = layout;
	nest_level = -1;
	fs_image_enter(size, FSIMG_STATE_ANY);
}

/* Handle a chunk of data that was received via SDP on USB */
static void fs_image_sdp_rx_data(u8 *data_buf, int data_len)
{
	unsigned int chunk;

	/* We have data_len bytes, we need count bytes (which may be zero) */
	while ((data_len > 0) && (mode != FSIMG_MODE_DONE)) {
		chunk = min((unsigned int)data_len, count);
		if ((mode == FSIMG_MODE_IMAGE) || (mode == FSIMG_MODE_HEADER))
			memcpy(addr, data_buf, chunk);

		addr += chunk;
		data_buf += chunk;
		data_len -= chunk;
		count -= chunk;

		/* The next block for the interpreter is loaded, process it */
		while (!count && (mode != FSIMG_MODE_DONE))
			fs_image_handle();
	}
}

/* This is called when the SDP protocol starts a new file */
static void fs_image_sdp_new_file(u32 dnl_address, u32 size)
{
	fs_image_start(size, jobs, basic_init_callback, NULL);
}

static const struct sdp_stream_ops fs_image_sdp_stream_ops = {
	.new_file = fs_image_sdp_new_file,
	.rx_data = fs_image_sdp_rx_data,
};

/* Load FIRMWARE and optionally BOARD-CFG via SDP from USB */
void fs_image_all_sdp(bool need_cfg, basic_init_t basic_init)
{
	unsigned int jobs_todo = FSIMG_FW_JOBS;

	if (need_cfg)
		jobs |= FSIMG_JOB_CFG;

	jobs = jobs_todo;
	basic_init_callback = basic_init;

	/* Stream the file and load appropriate parts */
	spl_sdp_stream_image(&fs_image_sdp_stream_ops, true);

	/* Stream until a valid NBoot with all jobs was downloaded */
	while (jobs) {
		debug("Jobs not done: 0x%x\n", jobs);
		jobs = jobs_todo;
		spl_sdp_stream_continue(&fs_image_sdp_stream_ops, true);
	};
}

#ifdef CONFIG_NAND_MXS
/* Switch hwpart on NAND */
static int fs_image_set_hwpart_nand(struct flash_info_spl *fi, int copy)
{
	/* Nothing to do */
	return 0;
}

/* Get flash_info with NAND specific info */
static int fs_image_get_flash_info_spl_nand(struct flash_info_spl *fi)
{
	/* Set offsets for the two copies */
	fi->offs[0] = CONFIG_FUS_BOARDCFG_NAND0;
	fi->offs[1] = CONFIG_FUS_BOARDCFG_NAND1;

	/* Set access functions */
	fi->load = nand_spl_load_image;
	fi->set_hwpart = fs_image_set_hwpart_nand;

	fi->layout = "nand";

	return 0;
}
#endif /* CONFIG_NAND_MXS */

#ifdef CONFIG_MMC
/* Load MMC data from arbitrary offsets, not necessarily MMC block aligned */
static int fs_image_gen_load_mmc(uint32_t offs, unsigned int size, void *buf)
{
	unsigned long n;
	unsigned int chunk_offs;
	unsigned int chunk_size;
	static u8 *local_buffer;	/* Space for one MMC block */
	struct mmc *mmc;
	struct blk_desc *blk_desc;
	unsigned long blksz;
//###	int err;

	mmc = find_mmc_device(0);
#if 0 //### we can skip initialization, already done in fs_image_load_system()
	if (!mmc) {
		puts("MMC boot device not found\n");
		return -ENODEV;
	}
	err = mmc_init(mmc);
	if (err) {
		printf("mmc_init() failed (%d)\n", err);
		return err;
	}
#endif
	blk_desc = mmc_get_blk_desc(mmc);
	blksz = blk_desc->blksz;

	/* We need a buffer for one MMC block; only allocate once */
	if (!local_buffer) {
		local_buffer = malloc(blksz);
		if (!local_buffer) {
			puts("Can not allocate local buffer for MMC\n");
			return -ENOMEM;
		}
	}

	chunk_offs = offs % blksz;
	offs /= blksz;			/* From now on offs is in MMC blocks */

	/*
	 * If not starting from an MMC block boundary, load one block to local
	 * buffer and take some bytes from the end to get aligned.
	 */
	if (chunk_offs) {
		chunk_size = blksz - chunk_offs;
		if (chunk_size > size)
			chunk_size = size;
		n = blk_dread(blk_desc, offs, 1, local_buffer);
		if (IS_ERR_VALUE(n))
			return (int)n;
		if (n < 1)
			return -EIO;
		memcpy(buf, local_buffer + chunk_offs, chunk_size);
		offs++;
		buf += chunk_size;
		size -= chunk_size;
	}

	/*
	 * Load full blocks directly to target address. This assumes that buf
	 * is 32 bit aligned all the time. Our F&S images are always padded to
	 * 16 bytes, so this should be no problem.
	 */
	if (size >= blksz) {
		if ((unsigned long)buf & 3)
			puts("### Aaargh! buf not 32-bit aligned!\n");
		chunk_size = size / blksz;
		n = blk_dread(blk_desc, offs, chunk_size, buf);
		if (IS_ERR_VALUE(n))
			return (int)n;
		if (n < chunk_size)
			return -EIO;
		offs += chunk_size;
		chunk_size *= blksz;
		buf += chunk_size;
		size -= chunk_size;
	}

	/*
	 * If there are some bytes remaining, load one block to local buffer
	 * and take these bytes from there.
	 */
	if (size > 0) {
		n = blk_dread(blk_desc, offs, 1, local_buffer);
		if (IS_ERR_VALUE(n))
			return (int)n;
		if (n < 1)
			return -EIO;
		memcpy(buf, local_buffer, size);
	}

	return 0;
}

/* Switch hwpart on MMC */
static int fs_image_set_hwpart_mmc(struct flash_info_spl *fi, int copy)
{
	int err;
	u8 hwpart = fi->hwpart[copy];

	err = blk_dselect_hwpart(fi->blk_desc, hwpart);
	if (err)
		printf("Cannot switch to hwpart %d\n", hwpart);

	return err;
}

/* Get flash_info with MMC specific info */
static int fs_image_get_flash_info_spl_mmc(struct flash_info_spl *fi)
{
	u8 boot_hwpart;

	/* Start mmc device once */
	fi->blk_desc = blk_get_devnum_by_type(IF_TYPE_MMC, 0);
	if (!fi->blk_desc) {
		printf("Cannot start MMC boot device\n");
		return -ENODEV;
	}
	fi->mmc = find_mmc_device(fi->blk_desc->devnum);
	if (!fi->mmc) {
		printf("Cannot find MMC device\n");
		return -ENODEV;
	}

	/* Determine hwpart that we boot from */
	boot_hwpart = EXT_CSD_EXTRACT_BOOT_PART(fi->mmc->part_config);
	if (boot_hwpart > 2)
		boot_hwpart = 0;

	/* Set offsets and hwparts for the two copies */
	fi->offs[0] = CONFIG_FUS_BOARDCFG_MMC0;
	fi->offs[1] = boot_hwpart ? fi->offs[0] : CONFIG_FUS_BOARDCFG_MMC1;
	fi->hwpart[0] = boot_hwpart;
#ifndef CONFIG_IMX8MM
	if (boot_hwpart)
		boot_hwpart = 3 - boot_hwpart;
#else
	fi->hwpart[1] = boot_hwpart;
#endif

	/* Set access functions */
	fi->load = fs_image_gen_load_mmc;
	fi->set_hwpart = fs_image_set_hwpart_mmc;

	fi->layout = boot_hwpart ? "emmc-boot" : "sd-user";

	return 0;
}
#endif /* CONFIG_MMC */

/* Load FIRMWARE from MMC/NAND using state machine */
static int fs_image_loop(struct flash_info_spl *fi, struct fs_header_v1_0 *cfg,
			 unsigned int start)
{
	int err;
	unsigned int end;
	void *fdt;
	int offs;
	unsigned int board_cfg_size, nboot_size;

	fdt = fs_image_find_cfg_fdt(cfg);
	if (!fdt)
		return -EINVAL;

	offs = fs_image_get_nboot_info_offs(fdt);
	if (offs < 0)
		return -EINVAL;

        board_cfg_size = fdt_getprop_u32_default_node(fdt, offs, 0,
						      "board-cfg-size", 0);
	if (!board_cfg_size)
		board_cfg_size = fs_image_get_size(cfg, true);

	/* Go to the layout subnode */
	offs = fdt_subnode_offset(fdt, offs, fi->layout);
	if (offs < 0)
		return -EINVAL;

	nboot_size = fdt_getprop_u32_default_node(fdt, offs, 0, "nboot-size", 0);
	if (!nboot_size)
		return -EINVAL;

	end = start + nboot_size;
	start += board_cfg_size;

	/*
	 * ### TODO: Handle (=skip) bad blocks in case of NAND (if load ==
	 * nand_spl_load_image) Basic idea: nand_spl_load_image() already
	 * skips bad blocks. So when incrementing offs, check the region for
	 * bad blocks and increase offs again according to the number of
	 * bad blocks in the region. Problem: we do not have info about NAND
	 * here like block sizes, so maybe have such a function in NAND driver.
	 */
	do {
		if (count) {
			if ((mode == FSIMG_MODE_IMAGE)
			    || (mode == FSIMG_MODE_HEADER)) {
				if (start + count >= end)
					return -EFBIG;
				err = fi->load(start, count, addr);
				if (err)
					return err;
				addr += count;
			}
			start += count;
		}
		fs_image_handle();
	} while (mode != FSIMG_MODE_DONE);

	return 0;
}

/* Load the BOARD-CFG and (if basic_init is not NULL) the FIRMWARE */
int fs_image_load_system(enum boot_device boot_dev, bool secondary,
			 basic_init_t basic_init)
{
	int copy, start_copy;
	struct flash_info_spl fi;

	unsigned int start;
	void *cfg = fs_image_get_cfg_addr();
	int err;

	switch (boot_dev) {
#ifdef CONFIG_NAND_MXS
	case NAND_BOOT:
		err = fs_image_get_flash_info_spl_nand(&fi);
		break;
#endif
#ifdef CONFIG_MMC
	case MMC1_BOOT:
	case MMC3_BOOT:
		err = fs_image_get_flash_info_spl_mmc(&fi);
		break;
#endif
	default:
		return -ENODEV;
	}
	if (err)
		return err;

	/* Try both copies (second copy first if running on secondary SPL) */
	start_copy = secondary ? 1 : 0;
	copy = start_copy;
	do {
		start = fi.offs[copy];

		/* Load BOARD-CFG to OCRAM (normal load) and validate */
		if (!fi.set_hwpart(&fi, copy)
		    && !fi.load(start, FSH_SIZE, &one_fsh)
		    && fs_image_match(&one_fsh, "BOARD-CFG", NULL)
		    && !fi.load(start, fs_image_get_size(&one_fsh, true), cfg)
		    && fs_image_is_ocram_cfg_valid())
 		{
			/* BOARD-CFG successfully loaded */
			fs_image_set_board_id_from_cfg();
			debug("Got valid BOARD-CFG from flash\n");

			/* basic_init == NULL means only load BOARD-CFG */
			if (!basic_init)
				return 0;

			/* Try to load FIRMWARE (with state machine) */
			fs_image_start(FSH_SIZE, FSIMG_FW_JOBS, basic_init,
				       fi.layout);
			if (!fs_image_loop(&fi, cfg, start) && !jobs)
				return 0;
		}

		/* No, did not work, try other copy. */
		copy = 1 - copy;
	} while (copy != start_copy);

	return -ENOENT;
}

#endif /* CONFIG_SPL_BUILD */
