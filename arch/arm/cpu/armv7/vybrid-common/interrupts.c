/*
 * (C) Copyright 2014
 * F&S Elektronik Systeme GmbH <www.fs-net.de>
 *
 * SPDX-License-Identifier:	GPL-2.0+
 */

#include <common.h>
#include <asm/io.h>
#include <asm/arch/vybrid-regs.h>	/* CA5SCU_ICCIAR, N_IRQS, ... */
#include <irq_func.h>

struct _irq_handler {
	void *data;
	interrupt_handler_t *isr;
};

static struct _irq_handler irq_handler[N_IRQS];

void default_isr(void *data)
{
	printf("*** Unhandled IRQ %d ***\n", (int)data);
}

void do_irq(struct pt_regs *pt_regs)
{
	u32 iar;
	int irq;

	/* Acknowledge interrupt by reading Interrupt Acknowledge Register IAR:
	   [12:10] is the signalling CPU, [9:0] is the interrupt ID */
	iar = __raw_readl(GICC_IAR);
	irq = (int)(iar & 0x3ff);
	if (irq < N_IRQS)
		irq_handler[irq].isr(irq_handler[irq].data);

	/* Signal end of interrupt handling by writing the previous IAR value
	   to the End Of Interrupt Register EOIR */
	if (irq < 1020)
		__raw_writel(iar, GICC_EOIR);
}

static inline void gic_enable_irq(int irq)
{
	int offs = (irq / 32) * 4;
	int bit  = 1 << (irq % 32);

	/* Route the interrupt to CPU0 (= Cortex A5) */
	if (irq >= 32)
		__raw_writew(MSCM_IRSPRC_CP0E, MSCM_IRSPRC + 2 * (irq-32));

	/* Handle as non-secure interrupt in Interrupt Security Register ISR */
	__raw_writel(bit, GICD_ISR + offs);

	/* Enable by writing to Interrupt Set-Enable Register ISER */
	__raw_writel(bit, GICD_ISER + offs);
}

static inline void gic_disable_irq(int irq)
{
	/* Disable by writing to Interrupt Clear-Enable Register ICER */
	__raw_writel(1 << (irq % 32), GICD_ICER + (irq / 32) * 4);
}

void irq_install_handler(int irq, interrupt_handler_t *isr, void *data)
{
	if ((irq < N_IRQS) && (irq_handler[irq].isr == default_isr)) {
		irq_handler[irq].data = data;
		irq_handler[irq].isr = isr;
		gic_enable_irq(irq);
	}
}

void irq_free_handler(int irq)
{
	if (irq < N_IRQS) {
		gic_disable_irq(irq);
		irq_handler[irq].isr = default_isr;
		irq_handler[irq].data = (void *)irq;
	}
}

int arch_interrupt_init(void)
{
	int irq;

	/* Install default interrupt handlers, disable interrupts */
	for (irq = 0; irq < N_IRQS; irq++) {
		irq_handler[irq].data = (void *)irq;
		irq_handler[irq].isr = default_isr;
		gic_disable_irq(irq);
	}

	/* Set Priority Mask Register PMR to lowest priority, i.e. let all
	   interrupts through to the CPU */
	__raw_writel(0xff, GICC_PMR);

	/* Enable signaling of both groups of interrupts in ICR */
	__raw_writel(0x7, GICC_ICR);

	/* Enable both groups of interrupts in DCR */
	__raw_writel(0x3, GICD_DCR);

	return 0;
}
