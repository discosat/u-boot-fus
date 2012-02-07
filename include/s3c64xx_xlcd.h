/*
 * Hardware specific LCD controller part for S3C64XX
 *
 * (C) Copyright 2011
 * Hartmut Keller, F&S Elektronik Systeme GmbH, keller@fs-net.de
 *
 * See file CREDITS for list of people who contributed to this
 * project.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of
 * the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.	 See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston,
 * MA 02111-1307 USA
 */

#ifndef _S3C64XX_XLCD_H_
#define _S3C64XX_XLCD_H_

#include "cmd_lcd.h"			  /* vidinfo_t */

/* Initialize controller hardware and fill in vidinfo */
void s3c64xx_xlcd_init(vidinfo_t *pvi);

#endif /*!_S3C64XX_XLCD_H_*/
