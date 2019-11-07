/*
 * Copyright (C) 2019 F&S Elektronik Systeme GmbH
 *
 * Prepare images for secure boot (tested only for i.MX6 CPUs).
 *
 * SPDX-License-Identifier:	GPL-2.0+
 *
 */

#include <common.h>		//ALIGN, le16_to_cpu
#include <jffs2/jffs2.h>	//find_dev_and_part, struct part_info
#include <nand.h>
/* HAB includes  */
#include <asm/mach-imx/checkboot.h>
#include <asm/mach-imx/hab.h>


/*
 * Function:   ivt_header_error(const char *err_str, struct ivt_header *ivt_hdr, int message)
 *
 * Parameters: err_str -> string of error type
 *             ivt_hdr -> Pointer to putative ivt header
 *             message -> print error message if set to 1
 *
 * Return:     return 1 for error.
 *
 * Content:    This function prints the error message for no ivt header. Ported
 *             from arch/arm/mach-imx/hab.c, but this static so no access.
 */
int ivt_header_error(const char *err_str, struct ivt_header *ivt_hdr, int message)
{
	if (message == 1) {
		printf("%s magic=0x%x length=0x%02x version=0x%x\n", err_str,
		       ivt_hdr->magic, ivt_hdr->length, ivt_hdr->version);
	}
	return 1;
}

/*
 * Function:   verify_ivt_header(struct ivt_header *ivt_hdr, int message)
 *
 * Parameters: ivt_hdr -> Pointer to putative ivt header
 *             message -> print error message if set to 1
 *
 * Return:     return 1 for error.
 *
 * Content:    This function checks if there is an ivt. If there is no
 *             ivt then an error message will occur. Ported from
 *             arch/arm/mach-imx/hab.c, but this is static so no access.
 */
int verify_ivt_header(struct ivt_header *ivt_hdr, int message)
{
	int result = 0;

	if (ivt_hdr->magic != IVT_HEADER_MAGIC)
		result = ivt_header_error("bad magic", ivt_hdr, message);

	if (be16_to_cpu(ivt_hdr->length) != IVT_TOTAL_LENGTH)
		result = ivt_header_error("bad length", ivt_hdr, message);

	if (ivt_hdr->version != IVT_HEADER_V1 &&
	    ivt_hdr->version != IVT_HEADER_V2)
		result = ivt_header_error("bad version", ivt_hdr, message);

	return result;
}


/*
 * Function:   check_for_ivt_void(const void **addr, int *is_ivt)
 *
 * Parameters: **addr  -> Pointer to putative ivt header
 *             *is_ivt -> Pointer to know there is an ivt image
 *
 * Return:     -
 *
 * Content:    This function checks if the given pointer points to an ivt.
 *             If there is an ivt we increment the pointer to show directly
 *             to the image itself instead of the ivt. We also perceive that
 *             we have increment the pointer so we can later switch back to
 *             the ivt and then we can check the image.
 */
void check_for_ivt_void(const void **addr, int *is_ivt)
{
	struct ivt *ivt;
	int ret = 1;
	ivt = (struct ivt *)*addr;
	ret = verify_ivt_header(&ivt->hdr, 0);
	if (!ret) {
		/* IVT found */
		*addr += HAB_HEADER;
		is_ivt[0] = 1;
	}
}

/*
 * Function:   check_for_ivt_char(int argc, char * const argv[], int *ivt_states)
 *
 * Parameters: argc        -> Count of given arguments in argv
 *             *argv[]     -> Char array of given arguments
 *             *ivt_states -> Pointer to know that we have incremented the
 *                            original pointer to show directly to the image.
 *
 * Return:     -
 *
 * Content:    This function checks all arguments if there is an ivt and if
 *             there is an ivt check it if it´s an FIT image or an uImage. If
 *             not increment the pointer to point directly to the image itself.
 */
void check_for_ivt_char(int argc, char * const argv[], int *ivt_states)
{
	struct ivt *ivt;
	int ret = 1;

	for (int i = 0; i < argc; i++)
	{
		if (strcmp(argv[i], "-") !=  0) {
			ivt = (struct ivt *)parse_loadaddr(argv[i], NULL);
			ret = verify_ivt_header(&ivt->hdr, 0);
			if (!ret) {
				/* IVT found */
				/* check if legacy or FIT image, if yes verify
				 * it and cut ivt.
				 */
				if (IS_UIMAGE_IVT(parse_loadaddr(argv[i], NULL))) {
					ret = prepare_authentication(parse_loadaddr(argv[i], NULL), CUT_IVT, 0, 0);
				}
				else if (IS_DEVTREE_IVT(parse_loadaddr(argv[i], NULL)) && i == 0) {
					ret = prepare_authentication(parse_loadaddr(argv[i], NULL), CUT_IVT, 0, 0);
				}
				else {
					sprintf(argv[i], "%lx", parse_loadaddr(argv[i], NULL) + HAB_HEADER);
					ivt_states[i] = 1;
				}
			}
		}
	}
}


/*
 * Function:   GetLoaderType(u32 addr)
 *
 * Parameters: addr -> start address of image
 *
 * Return:     get the type of the image.
 *
 * Content:    we need the name of the image to set the name as a string so we
 *             can get access to the flash partition with this string name.
 */
LOADER_TYPE GetLoaderType(u32 addr)
{
	if (IS_UBOOT(addr))
		return LOADER_UBOOT;

	if (IS_UBOOT_IVT(addr))
		return LOADER_UBOOT_IVT;

	if (IS_UIMAGE(addr) || IS_ZIMAGE(addr))
		return LOADER_KERNEL;

	if (IS_UIMAGE_IVT(addr) || IS_ZIMAGE_IVT(addr))
		return LOADER_KERNEL_IVT;

	if (IS_DEVTREE(addr))
		return LOADER_FDT;

	if (IS_DEVTREE_IVT(addr))
		return LOADER_FDT_IVT;

	return LOADER_NONE;
}


/*
 * Function:   memExchange(u32 srcaddr, u32 dstaddr, u32 length)
 *
 * Parameters: u32 srcaddr -> start address of the given image
 *             u32 dstaddr -> destination address of the given image
 *             u32 length  -> length of the image
 *
 * Return:     -
 *
 * Content:    Copies memory blocks from 'srcaddr' to 'dstaddr' with 'length'.
 */
void memExchange(u32 srcaddr, u32 dstaddr, u32 length)
{
	u32 *src_addr = (u32*) srcaddr;
	u32 *dst_addr = (u32*) dstaddr;
	u32 image_length = (length + 3) & ~3;
	int i = 0;
	if((u32)src_addr  < (u32) dst_addr) {
		src_addr = (u32*)(srcaddr + image_length);
		dst_addr = (u32*)(dstaddr + image_length);
		for(i=((image_length)/4); i>=0; i--) {
				*dst_addr = *src_addr;
				dst_addr--;
				src_addr--;
			}
	} else if((u32)src_addr > (u32)dst_addr) {
		src_addr = (u32*)srcaddr;
		dst_addr = (u32*)dstaddr;
		for(i=0; i<=image_length/4;i++) {
			*dst_addr = *src_addr;
			dst_addr++;
			src_addr++;
		}
	}
}


/*
 * Function:   makeSaveCopy(u32 srcaddr, u32 length)
 *
 * Parameters: srcaddr -> start address of image
 *             length  -> length of the image
 *
 * Return:     get save address of the image.
 *
 * Content:    before the image will be checked it must be saved to another
 *             RAM address. This is necessary because if a encrypted image
 *             will be checked it will be decrypted at directly at the RAM
 *             address so we only have a decrypted image but we want to store
 *             an encrypted image in the flash. So thats why we need a save
 *             copy of the original image.
 */
u32 makeSaveCopy(u32 srcaddr, u32 length)
{
	struct ivt *ivt = (struct ivt *)srcaddr;
	u32 *checkaddr = 0x0;
	u32 *saveaddr = 0x0;

	checkaddr = (u32*) ivt->self;

	if(srcaddr < (u32)checkaddr)
		saveaddr = (u32*)(checkaddr + length);
	else
		saveaddr = (u32*)(srcaddr + length);

	memExchange(srcaddr, (u32)saveaddr, length);

	return (u32)saveaddr;
}


/*
 * Function:   getImageLength(u32 addr)
 *
 * Parameters: addr -> start address of image
 *
 * Return:     length of the image
 *
 * Content:    get the image length from the ivt.
 */
u32 getImageLength(u32 addr)
{
	struct ivt *ivt = (struct ivt *)addr;
	signed long offset = (signed long)((signed long)addr - (signed long)ivt->self);
	struct boot_data *data = (struct boot_data *)(ivt->boot + offset);
	return data->length;
}


/*
 * Function:   check_flash_partition(char *img_name, loff_t *off, u32 length)
 *
 * Parameters: img_name -> start address of image to be checked
 *             off      -> image length
 *             length   -> image name
 *
 * Return:      0 -> check successful
 *             -1 -> check unsuccessful
 *
 * Content:    Checks if the image will be written to the correct partition
 *             and checks if the image fits in the partition.
 */
int check_flash_partition(char *img_name, loff_t *off, u32 length)
{
	struct mtd_device *dev;
	struct part_info *part;
	u8 pnum;
	int ret = -1;

	find_dev_and_part(img_name, &dev, &pnum, &part);
	if(*off != part->offset) {
		printf("\nWrong partition!!\n");
		printf("\nAborting ...\n\n");
		ret = -1;
	} else if(part->size < length) {
		printf("\nPartition is too small!!\n");
		printf("\nAborting ...\n\n");
		ret = -1;
	} else {
		ret = 0;
	}

	return ret;
}


/*
 * Function:   prepare_authentication(u32 addr, OPTIONS eOption, loff_t *off, loff_t *size)
 *
 * Parameters: u32 addr        -> address in RAM where the kernel is loaded
 *             OPTIONS eOption -> enum what to do with the image
 *             loff_t *off     -> start address in flash where to write
 *             loff_t *size    -> image size
 *
 * Return:     int -> verification of the image
 *
 * Content:    Prepare the image that it can be verified.
 */
int prepare_authentication(u32 addr, OPTIONS eOption, loff_t *off, loff_t *size)
{
	/* HAB Variables */
	struct ivt *ivt;
	struct ivt_header *ivt_hdr;
	u32 check_addr = 0;
	u32 save_addr = 0;
	u32 length = 0;
	char *img_name = "";
	int ret = 1;

	/* get image type */
	LOADER_TYPE boot_loader_type = GetLoaderType(addr);

	/* check which image type we have and set img_name */
	if(boot_loader_type & (LOADER_UBOOT_IVT | LOADER_UBOOT)) {
		img_name = "UBoot";
	} else if(boot_loader_type & (LOADER_KERNEL_IVT | LOADER_KERNEL)) {
		img_name = "Kernel";
	} else if(boot_loader_type & (LOADER_FDT_IVT | LOADER_FDT)) {
		img_name = "FDT";
	} else {
		printf("invalid image type!\n");
		ret = 1;
		goto exit;
	}

	ivt = (struct ivt *)addr;
	check_addr = ivt->self;

	/* Calculate IVT address header */
	ivt_hdr = &ivt->hdr;

	/* Verify IVT header bugging out on error */
	if (verify_ivt_header(ivt_hdr, 1))
		goto exit;

	length = getImageLength(addr);

	if (eOption == BACKUP_IMAGE) {
		ret = check_flash_partition(img_name, off, length);
		if(ret != 0) {
			ret = 1;
			goto exit;
		}
		save_addr = makeSaveCopy(addr, length);
	}

	if((addr != check_addr)) {
		memExchange(addr, check_addr, length);
	}

	ret = imx_hab_authenticate_image(check_addr, length, 0x0);

	if (eOption == BACKUP_IMAGE) {
		/* get original image saved by 'makeSaveCopy' */
		memExchange(save_addr, addr, length);
	}
	else if(eOption == CUT_IVT) {
		memExchange(((u32)check_addr + HAB_HEADER), addr, length);
	}
exit:
		return ret;
}
