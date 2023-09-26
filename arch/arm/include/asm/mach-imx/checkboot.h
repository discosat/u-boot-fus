/*
 * Copyright (C) 2020 F&S Elektronik Systeme GmbH
 *
 * SPDX-License-Identifier:    GPL-2.0+
 *
*/

#ifndef __checkboot_H__
#define __checkboot_H__


#include <common.h>
#include <asm/mach-imx/hab.h>

/* UBoot IVT defines (so uboot recognizes the uboot with ivt)  */
#define IS_UBOOT(pAddr)      	(__le32_to_cpup((__le32*)(pAddr + 0x3c)) == 0x12345678)
#define IS_UBOOT_7ULP(pAddr)	(__le32_to_cpup((__le32*)(pAddr + 0xc3c)) == 0x12345678)
#define IS_UBOOT_IVT(pAddr)  	(__le32_to_cpup((__le32*)(pAddr + 0x3c + HAB_HEADER)) == 0x12345678)
#define IS_UIMAGE(pAddr)     	(__be32_to_cpup((__be32*)(pAddr)) == 0x27051956)
#define IS_UIMAGE_IVT(pAddr)	(__be32_to_cpup((__be32*)(pAddr + HAB_HEADER)) == 0x27051956)
#define IS_ZIMAGE(pAddr)     	(__be32_to_cpup((__be32*)(pAddr + 0x24)) == 0x18286f01)
#define IS_ZIMAGE_IVT(pAddr)	(__be32_to_cpup((__be32*)(pAddr + 0x24 + HAB_HEADER)) == 0x18286f01)
#define IS_DEVTREE(pAddr)	(__be32_to_cpup((__be32*)(pAddr)) == 0xd00dfeed)
#define IS_DEVTREE_IVT(pAddr)	(__be32_to_cpup((__be32*)(pAddr + HAB_HEADER)) == 0xd00dfeed)

#define HAB_HEADER         0x40

struct __packed boot_data {
	uint32_t	start;
	uint32_t	length;
	uint32_t	plugin;
};

typedef enum eLoaderType {
	LOADER_NONE = 0,
	LOADER_UBOOT = 1,
	LOADER_UBOOT_IVT = 2,
	LOADER_KERNEL = 4,
	LOADER_KERNEL_IVT = 8,
	LOADER_FDT = 16,
	LOADER_FDT_IVT = 32,
} LOADER_TYPE;


typedef enum eOptions {
    DO_NOTHING   = 0x0,
    BACKUP_IMAGE = 0x1,
    CUT_IVT      = 0x2,
} OPTIONS;

int ivt_header_error(const char *err_str, struct ivt_header *ivt_hdr, int message);
int verify_ivt_header(struct ivt_header *ivt_hdr, int message);
void memExchange(uintptr_t srcaddr, uintptr_t dstaddr, uintptr_t length);
uintptr_t makeSaveCopy(uintptr_t srcaddr, uintptr_t length);
uintptr_t getImageLength(uintptr_t addr);
int check_flash_partition(uintptr_t addr, OPTIONS eOption, loff_t off, loff_t length);
int parse_images_for_authentification(int argc, char * const argv[]);
int prepare_authentication(uintptr_t addr, OPTIONS eOption);

#endif
