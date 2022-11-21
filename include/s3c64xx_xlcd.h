/*
 * Hardware specific LCD controller part for S3C64XX
 *
 * (C) Copyright 2011
 * Hartmut Keller, F&S Elektronik Systeme GmbH, keller@fs-net.de
 *
 * SPDX-License-Identifier:	GPL-2.0+
 */

#ifndef _S3C64XX_XLCD_H_
#define _S3C64XX_XLCD_H_

#include "cmd_lcd.h"			  /* vidinfo_t */

/* Initialize controller hardware and fill in vidinfo */
void s3c64xx_xlcd_init(vidinfo_t *pvi);

#endif /*!_S3C64XX_XLCD_H_*/
