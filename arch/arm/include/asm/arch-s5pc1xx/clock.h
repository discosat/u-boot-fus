/*
 * (C) Copyright 2009 Samsung Electronics
 * Minkyu Kang <mk7.kang@samsung.com>
 * Heungjun Kim <riverful.kim@samsung.com>
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
 *
 */

#ifndef __ASM_ARM_ARCH_CLOCK_H_
#define __ASM_ARM_ARCH_CLOCK_H_

#ifndef __ASSEMBLY__
struct s5pc100_clock {
	unsigned int	apll_lock;
	unsigned int	mpll_lock;
	unsigned int	epll_lock;
	unsigned int	hpll_lock;
	unsigned char	res1[0xf0];
	unsigned int	apll_con;
	unsigned int	mpll_con;
	unsigned int	epll_con;
	unsigned int	hpll_con;
	unsigned char	res2[0xf0];
	unsigned int	src0;
	unsigned int	src1;
	unsigned int	src2;
	unsigned int	src3;
	unsigned char	res3[0xf0];
	unsigned int	div0;
	unsigned int	div1;
	unsigned int	div2;
	unsigned int	div3;
	unsigned int	div4;
	unsigned char	res4[0x1ec];
	unsigned int	gate_d00;
	unsigned int	gate_d01;
	unsigned int	gate_d02;
	unsigned char	res5[0x54];
	unsigned int	gate_sclk0;
	unsigned int	gate_sclk1;
};

struct s5pc110_clock {
	unsigned int	apll_lock;	  /* 0x0000 */
	unsigned char	res1[0x4];
	unsigned int	mpll_lock;	  /* 0x0008 */
	unsigned char	res2[0x4];
	unsigned int	epll_lock;	  /* 0x0010 */
	unsigned char	res3[0xc];
	unsigned int	vpll_lock;	  /* 0x0020 */
	unsigned char	res4[0xdc];
	unsigned int	apll_con;	  /* 0x0100 */
	unsigned int    apll_con1;	  /* 0x0104 */
	unsigned int	mpll_con;	  /* 0x0108 */
	unsigned char	res5[0x4];
	unsigned int	epll_con;	  /* 0x0110 */
	unsigned int	epll_con1;	  /* 0x0114 */
	unsigned char	res6[0x8];
	unsigned int	vpll_con;	  /* 0x0120 */
	unsigned char	res7[0xdc];
	unsigned int	src0;		  /* 0x0200 */
	unsigned int	src1;		  /* 0x0204 */
	unsigned int	src2;		  /* 0x0208 */
	unsigned int	src3;		  /* 0x020c */
	unsigned int	src4;		  /* 0x0210 */
	unsigned int	src5;		  /* 0x0214 */
	unsigned int	src6;		  /* 0x0218 */
	unsigned char	res8[0x64];
	unsigned int	src_mask0;	  /* 0x0280 */
	unsigned int	src_mask1;	  /* 0x0284 */
	unsigned char	res9[0x78];
	unsigned int	div0;		  /* 0x0300 */
	unsigned int	div1;		  /* 0x0304 */
	unsigned int	div2;		  /* 0x0308 */
	unsigned int	div3;		  /* 0x030c */
	unsigned int	div4;		  /* 0x0310 */
	unsigned int	div5;		  /* 0x0314 */
	unsigned int	div6;		  /* 0x0318 */
	unsigned int	div7;		  /* 0x031c */
	unsigned char	res10[0x124];
	unsigned int	gate_sclk;	  /* 0x0444 */
	unsigned char	res11[0x18];
	unsigned int	gate_ip0;	  /* 0x0460 */
	unsigned int	gate_ip1;	  /* 0x0464 */
	unsigned int	gate_ip2;	  /* 0x0468 */
	unsigned int	gate_ip3;	  /* 0x046c */
	unsigned int	gate_ip4;	  /* 0x0470 */
	unsigned char	res12[0xc];
	unsigned int	gate_block;	  /* 0x0480 */
	unsigned int	gate_ip5;	  /* 0x0484 */
	unsigned char	res13[0x78];
	unsigned int	out;		  /* 0x0500 */
	unsigned char	res14[0xafc];
	unsigned int	div_stat0;	  /* 0x1000 */
	unsigned int	div_stat1;	  /* 0x1004 */
	unsigned char	res15[0xf8];
	unsigned int	mux_stat0;	  /* 0x1100 */
	unsigned int	mux_stat1;	  /* 0x1104 */
	unsigned char	res16[0xef8];
	unsigned int	swreset;	  /* 0x2000 */
	unsigned char	res17[0xffc];
	unsigned int	dcgidx_map0;	  /* 0x3000 */
	unsigned int	dcgidx_map1;	  /* 0x3004 */
	unsigned int	dcgidx_map2;	  /* 0x3008 */
	unsigned char	res18[0x14];
	unsigned int	dcgperf_map0;	  /* 0x3020 */
	unsigned int	dcgperf_map1;	  /* 0x3024 */
	unsigned char	res19[0x18];
	unsigned int	dvcidx_map;	  /* 0x3040 */
	unsigned char	res20[0x1c];
	unsigned int	freq_cpu;	  /* 0x3060 */
	unsigned int	freq_dpm;	  /* 0x3064 */
	unsigned char	res21[0x18];
	unsigned int	dvsemclk_en;	  /* 0x3080 */
	unsigned int	maxperf;	  /* 0x3084 */
	unsigned char	res22[0x78];
	unsigned int	apll_con0_l8;	  /* 0x3100 */
	unsigned int	apll_con0_l7;	  /* 0x3104 */
	unsigned int	apll_con0_l6;	  /* 0x3108 */
	unsigned int	apll_con0_l5;	  /* 0x310c */
	unsigned int	apll_con0_l4;	  /* 0x3110 */
	unsigned int	apll_con0_l3;	  /* 0x3114 */
	unsigned int	apll_con0_l2;	  /* 0x3118 */
	unsigned int	apll_con0_l1;	  /* 0x311c */
	unsigned char	res23[0xe0];
	unsigned int	div_iem_l8;	  /* 0x3200 */
	unsigned int	div_iem_l7;	  /* 0x3204 */
	unsigned int	div_iem_l6;	  /* 0x3208 */
	unsigned int	div_iem_l5;	  /* 0x320c */
	unsigned int	div_iem_l4;	  /* 0x3210 */
	unsigned int	div_iem_l3;	  /* 0x3214 */
	unsigned int	div_iem_l2;	  /* 0x3218 */
	unsigned int	div_iem_l1;	  /* 0x321c */
	unsigned char	res24[0xe0];
	unsigned int	apll_con1_l8;	  /* 0x3300 */
	unsigned int	apll_con1_l7;	  /* 0x3304 */
	unsigned int	apll_con1_l6;	  /* 0x3308 */
	unsigned int	apll_con1_l5;	  /* 0x330c */
	unsigned int	apll_con1_l4;	  /* 0x3310 */
	unsigned int	apll_con1_l3;	  /* 0x3314 */
	unsigned int	apll_con1_l2;	  /* 0x3318 */
	unsigned int	apll_con1_l1;	  /* 0x331c */
	unsigned char	res25[0x2de0];
	unsigned int	general_ctrl;	  /* 0x6100 */
	unsigned char	res26[0xf04];
	unsigned int	display_ctrl;	  /* 0x7008 */
	unsigned int	audio_endian;	  /* 0x700c */
};
#endif

#endif
