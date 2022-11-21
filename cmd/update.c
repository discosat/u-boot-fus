/*
 * (C) Copyright 2015
 * Hartmut Keller, F&S Elektronik Systeme GmbH, keller@fs-net.de
 *
 * SPDX-License-Identifier:	GPL-2.0+
 */

#include <common.h>
#include <command.h>
#ifdef CONFIG_CMD_UPDATE
#include <net.h>			/* proto_t, NetLoop() */
#endif
#ifdef CONFIG_MMC
#include <mmc.h>			/* mmc_init() */
#endif
#ifdef CONFIG_USB_STORAGE
#include <usb.h>			/* usb_init(), usb_stor_scan() */
#endif
#ifdef CONFIG_CMD_UBIFS
#include <mtd/ubi-user.h>		/* UBI_MAX_VOLUME_NAME */
#endif
#include <update.h>			/* enum update_action */
#include <linux/ctype.h>		/* isdigit(), isalnum(), ... */
#include <jffs2/load_kernel.h>		/* struct mtd_device, ... */
#include <fs.h>				/* FS_TYPE_ANY, fs_read(), ... */
#include <nand.h>			/* get_nand_dev_by_index() */
#include <cpu_func.h>			/* flush_cache() */
#include <image.h>			/* image_source_script() */

#ifndef CONFIG_CMD_SOURCE
#error You need CONFIG_CMD_SOURCE when you define CONFIG_CMD_UPDATE
#endif

#define UPDATEDEV "updatedev"

static int update_ram(const char *action, const char **check,
		      unsigned long *addr)
{
	const char *p = *check + 3;	/* Skip "ram" */
	unsigned long newaddr = *addr;

	if ((*p == '@') && isxdigit(*(++p)))
		newaddr = simple_strtoul(p, (char **)&p, 16);

	if (*p && (*p != ','))
		return 1;		/* Parse error */

	*check = p;
	*addr = newaddr;
	printf("---- Trying %s from ram@%08lx ----\n", action, newaddr);

	env_set(UPDATEDEV, "ram");

	return 0;
}

#ifdef CONFIG_CMD_MTDPARTS
#ifdef CONFIG_CMD_UBIFS
extern int cmd_ubifs_mount(const char *vol_name);
extern int set_ubi_part(const char *part_name, const char *vid_header_offset);
extern int ubifs_load(const char *filename, u32 addr, u32 size);

static int update_ubi(const char *action, const char *part_name,
		      const char *vol_name, const char *fname,
		      unsigned long addr)
{
	printf("---- Trying %s from %s on ubi volume %s with %s ----\n",
	       action, part_name, vol_name, fname);

	if (set_ubi_part(part_name, NULL))
		return -1;
	if (cmd_ubifs_mount(vol_name))
		return -1;
	if (ubifs_load(fname, addr, 0))
		return -1;

	env_set(UPDATEDEV, "ubi");

	return 0;
}
#endif /* CONFIG_CMD_UBIFS */

extern int nand_load_image(struct mtd_info *mtd, ulong offset, ulong addr,
			   int show);

static int update_mtd(const char *action, const char **check, const char *fname,
		      unsigned long addr, unsigned int len)
{
	char part_name[PARTITION_MAXLEN];
#ifdef CONFIG_CMD_UBIFS
	char vol_name[UBI_MAX_VOLUME_NAME] = "";
#endif
	const char *p = *check + len;	/* skip "nand" */
	unsigned long offset = 0;
	struct mtd_device *dev;
	struct mtd_info *mtd;
	struct part_info *part;
	u8 pnum;

	if (len) {
		int devnum, partnum;

		memcpy(part_name, *check, len);
		if (!isdigit(*p))
			return 1;

		devnum = (int)simple_strtoul(p, (char **)&p, 10);
		if ((*p != ':') || !isdigit(*(++p)))
			return 1;

		partnum = (int)simple_strtoul(p, (char **)&p, 10);

		sprintf(part_name + len, "%d,%d", devnum, partnum);
	} else {
		while (*p && (*p != ',') && (*p != '.') && (*p != '+')) {
			part_name[len++] = *p++;
			if (len >= PARTITION_MAXLEN)
				return 1;
		}
		part_name[len] = 0;
	}

	if (*p == '.') {
		p++;
		if (strncmp(p, "jffs2", 5) == 0)
			p += 5;
		else if ((*p == 'e') || (*p == 'i'))
			p++;
#ifdef CONFIG_CMD_UBIFS
		else if (strncmp(p, "ubi(", 4) == 0) {
			p += 4;

			len = 0;
			while (*p && (*p != ')')) {
				vol_name[len++] = *p++;
				if (len >= UBI_MAX_VOLUME_NAME)
					return 1;
			}
			if ((len == 0) || *p++ != ')')
				return 1;
			vol_name[len] = 0;
		}
#endif
		else
			return 1;
	}
	if ((*p == '+') && isxdigit(*(++p)))
		offset = simple_strtoul(p, (char **)&p, 16);

	if (*p && (*p != ','))
		return 1;

	*check = p;
	if (mtdparts_init())
		return -1;

#ifdef CONFIG_CMD_UBIFS
	if (vol_name[0])
		return update_ubi(action, part_name, vol_name, fname, addr);
#endif
	printf("---- Trying %s from %s+%08lx ----\n", action, part_name,
	       offset);

	if (find_dev_and_part(part_name, &dev, &pnum, &part)) {
		printf("Partition %s not found!\n", part_name);
		return -1;
	}

	mtd = get_nand_dev_by_index(dev->id->num);
	if (nand_load_image(mtd, part->offset, addr, 0))
		return -1;

	env_set(UPDATEDEV, "nand");

	return 0;
}
#endif /* CONFIG_CMD_MTDPARTS */

#if (defined(CONFIG_MMC) || defined(CONFIG_USB_STORAGE))
#define MAX_IF_DEV_PART_STR 16
static int get_if_dev_part_str(char *if_dev_part_str, const char **check)
{
	const char *p = *check;
	int i = 0;

	/* Copy first three letters (interface name, e.g. "usb", "mmc") */
	if_dev_part_str[i++] = *p++;
	if_dev_part_str[i++] = *p++;
	if_dev_part_str[i++] = *p++;

	/* Add a blank */
	if_dev_part_str[i++] = ' ';

	/* Scan any leading blanks before device/part description  */
	while (*p == ' ')
		p++;

	/* Copy device and partition number; use "0" if no device is given */
	do {
		if (!isdigit(*p) && (*p != ':'))
			break;
		if_dev_part_str[i++] = *p++;
	} while (i < MAX_IF_DEV_PART_STR);
	if (i == 4)
		if_dev_part_str[i++] = '0';
	if_dev_part_str[i] = 0;

	if (*p && (*p != ','))
		return 1;		/* Parse error */

	*check = p;
	return 0;
}
#endif /* CONFIG_MMC || CONFIG_USB_STORAGE */

#ifdef CONFIG_MMC
static int update_mmc(const char *action, const char **check, const char *fname,
		      unsigned long addr)
{
	char if_dev_part_str[MAX_IF_DEV_PART_STR + 1];
	int dev_num;
	struct mmc *mmc;

	if (get_if_dev_part_str(if_dev_part_str, check))
		return 1;		/* Parse error */

	printf("---- Trying %s from %s with %s ----\n", action,
	       if_dev_part_str, fname);

	dev_num = simple_strtoul(if_dev_part_str + 4, NULL, 0);
	mmc = find_mmc_device(dev_num);
	if (!mmc)
		return -1;

	mmc_init(mmc);

	/* If the device is valid and we can open the partition, try to
	   load the update file */
	if (fs_set_blk_dev("mmc", if_dev_part_str + 4, FS_TYPE_ANY))
		return -1;		  /* Device or partition not valid */

	if (fs_read(fname, addr, 0, 0, NULL) < 0)
		return -1;		  /* File not found or I/O error */

	env_set(UPDATEDEV, if_dev_part_str);

	return 0;
}
#endif /* CONFIG_MMC */

#ifdef CONFIG_USB_STORAGE
static int update_usb(const char *action, const char **check, const char *fname,
		      unsigned long addr)
{
	char if_dev_part_str[MAX_IF_DEV_PART_STR + 1];
	static int usb_init_done = 0;

	if (get_if_dev_part_str(if_dev_part_str, check))
		return 1;		/* Parse error */

	printf("---- Trying %s from %s with %s ----\n", action,
	       if_dev_part_str, fname);

	/* Init USB only once during update */
	if (!usb_init_done) {
	        if (usb_init(0) < 0)
			return -1;

		/* Try to recognize storage devices immediately */
		usb_stor_scan(0);
		usb_init_done = 1;
	}

	/* If the device is valid and we can open the partition, try to
	   load the update file */
	if (fs_set_blk_dev("usb", if_dev_part_str + 4, FS_TYPE_ANY))
		return -1;		  /* Device or partition not valid */

	if (fs_read(fname, addr, 0, 0, NULL) < 0)
		return -1;		  /* File not found or I/O error */

	env_set(UPDATEDEV, if_dev_part_str);
	return 0;
}
#endif /* CONFIG_USB_STORAGE && CONFIG_FS_FAT */

#ifdef CONFIG_CMD_NET
static int update_net(const char *action, const char **check, const char *fname,
		      unsigned long addr, enum proto_t proto, int len)
{
	int size;
	int i;
	const char *p = *check;

	if (p[len] && (p[len] != ','))
		return 1;		/* Parse error */

	*check = p + len;
	printf("---- Trying %s from ", action);
	for (i = 0; i < len; i++)
		putc(p[i]);
	printf(" with %s ----\n", fname);

	copy_filename(net_boot_file_name, fname, sizeof(net_boot_file_name));
	set_fileaddr(addr);
	size = net_loop(proto);
	if (size < 0)
		return -1;

	/* flush cache */
	flush_cache(addr, size);

	/* Set fileaddr and filesize */
	env_set_fileinfo(size);
	env_set(UPDATEDEV, "net");

	return 0;
}
#endif /* CONFIG_CMD_NET */

/*
 * Check the devices given as comma separated list in check whether they
 * contain an update script with name given in fname. If yes, load it to the
 * address given in addr and execute it (by sourcing it). If not, check next
 * device. The action is usually "update" or "install". It is shown to the
 * user and is also used to determine the names of the environment variables
 * and/or the script name that are used as defaults if the arguments are zero.
 */
int update_script(enum update_action action_id, const char *check,
		  const char *fname, unsigned long addr)
{
	char varname[20];
	int ret;
	char *action;

	switch (action_id) {
	default:
	case UPDATE_ACTION_NONE:
		return 1;
	case UPDATE_ACTION_UPDATE:
		action = "update";
		break;
	case UPDATE_ACTION_INSTALL:
		action = "install";
		break;
	case UPDATE_ACTION_RECOVER:
		action = "recover";
		break;
	}

	/* If called without devices argument, get devices to check from
	   environment variable "<action>check", i.e. "updatecheck" or
	   "installcheck". */
	if (!check) {
		sprintf(varname, "%scheck", action);
		check = env_get(varname);
		if (!check)
			return 1;	  /* Variable not set */
	}

	/* If called without load address argument, get address from
	   environment variable "<action>addr, i.e. "updateaddr" or
	   "installaddr". If the variable does not exist, use $(loadaddr) */
	if (!addr) {
		sprintf(varname, "%saddr", action);
		addr = env_get_ulong(varname, 16, get_loadaddr());
	}

	/* If called without script filename argument, get filename from
	   environment variable "<action>script", i.e. "updatescript" or
	   "installscript". If the variable does not exist, use
	   "<action>.scr" as default script name, i.e. "update.scr" or
	   "install.scr". */
	if (!fname) {
		sprintf(varname, "%sscript", action);
		fname = env_get(varname);
		if (!fname) {
			sprintf(varname, "%s.scr", action);
			fname = varname;
		}
	}

	/* Parse devices and check for script */
	do {
		/* Skip any commas and whitespace */
		while ((*check == ',') || (*check == ' ') || (*check == '\t'))
		       check++;
		if (!*check)
			break;

		if (!isalpha(*check) && (*check != '_'))
			ret = 1;
		else if (strncmp(check, "ram", 3) == 0)
			ret = update_ram(action, &check, &addr);
#if defined(CONFIG_MMC) && defined(CONFIG_FS_FAT)
		else if (strncmp(check, "mmc", 3) == 0)
			ret = update_mmc(action, &check, fname, addr);
#endif
#if defined(CONFIG_USB_STORAGE) && defined(CONFIG_FS_FAT)
		else if (strncmp(check, "usb", 3) == 0)
			ret = update_usb(action, &check, fname, addr);
#endif
#ifdef CONFIG_CMD_NET
		else if (strncmp(check, "tftp", 4) == 0)
			ret = update_net(action, &check, fname, addr, TFTPGET, 4);
		else if (strncmp(check, "nfs", 3) == 0)
			ret = update_net(action, &check, fname, addr, NFS, 3);
		else if (strncmp(check, "dhcp", 4) == 0)
			ret = update_net(action, &check, fname, addr, DHCP, 4);
#endif
#ifdef CONFIG_CMD_MTDPARTS
		else if (strncmp(check, "nand", 4) == 0)
			ret = update_mtd(action, &check, fname, addr, 4);
		else
			ret = update_mtd(action, &check, fname, addr, 0);
#else
		else
			ret = 1;
#endif

		if (!ret) {
			/* Do a simple check if we have a valid image */
			switch (genimg_get_format((void *)addr)) {
			case IMAGE_FORMAT_LEGACY:
#if defined(CONFIG_FIT)
			case IMAGE_FORMAT_FIT:
#endif
				puts("Loaded!\n");

				ret = image_source_script(addr, NULL);

				printf("---- %s %s ----\n", action,
				       ret ? "FAILED!" : "COMPLETE!");

				/* Kill the magic number to avoid that the
				   script in RAM triggers again at the next
				   start. */
				*(unsigned int *)addr = 0;

				/* If the script fails due to an error, we
				   return 0 nonetheless because otherwise we
				   might unintentionally boot the system */
				return 0;

			default:
				ret = -1;
				break;
			}
		}
		if (ret < 0)
			puts("Failed!\n");
		else {
			printf("---- Invalid %s device ", action);
			while (*check && (*check != ','))
				putc(*check++);
			puts(", ignored ----\n");
		}
	} while (*check);
	printf("---- No %s script found ----\n", action);

	return 1;
}

int do_update(cmd_tbl_t *cmdtp, int flag, int argc, char * const argv[])
{
	char *check = NULL;
	char *fname = NULL;
	unsigned long addr = 0;
	enum update_action action_id = UPDATE_ACTION_UPDATE;
	const char *ext;

	/* Take action from extension if given */
	ext = strchr(argv[0], '.');
	if (ext) {
		size_t len = strlen(++ext);
		if (len) {
			if (strncmp(ext, "install", len) == 0)
				action_id = UPDATE_ACTION_INSTALL;
			else if (strncmp(ext, "recover", len) == 0)
				action_id = UPDATE_ACTION_RECOVER;
			else if (strncmp(ext, "update", len) != 0)
				return 1;
		}
	}

	/* Take arguments if given */
	if (argc > 1) {
		if (strcmp(argv[1], ".") != 0)
			check = argv[1];
		if (argc > 2) {
			if (strcmp(argv[2], ".") != 0)
				fname = argv[2];
			if (argc > 3)
				addr = parse_loadaddr(argv[3], NULL);
		}
	}

	return update_script(action_id, check, fname, addr);
}


U_BOOT_CMD(
	update, 4, 0,	do_update,
	"update system from external device",
	"[updatecheck [updatescript [updateaddr]]]\n"
	"   Check the devices given in updatecheck whether they contain a\n"
	"   scriptfile as given in updatescript. If the script is found\n"
	"   anywhere, load it to the address given in updateaddr and update\n"
	"   the system by sourcing (executing) it. You can use '.' to skip an\n"
	"   argument.\n\n"
	"   updatecheck is a comma separated list built from the following\n"
	"   device specifications:\n\n"
	"   ram[@addr]\n"
	"      Execute script from RAM at address <addr>\n"
#if defined(CONFIG_MMC) && defined(CONFIG_FS_FAT)
	"   mmc[<dev_num>[:<part_num>]]\n"
	"      Load script from MMC device <dev_num>, partition <part_num>\n"
#endif
#if defined(CONFIG_USB_STORAGE) && defined(CONFIG_FS_FAT)
	"   usb{<dev_num>[:<part_num>]]\n"
	"      Load script from USB device <dev_num>, partition <part_num>\n"
#endif
#ifdef CONFIG_CMD_MTDPARTS
	"   nand[<dev_num>[:<part_num>]][+<offset>]   or\n"
	"   <part_name>[+<offset>]\n"
	"      Load script from MTD device given by device number <dev_num> and\n"
	"      partition number <part_num> or by partition name <part_name>\n"
	"      at offset <offset>.\n"
#ifdef CONFIG_CMD_UBIFS
	"   nand[<dev_num>[:<part_num>]].ubi(<vol_name>)   or\n"
	"   <part_name>.ubi(<vol_name>)\n"
	"      Load script from UBI volume <vol_name> on MTD device given by\n"
	"      device number <dev_num> and partition number <part_num> or by\n"
	"      partition name <part_name>. The filesystem in the volume must\n"
	"      be ubifs.\n"
#endif
#endif
	"\n"
	"   Any missing argument is replaced by the environment variable of\n"
	"   the same name or, if no such variable exists, by the following\n"
	"   default values:\n"
	"     updatecheck:   empty, don't update automatically\n"
	"     updatescript:  update.scr\n"
	"     updateaddr:    $loadaddr\n"
	"\nupdate.install [installcheck [installscript [installaddr]]]\n"
	"   Same as above, with following environment variables/defaults:\n"
	"     installcheck:  empty, don't install automatically\n"
	"     installscript: install.scr\n"
	"     installaddr:   $loadaddr\n"
	"\nupdate.recover [recovercheck [recoverscript [recoveraddr]]]\n"
	"   Same as above, with following environment variables/defaults:\n"
	"     recovercheck:  empty, don't recover automatically\n"
	"     recoverscript: recover.scr\n"
	"     recoveraddr:   $loadaddr\n"
);
