/*
 * fs_fdt_common.h
 *
 * (C) Copyright 2018
 * Hartmut Keller, F&S Elektronik Systeme GmbH, keller@fs-net.de
 *
 * Common FDT access and manipulation
 *
 * SPDX-License-Identifier:	GPL-2.0+
 */

#ifndef __FS_FDT_COMMON_H__
#define __FS_FDT_COMMON_H__

/* Set a generic value, if it was not already set in the device tree */
void fs_fdt_set_val(void *fdt, int offs, const char *name, const void *val,
		    int len, int force);

/* Set a string value */
void fs_fdt_set_string(void *fdt, int offs, const char *name, const char *str,
		       int force);

/* Set a u32 value as a string (usually for bdinfo) */
void fs_fdt_set_u32str(void *fdt, int offs, const char *name, u32 val,
		       int force);

/* Set a u32 value */
void fs_fdt_set_u32(void *fdt, int offs, const char *name, u32 val, int force);

/* Set ethernet MAC address aa:bb:cc:dd:ee:ff for given index */
void fs_fdt_set_macaddr(void *fdt, int offs, int id);

/* Set MAC address in bdinfo as MAC_WLAN and in case of Silex as Silex-MAC */
void fs_fdt_set_wlan_macaddr(void *fdt, int offs, int id, int silex);

/* If environment variable exists, set a string property with the same name */
void fs_fdt_set_getenv(void *fdt, int offs, const char *name, int force);

/* Open a node, warn if the node does not exist */
int fs_fdt_path_offset(void *fdt, const char *path);

/* Enable or disable node given by path, overwrite any existing status value */
void fs_fdt_enable(void *fdt, const char *path, int enable);

/* Store common board specific values in node bdinfo */
void fs_fdt_set_bdinfo(void *fdt, int offs);

#endif /* !__FS_FDT_COMMON_H__ */
