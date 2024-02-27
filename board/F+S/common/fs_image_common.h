// SPDX-License-Identifier: GPL-2.0+
/*
 * Copyright 2021 F&S Elektronik Systeme GmbH
 *
 * Hartmut Keller, F&S Elektronik Systeme GmbH, keller@fs-net.de
 *
 * F&S image processing
 *
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
#define FSH_FLAGS_CRC32 0x4000		/* CRC32 of image in type[12..15] */
#define FSH_FLAGS_SECURE 0x2000		/* CRC32 of header in type[12..15] */

#define FSH_SIZE sizeof(struct fs_header_v1_0)

/* Return the F&S architecture */
const char *fs_image_get_arch(void);

/* Check if this is an F&S image */
bool fs_image_is_fs_image(const struct fs_header_v1_0 *fsh);

/* Return the intended address of the board configuration in OCRAM */
void *fs_image_get_regular_cfg_addr(void);

/* Return the real address of the board configuration in OCRAM */
void *fs_image_get_cfg_addr(void);

/* Return the fdt part of the board configuration in OCRAM */
void *fs_image_get_cfg_fdt(void);

/* Return the fdt part of the given board configuration */
void *fs_image_find_cfg_fdt(struct fs_header_v1_0 *fsh);

/* Return the address of the /board-cfg node */
int fs_image_get_board_cfg_offs(void *fdt);

/* Return the address of the /nboot-info node */
int fs_image_get_nboot_info_offs(void *fdt);

/* Return NBoot version by looking in given fdt (or BOARD-CFG if NULL) */
const char *fs_image_get_nboot_version(void *fdt);

/* Read the image size (incl. padding) from an F&S header */
unsigned int fs_image_get_size(const struct fs_header_v1_0 *fsh,
			       bool with_fs_header);

/* Check image magic, type and descr; return true on match */
bool fs_image_match(const struct fs_header_v1_0 *fsh,
		    const char *type, const char *descr);

/* Check id, return also true if revision is less than revision of compare_id */
bool fs_image_match_board_id(struct fs_header_v1_0 *fsh);

/* Read property from board-rev subnode or board-cfg main node */
const void *fs_image_getprop(const void *fdt, int cfg_offs, int rev_offs,
			     const char *name, int *lenp);

/* Read u32 property from board-rev subnode or board-cfg main node */
u32 fs_image_getprop_u32(const void *fdt, int cfg_offs, int rev_offs,
			 int cell, const char *name, const u32 dflt);

/* Add the board revision as BOARD-ID to the given BOARD-CFG and update CRC32 */
void fs_image_board_cfg_set_board_rev(struct fs_header_v1_0 *cfg_fsh);

/* Return the current BOARD-ID */
const char *fs_image_get_board_id(void);

/* Set the compare_id that will be used in fs_image_match_board_id() */
void fs_image_set_compare_id(const char id[MAX_DESCR_LEN]);

/* Get the board-rev from BOARD-ID (in compare-id) */
unsigned int fs_image_get_board_rev(void);

/* Set the board_id and compare_id from the BOARD-CFG */
void fs_image_set_board_id_from_cfg(void);

/* Find board-cfg subnode matching the board-rev in the BOARD-ID */
int fs_image_get_board_rev_subnode(const void *fdt, int offs);

/* Find board-rev and return matching board-cfg subnode (U-Boot f-phase) */
int fs_image_get_board_rev_subnode_f(const void *fdt, int offs,
				     uint *board_rev);

/* Check if the F&S image is signed (followed by an IVT) */
bool fs_image_is_signed(struct fs_header_v1_0 *fsh);

/* Check IVT integrity of F&S image and return size and validation address */
void *fs_image_get_ivt_info(struct fs_header_v1_0 *fsh, u32 *size);

/* Validate a signed image; it has to be at the validation address */
bool fs_image_is_valid_signature(struct fs_header_v1_0 *fsh);

/* Verify CRC32 of given image */
int fs_image_check_crc32(const struct fs_header_v1_0 *fsh);

/* Make sure that BOARD-CFG in OCRAM is valid */
bool fs_image_is_ocram_cfg_valid(void);

/* Authenticate an FS-Image at a testing address and copy it to its load address */
#ifdef CONFIG_FS_SECURE_BOOT
int authenticate_fs_image(void *final_addr, void *check_addr,
	ulong image_offset, int image_type, bool header);
struct sb_info {
	void *final_addr;
	void *check_addr;
	bool header;
	struct ivt* image_ivt;
	int image_type;
};
#endif

/* ------------- Stuff only for U-Boot ------------------------------------- */

#ifndef CONFIG_SPL_BUILD

/* Return if currently running from Secondary SPL. */
bool fs_image_is_secondary(void);

/* Return if currently running from Secondary UBoot. */
bool fs_image_is_secondary_uboot(void);

/*
 * Search board configuration in OCRAM; return true if it was found.
 * From now on, fs_image_get_cfg_addr() will return the right address.
 */
bool fs_image_find_cfg_in_ocram(void);

/* Get count values from given device tree property and check alignment */
int fs_image_get_fdt_val(void *fdt, int offs, const char *name, uint align,
			 int count, uint *val);

#ifdef CONFIG_NAND_MXS
int fs_image_get_known_env_nand(uint index, uint start[2], uint *size);
#endif

#ifdef CONFIG_MMC
int fs_image_get_known_env_mmc(uint index, uint start[2], uint *size);
#endif

#endif /* !CONFIG_SPL_BUILD */

/* ------------- Stuff only for SPL ---------------------------------------- */

#ifdef CONFIG_SPL_BUILD

typedef void (*basic_init_t)(const char *layout_name);

/* Mark BOARD_CFG to tell U-Boot that we are running on Secondary SPL */
void fs_image_mark_secondary(void);

/* Mark BOARD_CFG to tell U-Boot that we are running on Secondary UBoot */
void fs_image_mark_secondary_uboot(void);

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
