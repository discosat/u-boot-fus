/*
 * fs_usb_common.h
 *
 * (C) Copyright 2018
 * Hartmut Keller, F&S Elektronik Systeme GmbH, keller@fs-net.de
 *
 * Common USB code for OTG mode, bus power, polarities, environment handling
 *
 * SPDX-License-Identifier:	GPL-2.0+
 */

#ifndef __FS_USB_COMMON_H__
#define __FS_USB_COMMON_H__

#include <asm/imx-common/iomux-v3.h>	/* iomux_v3_cfg_t */

/* Possible USB modes */
#define FS_USB_DEVICE     1		      /* DEV only */
#define FS_USB_HOST       2		      /* HOST only */
#define FS_USB_OTG_DEVICE (FS_USB_DEVICE + 2) /* DEV + HOST, DEV as default */
#define FS_USB_OTG_HOST   (FS_USB_HOST + 2)   /* DEV + HOST, HOST as default */

/*
 * F&S USB port configuration
 *
 * mode:       Possible modes: one of the above
 * pwr_pad:    Pad for host power switching (only used in host mode)
 * pwr_gpio:   GPIO number for power pad (if -1, use USB controller function)
 * pwr_pol:    Power signal polarity 0: Active high, 1: Active low
 * id_pad:     ID; set to NULL if no ID signal available (valid if FS_USB_OTG)
 * id_gpio:    GPIO number for ID pad (must be given if od_pad is non-NULL)
 * reset_pad:  RESET for USB hub; set NULL if no such signal (HOST mode only)
 * reset_gpio: GPIO number for RESET pad
 *
 * If CONFIG_FS_USB_PWR_USBNC is set, the dedicated PWR function of the USB
 * controller will be used to switch host power (where available). Otherwise
 * the host power will be switched by using the pad as GPIO.
 */
struct fs_usb_port_cfg {
	unsigned int mode;
	iomux_v3_cfg_t const *pwr_pad;
	int pwr_gpio;
	int pwr_pol;
	iomux_v3_cfg_t const *id_pad;
	int id_gpio;
	iomux_v3_cfg_t const *reset_pad;
	int reset_gpio;
};

/* Initialize one port, set role if OTG, switch on power if host */
int fs_usb_set_port(int index, struct fs_usb_port_cfg *cfg);

#endif /* !__FS_USB_COMMON_H__ */
