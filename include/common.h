/* SPDX-License-Identifier: GPL-2.0+ */
/*
 * Common header file for U-Boot
 *
 * This file still includes quite a few headers that should be included
 * individually as needed. Patches to remove things are welcome.
 *
 * (C) Copyright 2000-2009
 * Wolfgang Denk, DENX Software Engineering, wd@denx.de.
 */

#ifndef __COMMON_H_
#define __COMMON_H_	1

#ifndef __ASSEMBLY__		/* put C only stuff in this section */
#include <config.h>
#include <errno.h>
#include <time.h>
#include <asm-offsets.h>
#include <linux/bitops.h>
#include <linux/bug.h>
#include <linux/delay.h>
#include <linux/types.h>
#include <linux/printk.h>
#include <linux/string.h>
#include <linux/stringify.h>
#include <asm/ptrace.h>
#include <stdarg.h>
#include <stdio.h>
#include <linux/kernel.h>
#include <part.h>
#include <flash.h>
#include <image.h>
#include <log.h>
#include <asm/u-boot.h> /* boot information for Linux kernel */
#include <asm/global_data.h>	/* global data used for startup functions */
#include <init.h>
#include <display_options.h>

/* Get the load address; should be the same as environment variable loadaddr */
ulong get_loadaddr(void);

/* Parse address, in case of "." return current get_loadaddr() */
ulong parse_loadaddr(const char *buffer, char **endp);

/* Like simple_loadaddr(), but return error in case of trailing garbage */
int strict_parse_loadaddr(const char *buffer, ulong *loadaddr);

/* Set address where to load next file */
void set_fileaddr(ulong addr);

/* Get address where to load next file */
ulong get_fileaddr(void);

/* Set environment variables fileaddr and filesize */
void env_set_fileinfo(ulong size);

#include <uuid.h>
#include <vsprintf.h>
#include <net.h>
#include <bootstage.h>
#endif	/* __ASSEMBLY__ */

/* Pull in stuff for the build system */
#ifdef DO_DEPS_ONLY
# include <env_internal.h>
#endif

#endif	/* __COMMON_H_ */
