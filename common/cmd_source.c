/*
 * (C) Copyright 2012
 * Hartmut Keller, F&S Elektronik Systeme GmbH, keller@fs-net.de
 *
 * (C) Copyright 2001
 * Kyle Harris, kharris@nexus-tech.net
 *
 * See file CREDITS for list of people who contributed to this
 * project.
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
 */

/*
 * The "source" command allows to define "script images", i. e. files
 * that contain command sequences that can be executed by the command
 * interpreter. It returns the exit status of the last command
 * executed from the script. This is very similar to running a shell
 * script in a UNIX shell, hence the name for the command.
 */

/* #define DEBUG */

#include <common.h>
#include <command.h>
#include <image.h>
#include <malloc.h>
#include <fat.h>			  /* fat_register_device(), ... */
#ifdef CONFIG_CMD_UPDATE
#include <net.h>			  /* proto_t, NetLoop() */
#endif
#ifdef CONFIG_MMC
#include <mmc.h>			  /* mmc_init() */
#endif
#ifdef CONFIG_USB_STORAGE
#include <usb.h>			  /* usb_init(), usb_stor_scan() */
#endif
#include <asm/byteorder.h>
#if defined(CONFIG_8xx)
#include <mpc8xx.h>
#endif
#ifdef CONFIG_SYS_HUSH_PARSER
#include <hush.h>
#endif

int
source (ulong addr, const char *fit_uname)
{
	ulong		len;
	image_header_t	*hdr;
	ulong		*data;
	char		*cmd;
	int		rcode = 0;
	int		verify;
#if defined(CONFIG_FIT)
	const void*	fit_hdr;
	int		noffset;
	const void	*fit_data;
	size_t		fit_len;
#endif

	verify = getenv_yesno ("verify");

	switch (genimg_get_format ((void *)addr)) {
	case IMAGE_FORMAT_LEGACY:
		hdr = (image_header_t *)addr;

		if (!image_check_magic (hdr)) {
			puts ("Bad magic number\n");
			return 1;
		}

		if (!image_check_hcrc (hdr)) {
			puts ("Bad header crc\n");
			return 1;
		}

		if (verify) {
			if (!image_check_dcrc (hdr)) {
				puts ("Bad data crc\n");
				return 1;
			}
		}

		if (!image_check_type (hdr, IH_TYPE_SCRIPT)) {
			puts ("Bad image type\n");
			return 1;
		}

		/* get length of script */
		data = (ulong *)image_get_data (hdr);

		if ((len = uimage_to_cpu (*data)) == 0) {
			puts ("Empty Script\n");
			return 1;
		}

		/*
		 * scripts are just multi-image files with one component, seek
		 * past the zero-terminated sequence of image lengths to get
		 * to the actual image data
		 */
		while (*data++);
		break;
#if defined(CONFIG_FIT)
	case IMAGE_FORMAT_FIT:
		if (fit_uname == NULL) {
			puts ("No FIT subimage unit name\n");
			return 1;
		}

		fit_hdr = (const void *)addr;
		if (!fit_check_format (fit_hdr)) {
			puts ("Bad FIT image format\n");
			return 1;
		}

		/* get script component image node offset */
		noffset = fit_image_get_node (fit_hdr, fit_uname);
		if (noffset < 0) {
			printf ("Can't find '%s' FIT subimage\n", fit_uname);
			return 1;
		}

		if (!fit_image_check_type (fit_hdr, noffset, IH_TYPE_SCRIPT)) {
			puts ("Not a image image\n");
			return 1;
		}

		/* verify integrity */
		if (verify) {
			if (!fit_image_check_hashes (fit_hdr, noffset)) {
				puts ("Bad Data Hash\n");
				return 1;
			}
		}

		/* get script subimage data address and length */
		if (fit_image_get_data (fit_hdr, noffset, &fit_data, &fit_len)) {
			puts ("Could not find script subimage data\n");
			return 1;
		}

		data = (ulong *)fit_data;
		len = (ulong)fit_len;
		break;
#endif
	default:
		puts ("Wrong image format for \"source\" command\n");
		return 1;
	}

	debug ("** Script length: %ld\n", len);

	if ((cmd = malloc (len + 1)) == NULL) {
		return 1;
	}

	/* make sure cmd is null terminated */
	memmove (cmd, (char *)data, len);
	*(cmd + len) = 0;

#ifdef CONFIG_SYS_HUSH_PARSER /*?? */
	rcode = parse_string_outer (cmd, FLAG_PARSE_SEMICOLON);
#else
	{
		char *line = cmd;
		char *next = cmd;

		/*
		 * break into individual lines,
		 * and execute each line;
		 * terminate on error.
		 */
		while (*next) {
			if (*next == '\n') {
				*next = '\0';
				/* run only non-empty commands */
				if (*line) {
					debug ("** exec: \"%s\"\n",
						line);
					if (run_command(line, 0) < 0) {
						rcode = 1;
						break;
					}
				}
				line = next + 1;
			}
			++next;
		}
		if (rcode == 0 && *line)
			rcode = (run_command(line, 0) >= 0);
	}
#endif
	free (cmd);
	return rcode;
}

/**************************************************/
#if defined(CONFIG_CMD_SOURCE)
int
do_source (cmd_tbl_t *cmdtp, int flag, int argc, char * const argv[])
{
	ulong addr;
	int rcode;
	const char *fit_uname = NULL;

	/* Find script image */
	if (argc < 2) {
		addr = get_loadaddr();
		debug ("*  source: default load address = 0x%08lx\n", addr);
#if defined(CONFIG_FIT)
	} else if (fit_parse_subimage(argv[1], get_loadaddr(), &addr,
				       &fit_uname)) {
		debug ("*  source: subimage '%s' from FIT image at 0x%08lx\n",
				fit_uname, addr);
#endif
	} else {
		addr = parse_loadaddr(argv[1], NULL);
		debug ("*  source: cmdline image address = 0x%08lx\n", addr);
	}

	printf ("## Executing script at %08lx\n", addr);
	rcode = source (addr, fit_uname);
	return rcode;
}

U_BOOT_CMD(
	source, 2, 0,	do_source,
	"run script from memory",
	"[addr]\n"
	"\t- run script starting at addr\n"
	"\t- A valid image header must be present"
#if defined(CONFIG_FIT)
	"\n"
	"For FIT format uImage addr must include subimage\n"
	"unit name in the form of addr:<subimg_uname>"
#endif
);


#ifdef CONFIG_CMD_UPDATE

#ifdef CONFIG_MMC
static int update_mmc(unsigned long addr, int devnum, int partnum,
		      const char *fname)
{
	struct mmc *mmc;
	block_dev_desc_t *dev_desc;

	mmc = find_mmc_device(devnum);
	if (!mmc)
		return -1;

	mmc_init(mmc);

	/* If the device is valid and we can open the partition, try to
	   load the update file */
	dev_desc = get_dev("mmc", devnum);
	if (!dev_desc || (fat_register_device(dev_desc, partnum) < 0))
		return -1;		  /* Device or partition not valid */

	if (file_fat_read(fname, (unsigned char *)addr, 0) < 0)
		return -1;		  /* File not found or I/O error */

	return 0;
}
#endif /* CONFIG_MMC */

#ifdef CONFIG_USB_STORAGE
static int update_usb(unsigned long addr, int devnum, int partnum,
		      const char *fname)
{
	static int usb_init_done = 0;
	block_dev_desc_t *dev_desc;

	/* Init USB only once during update */
	if (!usb_init_done) {
	        if (usb_init() < 0)
			return -1;

		/* Try to recognize storage devices immediately */
		usb_stor_scan(1);
		usb_init_done = 1;
	}

	/* If the device is valid and we can open the partition, try to
	   load the update file */
	dev_desc = get_dev("usb", devnum);
	if (!dev_desc || (fat_register_device(dev_desc, partnum) < 0))
		return -1;		  /* Device or partition not valid */

	if (file_fat_read(fname, (unsigned char *)addr, 0) < 0)
		return -1;		  /* File not found or I/O error */

	return 0;
}
#endif /* CONFIG_USB_STORAGE */

#ifdef CONFIG_CMD_NET
static int update_net(enum proto_t proto, unsigned long addr, const char *fname)
{
	int size;

	copy_filename(BootFile, fname, sizeof(BootFile));
	set_loadaddr(addr);
	size = NetLoop(proto);
	if (size < 0)
		return -1;

	/* flush cache */
	flush_cache(addr, size);

	return 0;
}

static int update_tftp(unsigned long addr, int devnum, int partnum,
		       const char *fname)
{
	return update_net(TFTPGET, addr, fname);
}

static int update_nfs(unsigned long addr, int devnum, int partnum,
		      const char *fname)
{
	return update_net(NFS, addr, fname);
}

static int update_dhcp(unsigned long addr, int devnum, int partnum,
		       const char *fname)
{
	return update_net(DHCP, addr, fname);
}
#endif /* CONFIG_CMD_NET */


#define UPDATE_FLAGS_NET 0x01	/* Network device, ignore devnum & partnum */
struct updateinfo {
	char *devname;
	unsigned int flags;
	int (*update)(unsigned long addr, int devnum, int partnum,
		      const char *fname);
};

static struct updateinfo upinfo[] = {
#ifdef CONFIG_MMC
	{"mmc", 0, update_mmc},
#endif
#ifdef CONFIG_USB_STORAGE
	{"usb", 0, update_usb},
#endif
#ifdef CONFIG_CMD_NET
	{"tftp", UPDATE_FLAGS_NET, update_tftp},
	{"nfs", UPDATE_FLAGS_NET, update_nfs},
	{"dhcp", UPDATE_FLAGS_NET, update_dhcp},
#endif
};


/*
 * Check the devices given as comma separated list in check whether they
 * contain an update script with name given in fname. If yes, load it to the
 * address given in addr and execute it (by sourcing it). If not, check next
 * device. The action is usually "update" or "install". It is shown to the
 * user and is also used to determine the names of the environment variables
 * and/or the script name that are used as defaults if the arguments are zero.
 */
int update_script(const char *action, const char *check, const char *fname,
		  unsigned long addr)
{
	char c;
	const char *devname;
	int devnamelen;
	int devnum;
	int partnum;
	int i;
	struct updateinfo *p;
	char varname[20];

	/* If called without devices argument, get devices to check from
	   environment variable "<action>check", i.e. "updatecheck" or
	   "installcheck". */
	if (!check) {
		sprintf(varname, "%scheck", action);
		check = getenv(varname);
		if (!check)
			return 1;	  /* Variable not set */
	}

	/* If called without load address argument, get address from
	   environment variable "<action>addr, i.e. "updateaddr" or
	   "installaddr". If the variable does not exist, use $(loadaddr) */
	if (!addr) {
		sprintf(varname, "%saddr", action);
		addr = getenv_ulong(varname, 16, get_loadaddr());
	}

	/* If called without script filename argument, get filename from
	   environment variable "<action>script", i.e. "updatescript" or
	   "installscript". If the variable does not exist, use
	   "<action>.scr" as default script name, i.e. "update.scr" or
	   "install.scr". */
	if (!fname) {
		sprintf(varname, "%sscript", action);
		fname = getenv(varname);
		if (!fname) {
			sprintf(varname, "%s.scr", action);
			fname = varname;
		}
	}

	/* Parse devices and check for script */
	c = *check;
	do {
		/* Skip any commas */
		while (c == ',')
		       c = *(++check);
		if (!c)
			break;

		/* Scan device name */
		devname = check;
		while ((c >= 'a') && (c <= 'z'))
		       c = *(++check);
		devnamelen = check - devname;

		/* Scan optional device number, default 0 */
		devnum = 0;
		while ((c >= '0') && (c <= '9')) {
			devnum = 10*devnum + c - '0';
			c = *(++check);
		}

		/* Scan optional partition number, default 1 */
		if (c != ':')
			partnum = 1;
		else {
			partnum = 0;
			c = *(++check);
			while ((c >= '0') && (c <= '9')) {
				partnum = 10*partnum + c - '0';
				c = *(++check);
			}
		}

		/* Search if device name is known */
		for (i = 0, p = upinfo; i < ARRAY_SIZE(upinfo); i++, p++) {
			if ((devnamelen == strlen(p->devname)) &&
			    !strncmp(devname, p->devname, devnamelen))
				break;
		}
		printf("---- Trying %s with %s from ", action, fname);
		if ((i < ARRAY_SIZE(upinfo)) && (!c || (c == ','))) {
			if (p->flags & UPDATE_FLAGS_NET)
				printf("%s%d ----\n", p->devname, devnum);
			else
				printf("%s%d:%d ----\n", p->devname, devnum,
				       partnum);
			if (p->update(addr, devnum, partnum, fname) < 0)
				puts("Failed!\n");
			else {
				puts("Success!\n");
				source(addr, NULL);

				/* If the script fails due to an error, we
				   return 0 nonetheless because otherwise we
				   might unintentionally boot the system */
				return 0;
			}
		} else {
			check = devname;
			c = *check;
			while (c && (c != ',')) {
				putc(c);
				c = *(++check);
			}
			puts(" ----\nUnknown device, ignored\n");
		}
	} while (c);
	printf("---- No %s script found ----\n", action);

	return 1;
}

int do_update(cmd_tbl_t *cmdtp, int flag, int argc, char * const argv[])
{
	char *check = NULL;
	char *fname = NULL;
	unsigned long addr = 0;

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

	/* Try update */
	return update_script((argv[0][0] == 'u') ? "update" : "install",
			     check, fname, addr);
}


U_BOOT_CMD(
	update, 4, 0,	do_update,
	"update system from external device",
	"[updatecheck [updatescript [updateaddr]]]\n"
	" - Check the devices given in updatecheck whether they contain a\n"
	"   scriptfile as given in updatescript. If the script is found\n"
	"   anywhere, load it to the address given in updateaddr and update\n"
	"   the system by sourcing (executing) it. Any missing argument is\n"
	"   replaced by the environment variable of the same name or, if no\n"
	"   such variable exists, by the default values 'mmc,usb' for\n"
	"   updatecheck, 'update.scr' for updatescript and $(loadaddr) for\n"
	"   updateaddr. You can use '.' to skip an argument.\n"
);

U_BOOT_CMD(
	install, 4, 0,	do_update,
	"install system from external device",
	"[installcheck [installscript [installaddr]]]\n"
	" - Check the devices given in installcheck whether they contain a\n"
	"   scriptfile as given in installscript. If the script is found\n"
	"   anywhere, load it to the address given in installaddr and install\n"
	"   the system by sourcing (executing) it. Any missing argument is\n"
	"   replaced by the environment variable of the same name or, if no\n"
	"   such variable exists, by the default values 'mmc,usb' for\n"
	"   installcheck, 'install.scr' for installscript and $(loadaddr) for\n"
	"   installaddr. You can use '.' to skip an argument.\n"
);

#endif /* CONFIG_CMD_UPDATE */

#endif /* CONFIG_CMD_SOURCE */
