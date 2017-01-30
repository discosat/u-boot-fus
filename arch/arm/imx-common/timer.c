/*
 * (C) Copyright 2007
 * Sascha Hauer, Pengutronix
 *
 * (C) Copyright 2009 Freescale Semiconductor, Inc.
 *
 * SPDX-License-Identifier:	GPL-2.0+
 */

#include <common.h>
#include <asm/io.h>
#include <div64.h>
#include <asm/arch/imx-regs.h>
#include <asm/arch/clock.h>

#define TIMER_BASE_ADDR	GPT1_BASE_ADDR
#define TARGET_CLOCK	3000000

/* General purpose timers registers */
struct mxc_gpt {
	unsigned int control;
	unsigned int prescaler;
	unsigned int status;
	unsigned int interrupt;
	unsigned int ocr1;		/* Output compare registers */
	unsigned int ocr2;
	unsigned int ocr3;
	unsigned int icr1;		/* Input capture registers */
	unsigned int icr2;
	unsigned int counter;
};

/* General purpose timer bitfields */
#define GPTCR_SWR		(1 << 15)	/* Software reset */
#define GPTCR_FRR		(1 << 9)	/* Freerun / restart */
#define GPTCR_CLKSRC_OFF	(0 << 6)	/* Clock off */
#define GPTCR_CLKSRC_IPG	(1 << 6)	/* MXC_IPG_CLK */
#define GPTCR_CLKSRC_IPGPER	(2 << 6)	/* MXC_IPG_PERCLK */
#define GPTCR_CLKRSC_EXT	(3 << 6)	/* CLKIN */
#define GPTCR_CLKSRC_32KHZ	(4 << 6)	/* MXC_CLK32 */
/* Other clock sources (5 to 7) depend on CPU type */
#define GPTCR_TEN		1		/* Timer enable */

#define GPTSR_ROV		(1 << 5)	/* Rollover */
#define GPTSR_OF1		(1 << 0)	/* Output compare 1 event */

/*
 * For __udelay() and get_timer() use standard implementation from lib/time.c.
 *
 * IMPORTANT! Do not use global variables here, because the timer module is
 * initialized before RAM is officially available.
 */

/* Use GPT1 for system timer */
int timer_init(void)
{
	DECLARE_GLOBAL_DATA_PTR;
	unsigned int tmp = 0;
	static struct mxc_gpt *gpt = (struct mxc_gpt *)TIMER_BASE_ADDR;

	/* Reset GPT */
	__raw_writel(GPTCR_SWR, &gpt->control);

	/* Wait for reset to complete */
	while (__raw_readl(&gpt->control) & GPTCR_SWR) {
		if (++tmp >= 100)
			break;
	}

	/*
	 * Use IPG_PERCLK as base. This is usually 24 MHz or 66 MHz. Target
	 * clock is freerun mode at 3MHz. This allows for udelay() accuracy of
	 * +/- 0.33us. So for example udelay(1) is at least 0.67us! On the
	 * other hand this means the 32 bit counter will roll over after
	 * 2^32/3000000 = 1431.65 s = ~24 min. If get_ticks() is not called
	 * during this time, the carry to the high word of the 64-bit tick
	 * counter value is missed.
	 */
	tmp = mxc_get_clock(MXC_IPG_PERCLK);
	tmp += TARGET_CLOCK/2;
	tmp /= TARGET_CLOCK;
	__raw_writel(tmp - 1, &gpt->prescaler);

	tmp = __raw_readl(&gpt->control);
	tmp |= GPTCR_CLKSRC_IPGPER | GPTCR_TEN | GPTCR_FRR;
	__raw_writel(tmp, &gpt->control);

	/* Clear roll-over and output compare 1 event flags */
	__raw_writel(GPTSR_ROV | GPTSR_OF1, &gpt->status);

	tmp = __raw_readl(&gpt->counter);
	gd->timebase_h = 0;
	gd->timebase_l = tmp;

	/*
	 * To have an indication when a full counter cycle is over, save the
	 * current counter value as compare value 1; to avoid that the OF1
	 * event triggers immediately, subtract 1.
	 */
	__raw_writel(tmp - 1, &gpt->ocr1);

	return 0;
}

/* Return current tick counter */
unsigned long long get_ticks(void)
{
	DECLARE_GLOBAL_DATA_PTR;
	static struct mxc_gpt *gpt = (struct mxc_gpt *)TIMER_BASE_ADDR;
	unsigned int now;
	unsigned int status;

	/* 
	 * Read current timer value (timebase_l), increase high word
	 * (timebase_h) on roll-over. This implies that get_ticks() is called
	 * in regular intervals so that the 32 bit counter does not roll-over
	 * repeatedly. If we have roll-over and no output compare 1 event flag
	 * (OF1), we can be sure that less than a full count cycle has passed,
	 * i.e. there was definitely only one roll-over. But if OF1 is also
	 * set, we can not tell for sure if roll-over has occured more than
	 * once. Then the 64 bits ticks counter may be inaccurate, i.e. does
	 * not show the right number of ticks since system start anymore.
	 * However all following time measurements are still precise.
	 *
	 * The counter is read in a loop to handle the case that roll-over
	 * happens right between reading the counter and the status.
	 */
	do {
		now = __raw_readl(&gpt->counter);
		status = __raw_readl(&gpt->status);
		__raw_writel(status, &gpt->status);
		if (status & GPTSR_ROV) {
			gd->timebase_h++;
			if (status & GPTSR_OF1)
				debug("Ticks count may be inaccurate\n");
		}
	} while (status & GPTSR_ROV);

	/* Update the OCR1 value (indicator for a full count cycle) */
	gd->timebase_l = now;
	__raw_writel(now - 1, &gpt->ocr1);

	return (((unsigned long long)gd->timebase_h) << 32) | now;
}

/* Return timer base frequency (used to convert between time and ticks */
ulong get_tbclk(void)
{
	static struct mxc_gpt *gpt = (struct mxc_gpt *)TIMER_BASE_ADDR;
	unsigned int prescaler = __raw_readl(&gpt->prescaler) + 1;

	/*
	 * This is more or less TARGET_CLOCK, but may slightly differ if
	 * MXC_IPG_PERCLK is not divisible by TARGET_CLOCK without remainder.
	 * So return the true value here.
	 */
	return mxc_get_clock(MXC_IPG_PERCLK)/prescaler;
}
