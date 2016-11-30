/*
 * Hardware specific part for blink timer on i.MX6 CPUs
 *
 * (C) Copyright 2016
 * F&S Elektronik Systeme GmbH <www.fs-net.de>
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
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.	 See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston,
 * MA 02111-1307 USA
 */

#include <common.h>
#include <errno.h>			/* ENXIO */
#include <asm/io.h>			/* writel(), readl() */
#include <asm/arch/imx-regs.h>		/* struct epit, EPIT1_BASE_ADDR */
#include <asm/arch/clock.h>		/* MXC_HCLK */
#include <asm/arch/crm_regs.h>		/* MXC_CCM_CCGR1_EPIT1S_MASK, ... */

static unsigned int shift;
static unsigned int use_count;

static void blink_isr(void *data)
{
	struct epit_regs *epit = (struct epit_regs *)EPIT1_BASE_ADDR;

	run_blink_callbacks();

	/* Clear output compare interrupt flag to remove interrupt source */
	writel(1, &epit->sr);
}

void set_blink_timer(unsigned int blink_delay)
{
	struct epit_regs *epit = (struct epit_regs *)EPIT1_BASE_ADDR;

	writel(blink_delay << shift, &epit->lr);
}

int start_blink_timer(unsigned int blink_delay)
{
	u32 val;
	struct epit_regs *epit = (struct epit_regs *)EPIT1_BASE_ADDR;
	struct mxc_ccm_reg *imx_ccm = (struct mxc_ccm_reg *)CCM_BASE_ADDR;
	unsigned int retries = 100;
	u32 prescaler;

	if (use_count++) {
		set_blink_timer(blink_delay);
		return 0;
	}

	/* Enable EPIT clock and reset EPIT */
	writel(readl(&imx_ccm->CCGR1) | MXC_CCM_CCGR1_EPIT1S_MASK,
	       &imx_ccm->CCGR1);
	writel(EPIT_CR_SWR, &epit->cr);

	/*
	 * Try to use 1kHz as EPIT frequency. If this is not possible, try to
	 * double the frequency until the prescaler is <= 4096. For example
	 * for a 24MHz source clock we have to multiply by 8 (shift by 3) to
	 * 8kHz to get a prescaler of 3000.
	 */
	prescaler = mxc_get_clock(MXC_IPG_PERCLK) / 1000;
	shift = 0;
	while (prescaler > 4096) {
		prescaler >>= 1;
		shift++;
	}

	/* Wait until EPIT comes out of reset */
	do {
		if (!(readl(&epit->cr) & EPIT_CR_SWR))
			break;
	} while (--retries);
	if (!retries) {
		printf("blink_timer: EPIT does not come out of reset\n");
		return -ENXIO;
	}

	/* Add an interrupt handler to be called when timer expires */
	irq_install_handler(IRQ_EPIT1, blink_isr, NULL);

	/* Set clock source and prescaler */
	val = EPIT_CR_PRESCALER(prescaler);
	val |= EPIT_CR_CLKSRC_24M | EPIT_CR_IOVW | EPIT_CR_RLD;
	writel(val, &epit->cr);

	/* Activate interrupt and enable EPIT */
	val |= EPIT_CR_EN | EPIT_CR_OCIEN;
	writel(val, &epit->cr);

	set_blink_timer(blink_delay);

	return 0;
}

void stop_blink_timer(void)
{
	struct epit_regs *epit = (struct epit_regs *)EPIT1_BASE_ADDR;
	struct mxc_ccm_reg *imx_ccm = (struct mxc_ccm_reg *)CCM_BASE_ADDR;

	use_count = 0;

	/* Disable EPIT and stop clock */
	writel(0, &epit->cr);
	writel(readl(&imx_ccm->CCGR1) & ~MXC_CCM_CCGR1_EPIT1S_MASK,
	       &imx_ccm->CCGR1);

	/* Remove interrupt handler */
	irq_free_handler(IRQ_EPIT1);
}
