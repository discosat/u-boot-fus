/*
 * (C) Copyright 2000
 * Wolfgang Denk, DENX Software Engineering, wd@denx.de.
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
 * Boot support
 */
#include <common.h>
#include <command.h>
#include <stdio_dev.h>

int do_coninfo (cmd_tbl_t * cmd, int flag, int argc, char * const argv[])
{
	int l;
	struct stdio_dev *pdev;
	struct stdio_dev *pdevstart;

	/* Scan for valid output and input devices */

	puts ("List of available devices:\n");

	pdevstart = stdio_get_list();
	if (pdevstart) {
		pdev = pdevstart;
		do {
			printf("%-16s%08x %c%c%c ",
			       pdev->name, pdev->flags,
			       (pdev->flags & DEV_FLAGS_SYSTEM) ? 'S' : '.',
			       (pdev->flags & DEV_FLAGS_INPUT) ? 'I' : '.',
			       (pdev->flags & DEV_FLAGS_OUTPUT) ? 'O' : '.');

			for (l = 0; l < MAX_FILES; l++) {
#ifdef CONFIG_CONSOLE_MUX
				struct stdio_dev *p;

				for (p = stdio_devices[l]; p;
				     p = p->file_next[l]) {
					if (p == pdev) {
						printf("%s ", stdio_names[l]);
						break;
					}
				}
#else
				if (stdio_devices[l] == pdev)
					printf("%s ", stdio_names[l]);
#endif
			}
			putc ('\n');
			pdev = pdev->next;
		} while (pdev != pdevstart);
	}
	return 0;
}


/***************************************************/

U_BOOT_CMD(
	coninfo,	3,	1,	do_coninfo,
	"print console devices and information",
	""
);
