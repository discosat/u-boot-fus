/*
 * (C) Copyright 2000-2009
 * Wolfgang Denk, DENX Software Engineering, wd@denx.de.
 *
 * SPDX-License-Identifier:	GPL-2.0+
 */

#include <common.h>
#include <watchdog.h>

#ifndef CONFIG_WD_PERIOD
# define CONFIG_WD_PERIOD	(10 * 1000 * 1000)	/* 10 seconds default*/
#endif

/* ------------------------------------------------------------------------- */

void udelay(unsigned long usec)
{
	ulong kv;

	do {
		WATCHDOG_RESET();
		kv = usec > CONFIG_WD_PERIOD ? CONFIG_WD_PERIOD : usec;
		__udelay (kv);
		usec -= kv;
	} while(usec);
}

void mdelay(unsigned long msec)
{
	while (msec--)
		udelay(1000);
}

ulong __timer_get_boot_us(void)
{
	static ulong base_time;

	/*
	 * We can't implement this properly. Return 0 on the first call and
	 * larger values after that.
	 */
	if (base_time)
		return get_timer(base_time) * 1000;
	base_time = get_timer(0);
	return 0;
}

ulong timer_get_boot_us(void)
	__attribute__((weak, alias("__timer_get_boot_us")));
