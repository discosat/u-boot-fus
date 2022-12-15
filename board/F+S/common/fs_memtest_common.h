/*
 * fs_memtest_common.h
 *
 * (C) Copyright 2022
 * F&S Elektronik Systeme GmbH
 *
 * Common memory test based on memtester by Charles Cazabon.
 *
 * SPDX-License-Identifier:	GPL-2.0+
 */

#ifndef __FS_MEMTESTER_COMMON_H__
#define __FS_MEMTESTER_COMMON_H__

void memtester(size_t dramStartAddress, unsigned long memsize);

#endif /* !__FS_MEMTESTER_COMMON_H__ */
