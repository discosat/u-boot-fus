/*
 * Copyright (C) 2009 Samsung Electronics
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
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston,
 * MA 02111-1307 USA
 */
#include <common.h>
#include <asm/io.h>
#include <asm/arch/clk.h>
#include <asm/arch/cpu.h>		  /* cpu_is_*() */

char const *s5pc110_dev_id[] = {
	"S5PV210",
	"S5PC110",
	"S5PC111",
};

#ifdef CONFIG_ARCH_CPU_INIT
int arch_cpu_init(void)
{
	s5p_set_cpu_id();

	return 0;
}
#endif

u32 get_device_type(void)
{
	DECLARE_GLOBAL_DATA_PTR;

	return gd->cpu_id;
}

#ifdef CONFIG_DISPLAY_CPUINFO
int print_cpuinfo(void)
{
	char buf[32];

	puts("CPU:   ");
	if (cpu_is_s5pc100())
		puts("S5PC100");
	else if (cpu_is_s5pc110())	  /* Device ID in [3:0] */
		puts(s5pc110_dev_id[readl(S5PC100_PRO_ID) & 0x0f]);
	else
		puts("(unknown)");

	printf("@%sMHz\n", strmhz(buf, get_arm_clk()));

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
