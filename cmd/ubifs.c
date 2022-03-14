// SPDX-License-Identifier: GPL-2.0+
/*
 * (C) Copyright 2008
 * Stefan Roese, DENX Software Engineering, sr@denx.de.
 */


/*
 * UBIFS command support
 */

#undef DEBUG

#include <common.h>
#include <config.h>
#include <command.h>
#include <mtd/ubi-user.h>			/* UBI_MAX_VOLUME_NAME */
#include <ubifs_uboot.h>

static int ubifs_mounted;
static char vol_mounted[UBI_MAX_VOLUME_NAME];

void cmd_ubifs_umount(void)
{
	uboot_ubifs_umount();
	ubifs_mounted = 0;
}

int cmd_ubifs_mount(const char *vol_name)
{
	int ret;

	if (ubifs_mounted && !strncmp(vol_mounted, vol_name, UBI_MAX_VOLUME_NAME))
		return 0;		/* Already mounted */
	if (ubifs_mounted)
		cmd_ubifs_umount();

	ubifs_init();

	ret = uboot_ubifs_mount(vol_name);
	if (ret)
		return -1;

	strncpy(vol_mounted, vol_name, UBI_MAX_VOLUME_NAME);
	ubifs_mounted = 1;

	return 0;
}

static int do_ubifs_mount(cmd_tbl_t *cmdtp, int flag, int argc,
			  char *const argv[])
{
	if (argc != 2)
		return CMD_RET_USAGE;

	debug("Using volume %s\n", argv[1]);

	return cmd_ubifs_mount(argv[1]);
}

int ubifs_is_mounted(void)
{
	return ubifs_mounted;
}

static int do_ubifs_umount(cmd_tbl_t *cmdtp, int flag, int argc,
				char * const argv[])
{
	if (argc != 1)
		return CMD_RET_USAGE;

	if (ubifs_mounted == 0) {
		printf("No UBIFS volume mounted!\n");
		return -1;
	}

	cmd_ubifs_umount();

	return 0;
}

static int do_ubifs_ls(cmd_tbl_t *cmdtp, int flag, int argc, char *const argv[])
{
	char *filename = "/";
	int ret;

	if (!ubifs_mounted) {
		printf("UBIFS not mounted, use ubifsmount to mount volume first!\n");
		return -1;
	}

	if (argc == 2)
		filename = argv[1];
	debug("Using filename %s\n", filename);

	ret = ubifs_ls(filename);
	if (ret) {
		printf("** File not found %s **\n", filename);
		ret = CMD_RET_FAILURE;
	}

	return ret;
}

static int do_ubifs_load(cmd_tbl_t *cmdtp, int flag, int argc,
				char * const argv[])
{
	const char *filename;
	int ret;
	u32 addr;
	u32 size;

	if (!ubifs_mounted) {
		printf("UBIFS not mounted, use ubifs mount to mount volume first!\n");
		return -1;
	}

	addr = (argc > 1) ? parse_loadaddr(argv[1], NULL) : get_loadaddr();
	filename = (argc > 2) ? parse_bootfile(argv[2]) : get_bootfile();
	size = (argc > 3) ? simple_strtoul(argv[3], NULL, 16) : 0;

	debug("Loading file '%s' to address 0x%08x (size %d)\n",
	      filename, addr, size);

	ret = ubifs_load(filename, addr, size);
	if (ret) {
		printf("** File not found %s **\n", filename);
		ret = CMD_RET_FAILURE;
	}

	return ret;
}

U_BOOT_CMD(
	ubifsmount, 2, 0, do_ubifs_mount,
	"mount UBIFS volume",
	"<volume-name>\n"
	"    - mount 'volume-name' volume"
);

U_BOOT_CMD(
	ubifsumount, 1, 0, do_ubifs_umount,
	"unmount UBIFS volume",
	"    - unmount current volume"
);

U_BOOT_CMD(
	ubifsls, 2, 0, do_ubifs_ls,
	"list files in a directory",
	"[directory]\n"
	"    - list files in a 'directory' (default '/')"
);

U_BOOT_CMD(
	ubifsload, 4, 0, do_ubifs_load,
	"load file from an UBIFS filesystem",
	"<addr> <filename> [bytes]\n"
	"    - load file 'filename' to address 'addr'"
);
