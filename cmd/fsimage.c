/*
 * Copyright 2021 F&S Elektronik Systeme GmbH
 * Hartmut Keller <keller@fs-net.de>
 *
 * Handle F&S NBOOT images.
 *
 * SPDX-License-Identifier:	GPL-2.0+
 */

#include <common.h>
#include <command.h>
#include "../board/F+S/common/fs_image_common.h"	/* fs_image_*() */

static int do_fsimage_arch(cmd_tbl_t *cmdtp, int flag, int argc,
			   char * const argv[])
{
	printf("%s\n", fs_image_get_arch());

	return 0;
}

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
/* Printf FDT content of current BOARD-CFG */
static int do_fsimage_boardcfg(cmd_tbl_t *cmdtp, int flag, int argc,
			       char * const argv[])
{
	void *fdt = fs_image_get_cfg_addr_check(false);

	if (!fdt)
		return 1;

	printf("FDT part of BOARD-CFG located at 0x%lx\n", (ulong)fdt);

	return fdt_print(fdt, "/", NULL, 5);
}
#endif

static int do_fsimage_firmware(cmd_tbl_t *cmdtp, int flag, int argc,
			       char * const argv[])
{
	unsigned long addr;

	if (argc > 1)
		addr = parse_loadaddr(argv[1], NULL);
	else
		addr = get_loadaddr();

	return fs_image_load_firmware(addr) ? 1 : 0;
}

static int do_fsimage_list(cmd_tbl_t *cmdtp, int flag, int argc,
			   char * const argv[])
{
	unsigned long addr;

	if (argc > 1)
		addr = parse_loadaddr(argv[1], NULL);
	else
		addr = get_loadaddr();

	return fs_image_list(addr) ? 1 : 0;
}

static int do_fsimage_save(cmd_tbl_t *cmdtp, int flag, int argc,
			   char * const argv[])
{
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
		addr = parse_loadaddr(argv[1], NULL);
	else
		addr = get_loadaddr();

	return fs_image_save(addr, force) ? 1 : 0;
}

static int do_fsimage_fuse(cmd_tbl_t *cmdtp, int flag, int argc,
			   char * const argv[])
{
	puts("### Not yet implemented\n");

	return 1;
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
