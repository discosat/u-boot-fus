/*
 * (C) Copyright 2003
 * H. Keller, keller@fs-net.de
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

/*
 * Date & Time support for the built-in Samsung S5PC1xx RTC
 */

#include <config.h>
#include <common.h>
#include <rtc.h>			  /* struct rtc_time */
#include <asm/arch/cpu.h>		  /* samsung_get_base_rtc() */

struct s5p_rtc {			  /* Offsets */
	volatile unsigned int res0[12];	  /* 0x00 */
	volatile unsigned int INTP;	  /* 0x30 */
	volatile unsigned int res1[3];	  /* 0x34 */
        volatile unsigned int RTCCON;	  /* 0x40 */
        volatile unsigned int TICNT;	  /* 0x44 */
        volatile unsigned int res2[2];	  /* 0x48 */
        volatile unsigned int RTCALM;	  /* 0x50 */
        volatile unsigned int ALMSEC;	  /* 0x54 */
        volatile unsigned int ALMMIN;	  /* 0x58 */
        volatile unsigned int ALMHOUR;	  /* 0x5c */
        volatile unsigned int ALMDATE;	  /* 0x60 */
        volatile unsigned int ALMMON;	  /* 0x64 */
        volatile unsigned int ALMYEAR;	  /* 0x68 */
        volatile unsigned int res3[1];	  /* 0x6c */
        volatile unsigned int BCDSEC;	  /* 0x70 */
        volatile unsigned int BCDMIN;	  /* 0x74 */
        volatile unsigned int BCDHOUR;	  /* 0x78 */
        volatile unsigned int BCDDATE;	  /* 0x7c */
        volatile unsigned int BCDDAY;	  /* 0x80 */
        volatile unsigned int BCDMON;	  /* 0x84 */
        volatile unsigned int BCDYEAR;	  /* 0x88 */
        volatile unsigned int res4[1];	  /* 0x8c */
        volatile unsigned int CURTICCNT;  /* 0x90 */
};

/* Convert BCD number to binary number */
static int s5p_bcd2bin(unsigned int bcd)
{
	if (bcd == 0)
		return 0;
	return s5p_bcd2bin(bcd >> 4) * 10 + ((int)bcd & 0xf);
}

/* Convert binary number to BCD number */
static unsigned int s5p_bin2bcd(int bin)
{
	if (bin == 0)
		return 0;
	return (s5p_bin2bcd(bin/10) << 4) | ((unsigned int)bin % 10);
}

/* ------------------------------------------------------------------------- */

int rtc_get(struct rtc_time *tmp)
{
	unsigned int sec, min, hour, mday, wday, mon, year;
	struct s5p_rtc *rtc = (struct s5p_rtc *)samsung_get_base_rtc();

	/* enable access to RTC registers */
	rtc->RTCCON |= 0x01;

	/* read RTC registers */
	do {
		sec	= rtc->BCDSEC;
		min	= rtc->BCDMIN;
		hour	= rtc->BCDHOUR;
		mday	= rtc->BCDDATE;
		wday	= rtc->BCDDAY;
		mon	= rtc->BCDMON;
		year	= rtc->BCDYEAR;
	} while (sec != rtc->BCDSEC);

	/* disable access to RTC registers */
	rtc->RTCCON &= ~0x01;

	/* year has 2 digits on s5pc100 and 3 digits on s5pc110 */
	year &= cpu_is_s5pc100() ? 0x00ff : 0x0fff;
	tmp->tm_sec  = s5p_bcd2bin(sec  & 0x7F);
	tmp->tm_min  = s5p_bcd2bin(min  & 0x7F);
	tmp->tm_hour = s5p_bcd2bin(hour & 0x3F);
	tmp->tm_mday = s5p_bcd2bin(mday & 0x3F);
	tmp->tm_mon  = s5p_bcd2bin(mon & 0x1F);
	tmp->tm_year = s5p_bcd2bin(year) + 2000;
	tmp->tm_wday = s5p_bcd2bin(wday & 0x07);
	tmp->tm_yday = 0;
	tmp->tm_isdst= 0;

#ifdef RTC_DEBUG
	printf("Get DATE: %4d-%02d-%02d (wday=%d)  TIME: %2d:%02d:%02d\n",
	       tmp->tm_year, tmp->tm_mon, tmp->tm_mday, tmp->tm_wday,
	       tmp->tm_hour, tmp->tm_min, tmp->tm_sec);
#endif

	return 0;
}

int rtc_set(struct rtc_time *tmp)
{
	struct s5p_rtc *const rtc = (struct s5p_rtc *)samsung_get_base_rtc();
	unsigned int sec, min, hour, mday, wday, mon, year;

#ifdef RTC_DEBUG
	printf("Set DATE: %4d-%02d-%02d (wday=%d)  TIME: %2d:%02d:%02d\n",
	       tmp->tm_year, tmp->tm_mon, tmp->tm_mday, tmp->tm_wday,
	       tmp->tm_hour, tmp->tm_min, tmp->tm_sec);
#endif
	year	= s5p_bin2bcd(tmp->tm_year);
	mon	= s5p_bin2bcd(tmp->tm_mon);
	wday	= s5p_bin2bcd(tmp->tm_wday);
	mday	= s5p_bin2bcd(tmp->tm_mday);
	hour	= s5p_bin2bcd(tmp->tm_hour);
	min	= s5p_bin2bcd(tmp->tm_min);
	sec	= s5p_bin2bcd(tmp->tm_sec);

	printf("tm_year = %d, year=0x%x, ", tmp->tm_year, year);

	/* year has 2 digits on s5pc100 and 3 digits on s5pc110 */
	year &= cpu_is_s5pc100() ? 0x00ff : 0x0fff;

	printf("year=0x%x\n", year);

	/* enable access to RTC registers */
	rtc->RTCCON |= 0x01;

	/* write RTC registers */
	rtc->BCDSEC	= sec;
	rtc->BCDMIN	= min;
	rtc->BCDHOUR	= hour;
	rtc->BCDDATE	= mday;
	rtc->BCDDAY	= wday;
	rtc->BCDMON	= mon;
	rtc->BCDYEAR	= year;

	/* disable access to RTC registers */
	rtc->RTCCON &= ~0x01;

	return 0;
}

void rtc_reset (void)
{
	struct s5p_rtc *const rtc = (struct s5p_rtc *)samsung_get_base_rtc();

	rtc->RTCCON = (rtc->RTCCON & ~0x06) | 0x08;
	rtc->RTCCON &= ~(0x08|0x01);
}
