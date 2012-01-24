/*
 * (C) Copyright 2003
 * David Müller ELSOFT AG Switzerland. d.mueller@elsoft.ch
 *
 * (C) Copyright 2008
 * Guennadi Liakhovetki, DENX Software Engineering, <lg@denx.de>
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
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston,
 * MA 02111-1307 USA
 */

/************************************************
 * NAME	    : S3C64XX.h
 * Version  : 31.3.2003
 *
 * common stuff for SAMSUNG S3C64XX SoC
 ************************************************/

#ifndef __S3C64XX_H__
#define __S3C64XX_H__

#if defined(CONFIG_SYNC_MODE) && defined(CONFIG_S3C6400)
#error CONFIG_SYNC_MODE unavailable on S3C6400, please, fix your configuration!
#endif

#include <asm/types.h>

/* UART (see manual chapter 11) */
typedef struct {
	volatile u32	ULCON;
	volatile u32	UCON;
	volatile u32	UFCON;
	volatile u32	UMCON;
	volatile u32	UTRSTAT;
	volatile u32	UERSTAT;
	volatile u32	UFSTAT;
	volatile u32	UMSTAT;
#ifdef __BIG_ENDIAN
	volatile u8	res1[3];
	volatile u8	UTXH;
	volatile u8	res2[3];
	volatile u8	URXH;
#else /* Little Endian */
	volatile u8	UTXH;
	volatile u8	res1[3];
	volatile u8	URXH;
	volatile u8	res2[3];
#endif
	volatile u32	UBRDIV;
#ifdef __BIG_ENDIAN
	volatile u8	res3[2];
	volatile u16	UDIVSLOT;
#else
	volatile u16	UDIVSLOT;
	volatile u8	res3[2];
#endif
	volatile u32	UINTP;
	volatile u32	UINTSP;
	volatile u32	UINTM;
} s3c64xx_uart;

/* RTC (see manual chapter 17) */
typedef struct {
#ifdef __BIG_ENDIAN
	volatile u8	res1[67];
	volatile u8	RTCCON;
	volatile u8	res2[3];
	volatile u8	TICNT;
	volatile u8	res3[11];
	volatile u8	RTCALM;
	volatile u8	res4[3];
	volatile u8	ALMSEC;
	volatile u8	res5[3];
	volatile u8	ALMMIN;
	volatile u8	res6[3];
	volatile u8	ALMHOUR;
	volatile u8	res7[3];
	volatile u8	ALMDATE;
	volatile u8	res8[3];
	volatile u8	ALMMON;
	volatile u8	res9[3];
	volatile u8	ALMYEAR;
	volatile u8	res10[3];
	volatile u8	RTCRST;
	volatile u8	res11[3];
	volatile u8	BCDSEC;
	volatile u8	res12[3];
	volatile u8	BCDMIN;
	volatile u8	res13[3];
	volatile u8	BCDHOUR;
	volatile u8	res14[3];
	volatile u8	BCDDATE;
	volatile u8	res15[3];
	volatile u8	BCDDAY;
	volatile u8	res16[3];
	volatile u8	BCDMON;
	volatile u8	res17[3];
	volatile u8	BCDYEAR;
#else /*  little endian */
	volatile u8	res0[64];
	volatile u8	RTCCON;
	volatile u8	res1[3];
	volatile u8	TICNT;
	volatile u8	res2[11];
	volatile u8	RTCALM;
	volatile u8	res3[3];
	volatile u8	ALMSEC;
	volatile u8	res4[3];
	volatile u8	ALMMIN;
	volatile u8	res5[3];
	volatile u8	ALMHOUR;
	volatile u8	res6[3];
	volatile u8	ALMDATE;
	volatile u8	res7[3];
	volatile u8	ALMMON;
	volatile u8	res8[3];
	volatile u8	ALMYEAR;
	volatile u8	res9[3];
	volatile u8	RTCRST;
	volatile u8	res10[3];
	volatile u8	BCDSEC;
	volatile u8	res11[3];
	volatile u8	BCDMIN;
	volatile u8	res12[3];
	volatile u8	BCDHOUR;
	volatile u8	res13[3];
	volatile u8	BCDDATE;
	volatile u8	res14[3];
	volatile u8	BCDDAY;
	volatile u8	res15[3];
	volatile u8	BCDMON;
	volatile u8	res16[3];
	volatile u8	BCDYEAR;
	volatile u8	res17[3];
#endif
} s3c64xx_rtc;

/* PWM TIMER (see manual chapter 10) */
typedef struct {
	volatile u32	TCNTB;
	volatile u32	TCMPB;
	volatile u32	TCNTO;
} s3c64xx_timer;

typedef struct {
	volatile u32	TCFG0;
	volatile u32	TCFG1;
	volatile u32	TCON;
	s3c64xx_timer	ch[4];
	volatile u32	TCNTB4;
	volatile u32	TCNTO4;
} s3c64xx_timers;

#endif /*__S3C64XX_H__*/
