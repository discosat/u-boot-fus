/*
 * (C) Copyright 2009 Faraday Technology
 * Po-Yu Chuang <ratbert@faraday-tech.com>
 *
 * Copyright (C) 2010 Andes Technology Corporation
 * Shawn Lin, Andes Technology Corporation <nobuhiro@andestech.com>
 * Macpaul Lin, Andes Technology Corporation <macpaul@andestech.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

#include <common.h>
#include <asm/io.h>
#include "ftpmu010.h"

static struct ftpmu010 *pmu = (struct ftpmu010 *)CONFIG_FTPMU010_BASE;

void ftpmu010_32768osc_enable(void)
{
	unsigned int oscc;

	/* enable the 32768Hz oscillator */
	oscc = readl(&pmu->OSCC);
	oscc &= ~(FTPMU010_OSCC_OSCL_OFF | FTPMU010_OSCC_OSCL_TRI);
	writel(oscc, &pmu->OSCC);

	/* wait until ready */
	while (!(readl(&pmu->OSCC) & FTPMU010_OSCC_OSCL_STABLE))
		;

	/* select 32768Hz oscillator */
	oscc = readl(&pmu->OSCC);
	oscc |= FTPMU010_OSCC_OSCL_RTCLSEL;
	writel(oscc, &pmu->OSCC);
}

void ftpmu010_dlldis_disable(void)
{
	unsigned int pdllcr0;

	pdllcr0 = readl(&pmu->PDLLCR0);
	pdllcr0 |= FTPMU010_PDLLCR0_DLLDIS;
	writel(pdllcr0, &pmu->PDLLCR0);
}

void ftpmu010_sdram_clk_disable(unsigned int cr0)
{
	unsigned int pdllcr0;

	pdllcr0 = readl(&pmu->PDLLCR0);
	pdllcr0 |= FTPMU010_PDLLCR0_HCLKOUTDIS(cr0);
	writel(pdllcr0, &pmu->PDLLCR0);
}
