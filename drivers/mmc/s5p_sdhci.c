/*
 * (C) Copyright 2012 SAMSUNG Electronics
 * Jaehoon Chung <jh80.chung@samsung.com>
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
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#include <common.h>
#include <malloc.h>
#include <sdhci.h>
#include <asm/arch/cpu.h>		  /* samsung_get_base_mmc() */
#include <asm/arch/mmc.h>

//static char *S5P_NAME = "SAMSUNG SDHCI";
static char *S5P_NAME = "s5p_sdhci";
static void s5p_sdhci_set_control_reg(struct sdhci_host *host)
{
	unsigned long val, ctrl;
	/*
	 * SELCLKPADDS[17:16]
	 * 00 = 2mA
	 * 01 = 4mA
	 * 10 = 7mA
	 * 11 = 9mA
	 */
	sdhci_writel(host, SDHCI_CTRL4_DRIVE_MASK(0x3), SDHCI_CONTROL4);

	val = sdhci_readl(host, SDHCI_CONTROL2);
	val &= SDHCI_CTRL2_SELBASECLK_SHIFT;

	val |=	SDHCI_CTRL2_ENSTAASYNCCLR |
		SDHCI_CTRL2_ENCMDCNFMSK |
		SDHCI_CTRL2_ENFBCLKRX |
		SDHCI_CTRL2_ENCLKOUTHOLD;

	sdhci_writel(host, val, SDHCI_CONTROL2);

	/*
	 * FCSEL3[31] FCSEL2[23] FCSEL1[15] FCSEL0[7]
	 * FCSel[3:2] : Tx Feedback Clock Delay Control
	 * FCSel[1:0] : Rx Feedback Clock Delay Control
	 *      00, 01: Inverter delay: output with rising edge of CLK, i.e.
	 *              about +10ns @50MHz as one full clock cycle is 20ns
	 *      10, 11: Basic delay: output with falling edge of CLK, plus
	 *              about 2ns @50MHz
	 * ### 19.12.2012 HK: Settings above are verified on S5PV210!
	 */
	val = SDHCI_CTRL3_FCSEL3 | SDHCI_CTRL3_FCSEL1;
	sdhci_writel(host, val, SDHCI_CONTROL3);

	/*
	 * SELBASECLK[5:4]
	 * 00/01 = HCLK
	 * 10 = SCLK_MMC from SYSCON
	 * 11 = reserved
	 */
	ctrl = sdhci_readl(host, SDHCI_CONTROL2);
	ctrl &= ~SDHCI_CTRL2_SELBASECLK_MASK(0x3);
	ctrl |= SDHCI_CTRL2_SELBASECLK_MASK(0x2);
	sdhci_writel(host, ctrl, SDHCI_CONTROL2);
}

int s5p_sdhci_init(u32 regbase, u32 max_clk, u32 min_clk, u32 quirks)
{
	struct sdhci_host *host = NULL;
	host = (struct sdhci_host *)malloc(sizeof(struct sdhci_host));
	if (!host) {
		printf("sdhci__host malloc fail!\n");
		return 1;
	}

	host->name = S5P_NAME;
	host->ioaddr = (void *)regbase;
	host->quirks = quirks;

	//### 19.12.2012 HK: We actually do have the HISPD bit (even if it is
	//### called OUTEDGEINV in the Samsung docu), but it does not make
	//### sense (and does not work) if it is set. Everything is OK if the
	//### HISPD bit stays 0 even on clocks > 25MHz. The voltages look
	//### perfectly right, so no reason to have a quirk here.
	host->quirks |= SDHCI_QUIRK_NO_HISPD_BIT;
	//host->quirks |= SDHCI_QUIRK_NO_HISPD_BIT | SDHCI_QUIRK_BROKEN_VOLTAGE;
	//host->voltages = MMC_VDD_32_33 | MMC_VDD_33_34 | MMC_VDD_165_195;
	host->version = sdhci_readw(host, SDHCI_HOST_VERSION);

	/* Throw away manufacturer info, just keep host controller version */
	host->version &= 0x00ff;

	host->set_control_reg = &s5p_sdhci_set_control_reg;

	host->host_caps = MMC_MODE_HC;

	add_sdhci(host, max_clk, min_clk);
	return 0;
}
