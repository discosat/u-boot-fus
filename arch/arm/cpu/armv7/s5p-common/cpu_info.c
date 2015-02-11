/*
 * Copyright (C) 2009 Samsung Electronics
 * Minkyu Kang <mk7.kang@samsung.com>
 *
 * SPDX-License-Identifier:	GPL-2.0+
 */
#include <common.h>
#include <asm/io.h>
#include <asm/arch/clk.h>
#include <asm/arch/cpu.h>		  /* cpu_is_*() */

#ifdef CONFIG_ARCH_CPU_INIT
int arch_cpu_init(void)
{
	return 0;
}
#endif

u32 get_device_type(void)
{
	return GET_S5P_CPU_ID();
}

#ifdef CONFIG_DISPLAY_CPUINFO
int print_cpuinfo(void)
{
	char buf[32];

	printf("CPU:\t%s@%sMHz\n",
		s5p_get_cpu_name(), strmhz(buf, get_arm_clk()));
#if 0 //####
	printf("###ACLK=%sMHz ", strmhz(buf, get_pll_clk(APLL)));
	printf("MCLK=%sMHz ", strmhz(buf, get_pll_clk(MPLL)));
	printf("ECLK=%sMHz ", strmhz(buf, get_pll_clk(EPLL)));
	printf("VCLK=%sMHz ", strmhz(buf, get_pll_clk(VPLL)));
	printf("PWM_CLK=%sMHz\n", strmhz(buf, get_pwm_clk()));
#endif //####

	return 0;
}
#endif
