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

/* F&S header (V0.0) for a generic file */
struct fs_header_v0_0 {			/* Size: 16 Bytes */
	char magic[4];			/* "FS" + two bytes operating system
					   (e.g. "LX" for Linux) */
	u32 file_size_low;		/* Image size [31:0] */
	u32 file_size_high;		/* Image size [63:32] */
	u16 flags;			/* See flags below */
	u8 padsize;			/* Number of padded bytes at end */
	u8 version;			/* Header version x.y:
					   [7:4] major x, [3:0] minor y */
};

/* F&S header (V1.0) for a generic file */
struct fs_header_v1_0 {			/* Size: 64 bytes */
	struct fs_header_v0_0 info;	/* Image info, see above */
	char type[16];			/* Image type, e.g. "U-BOOT" */
	union {
		char descr[32];		/* Description, null-terminated */
		u8 p8[32];		/* 8-bit parameters */
		u16 p16[16];		/* 16-bit parameters */
		u32 p32[8];		/* 32-bit parameters */
		u64 p64[4];		/* 64-bit parameters */
	} param;
};

/* Possible values for flags entry above */
#define FSH_FLAGS_DESCR 0x8000		/* Description descr is present */
#define FSH_FLAGS_CRC32 0x4000		/* p32[7] holds the CRC32 checksum of
					   the image (without header) */
#define FSH_SIZE sizeof(struct fs_header_v1_0)

/* Structure to hold regions in NAND/eMMC for an image, taken from nboot-info */
struct storage_info {
	const fdt32_t *start;		/* List of start addresses *-start */
	unsigned int size;		/* *-size entry */
	unsigned int count;		/* Number of entries in start */
};

/* Return the F&S architecture */
const char *fs_image_get_arch(void);

/* Check if this is an F&S image */
bool fs_image_is_fs_image(const struct fs_header_v1_0 *fsh);

/* Return the address of the board configuration in OCRAM */
void *fs_image_get_cfg_addr(bool with_fs_header);

/* Return the address of the /board-cfg node */
int fs_image_get_cfg_offs(void *fdt);

/* Return the address of the /nboot-info node */
int fs_image_get_info_offs(void *fdt);

/* Return the address of the /board-cfg node */
int fs_image_get_cfg_offs(void *fdt);

/* Get the board-cfg-size from nboot-info */
int fs_image_get_board_cfg_size(void *fdt, int offs, unsigned int align,
				unsigned int *size);

/* Get nboot-start and nboot-size values from nboot-info */
int fs_image_get_nboot_info(void *fdt, int offs, unsigned int align,
			    struct storage_info *si);

/* Get spl-start and spl-size values from nboot-info */
int fs_image_get_spl_info(void *fdt, int offs, unsigned int align,
			  struct storage_info *si);

/* Get uboot-start and uboot-size values from nboot-info */
int fs_image_get_uboot_info(void *fdt, int offs, unsigned int align,
			    struct storage_info *si);

/* Return NBoot version by looking in given fdt (or BOARD-CFG if NULL) */
const char *fs_image_get_nboot_version(void *fdt);

/* Read the image size (incl. padding) from an F&S header */
unsigned int fs_image_get_size(const struct fs_header_v1_0 *fsh,
			       bool with_fs_header);

/* Check image magic, type and descr; return true on match */
bool fs_image_match(const struct fs_header_v1_0 *fsh,
		    const char *type, const char *descr);

/* Check id, return also true if revision is less than revision of compare id */
bool fs_image_match_board_id(struct fs_header_v1_0 *fsh, const char *type);


/* Set the compare id that will used in fs_image_match_board_id() */
void fs_image_set_board_id_compare(const char *id);


/* ------------- Stuff only for SPL ---------------------------------------- */

#ifdef CONFIG_SPL_BUILD

typedef void (*basic_init_t)(void);

/* Load FIRMWARE and optionally BOARD-CFG via SDP from USB */
void fs_image_all_sdp(bool need_cfg, basic_init_t basic_init);

/* Load BOARD-CFG and optionally FIRMWARE from NAND or MMC */
int fs_image_load_system(enum boot_device boot_dev, bool secondary,
			 basic_init_t basic_init);

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
#endif /* CONFIG_SPL_BUILD */

#endif /* !__FS_IMAGE_COMMON_H__ */

