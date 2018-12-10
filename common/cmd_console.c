/*
 * (C) Copyright 2000
 * Wolfgang Denk, DENX Software Engineering, wd@denx.de.
 *
 * SPDX-License-Identifier:	GPL-2.0+
 */

/*
 * Boot support
 */
#include <common.h>
#include <command.h>
#include <stdio_dev.h>

extern void _do_coninfo (void);
static int do_coninfo(cmd_tbl_t *cmd, int flag, int argc, char * const argv[])
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
			printf("%-16s%08x %c%c ",
			       pdev->name, pdev->flags,
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
