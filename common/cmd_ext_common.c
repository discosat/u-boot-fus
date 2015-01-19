/*
 * (C) Copyright 2011 - 2012 Samsung Electronics
 * EXT2/4 filesystem implementation in Uboot by
 * Uma Shankar <uma.shankar@samsung.com>
 * Manjunatha C Achar <a.manjunatha@samsung.com>
 *
 * Ext4fs support
 * made from existing cmd_ext2.c file of Uboot
 *
 * (C) Copyright 2004
 * esd gmbh <www.esd-electronics.com>
 * Reinhard Arlt <reinhard.arlt@esd-electronics.com>
 *
 * made from cmd_reiserfs by
 *
 * (C) Copyright 2003 - 2004
 * Sysgo Real-Time Solutions, AG <www.elinos.com>
 * Pavel Bartusek <pba@sysgo.com>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of
 * the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston,
 * MA 02111-1307 USA
 *
 */

/*
 * Changelog:
 *	0.1 - Newly created file for ext4fs support. Taken from cmd_ext2.c
 *	        file in uboot. Added ext4fs ls load and write support.
 */

#include <common.h>
#include <part.h>
#include <config.h>
#include <command.h>
#include <image.h>
#include <linux/ctype.h>
#include <asm/byteorder.h>
#include <ext_common.h>
#include <ext4fs.h>
#include <linux/stat.h>
#include <malloc.h>

#if defined(CONFIG_CMD_USB) && defined(CONFIG_USB_STORAGE)
#include <usb.h>
#endif

#if !defined(CONFIG_DOS_PARTITION) && !defined(CONFIG_EFI_PARTITION)
#error DOS or EFI partition support must be selected
#endif

#define DOS_PART_MAGIC_OFFSET		0x1fe
#define DOS_FS_TYPE_OFFSET		0x36
#define DOS_FS32_TYPE_OFFSET		0x52

int do_ext_load(cmd_tbl_t *cmdtp, int flag, int argc,
						char *const argv[])
{
	const char *filename = NULL;
	int dev, part;
	ulong addr = 0;
	int filelen;
	disk_partition_t info;
	block_dev_desc_t *dev_desc;
	char buf[12];
	unsigned long count;

	if (argc < 3)
		return CMD_RET_USAGE;
	addr = (argc > 3) ? parse_loadaddr(argv[3], NULL) : get_loadaddr();
	filename = (argc > 4) ? parse_bootfile(argv[4]) : get_bootfile();
	count = (argc > 5) ? simple_strtoul(argv[5], NULL, 16) : 0;

	if (!filename) {
		puts("** No boot file defined **\n");
		return 1;
	}

	part = get_device_and_partition(argv[1], argv[2], &dev_desc, &info, 1);
	if (part < 0)
		return 1;

	set_loadaddr(addr);
	dev = dev_desc->dev;
	printf("Loading file \"%s\" from %s device %d%c%c\n",
		filename, argv[1], dev,
		part ? ':' : ' ', part ? part + '0' : ' ');

	ext4fs_set_blk_dev(dev_desc, &info);

	if (!ext4fs_mount(info.size)) {
		printf("** Bad ext2 partition or disk - %s %d:%d **\n",
		       argv[1], dev, part);
		ext4fs_close();
		goto fail;
	}

	filelen = ext4fs_open(filename);
	if (filelen < 0) {
		printf("** File not found %s\n", filename);
		ext4fs_close();
		goto fail;
	}
	if ((count < filelen) && (count != 0))
		filelen = count;

	if (ext4fs_read((char *)addr, filelen) != filelen) {
		printf("** Unable to read \"%s\" from %s %d:%d **\n",
		       filename, argv[1], dev, part);
		ext4fs_close();
		goto fail;
	}

	ext4fs_close();

	printf("%d bytes read\n", filelen);
	sprintf(buf, "%X", filelen);
	setenv("filesize", buf);

	return 0;
fail:
	return 1;
}

int do_ext_ls(cmd_tbl_t *cmdtp, int flag, int argc, char *const argv[])
{
	const char *filename = "/";
	int dev;
	int part;
	block_dev_desc_t *dev_desc;
	disk_partition_t info;

	if (argc < 2)
		return cmd_usage(cmdtp);

	part = get_device_and_partition(argv[1], argv[2], &dev_desc, &info, 1);
	if (part < 0)
		return 1;

	if (argc == 4)
		filename = argv[3];

	dev = dev_desc->dev;
	ext4fs_set_blk_dev(dev_desc, &info);

	if (!ext4fs_mount(info.size)) {
		printf("** Bad ext2 partition or disk - %s %d:%d **\n",
		       argv[1], dev, part);
		ext4fs_close();
		goto fail;
	}

	if (ext4fs_ls(filename)) {
		printf("** Error extfs_ls() **\n");
		ext4fs_close();
		goto fail;
	};

	ext4fs_close();
	return 0;

fail:
	return 1;
}
