/*
 * (C) Copyright 2000-2003
 * Wolfgang Denk, DENX Software Engineering, wd@denx.de.
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
#include <asm/arch/vybrid-regs.h>
#include <asm/arch/clock.h>
#include <asm/io.h>

DECLARE_GLOBAL_DATA_PTR;

int get_clocks(void)
{
	/* ###TODO### Remove all the following settings */
	/* gd->bus_clk is used in the FEC driver. This should be moved there */
	gd->bus_clk = vybrid_get_clock(VYBRID_IPG_CLK);

	/* gd->ipg_clk will be set in timer.c:timer_init() */
	//gd->ipg_clk = vybrid_get_clock(VYBRID_IPG_CLK);

#ifdef CONFIG_FSL_ESDHC
	/* ### Remark: This file is only built if CONFIG_FSL_ESDHC is set
	   anyway, so this define is a no-op. And it shows that it is
	   completely wrong to init other values than gd->sdhc_clk here. */
	/* gd->sdhc_clk will be set in fsvybrid.c:board_mmc_init() */
	//gd->sdhc_clk = vybrid_get_esdhc_clk(1);
#endif

	return 0;
}
