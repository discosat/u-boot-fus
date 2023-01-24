/*
 * Copyright (C) 2020 F&S Elektronik Systeme GmbH
 *
 * Prepare images for secure boot (tested only for i.MX6 CPUs).
 *
 * SPDX-License-Identifier:	GPL-2.0+
 *
 */

#include <config.h>
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
 * Return:     1 -> error
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
 * Return:     0 -> ivt is available
 *             1 -> error
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
 * Function:   memExchange(uintptr_t srcaddr, uintptr_t dstaddr, uintptr_t length)
 *
 * Parameters: uintptr_t srcaddr -> start address of the given image
 *             uintptr_t dstaddr -> destination address of the given image
 *             uintptr_t length  -> length of the image
 *
 * Return:     -
 *
 * Content:    Copies memory blocks from 'srcaddr' to 'dstaddr' with 'length'.
 */
void memExchange(uintptr_t srcaddr, uintptr_t dstaddr, uintptr_t length)
{
	uintptr_t *src_addr = (uintptr_t*) srcaddr;
	uintptr_t *dst_addr = (uintptr_t*) dstaddr;
	uintptr_t image_length = (length + 3) & ~3;
	int i = 0;

	if ((uintptr_t)src_addr  < (uintptr_t) dst_addr) {
		src_addr = (uintptr_t*)(srcaddr + image_length);
		dst_addr = (uintptr_t*)(dstaddr + image_length);

		for (i=((image_length)/sizeof(uintptr_t)); i>=0; i--) {
			*dst_addr = *src_addr;
			dst_addr--;
			src_addr--;
		}
	} else if ((uintptr_t)src_addr > (uintptr_t)dst_addr) {
		src_addr = (uintptr_t*)srcaddr;
		dst_addr = (uintptr_t*)dstaddr;

		for(i=0; i<=image_length/sizeof(uintptr_t);i++) {
			*dst_addr = *src_addr;
			dst_addr++;
			src_addr++;
		}
	}
}


/*
 * Function:   makeSaveCopy(uintptr_t srcaddr, uintptr_t length)
 *
 * Parameters: srcaddr -> start address of image
 *             length  -> length of the image
 *
 * Return:     uintptr_t: Save address of the image.
 *
 * Content:    before the image will be checked it must be saved to another
 *             RAM address. This is necessary because if a encrypted image
 *             will be checked it will be decrypted at directly at the RAM
 *             address so we only have a decrypted image but we want to store
 *             an encrypted image in the flash. So thats why we need a save
 *             copy of the original image.
 */
uintptr_t makeSaveCopy(uintptr_t srcaddr, uintptr_t length)
{
	struct ivt *ivt = (struct ivt *)srcaddr;
	uintptr_t *checkaddr = 0x0;
	uintptr_t *saveaddr = 0x0;

	checkaddr = (uintptr_t*)(uintptr_t) ivt->self;

	if (srcaddr < (uintptr_t)checkaddr)
		saveaddr = (uintptr_t*)(checkaddr + length);
	else
		saveaddr = (uintptr_t*)(srcaddr + length);

	memExchange(srcaddr, (uintptr_t)saveaddr, length);

	return (uintptr_t)saveaddr;
}


/*
 * Function:   getImageLength(uintptr_t addr)
 *
 * Parameters: addr -> start address of image
 *
 * Return:     uintptr_t: length of the image
 *
 * Content:    get the image length from the ivt.
 */
uintptr_t getImageLength(uintptr_t addr)
{
	struct ivt *ivt = (struct ivt *)addr;
	signed long offset = (signed long)((signed long)addr - (signed long)ivt->self);
	struct boot_data *data = (struct boot_data *)(ivt->boot + offset);
	return data->length;
}


/*
 * Function:   check_flash_partition(uintptr_t addr, OPTIONS eOption, loff_t off, loff_t length)
 *
 * Parameters: addr     -> start address of image to be checked
 *             eOption  -> enum what to do with the image
 *             off      -> image length
 *             length   -> image name
 *
 * Return:     0 -> writing is allowed
 *             1 -> writing to nand partition forbidden
 *
 * Content:    Checks if the image will be written to the correct partition
 *             and checks if the image fits in the partition.
 */
int check_flash_partition(uintptr_t addr, OPTIONS eOption, loff_t off, loff_t length)
{
	struct mtd_device *dev;
	struct part_info *part;
	static const char *names[] = {SECURE_PARTITIONS};
	u8 pnum;

	for (int i = 0; i < ARRAY_SIZE(names); i++) {
		if (!find_dev_and_part(names[i], &dev, &pnum, &part)) {
			if (off >= part->offset && off < part->offset + part->size) {
				if (off == part->offset) {
					if (part->size >= length) {
						return prepare_authentication(addr, eOption);
					} else {
						printf("\nPartition is too small!!\n");
						printf("\nAborting ...\n\n");
						return 1;
					}
				} else {
					printf("\nWrong partition offset!!\n");
					printf("\nAborting ...\n\n");
					return 1;
				}
			}
		}
	}
	return 0;
}


/*
 * Function:   parse_images_for_authentification(int argc, char * const argv[])
 *
 * Parameters: argc  -> count of given arguments in argv
 *             argv  -> string array
 *
 * Return:     0 -> verification successful
 *             1 -> verifi failed
 *
 * Content:    This function convert the strings to u32 values and verify
 *             the images which are behind the addresses. Except if the
 *             string is a minus. Then it ignores the minus.
 */
int parse_images_for_authentification(int argc, char * const argv[])
{
	int ret = 0;

	for (int i = 0; i < argc; i++) {
		if (strcmp(argv[i], "-") !=  0) {

			ret = prepare_authentication(parse_loadaddr(argv[i], NULL), CUT_IVT);
			if (ret)
				return ret;
		}
	}

	return 0;
}


/*
 * Function:   prepare_authentication(uintptr_t addr, OPTIONS eOption)
 *
 * Parameters: addr    -> address in RAM where the image is loaded
 *             eOption -> enum what to do with the image
 *
 * Return:     0 -> authentication successful
 *             1 -> authentication failed
 *
 * Content:    Prepare the image that it can be verified.
 */
int prepare_authentication(uintptr_t addr, OPTIONS eOption)
{
	/* HAB Variables */
	struct ivt *ivt;
	uintptr_t save_addr = 0;
	uintptr_t length = 0;
	int ret = 1;

	ivt = (struct ivt *)addr;

	/* Verify IVT header bugging out on error */
	if (verify_ivt_header(&ivt->hdr, 1)){
		if(imx_hab_is_enabled())
			return ret;
		return 0;
	}

	length = getImageLength(addr);

	if (eOption == BACKUP_IMAGE)
		save_addr = makeSaveCopy(addr, length);

	if ((addr != ivt->self))
		memExchange(addr, ivt->self, length);

	ret = imx_hab_authenticate_image(ivt->self, length, 0x0);

	ivt = (struct ivt *)addr;

	if (eOption == BACKUP_IMAGE) {
		/* get original image saved by 'makeSaveCopy' */
		memExchange(save_addr, addr, length);
	} else if (eOption == CUT_IVT) {
		memExchange((ivt->self + HAB_HEADER), addr, length);
	}
	return ret;
}
