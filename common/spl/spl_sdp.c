/*
 * (C) Copyright 2016 Toradex
 * Author: Stefan Agner <stefan.agner@toradex.com>
 *
 * SPDX-License-Identifier:	GPL-2.0+
 */

#include <common.h>
#include <spl.h>
#include <usb.h>
#include <g_dnl.h>
#include <sdp.h>

DECLARE_GLOBAL_DATA_PTR;

void board_sdp_cleanup(void)
{
	board_usb_cleanup(CONFIG_SPL_SDP_USB_DEV, USB_INIT_DEVICE);
}


int spl_sdp_stream_continue(const struct sdp_stream_ops *ops, bool single)
{
	const int controller_index = CONFIG_SPL_SDP_USB_DEV;

	/* Should not return, unless in single mode when it returns after one
	   SDP command */
	sdp_handle(controller_index, ops, single);

	if (!single) {
		pr_err("SDP ended\n");
		return -EINVAL;
	}

	return 0;
}

/**
 * Load an image with Serial Download Protocol (SDP)
 *
 * @ops:	call-back functions for stream mode
 *
 * Typically, the file is downloaded and stored at the given address. In
 * stream mode, when ops is not NULL, data is not automatically stored, but
 * instead a call-back function is called for each data chunk that can handle
 * the data on the fly. For example it can only load the FDT part of a FIT
 * image, parse it, and then only loads those images that are meaningful and
 * ignores everything else.
 */
int spl_sdp_stream_image(const struct sdp_stream_ops *ops, bool single)
{
	int ret;
	const int controller_index = CONFIG_SPL_SDP_USB_DEV;

	board_usb_init(controller_index, USB_INIT_DEVICE);

	g_dnl_clear_detach();
	g_dnl_register("usb_dnl_sdp");

	ret = sdp_init(controller_index);
	if (ret) {
		pr_err("SDP init failed: %d\n", ret);
		return -ENODEV;
	}

	return spl_sdp_stream_continue(ops, single);
}

/**
 * Load an image with Serial Download Protocol (SDP)
 *
 * @spl_image:	info about the loaded image (ignored)
 * @bootdev:	info about the device to load from (ignored)
 *
 * Download an image with Serial Download Protocol (SDP).
 */
static int spl_sdp_load_image(struct spl_image_info *spl_image,
				struct spl_boot_device *bootdev)
{
	return spl_sdp_stream_image(NULL, false);
}

SPL_LOAD_IMAGE_METHOD("USB SDP", 0, BOOT_DEVICE_BOARD, spl_sdp_load_image);
