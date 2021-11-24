// SPDX-License-Identifier: GPL-2.0+
/*
 * (C) Copyright 2016 Toradex
 * Author: Stefan Agner <stefan.agner@toradex.com>
 */

#include <common.h>
#include <spl.h>
#include <usb.h>
#include <g_dnl.h>
#include <sdp.h>

static struct spl_image_info *image;

void board_sdp_cleanup(void)
{
	usb_gadget_release(CONFIG_SPL_SDP_USB_DEV);
}

int spl_sdp_stream_continue(const struct sdp_stream_ops *ops, bool single)
{
	const int controller_index = CONFIG_SPL_SDP_USB_DEV;
	int ret = 0;

	/* Should not return, unless in single mode when it returns after one
	   SDP command */
	if (single)
		ret = spl_sdp_handle(controller_index, image, ops, single);
	else
		ret = spl_sdp_handle(controller_index, NULL, ops, single);

	if (!single)
		return -EINVAL;

	return ret;
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
	int controller_index = CONFIG_SPL_SDP_USB_DEV;

	index = board_usb_gadget_port_auto();
	if (index >= 0)
		controller_index = index;

	usb_gadget_initialize(controller_index);

	g_dnl_clear_detach();
	ret = g_dnl_register("usb_dnl_sdp");
	if (ret) {
		pr_err("SDP dnl register failed: %d\n", ret);
		return ret;
	}

	ret = sdp_init(controller_index);
	if (ret) {
		pr_err("SDP init failed: %d\n", ret);
		return -ENODEV;
	}

	/*
	 * This command either loads a legacy image, jumps and never returns,
	 * or it loads a FIT image and returns it to be handled by the SPL
	 * code.
	 */
	ret = spl_sdp_stream_continue(ops, single);
	debug("SDP ended\n");

	usb_gadget_release(controller_index);
	return ret;
}

static int spl_sdp_load_image(struct spl_image_info *spl_image,
			      struct spl_boot_device *bootdev)
{
	int ret = 0;
	image = spl_image;
	ret = spl_sdp_stream_image(NULL, true);
	return ret;
}

SPL_LOAD_IMAGE_METHOD("USB SDP", 0, BOOT_DEVICE_BOARD, spl_sdp_load_image);
