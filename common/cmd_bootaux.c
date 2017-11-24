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
	ulong addr = 0;
	int ret = 0;
	enum aux_state state;

	state = arch_auxiliary_core_get(0);
	if (state == aux_undefined) {
		state = aux_off;
		arch_auxiliary_core_set(0, state, addr);
	}

	if (argc > 1) {
		if (!strcmp(argv[1], "start") && (state == aux_off))
			state = aux_stopped;
		else if (!strcmp(argv[1], "stop"))
			state = aux_stopped;
		else if (!strcmp(argv[1], "pause") && (state == aux_running))
			state = aux_paused;
		else if (!strcmp(argv[1], "continue") && (state == aux_paused))
			state = aux_running;
		else if (!strcmp(argv[1], "off"))
			state = aux_off;
		else if (!strcmp(argv[1], "run") || parse_loadaddr(argv[1], NULL))
		{
			state = aux_running;
			if(!strcmp(argv[1], "run"))
				addr = M4_BOOTROM_BASE_ADDR;
			else
				addr = parse_loadaddr(argv[1], NULL);
		}
		else
			state = aux_undefined;

		if (state != aux_undefined) {
			ret = arch_auxiliary_core_set(0, state, addr);
			if (ret) {
				printf("Starting auxiliary core failed\n");
				return CMD_RET_FAILURE;
			}
		}
	}

	state = arch_auxiliary_core_get(0);

	/* Print auxiliary core state */
	if (state == aux_off)
		printf("auxiliary core off\n");
	else if (state == aux_stopped)
		printf("auxiliary core stopped\n");
	else if (state == aux_running)
		printf("auxiliary core running\n");
	else
		printf("auxiliary core paused\n");

	return CMD_RET_SUCCESS;
}

U_BOOT_CMD(
	bootaux, CONFIG_SYS_MAXARGS, 1,	do_bootaux,
	"Handle auxiliary core",
	"start|stop - Start/stop auxiliary clock (required for TCM access)\n"
	"bootaux addr - Start software image at addr on auxiliary core\n"
	"bootaux - Show auxiliary core state\n"
);
