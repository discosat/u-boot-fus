/* SPDX-License-Identifier: GPL-2.0+ */
/*
 * Copyright (C) 2011
 * Stefano Babic, DENX Software Engineering, <sbabic@denx.de>
 */


#ifndef __ASM_ARCH_MX6_GPIO_H
#define __ASM_ARCH_MX6_GPIO_H

#include <asm/mach-imx/gpio.h>

#define IMX_GPIO_NR(port, index)		((((port)-1)*32)+((index)&31))

#endif	/* __ASM_ARCH_MX6_GPIO_H */
