/*
 * Hardware specific part for blink timer on Vybrid CPU
 *
 * (C) Copyright 2014
 * F&S Elektronik Systeme GmbH <www.fs-net.de>
 *
 * SPDX-License-Identifier:	GPL-2.0+
 */

#include <common.h>
#include <blink.h>
#include <asm/io.h>
#include <asm/arch/vybrid-regs.h>	/* CA5SCU_ICCIAR, N_IRQS, ... */

static void blink_isr(void *data)
{
	run_blink_callbacks();

	/* Clear timer compare flag to remove interrupt source */
	__raw_writel(__raw_readl(LPTMR0_CSR) | LPTMR_CSR_TCF, LPTMR0_CSR);
}

void set_blink_timer(unsigned int blink_delay)
{
	/* Disable Low Power Timer to allow modifying values */
	__raw_writel(0, LPTMR0_CSR);

	/* Select 1kHz clock, disable prescaler, set compare value for
	   blink_rate and enable Low Power Timer */
	__raw_writel(blink_delay, LPTMR0_CMR);
	__raw_writel((1 << LPTMR_PSR_PCS_SHIFT) | LPTMR_PSR_PBYP, LPTMR0_PSR);
	__raw_writel(LPTMR_CSR_TIE | LPTMR_CSR_TEN, LPTMR0_CSR);
}

int start_blink_timer(unsigned int blink_delay)
{
	/* Set timer parameters */
	set_blink_timer(blink_delay);

	/* Add an interrupt handler to be called when timer expires */
	irq_install_handler(IRQ_LPTimer0, blink_isr, NULL);

	return 0;
}

void stop_blink_timer(void)
{
	/* Disable Low Power Timer */
	__raw_writel(0, LPTMR0_CSR);

	/* Remove interrupt handler */
	irq_free_handler(IRQ_LPTimer0);
}
