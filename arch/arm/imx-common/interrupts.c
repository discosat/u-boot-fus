/*
 * (C) Copyright 2014
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
#include <asm/io.h>
#include <asm/arch/imx-regs.h>	/* CA5SCU_ICCIAR, N_IRQS, ... */

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
	struct gicc_regs *gicc = (struct gicc_regs *)IC_INTERFACES_BASE_ADDR;

	/* Acknowledge interrupt by reading Interrupt Acknowledge Register:
	   [12:10] is the signalling CPU, [9:0] is the interrupt ID */
	iar = readl(&gicc->iar);
	irq = (int)(iar & 0x3ff);
	if (irq < N_IRQS)
		irq_handler[irq].isr(irq_handler[irq].data);

	/* Signal end of interrupt handling by writing the previous iar value
	   to the End Of Interrupt Register */
	if (irq < 1020)
		writel(iar, &gicc->eoir);
}

static inline void gic_enable_irq(int irq)
{
	struct gicd_regs *gicd = (struct gicd_regs *)IC_DISTRIBUTOR_BASE_ADDR;
	int idx = (irq / 32);
	int bit  = 1 << (irq % 32);

	/* Clear pending flag and enable interrupt */
	writel(bit, &gicd->icpendr[idx]);
	writel(bit, &gicd->isenabler[idx]);
}

static inline void gic_disable_irq(int irq)
{
	struct gicd_regs *gicd = (struct gicd_regs *)IC_DISTRIBUTOR_BASE_ADDR;

	/* Disable by writing 1 to interrupt clear enable bit */
	writel(1 << (irq % 32), &gicd->icenabler[irq / 32]);
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
	int irq, i;
	struct gicd_regs *gicd = (struct gicd_regs *)IC_DISTRIBUTOR_BASE_ADDR;
	struct gicc_regs *gicc = (struct gicc_regs *)IC_INTERFACES_BASE_ADDR;

	/* Disable GIC by disabling both interrupt groups */
	writel(0, &gicd->ctlr);

	/* Disable all interrupts, clear all pending flags and move all
	   interrupts to group 0 (secure) */
	for (i = 0; i < N_IRQS/32; i++) {
		writel(0xFFFFFFFF, &gicd->icenabler[i]);
		writel(0xFFFFFFFF, &gicd->icpendr[i]);
		writel(0, &gicd->igroupr[i]);
	}

	/* Route all interrupts to CPU 0 (=Cortex-A)  */
	for (i = 0; i < N_IRQS; i++)
		writeb(1 << 0, &gicd->itargetsr[i]);

	/* Install default interrupt handlers */
	for (irq = 0; irq < N_IRQS; irq++) {
		irq_handler[irq].data = (void *)irq;
		irq_handler[irq].isr = default_isr;
	}

	/* Set priority mask to lowest priority, i.e. let all interrupts
	   through to the CPU */
	writel(0xff, &gicc->pmr);

	/* Enable signaling of both groups of interrupts */
	writel(0x7, &gicc->bpr);

	/* Enable GIC CPU interface by enabling both interrupt groups */
	writel(0x3, &gicc->ctlr);

	/* Enable GIC Distributor by enabling both interrupt groups */
	writel(0x3, &gicd->ctlr);

	return 0;
}
