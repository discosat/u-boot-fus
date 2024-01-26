// SPDX-License-Identifier: GPL-2.0+
/*
 * (C) Copyright 2016 Toradex
 * Author: Stefan Agner <stefan.agner@toradex.com>
 */

#include <common.h>
#include <log.h>
#include <spl.h>
#include <usb.h>
#include <g_dnl.h>
#include <sdp.h>

void board_sdp_cleanup(void)
{
	int controller_index = CONFIG_SPL_SDP_USB_DEV;
	int index = board_usb_gadget_port_auto();
	if (index >= 0)
		controller_index = index;
	usb_gadget_release(CONFIG_SPL_SDP_USB_DEV);
}


int spl_sdp_stream_continue(const struct sdp_stream_ops *ops, bool single)
{
	int controller_index = CONFIG_SPL_SDP_USB_DEV;
	int index = board_usb_gadget_port_auto();
	if (index >= 0)
		controller_index = index;

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
	int index;
	static int initdone;

	if (!initdone) {
		/* Only init the USB controller once while in SPL */
		int controller_index = CONFIG_SPL_SDP_USB_DEV;
		index = board_usb_gadget_port_auto();
		if (index >= 0)
			controller_index = index;

		usb_gadget_initialize(controller_index);

		g_dnl_clear_detach();
		g_dnl_register("usb_dnl_sdp");

		ret = sdp_init(controller_index);
		if (ret) {
			pr_err("SDP init failed: %d\n", ret);
			return -ENODEV;
		}

		initdone = 1;
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
