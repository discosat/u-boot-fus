/*
 * Copyright 2021 F&S Elektronik Systeme GmbH
 *
 * Hartmut Keller, F&S Elektronik Systeme GmbH, keller@fs-net.de
 *
 * F&S image processing
 *
 * SPDX-License-Identifier:	GPL-2.0+
 */

#ifndef __FS_IMAGE_COMMON_H__
#define __FS_IMAGE_COMMON_H__

#define MAX_TYPE_LEN 16
#define MAX_DESCR_LEN 32

/* Jobs to do when streaming image data */
#define FSIMG_JOB_CFG BIT(0)
#define FSIMG_JOB_DRAM BIT(1)
#define FSIMG_JOB_ATF BIT(2)
#define FSIMG_JOB_TEE BIT(3)

/* Load mode */
enum fsimg_mode {
	FSIMG_MODE_HEADER,		/* Loading F&S header */
	FSIMG_MODE_IMAGE,		/* Loading F&S image */
	FSIMG_MODE_SKIP,		/* Skipping data */
	FSIMG_MODE_DONE,		/* F&S image done */
};

typedef void (*basic_init_t)(void);

/* Return the F&S architecture */
const char *fs_image_get_arch(void);

/* Return the BOARD-ID; id must have room for MAX_DESCR_LEN characters */
int fs_image_get_board_id(char *id);

/* Return the address of the board configuration in OCRAM */
void *fs_image_get_cfg_addr(bool with_fs_header);

/* Return the address of the /board-cfg node */
int fs_image_get_cfg_offs(void *fdt);

/* Return NBoot version by looking in given fdt (or BOARD-CFG if NULL) */
const char *fs_image_get_nboot_version(void *fdt);

/* Load FIRMWARE and optionally BOARD-CFG via SDP from USB */
void fs_image_all_sdp(unsigned int jobs_todo, basic_init_t basic_init);

/* Load F&S image with given type/descr from NAND at offset to given buffer */
int fs_image_load_nand(unsigned int offset, char *type, char *descr,
		       void *buf, bool keep_header);

/* Load FIRMWARE from NAND */
unsigned int fs_image_fw_nand(unsigned int jobs_todo, basic_init_t basic_init);

/* Load BOARD-CFG from NAND */
int fs_image_cfg_nand(void);

/* Load F&S image with given type/descr from MMC at offset to given buffer */
int fs_image_load_mmc(unsigned int offset, char *type, char *descr,
		       void *buf, bool keep_header);

/* Load FIRMWARE from eMMC */
unsigned int fs_image_fw_mmc(unsigned int jobs_todo, basic_init_t basic_init);

/* Load BOARD-CFG from eMMC */
int fs_image_cfg_mmc(void);

/* Check if board configuration in OCRAM is OK and return the address */
void *fs_image_get_cfg_addr_check(bool with_fs_header);

/* Load FIRMWARE from NAND to given address */
int fs_image_load_firmware(unsigned long addr);

/* List contents of an F&S image at given address */
int fs_image_list(unsigned long addr);

/* Save the F&S NBoot image at given address to the appropriate device */
int fs_image_save(unsigned long addr, bool force);

/* Burn the fuses according to the NBoot image at given address */
int fs_image_fuse(unsigned long addr, bool force);

#endif /* !__FS_IMAGE_COMMON_H__ */

