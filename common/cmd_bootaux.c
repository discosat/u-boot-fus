/*
 * Copyright (C) 2014 Freescale Semiconductor, Inc.
 *
 * SPDX-License-Identifier:	GPL-2.0+
 */

#include <common.h>
#include <command.h>
#include <asm/arch/sys_proto.h>		/* arch_auxiliary_*() */

/*
 * To i.MX6SX and i.MX7D, the image supported by bootaux needs
 * the reset vector at the head for the image, with SP and PC
 * as the first two words.
 *
 * Per the cortex-M reference manual, the reset vector of M4 needs
 * to exist at 0x0 (TCMUL). The PC and SP are the first two addresses
 * of that vector.  So to boot M4, the A core must build the M4's reset
 * vector with getting the PC and SP from image and filling them to
 * TCMUL. When M4 is kicked, it will load the PC and SP by itself.
 * The TCMUL is mapped to (M4_BOOTROM_BASE_ADDR) at A core side for
 * accessing the M4 TCMUL.
 */
int do_bootaux(cmd_tbl_t *cmdtp, int flag, int argc, char * const argv[])
{
	ulong addr;
	int ret = 0;

	if (argc > 1) {
		if (!strcmp(argv[1], "start"))
			arch_auxiliary_clock_enable(0, 1);
		else if (!strcmp(argv[1], "stop"))
			arch_auxiliary_clock_enable(0, 0);
		else if (!arch_auxiliary_core_check_up(0)) {
			addr = parse_loadaddr(argv[1], NULL);
			ret = arch_auxiliary_core_up(0, addr);
			if (ret) {
				printf("Starting auxiliary core failed\n");
				return CMD_RET_FAILURE;
			}
		}
	}

	/* Print auxiliary core state */
	printf("Auxiliary clock %sabled, auxiliary core %s\n",
	       arch_auxiliary_clock_check(0) ? "en" : "dis",
	       arch_auxiliary_core_check_up(0) ? "running" : "stopped");

	return CMD_RET_SUCCESS;
}

U_BOOT_CMD(
	bootaux, CONFIG_SYS_MAXARGS, 1,	do_bootaux,
	"Handle auxiliary core",
	"start|stop - Start/stop auxiliary clock (required for TCM access)\n"
	"bootaux addr - Start software image at addr on auxiliary core\n"
	"bootaux - Show auxiliary core state\n"
);
