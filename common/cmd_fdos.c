/*
 * (C) Copyright 2002
 * Stäubli Faverges - <www.staubli.com>
 * Pierre AUBERT  p.aubert@staubli.com
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
 * Dos floppy support
 */

#include <common.h>
#include <config.h>
#include <command.h>
#include <fdc.h>

/*-----------------------------------------------------------------------------
 * do_fdosboot --
 *-----------------------------------------------------------------------------
 */
int do_fdosboot(cmd_tbl_t *cmdtp, int flag, int argc, char * const argv[])
{
    char *name;
    int size;
    int drive = CONFIG_SYS_FDC_DRIVE_NUMBER;
    ulong addr = get_loadaddr();

    /* pre-set Boot file name */
    if ((name = getenv("bootfile")) == NULL) {
	name = "uImage";
    }

    switch (argc) {
    case 1:
	break;
    case 2:
	/* only one arg - accept two forms:
	 * just load address, or just boot file name.
	 * The latter form must be written "filename" here.
	 */
	if (argv[1][0] == '"') {	/* just boot filename */
	    name = argv [1];
	} else {			/* load address	*/
	    addr = parse_loadaddr(argv[1], NULL);
	}
	break;
    case 3:
	addr = parse_loadaddr(argv[1], NULL);
	name = argv [2];
	break;
    default:
	return CMD_RET_USAGE;
    }
    set_loadaddr(addr);

    /* Init physical layer                                                   */
    if (!fdc_fdos_init (drive)) {
	return (-1);
    }

    /* Open file                                                             */
    if (dos_open (name) < 0) {
	printf ("Unable to open %s\n", name);
	return 1;
    }
    if ((size = dos_read (addr)) < 0) {
	printf ("boot error\n");
	return 1;
    }
    flush_cache (addr, size);

    setenv_hex("filesize", size);

    printf("Floppy DOS load complete: %d bytes loaded to 0x%lx\n",
	   size, addr);

    return bootm_maybe_autostart(cmdtp, argv[0]);
}

/*-----------------------------------------------------------------------------
 * do_fdosls --
 *-----------------------------------------------------------------------------
 */
int do_fdosls(cmd_tbl_t *cmdtp, int flag, int argc, char * const argv[])
{
    char *path = "";
    int drive = CONFIG_SYS_FDC_DRIVE_NUMBER;

    switch (argc) {
    case 1:
	break;
    case 2:
	path = argv [1];
	break;
    }

    /* Init physical layer                                                   */
    if (!fdc_fdos_init (drive)) {
	return (-1);
    }
    /* Open directory                                                        */
    if (dos_open (path) < 0) {
	printf ("Unable to open %s\n", path);
	return 1;
    }
    return (dos_dir ());
}

U_BOOT_CMD(
	fdosboot,	3,	0,	do_fdosboot,
	"boot from a dos floppy file",
	"[loadAddr] [filename]"
);

U_BOOT_CMD(
	fdosls,	2,	0,	do_fdosls,
	"list files in a directory",
	"[directory]"
);
