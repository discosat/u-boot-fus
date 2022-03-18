// SPDX-License-Identifier: GPL-2.0+
/*
 * Copyright (C) 2016 Freescale Semiconductor, Inc.
 * Copyright (C) 2018 F&S Elektronik Systeme GmbH
 */

#include <common.h>
#include <asm/io.h>
#ifndef CONFIG_ARCH_MX7ULP
#include <asm/arch/crm_regs.h>
#else
#include <asm/arch-mx7ulp/sys_proto.h>
#endif
#include <asm/mach-imx/sys_proto.h>
#include <command.h>
#include <imx_sip.h>
#include <linux/compiler.h>

/* For the bootaux command we implemented a state machine to switch between
 * the different modes. Below you find the bit coding for the state machine.
 * The different states will be set in the arch_auxiliary_core_set function and
 * to get the current state you can use the function arch_auxiliary_core_get.
 * If we get a undefined state we will immediately set the state to aux_off.
 */
/******************************************************************************
****************|              bit coding             |   state   |************
-------------------------------------------------------------------------------
****************| assert_reset | m4_clock | m4_enable |   state   |************
===============================================================================
****************|       1      |     0    |     0     |    off    |************
****************|       1      |     1    |     1     |  stopped  |************
****************|       0      |     1    |     1     |  running  |************
****************|       0      |     0    |     1     |  paused   |************
****************|       0      |     0    |     0     | undefined |************
****************|       0      |     1    |     0     | undefined |************
****************|       1      |     0    |     1     | undefined |************
****************|       1      |     1    |     0     | undefined |************
*******************************************************************************
**|   transitions    |   state   |            transitions             |********
*******************************************************************************
                      -----------
                      |   OFF   |
                      -----------
          |          |           ^           ^                   ^
          |    Start |           |  off      |                   |
          |          v           |           |                   |
          |           -----------            |                   |
    run/  |           | Stopped |            | off               |
    addr  |           -----------            |                   |
          |          |           ^           |           ^       |
          | run/addr |           | stop      |           |       | off
          v          v           |                       |       |
                      -----------                        |       |
                      | Running |                        | stop  |
                      -----------                        |       |
                     |           ^                       |       |
               pause |           | continue              |       |
                     v           | run/addr (restart)    |       |
                      -----------
                      | Paused  |
                      -----------
******************************************************************************/
enum aux_state {
	aux_off,
	aux_stopped,
	aux_running,
	aux_paused,
	aux_undefined,
};

static enum aux_state cur_state = aux_stopped;

#ifndef CONFIG_ARCH_MX7ULP
int arch_auxiliary_core_set_reset_address(ulong boot_private_data)
{
	u32 stack, pc;

	if (!boot_private_data)
		return 1;

	if (boot_private_data != M4_BOOTROM_BASE_ADDR) {
		stack = *(u32 *)boot_private_data;
		pc = *(u32 *)(boot_private_data + 4);

		/* Set the stack and pc to M4 bootROM */
		writel(stack, M4_BOOTROM_BASE_ADDR);
		writel(pc, M4_BOOTROM_BASE_ADDR + 4);
	}

	return 0;
}
#else
int arch_auxiliary_core_set_reset_address(ulong boot_private_data)
{
	u32 *dest_addr = (u32 *)boot_private_data;
	u32 pc = 0, tag = 0;

	if (boot_private_data != TCML_BASE)
	{
		printf("Address != 0x%x, abort!\n", TCML_BASE);
		return 1;
	}

	/* Set GP register to tell the M4 rom the image entry */
	/* We assume the M4 image has IVT head and padding which
	 * should be same as the one programmed into QSPI flash
	 */
	tag = *(dest_addr + 1024);
	if (tag != 0x402000d1 && tag !=0x412000d1)
		return -1;

	pc = *(dest_addr + 1025);

	writel(pc, SIM0_RBASE + 0x70); /*GP7*/

	return 0;
}

#endif

void arch_auxiliary_core_set(u32 core_id, enum aux_state state)
{
#if defined(CONFIG_IMX8M) || defined(CONFIG_IMX8MN)
        /* TODO: Currently only start state */
        if (state == aux_off || state == aux_stopped)
		call_imx_sip(IMX_SIP_SRC, IMX_SIP_SRC_M4_START, 0, 0, 0);
#elif CONFIG_ARCH_MX7ULP
	/* There is no way to switch between states. We are only able to
	 * start images if there is no pc set and no image is located in
	 * TCML.
	 */
	;
#else
	struct src *src_reg = (struct src *)SRC_BASE_ADDR;
	struct mxc_ccm_reg *mxc_ccm = (struct mxc_ccm_reg *)CCM_BASE_ADDR;

	if (state == aux_off || state == aux_stopped)
		/* Assert SW reset, i.e. stop M4 if running */
		setbits_le32(&src_reg->scr, 0x00000010);

	if (state == aux_off)
		/* Disable M4 */
		clrbits_le32(&src_reg->scr, 0x00400000);

	if (state == aux_off || state == aux_paused)
		/* Disable M4 clock */
		clrbits_le32(&mxc_ccm->CCGR3, MXC_CCM_CCGR3_M4_MASK);

	if (state == aux_stopped || state == aux_running)
		/* Enable M4 clock */
		setbits_le32(&mxc_ccm->CCGR3, MXC_CCM_CCGR3_M4_MASK);

	if (!(state == aux_off)) {
		/* Enable M4 */
		setbits_le32(&src_reg->scr, 0x00400000);
	}

	if (state == aux_running || state == aux_paused)
		/* Assert SW reset, i.e. stop M4 if running */
		clrbits_le32(&src_reg->scr, 0x00000010);
#endif
}

enum aux_state arch_auxiliary_core_get(u32 core_id)
{
#if defined(CONFIG_IMX8M) || defined(CONFIG_IMX8MN)
        /* TODO: check reg values mapping to the state */
	int reg = call_imx_sip(IMX_SIP_SRC, IMX_SIP_SRC_M4_STARTED, 0, 0, 0);
        
	if(reg)
		return aux_running;
        
        return aux_stopped; 
#elif CONFIG_ARCH_MX7ULP
	/* 7ULP always running, there is no possibility to shutdown any clock.
	 * We assume if reset state is POR and the local variable cur_state is
	 * set to aux_stopped we are in so called "stop" state.
	 */
	if (get_boot_mode() != SINGLE_BOOT || strcmp(get_reset_cause(), "POR")
			|| cur_state != aux_stopped)
		return aux_running;
	else
		return aux_stopped;
#else
	struct src *src_reg = (struct src *)SRC_BASE_ADDR;
	struct mxc_ccm_reg *mxc_ccm = (struct mxc_ccm_reg *)CCM_BASE_ADDR;
	int flags = 0;
	int reg = 0;

	reg = readl(&src_reg->scr);
	if (reg & 0x00000010)
		flags |= 0x4;
	if (reg & 0x00400000)
		flags |= 0x1;
	reg = readl(&mxc_ccm->CCGR3);
	if (reg & MXC_CCM_CCGR3_M4_MASK)
		flags |= 0x2;

	switch (flags)
	{
		case 0x4:
			return aux_off;
		case 0x7:
			return aux_stopped;
		case 0x3:
			return aux_running;
		case 0x1:
			return aux_paused;
	}

	return aux_undefined;
#endif
}

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
static int do_bootaux(cmd_tbl_t *cmdtp, int flag, int argc, char * const argv[])
{
	const char *state_name[] = {"off", "stopped", "running", "paused"};
	ulong addr = 0;
	int ret = 0;
	enum aux_state state;

#ifdef CONFIG_ARCH_MX7ULP
	if (get_boot_mode() != SINGLE_BOOT) {
		printf("Board not in single boot mode, abort!\n");
		return CMD_RET_FAILURE;
	}
#endif

	state = arch_auxiliary_core_get(0);
	if (state == aux_undefined) {
		state = aux_off;
		arch_auxiliary_core_set(0, state);
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
		else if (!strcmp(argv[1], "run") ||
			(argv[1][0] >= '0' && argv[1][0] <= '9'))
		{
			if(!strcmp(argv[1], "run"))
				addr = M4_BOOTROM_BASE_ADDR;
			else
				addr = parse_loadaddr(argv[1], NULL);

			state = aux_stopped;
			arch_auxiliary_core_set(0, state);

			if (arch_auxiliary_core_get(0) != aux_stopped) {
				printf("Aux core still running, abort!\n");
		return CMD_RET_FAILURE;
			}

			ret = arch_auxiliary_core_set_reset_address(addr);
			if (ret) {
				printf("Bad address\n");
				return CMD_RET_FAILURE;
			}

			state = aux_running;
			cur_state = aux_running;
		}
		else {
			printf("Command %s unknown or not allowed if auxiliary"
				    " core %s!\n", argv[1], state_name[state]);
			return CMD_RET_FAILURE;
		}

		arch_auxiliary_core_set(0, state);
	}

	state = arch_auxiliary_core_get(0);
	/* Print auxiliary core state */
	printf("auxiliary core %s\n", state_name[state]);

	return CMD_RET_SUCCESS;
}

U_BOOT_CMD(
	bootaux, CONFIG_SYS_MAXARGS, 1,	do_bootaux,
	"Handle auxiliary core",
	"start|stop - Start/stop auxiliary clock (required for TCM access)\n"
	"bootaux addr - Start software image at addr on auxiliary core\n"
	"bootaux - Show auxiliary core state\n"
);
