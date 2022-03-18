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
 *   | BOARD-ID (id) (optional)                   |
 *   |   +----------------------------------------+
 *   |   | NBOOT (arch)                           |
 *   |   |   +------------------------------------+
 *   |   |   | SPL (arch)                         |
 *   |   |   +------------------------------------+
 *   |   |   | BOARD-CONFIGS (arch)               |
 *   |   |   |   +--------------------------------+
 *   |   |   |   | BOARD-CFG (id)                 |
 *   |   |   |   +--------------------------------+
 *   |   |   |   | BOARD-CFG (id)                 |
 *   |   |   |   +--------------------------------+
 *   |   |   |   | ...                            |
 *   |   |   |---+--------------------------------+
 *   |   |   | FIRMWARE (arch)                    |
 *   |   |   |   +--------------------------------+
 *   |   |   |   | DRAM-SETTINGS (arch)           |
 *   |   |   |   |   +----------------------------+
 *   |   |   |   |   | DRAM-TYPE (DDR3L)          |
 *   |   |   |   |   |   +------------------------+
 *   |   |   |   |   |   | DRAM-FW (DDR3L)        |
 *   |   |   |   |   |   +------------------------+
 *   |   |   |   |   |   | DRAM-TIMING (ram-chip) |
 *   |   |   |   |   |   +------------------------+
 *   |   |   |   |   |   | DRAM-TIMING (ram-chip) |
 *   |   |   |   |   |   +------------------------+
 *   |   |   |   |   |   | ...                    |
 *   |   |   |   |   +---+------------------------+
 *   |   |   |   |   | DRAM-TYPE (DDR4)           |
 *   |   |   |   |   |   +------------------------+
 *   |   |   |   |   |   | DRAM-FW (DDR4)         |
 *   |   |   |   |   |   +------------------------+
 *   |   |   |   |   |   | DRAM-TIMING (ram-chip) |
 *   |   |   |   |   |   +------------------------+
 *   |   |   |   |   |   | DRAM-TIMING (ram-chip) |
 *   |   |   |   |   |   +------------------------+
 *   |   |   |   |   |   | ...                    |
 *   |   |   |   |   +---+------------------------+
 *   |   |   |   |   | DRAM-TYPE (LPDDR4)         |
 *   |   |   |   |   |   +------------------------+
 *   |   |   |   |   |   | DRAM-FW (LPDDR4)       |
 *   |   |   |   |   |   +------------------------+
 *   |   |   |   |   |   | DRAM-TIMING (ram-chip) |
 *   |   |   |   |   |   +------------------------+
 *   |   |   |   |   |   | DRAM-TIMING (ram-chip) |
 *   |   |   |   |   |   +------------------------+
 *   |   |   |   |   |   | ...                    |
 *   |   |   |   +---+---+------------------------+
 *   |   |   |   | ATF (arch)                     |
 *   |   |   |   +--------------------------------+
 *   |   |   |   | TEE (arch) (optional)          |
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
#include <spl.h>
#include <mmc.h>
#include <nand.h>
#include <sdp.h>
#include <asm/arch/ddr.h>
#include <asm/sections.h>

#include "fs_board_common.h"		/* fs_board_*() */
#include "fs_image_common.h"		/* Own interface */

/* Structure to handle board name and revision separately */
struct bnr {
	char name[MAX_DESCR_LEN];
	unsigned int rev;
};

static struct bnr compare_bnr;		/* Used for BOARD-ID comparisons */

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

/* Return the address of the board configuration in OCRAM */
void *fs_image_get_cfg_addr(bool with_fs_header)
{
	void *cfg = (void*)CONFIG_FUS_BOARDCFG_ADDR;

	if (!with_fs_header)
		cfg += FSH_SIZE;

	return cfg;
}

/* Return the address of the /nboot-info node */
int fs_image_get_info_offs(void *fdt)
{
	return fdt_path_offset(fdt, "/nboot-info");
}

/* Return the address of the /board-cfg node */
int fs_image_get_cfg_offs(void *fdt)
{
	return fdt_path_offset(fdt, "/board-cfg");
}

static int fs_image_invalid_nboot_info(const char *name)
{
	printf("Missing or invalid entry /nboot-info/%s in BOARD-CFG\n", name);

	return -EINVAL;
}

static int fs_image_get_storage_size(void *fdt, int offs, unsigned int align,
				     unsigned int *size, const char *prefix)
{
	char name[20];

	sprintf(name, "%s-size", prefix);
	*size = fdt_getprop_u32_default_node(fdt, offs, 0, name, 0);
	if (!*size)
		return fs_image_invalid_nboot_info(name);

	if (align && (*size % align))
		return fs_image_invalid_nboot_info(name);

	return 0;
}

/* Get the board-cfg-size from nboot-info */
int fs_image_get_board_cfg_size(void *fdt, int offs, unsigned int align,
				unsigned int *size)
{
	return fs_image_get_storage_size(fdt, offs, align, size, "board-cfg");
}

/* Return start and size from nboot-info for entries beginning with prefix */
static int fs_image_get_storage_info(void *fdt, int offs, unsigned int align,
				    struct storage_info *si, const char *prefix)
{
	char name[20];
	int len;
	int i;

	si->count = 0;
	sprintf(name, "%s-start", prefix);
	si->start = fdt_getprop(fdt, offs, name, &len);
	if (!si->start || !len || (len % sizeof(fdt32_t)))
		return fs_image_invalid_nboot_info(name);
	si->count = len / sizeof(fdt32_t);

	if (align) {
		for (i = 0; i < si->count; i++) {
			if (fdt32_to_cpu(si->start[i]) % align)
				return fs_image_invalid_nboot_info(name);
		}
	}

	return fs_image_get_storage_size(fdt, offs, align, &si->size, prefix);
}

/* Get nboot-start and nboot-size values from nboot-info */
int fs_image_get_nboot_info(void *fdt, int offs, unsigned int align,
			    struct storage_info *si)
{
	return fs_image_get_storage_info(fdt, offs, align, si, "nboot");
}

/* Get spl-start and spl-size values from nboot-info */
int fs_image_get_spl_info(void *fdt, int offs, unsigned int align,
			  struct storage_info *si)
{
	return fs_image_get_storage_info(fdt, offs, align, si, "spl");
}

/* Get uboot-start and uboot-size values from nboot-info */
int fs_image_get_uboot_info(void *fdt, int offs, unsigned int align,
			    struct storage_info *si)
{
	return fs_image_get_storage_info(fdt, offs, align, si, "uboot");
}


/* Return pointer to string with NBoot version */
const char *fs_image_get_nboot_version(void *fdt)
{
	int offs;

	if (!fdt)
		fdt = fs_image_get_cfg_addr(false);

	offs = fs_image_get_info_offs(fdt);
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

static void fs_image_get_board_name_rev(const char *id, struct bnr *bnr)
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

	bnr->name[rev] = 0;
	while (++rev < i)
		bnr->rev = bnr->rev * 10 + bnr->name[rev] - '0';
}

/* Check id, return also true if revision is less than revision of compare id */
bool fs_image_match_board_id(struct fs_header_v1_0 *fsh, const char *type)
{
	struct bnr bnr;

	/* Compare magic and type */
	if (!fs_image_match(fsh, type, NULL))
		return false;

	/* A config must include a description, this is the board ID */
	if (!(fsh->info.flags & FSH_FLAGS_DESCR))
		return false;

	/* Split board ID of the config we look at into name and rev */
	fs_image_get_board_name_rev(fsh->param.descr, &bnr);

	/* Compare with name and rev of the board we are running on */
	if (strncmp(bnr.name, compare_bnr.name, sizeof(bnr.name)))
		return false;
	if (bnr.rev > compare_bnr.rev)
		return false;

	return true;
}

/* Set the compare id that will used in fs_image_match_board_id() */
void fs_image_set_board_id_compare(const char *id)
{
	fs_image_get_board_name_rev(id, &compare_bnr);
}


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

/* Function to load a chunk of data frome either NAND or MMC */
typedef int (*load_function_t)(uint32_t offs, unsigned int size, void *buf);

static enum fsimg_mode mode;
static unsigned int count;
static void *addr;
static unsigned int jobs;
static int nest_level;
static const char *ram_type;
static const char *ram_timing;
static basic_init_t basic_init_callback;
static char board_id[MAX_DESCR_LEN];
static struct fs_header_v1_0 one_fsh;	/* Buffer for one F&S header */

#define MAX_NEST_LEVEL 8

static struct fsimg {
	unsigned int size;		/* Size of the main image data */
	unsigned int remaining;		/* Remaining bytes in this level */
} fsimg_stack[MAX_NEST_LEVEL];

#define reloc(addr, offs) addr = ((void *)addr + (unsigned long)offs)

/* Relocate dram_timing_info structure and initialize DRAM */
static int fs_image_init_dram(void)
{
	struct dram_timing_info *dti;
	unsigned long *p;

	/* Before we can init DRAM, we have to init the board config */
	basic_init_callback();

	/* The image starts with a pointer to the dram_timing variable */
	p = (unsigned long *)CONFIG_SPL_DRAM_TIMING_ADDR;
	dti = (struct dram_timing_info *)*p;

	return !ddr_init(dti);
}

/* Store board_id and split into name and revision to ease board_id matching */
static void fs_image_set_board_id(const char id[MAX_DESCR_LEN])
{
	memcpy(board_id, id, sizeof(board_id));
	fs_image_set_board_id_compare(id);
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
static void fs_image_copy(void *buf, unsigned int size)
{
	fsimg_stack[nest_level].remaining -= size;
	count = size;
	addr = buf;
	mode = FSIMG_MODE_IMAGE;
}

/* State machine: Skip data of given size */
static void fs_image_skip(unsigned int size)
{
	debug("%d: skip=0x%x, state=0x%x\n", nest_level, size, state);
	fsimg_stack[nest_level].remaining -= size;
	count = size;
	mode = FSIMG_MODE_SKIP;
}

/* State machine: If match, load, otherwise skip this sub-image */
static void fs_image_copy_or_skip(struct fs_header_v1_0 *fsh,
				  const char *type, const char *descr,
				  void *addr, unsigned int size)
{
	if (fs_image_match(fsh, type, descr))
		fs_image_copy(addr, size);
	else
		fs_image_skip(size);
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

/* State machine: Loading the F&S header of the (sub-)image is done */
static void fs_image_handle_header(void)
{
	struct fsimg *fsimg = &fsimg_stack[nest_level];
	unsigned int size;
	const char *arch;

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
		if (fs_image_match(&one_fsh, "BOARD-ID", NULL)) {
			/* Save ID and add job to load BOARD-CFG */
			fs_image_set_board_id(one_fsh.param.descr);
			jobs |= FSIMG_JOB_CFG;
			fs_image_enter(size, state);
			break;
		} else if (fs_image_match(&one_fsh, "NBOOT", arch)) {
			/* Simply enter image, no further action */
			fs_image_enter(size, state);
			break;
		} else if (fs_image_match(&one_fsh, "BOARD-CONFIGS", arch)) {
			fs_image_enter(size, FSIMG_STATE_BOARD_CFG);
			break;
		} else if (fs_image_match(&one_fsh, "FIRMWARE", arch)
			   && !(jobs & FSIMG_JOB_CFG)) {
			enum fsimg_state next = fs_image_get_fw_state();

			if (next != FSIMG_STATE_ANY) {
				fs_image_enter(size, next);
				break;
			}
		}

		/* Skip unknown or unneeded images */
		fs_image_skip(size);
		break;

	case FSIMG_STATE_BOARD_CFG:
		if (fs_image_match_board_id(&one_fsh, "BOARD-CFG")) {
			memcpy(fs_image_get_cfg_addr(true), &one_fsh, FSH_SIZE);
			fs_image_copy(fs_image_get_cfg_addr(false), size);
		} else
			fs_image_skip(size);
		break;

	case FSIMG_STATE_DRAM:
		/* Get DRAM type and DRAM timing from BOARD-CFG */
		if (fs_image_match(&one_fsh, "DRAM-SETTINGS", arch)) {
			void *fdt = fs_image_get_cfg_addr(false);
			int off = fs_image_get_cfg_offs(fdt);

			ram_type = fdt_getprop(fdt, off, "dram-type", NULL);
			ram_timing = fdt_getprop(fdt, off, "dram-timing", NULL);

			debug("Looking for: %s, %s\n", ram_type, ram_timing);

			fs_image_enter(size, FSIMG_STATE_DRAM_TYPE);
		} else
			fs_image_skip(size);
		break;

	case FSIMG_STATE_DRAM_TYPE:
		if (fs_image_match(&one_fsh, "DRAM-TYPE", ram_type))
			fs_image_enter(size, FSIMG_STATE_DRAM_FW);
		else
			fs_image_skip(size);
		break;

	case FSIMG_STATE_DRAM_FW:
		/* Load DDR training firmware behind SPL code */
		fs_image_copy_or_skip(&one_fsh, "DRAM-FW", ram_type,
				      &_end, size);
		break;

	case FSIMG_STATE_DRAM_TIMING:
		/* This may overlap ATF */
		fs_image_copy_or_skip(&one_fsh, "DRAM-TIMING", ram_timing,
				      (void *)CONFIG_SPL_DRAM_TIMING_ADDR,
				      size);
		break;

	case FSIMG_STATE_ATF:
		fs_image_copy_or_skip(&one_fsh, "ATF", arch,
				      (void *)CONFIG_SPL_ATF_ADDR, size);
		break;

	case FSIMG_STATE_TEE:
		fs_image_copy_or_skip(&one_fsh, "TEE", arch,
				      (void *)CONFIG_SPL_TEE_ADDR, size);
		break;
	}
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
		/* We have our config, skip remaining configs */
		debug("Got BOARD-CFG, ID=%s\n", board_id);
		jobs &= ~FSIMG_JOB_CFG;
		fs_image_skip(fsimg->remaining);
		break;

	case FSIMG_STATE_DRAM_FW:
		/* DRAM training firmware loaded, now look for DRAM timing */
		debug("Got DRAM-FW (%s)\n", ram_type);
		fs_image_next_header(FSIMG_STATE_DRAM_TIMING);
		break;

	case FSIMG_STATE_DRAM_TIMING:
		/* DRAM info complete, start it; job done if successful */
		debug("Got DRAM-TIMING (%s)\n", ram_timing);
		if (fs_image_init_dram())
			jobs &= ~FSIMG_JOB_DRAM;
		else
			debug("Init DDR failed\n");

		/* Skip remaining DRAM timings */
		fs_image_skip(fsimg->remaining);
		break;

	case FSIMG_STATE_ATF:
		/* ATF loaded, job done */
		debug("Got ATF\n");
		jobs &= ~FSIMG_JOB_ATF;
		fs_image_next_fw();
		break;

	case FSIMG_STATE_TEE:
		/* TEE loaded, job done */
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
			   basic_init_t basic_init)
{
	jobs = jobs_todo;
	basic_init_callback = basic_init;
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
	fs_image_start(size, jobs, basic_init_callback);
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
	int err;
	unsigned int cur_part, boot_part;

	mmc = find_mmc_device(0);
	if (!mmc) {
		puts("MMC boot device not found\n");
		return -ENODEV;
	}
	err = mmc_init(mmc);
	if (err) {
		printf("mmc_init() failed (%d)\n", err);
		return err;
	}
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

	/* Select partition where system boots from */
	cur_part = blk_desc->hwpart;
	boot_part = (mmc->part_config >> 3) & PART_ACCESS_MASK;
	if (boot_part == 7)
		boot_part = 0;
	if (cur_part != boot_part) {
		err = blk_dselect_hwpart(blk_desc, boot_part);
		if (err) {
			printf("Cannot switch to part %d on mmc0\n", boot_part);
			return err;
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

#endif /* CONFIG_MMC */

/* Load FIRMWARE from NAND using state machine */
static int fs_image_loop(struct fs_header_v1_0 *cfg, unsigned int start,
			 load_function_t load)
{
	int err;
	unsigned int end;
	void *fdt = cfg + 1;
	int offs = fs_image_get_info_offs(fdt);
	unsigned int board_cfg_size, nboot_size;

	if (!fdt)
		return -EINVAL;
	err = fs_image_get_board_cfg_size(fdt, offs, 0, &board_cfg_size);
	if (err)
		return err;
	err = fs_image_get_storage_size(fdt, offs, 0, &nboot_size, "nboot");
	if (err)
		return err;

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
				err = load(start, count, addr);
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
	bool copy = secondary;
	load_function_t load;
	unsigned int offs[2];
	unsigned int start;
	void *target = fs_image_get_cfg_addr(true);

	switch (boot_dev) {
#ifdef CONFIG_NAND_MXS
	case NAND_BOOT:
		offs[0] = CONFIG_FUS_BOARDCFG_NAND0;
		offs[1] = CONFIG_FUS_BOARDCFG_NAND1;
		load = nand_spl_load_image;
		break;
#endif
#ifdef CONFIG_MMC
	case MMC3_BOOT:
		offs[0] = CONFIG_FUS_BOARDCFG_MMC0;
		offs[1] = CONFIG_FUS_BOARDCFG_MMC1;
		load = fs_image_gen_load_mmc;
		break;
#endif
	default:
		return -ENODEV;
	}

	/* Try both copies (second copy first if running on secondary SPL) */
	do {
		start = copy ? offs[1] : offs[0];

		/* Try to load BOARD-CFG (normal load) */
		if (!load(start, FSH_SIZE, &one_fsh)
		    && (fs_image_match(&one_fsh, "BOARD-CFG", NULL))
		    && !load(start, fs_image_get_size(&one_fsh, true), target))
		{
			/* basic_init == NULL means only load BOARD-CFG */
			if (!basic_init)
				return 0;

			/* Try to load FIRMWARE (with state machine) */
			fs_image_start(FSH_SIZE, FSIMG_FW_JOBS, basic_init);
			if (!fs_image_loop(target, start, load) && !jobs)
				return 0;
		}

		/* No, did not work, try other copy */
		copy = !copy;
	} while (copy != secondary);

	return -ENOENT;
}

#endif /* CONFIG_SPL_BUILD */
