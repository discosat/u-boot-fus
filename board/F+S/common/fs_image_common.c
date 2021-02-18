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
 * configuration does not work for some reason, it is necessary to use the USP
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
 *   |   | NBoot (arch)                           |
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
 *   from the DRAM settings only the specific setting of the specific board
 *   would be needed, but it may bee too complicated to extract this part only
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
 * 2. Initialize DDR
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
#include <asm/arch/imx8m_ddr.h>
#include <asm/sections.h>

#include "fs_image_common.h"		/* Own interface */

/* F&S header (V0.0) for a generic file */
struct fs_header_v0_0 {			/* Size: 16 Bytes */
	char magic[4];			/* "FS" + two bytes operating system
					   (e.g. "LX" for Linux) */
	u32 file_size_low;		/* Image size [31:0] */
	u32 file_size_high;		/* Image size [63:32] */
	u16 flags;			/* See flags below */
	u8 padsize;			/* Number of padded bytes at end */
	u8 version;			/* Header version x.y:
					   [7:4] major x, [3:0] minor y */
};

/* F&S header (V1.0) for a generic file */
struct fs_header_v1_0 {			/* Size: 64 bytes */
	struct fs_header_v0_0 info;	/* Image info, see above */
	char type[16];			/* Image type, e.g. "U-BOOT" */
	union {
		char descr[32];		/* Description, null-terminated */
		u8 p8[32];		/* 8-bit parameters */
		u16 p16[16];		/* 16-bit parameters */
		u32 p32[8];		/* 32-bit parameters */
		u64 p64[4];		/* 64-bit parameters */
	} param;
};

/* Possible values for flags entry above */
#define FSH_FLAGS_DESCR 0x8000		/* Description descr is present */
#define FSH_FLAGS_CRC32 0x4000		/* p32[7] holds the CRC32 checksum of
					   the image (without header) */

/* Jobs within FIRMWARE part */
#define FSIMG_JOB_FIRMWARE (FSIMG_JOB_DRAM | FSIMG_JOB_ATF | FSIMG_JOB_TEE)

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
static char board_id[32];
static const char *ram_type;
static const char *ram_timing;
static const char *arch = CONFIG_SYS_BOARD;
static basic_init_t basic_init_callback;

struct fs_header_v1_0 fsh;		/* Header of current image */

#define MAX_NEST_LEVEL 8

static struct fsimg {
	unsigned int size;		/* Size of the main image data */
	unsigned int remaining;		/* Remaining bytes in this level */
} fsimg_stack[MAX_NEST_LEVEL];

static struct board_name_rev {
	char name[32];
	unsigned int rev;
} board_name_rev;

/* Return the address of the board configuration in OCRAM */
void *fs_image_get_cfg_addr(bool with_fs_header)
{
	void *cfg = (void*)CONFIG_FUS_BOARDCFG_ADDR;

	return with_fs_header ? cfg : cfg + sizeof(struct fs_header_v1_0);
}

/* Return the address of the /board-cfg node */
int fs_image_get_cfg_offs(void *fdt)
{
	return fdt_path_offset(fdt, "/board-cfg");
}

/* Read the image size (incl. padding) from an F&S header */
static unsigned int fs_image_get_size(const struct fs_header_v1_0 *fsh)
{
	/* We ignore the high word, boot images are definitely < 4GB */
	return fsh->info.file_size_low;
}

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
#if 0 //###
	{
		int i;
		unsigned long offset = (unsigned long)p;

		reloc(dti->ddrc_cfg, offset);
		reloc(dti->ddrphy_cfg, offset);
		reloc(dti->fsp_msg, offset);
		reloc(dti->ddrphy_trained_csr, offset);
		reloc(dti->ddrphy_pie, offset);
		for (i = 0; i < dti->fsp_msg_num; i++)
			reloc(dti->fsp_msg[i].fsp_cfg, offset);
	}
#endif //###

	return !ddr_init(dti);
}

static void fs_image_get_board_name_rev(const char id[32],
					struct board_name_rev *bnr)
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

	printf("### %s: name=%s rev=%u\n", id, bnr->name, bnr->rev);
}

/* Store board_id and split into name and revision to ease board_id matching */
static void fs_image_set_board_id(const char id[32])
{
	memcpy(board_id, id, sizeof(board_id));
	fs_image_get_board_name_rev(id, &board_name_rev);
}

/* Check image magic, type and descr; return true on match */
static bool fs_image_match(struct fs_header_v1_0 *fsh,
			   const char *type, const char *descr)
{
	if (!type)
		return false;

	if (strncmp(fsh->info.magic, "FSLX", sizeof(fsh->info.magic)))
		return false;

	if (strncmp(fsh->type, type, sizeof(fsh->type)))
		return false;

	if (descr) {
		if (!(fsh->info.flags & FSH_FLAGS_DESCR))
			return false;
		if (strncmp(fsh->param.descr, descr, sizeof(fsh->param.descr)))
			return false;
	}

	return true;
}

static bool fs_image_match_board_id(struct fs_header_v1_0 *fsh,
				    const char *type)
{
	struct board_name_rev bnr;

	/* Compare magic and type */
	if (!fs_image_match(fsh, type, NULL))
		return false;

	/* A config must include a description, this is the board ID */
	if (!(fsh->info.flags & FSH_FLAGS_DESCR))
		return false;

	/* Split board ID of the config we look at into name and rev */
	fs_image_get_board_name_rev(fsh->param.descr, &bnr);

	/* Compare with name and rev of the board we are running on */
	if (strncmp(bnr.name, board_name_rev.name, sizeof(bnr.name)))
		return false;
	if (bnr.rev > board_name_rev.rev)
		return false;

	return true;
}

/* Load next header in image */
static void fs_image_next_header(enum fsimg_state new_state)
{
	struct fsimg *fsimg = &fsimg_stack[nest_level];

	state = new_state;
	count = sizeof(struct fs_header_v1_0);
	if (count > fsimg->remaining) {
		/* Image too small, no room for F&S header, skip */
		count = fsimg->remaining;
		fsimg->remaining = 0;
		mode = FSIMG_MODE_SKIP;
	} else {
		/* Load F&S header */
		addr = &fsh;
		fsimg->remaining -= count;
		mode = FSIMG_MODE_HEADER;
	}
}

/* Enter a new (sub-) image with given size and load the header */
static void fs_image_enter(unsigned int size, enum fsimg_state new_state)
{
	fsimg_stack[nest_level].remaining -= size;
	fsimg_stack[++nest_level].remaining = size;
	fsimg_stack[nest_level].size = size;
	fs_image_next_header(new_state);
}

/* Load image of given size to given address and switch to new state */
static void fs_image_copy(void *buf, unsigned int size)
{
	fsimg_stack[nest_level].remaining -= size;
	count = size;
	addr = buf;
	mode = FSIMG_MODE_IMAGE;
}

/* Skip data of given size */
static void fs_image_skip(unsigned int size)
{
	printf("### %d: skip 0x%x, state=0x%x\n", nest_level, size, state);
	fsimg_stack[nest_level].remaining -= size;
	count = size;
	mode = FSIMG_MODE_SKIP;
}

static void fs_image_copy_or_skip(struct fs_header_v1_0 *fsh,
				  const char *type, const char *descr,
				  void *addr, unsigned int size)
{
	if (fs_image_match(fsh, type, descr)) {
		puts("### match\n");
		fs_image_copy(addr, size);
	} else {
		puts("### mismatch\n");
		fs_image_skip(size);
	}
}

/* Get the next FIRMWARE job */
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

/* Switch to next FIRMWARE state or skip remaining images */
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

/* Process the next F&S header */
static void fs_image_handle_header(void)
{
	struct fsimg *fsimg = &fsimg_stack[nest_level];
	unsigned int size;

	/* Check if magic is correct */
	if (strncmp(fsh.info.magic, "FSLX", sizeof(fsh.info.magic))) {
		/* This is no F&S header; skip remaining image */
		fs_image_skip(fsimg->remaining);
		return;
	}

	/* Get the image size (incuding padding) */
	size = fs_image_get_size(&fsh);

	/* Fill in size on topmost level, if we did not know it (NAND, MMC) */
	if (!nest_level && !fsimg->remaining)
		fsimg->remaining = size;

	printf("### %d: Found %s, size=0x%x remaining=0x%x, state=%d\n", nest_level, fsh.type, size, fsimg->remaining, state);

	{
		int i;
		for (i=0; i<=nest_level; i++)
			printf("### +++> %d: size=0x%x remaining=0x%x\n",
			       i, fsimg_stack[i].size, fsimg_stack[i].remaining);
	}


	switch (state) {
	case FSIMG_STATE_ANY:
		if (fs_image_match(&fsh, "BOARD-ID", NULL)) {
			/* Save ID and add job to load BOARD-CFG */
			fs_image_set_board_id(fsh.param.descr);
			jobs |= FSIMG_JOB_CFG;
			fs_image_enter(size, state);
			break;
		} else if (fs_image_match(&fsh, "NBOOT", arch)) {
			/* Simply enter image, no further action */
			fs_image_enter(size, state);
			break;
		} else if (fs_image_match(&fsh, "BOARD-CONFIGS", arch)) {
			fs_image_enter(size, FSIMG_STATE_BOARD_CFG);
			break;
		} else if (fs_image_match(&fsh, "FIRMWARE", arch)
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
		// ### TODO: Use highest revision number <= BOARD_ID revision
		if (fs_image_match_board_id(&fsh, "BOARD-CFG")) {
			memcpy(fs_image_get_cfg_addr(true), &fsh, sizeof(fsh));
			fs_image_copy(fs_image_get_cfg_addr(false), size);
		} else
			fs_image_skip(size);
		break;

	case FSIMG_STATE_DRAM:
		/* Get DRAM type and DRAM timing from BOARD-CFG */
		if (fs_image_match(&fsh, "DRAM-SETTINGS", arch)) {
			void *fdt = fs_image_get_cfg_addr(false);
			int off = fs_image_get_cfg_offs(fdt);

			ram_type = fdt_getprop(fdt, off, "dram-type", NULL);
			ram_timing = fdt_getprop(fdt, off, "dram-timing", NULL);

			printf("### Looking for: %s, %s\n", ram_type, ram_timing);

			fs_image_enter(size, FSIMG_STATE_DRAM_TYPE);
		} else
			fs_image_skip(size);
		break;

	case FSIMG_STATE_DRAM_TYPE:
		if (fs_image_match(&fsh, "DRAM-TYPE", ram_type))
			fs_image_enter(size, FSIMG_STATE_DRAM_FW);
		else
			fs_image_skip(size);
		break;

	case FSIMG_STATE_DRAM_FW:
		/* Load DDR training firmware behind SPL code */
		fs_image_copy_or_skip(&fsh, "DRAM-FW", ram_type, &_end, size);
		break;

	case FSIMG_STATE_DRAM_TIMING:
		/* This may overlap ATF */
		fs_image_copy_or_skip(&fsh, "DRAM-TIMING", ram_timing,
				      (void *)CONFIG_SPL_DRAM_TIMING_ADDR,
				      size);
		break;

	case FSIMG_STATE_ATF:
		fs_image_copy_or_skip(&fsh, "ATF", arch,
				      (void *)CONFIG_SPL_ATF_ADDR, size);
		break;

	case FSIMG_STATE_TEE:
		fs_image_copy_or_skip(&fsh, "TEE", arch,
				      (void *)CONFIG_SPL_TEE_ADDR, size);
		break;
	}
}

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
		printf("### Got BOARD-CFG, ID=%s\n", board_id);
		jobs &= ~FSIMG_JOB_CFG;
		fs_image_skip(fsimg->remaining);
		break;

	case FSIMG_STATE_DRAM_FW:
		/* DRAM training firmware loaded, now look for DRAM timing */
		printf("### Got DRAM-FW (%s)\n", ram_type);
		fs_image_next_header(FSIMG_STATE_DRAM_TIMING);
		break;

	case FSIMG_STATE_DRAM_TIMING:
		/* DRAM info complete, start it; job done if successful */
		printf("### Got DRAM-TIMING (%s)\n", ram_timing);
		if (fs_image_init_dram()) {
			puts("### Init DDR successful\n");
			jobs &= ~FSIMG_JOB_DRAM;
		} else
			puts("### Init DDR failed\n");

		/* Skip remaining DRAM timings */
		fs_image_skip(fsimg->remaining);
		break;

	case FSIMG_STATE_ATF:
		/* ATF loaded, job done */
		puts("### Got ATF\n");
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

static void fs_image_handle_skip(void)
{
	struct fsimg *fsimg = &fsimg_stack[nest_level];

	if (fsimg->remaining) {
		printf("### %d: skip: remaining=0x%x state=0x%x\n", nest_level, fsimg->remaining, state);
		fs_image_next_header(state);
		return;
	}

	if (!nest_level) {
		mode = FSIMG_MODE_DONE;
		return;
	}

	nest_level--;
	fsimg--;

	{
		int i;
		for (i=0; i<=nest_level; i++)
			printf("### ---> %d: size=0x%x remaining=0x%x\n",
			       i, fsimg_stack[i].size, fsimg_stack[i].remaining);
	}

	switch (state) {
	case FSIMG_STATE_ANY:
		fs_image_next_header(state);
		break;

	case FSIMG_STATE_BOARD_CFG:
		fs_image_next_header(FSIMG_STATE_ANY);
		break;

	case FSIMG_STATE_DRAM_TYPE:
		printf("### DRAM_TYPE: next fw: state=%d\n", state);
		fs_image_next_fw();
		break;

	case FSIMG_STATE_DRAM_FW:
	case FSIMG_STATE_DRAM_TIMING:
		printf("### FSIMG_STATE_DRAM_FW|TIMING: state=%d\n", state);
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

/* Handle the next part of an image */
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

static void fs_image_start(unsigned int size, unsigned int jobs_todo,
			   basic_init_t basic_init)
{
	jobs = jobs_todo;
	basic_init_callback = basic_init;
	nest_level = -1;
	fs_image_enter(size, FSIMG_STATE_ANY);
}

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
		if (!count)
			fs_image_handle();
	}
}

static void fs_image_sdp_new_file(u32 dnl_address, u32 size)
{
	fs_image_start(size, jobs, basic_init_callback);
}

static const struct sdp_stream_ops fs_image_sdp_stream_ops = {
	.new_file = fs_image_sdp_new_file,
	.rx_data = fs_image_sdp_rx_data,
};

/* Load FIRMWARE and optionally BOARD-CFG via SDP from USB */
void fs_image_all_sdp(unsigned int jobs_todo, basic_init_t basic_init)
{
	jobs = jobs_todo;
	basic_init_callback = basic_init;

	/* Stream the file and load appropriate parts */
	spl_sdp_stream_image(&fs_image_sdp_stream_ops, true);

	/* Stream further files until all necessary init jobs are done */
	while (jobs) {
		printf("### Jobs to do: 0x%x\n", jobs);
		spl_sdp_stream_continue(&fs_image_sdp_stream_ops, true);
	}
}

#ifdef CONFIG_NAND_MXS
/* Load F&S image with given type/descr from NAND at offset to given buffer */
int fs_image_load_nand(unsigned int offset, char *type, char *descr,
		       void *buf, bool keep_header)
{
	unsigned int size;
	struct fs_header_v1_0 *fsh = buf;
	int err;

	/* Load F&S header */
	err = nand_spl_load_image(offset, sizeof(struct fs_header_v1_0), fsh);
	if (err < 0)
		return err;

        if (!fs_image_match(fsh, type, descr))
		return -ENOENT;

	size = fs_image_get_size(fsh);

	/* Load image data */
	if (keep_header)
		size += sizeof(struct fs_header_v1_0);
	else
		offset += sizeof(struct fs_header_v1_0);

	return nand_spl_load_image(offset, size, buf);
}

/* Load FIRMWARE from NAND using state machine */
static int fs_image_loop_nand(unsigned int offs, unsigned int lim)
{
	int err;

	lim += offs;

	// ### TODO: Handle (skip) bad blocks
	do {
		if (count) {
			if ((mode == FSIMG_MODE_IMAGE)
			    || (mode == FSIMG_MODE_HEADER)) {
				if (offs + count >= lim)
					return -EFBIG;
				err = nand_spl_load_image(offs, count, addr);
				if (err)
					return err;
				addr += count;
			}
			offs += count;
		}
		fs_image_handle();
	} while (mode != FSIMG_MODE_DONE);

	return 0;
}

/* Load FIRMWARE using state machine, try both copies */
unsigned int fs_image_fw_nand(unsigned int jobs_todo, basic_init_t basic_init)
{
	unsigned int lim = CONFIG_FUS_FIRMWARE_NAND_SIZE;
	int err;

	/*
	 * Try first copy; we do not know the FIRMWARE size yet, but have room
	 * for the first header in any case; the size will be filled in by the
	 * state machine when it is known.
	 */
	fs_image_start(sizeof(struct fs_header_v1_0), jobs_todo, basic_init);
	err = fs_image_loop_nand(CONFIG_FUS_FIRMWARE_NAND_OFFSET1, lim);
	if (err) {
		/* Read error, try second copy to complete remaining jobs */
		fs_image_start(sizeof(struct fs_header_v1_0), jobs, basic_init);
		err = fs_image_loop_nand(CONFIG_FUS_FIRMWARE_NAND_OFFSET2, lim);
	}
	if (err)
		printf("Reading FIRMWARE failed (%d)\n", err);

	return jobs;
}

/* Load BOARD-CFG from NAND */
int fs_image_cfg_nand(void)
{
	struct fs_header_v1_0 *fsh = fs_image_get_cfg_addr(true);
	int err;
	char *type = "BOARD-CFG";

	err = fs_image_load_nand(CONFIG_FUS_BOARDCFG_NAND_OFFSET1,
				 type, NULL, fsh, true);
	if (err) {
		err = fs_image_load_nand(CONFIG_FUS_BOARDCFG_NAND_OFFSET2,
					 type, NULL, fsh, true);
		if (err)
			return err;
	}

	fs_image_set_board_id(fsh->param.descr);

	return 0;
}
#endif /* CONFIG_NAND_MXS */

#ifdef CONFIG_MMC
/* Load F&S image with given type/descr from MMC at offset to given buffer */
int fs_image_load_mmc(unsigned int offset, char *type, char *descr,
		       void *buf, bool keep_header)
{
	unsigned int size;
	struct fs_header_v1_0 *fsh = buf;
	int err;

	/* Load F&S header */
	// ### TODO
	err = -EINVAL; //###
	//err = nand_spl_load_image(offset, sizeof(struct fs_header_v1_0), fsh);
	if (err < 0)
		return err;

        if (!fs_image_match(fsh, type, descr))
		return -ENOENT;

	size = fs_image_get_size(fsh);

	/* Load image data */
	if (keep_header)
		size += sizeof(struct fs_header_v1_0);
	else
		offset += sizeof(struct fs_header_v1_0);

	//### TODO
	return -EINVAL; //###nand_spl_load_image(offset, size, buf);
}

/* Load FIRMWARE from eMMC */
unsigned int fs_image_fw_mmc(unsigned int jobs_todo, basic_init_t basic_init)
{
	//### TODO

	return jobs;
}

/* Load BOARD-CFG from eMMC */
int fs_image_cfg_mmc(void)
{
//### TODO
//###	return fs_image_load_mmc(CONFIG_SPL_BOARDCFG_MMC_OFFSET, "BOARD-CFG",
//###				  NULL, fs_image_get_cfg_addr(true), true);
	return -EINVAL; //###
}
#endif /* CONFIG_MMC */
