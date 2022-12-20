/*
 * (C) Copyright 2007
 * Sascha Hauer, Pengutronix
 *
 * Copyright 2012 Freescale Semiconductor, Inc.
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

#include <common.h>
#include <netdev.h>
#include <linux/errno.h>
#include <asm/io.h>
#include <asm/arch/vybrid-regs.h>
#include <asm/arch/clock.h>
#include <asm/arch/sys_proto.h>

#ifdef CONFIG_FSL_ESDHC
#include <fsl_esdhc.h>
#endif

const char *get_reset_cause(void)
{
	DECLARE_GLOBAL_DATA_PTR;
	const char *const resetcause[32] = {
		"Power-on Reset",
		"Cortex A5 WDOG Timer Reset",
		NULL,
		"CA5 WDOG reset",
		"CM4 WDOG reset",
		"JTAG HIGH-Z",
		NULL,
		"External Reset",
		"1.2V supply below 0.7V",
		"HP regulator's LVD",
		"ULP regulator's LVD",
		"3.3V main supply is unstable",
		"LP regulator's LVD",
		NULL,
		NULL,
		NULL,
		"MDM-AP system reset request is set",
		"Hard Fail State of System Security Monitor",
		"SRC_SCR SW Reset is set",
		"Platform's CSU alarm event",
		NULL,
		NULL,
		NULL,
		NULL,
		"Anadig regulator 1.1V unstable",
		"Anadig regulator 2.5V unstable",
		"Anadig regulator 3.0V unstable",
		"CMU even when FOSC freq less than 40MHz",
		"CMU event when BUS freq is out of range",
		"No clock is detected on FOSC",
		"No clock is detected on SOSC",
		"CM4 is in lockup"
	};
	int i;
	u32 cause = gd->arch.reset_cause;

	if (cause) {
		for (i = 0; i < 32; i++) {
			if ((cause & (1 << i)) && resetcause[i])
				return resetcause[i];
		}
	}

	return "unknown reset";
}

#if defined(CONFIG_DISPLAY_CPUINFO)
int print_cpuinfo(void)
{
	DECLARE_GLOBAL_DATA_PTR;
	u32 cpurev;
	u32 cause;
	struct src *src_regs = (struct src *)SRC_BASE_ADDR;

	cpurev = get_cpu_rev();
	printf("CPU:   Freescale Vybrid %x family rev%d.%d at %d MHz\n",
		(cpurev & 0xFFF000) >> 12,
		(cpurev & 0x000F0) >> 4,
		(cpurev & 0x0000F) >> 0,
		vybrid_get_clock(VYBRID_ARM_CLK) / 1000000);

	cause = readl(&src_regs->srsr);
	writel(cause, &src_regs->srsr);
	gd->arch.reset_cause = cause;

	printf("Reset: %s\n", get_reset_cause());

	return 0;
}
#endif

#ifdef CONFIG_CMD_NET
int cpu_eth_init(struct bd_info *bis)
{
	int rc = -ENODEV;

#if defined(CONFIG_FEC_MXC) && defined(CONFIG_FEC_MXC_PHYADDR)
        rc = fecmxc_initialize(bis);
#endif

	return rc;
}
#endif

/* Initializes on-chip MMC controllers.
 * to override, implement board_mmc_init()
 */
int cpu_mmc_init(struct bd_info *bis)
{
#ifdef CONFIG_FSL_ESDHC
	return fsl_esdhc_mmc_init(bis);
#else
	return 0;
#endif
}

void reset_cpu(ulong addr)
{
	__raw_writew(4, WDOG_A5_BASE_ADDR);
}
