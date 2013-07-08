/*
 * Copyright (C) 2009 Samsung Electronics
 * Heungjun Kim <riverful.kim@samsung.com>
 * Inki Dae <inki.dae@samsung.com>
 * Minkyu Kang <mk7.kang@samsung.com>
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
#include <asm/arch/pwm.h>
#include <asm/arch/clk.h>
#include <pwm.h>
#include <asm/arch/cpu.h>		  /* samsung_get_base_timer() */

DECLARE_GLOBAL_DATA_PTR;

int timer_init(void)
{
	struct s5p_timer *const timer =
		(struct s5p_timer *)samsung_get_base_timer();

	/* PWM Timer 4; init counter to PCLK/16/2 */
	pwm_init(4, MUX_DIV_2, 0);
	writel(0xFFFFFFFF, &timer->tcntb4);
	pwm_config(4, 1, 1);
	pwm_enable(4);

	gd->ticks = 0;
	gd->lastinc = 0;

	return 0;
}

/*
 * timer without interrupts
 */
unsigned long get_timer(unsigned long base)
{
	unsigned long now;
	struct s5p_timer *const timer =
		(struct s5p_timer *)samsung_get_base_timer();

	/* The timer is configured to count from 0xFFFFFFFF down to 0.
	   We want a timer value that counts up. So just compute the
	   difference 0xFFFFFFFF - timer value. It would be better to count
	   from 0 (as 2^32) down to 0 again, but when setting 0 into the
	   tcntb4, the timer will not count anymore. So 0xFFFFFFFF is the
	   maximum value that we can get. */
	now = 0xFFFFFFFF - readl(&timer->tcnto4);

	/* Increment ticks. By subtracting gd->lastinc, we make up for
	   overwrapping of the timer value.
	   REMARK: As we are not exactly counting from 2^32 down, but instead
	   from 2^32-1 (see above), we make an error of one tick at every wrap
	   around (about every 34 minutes @PCLK=67MHz). As other factors like
	   the execution time of the surrounding code that is not accounted
	   for result in far more bigger timing divergences, we can neglect
	   this little off-by-one error with a clear conscience. This keeps
	   the code clean and short. */
	gd->ticks += (unsigned long long)(now - gd->lastinc);
	gd->lastinc = now;

	return now - base;
}

/* delay x useconds */
void __udelay(unsigned long usec)
{
	unsigned long tmo, start;

	/* get current timestamp right here so that the tmo computation does
	   not add to the delay */
	start = get_timer(0);

	if (usec >= 1000) {
		/*
		 * if "big" number, spread normalization
		 * to seconds
		 * 1. start to normalize for usec to ticks per sec
		 * 2. find number of "ticks" to wait to achieve target
		 * 3. finish normalize.
		 */
		tmo = usec / 1000;
		tmo *= CONFIG_SYS_HZ;
		tmo /= 1000;
	} else {
		/* else small number, don't kill it prior to HZ multiply */
		tmo = usec * CONFIG_SYS_HZ;
		tmo /= (1000 * 1000);
	}


	/* loop till event */
	while (get_timer(start) < tmo)
		;	/* nop */
}


/*
 * This function is derived from PowerPC code (read timebase as long long).
 * On ARM it just returns the timer value.
 */
unsigned long long get_ticks(void)
{
	return gd->ticks;
}

/*
 * This function is derived from PowerPC code (timebase clock frequency).
 * On ARM it returns the number of timer ticks per second.
 */
unsigned long get_tbclk(void)
{
	return CONFIG_SYS_HZ;
}
