/*
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
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston,
 * MA 02111-1307 USA
 */

#include <common.h>
#include <s3c64xx-regs.h>
#include <s3c64x0.h>
#include <linux/mtd/nand.h>		  /* struct nand_ecclayout, ... */
#ifdef CONFIG_LCD
#include <cmd_lcd.h>			  /* PON_*, POFF_* */
#endif

/* ------------------------------------------------------------------------- */
#define CS8900_Tacs	(0x0)	// 0clk		address set-up
#define CS8900_Tcos	(0x4)	// 4clk		chip selection set-up
#define CS8900_Tacc	(0xE)	// 14clk	access cycle
#define CS8900_Tcoh	(0x1)	// 1clk		chip selection hold
#define CS8900_Tah	(0x4)	// 4clk		address holding time
#define CS8900_Tacp	(0x6)	// 6clk		page mode access cycle
#define CS8900_PMC	(0x0)	// normal(1data)page mode configuration

#ifdef __NAND_64MB__
/* 512+16 pages: ECC is in bytes 8..11 in OOB, bad block marker is in byte 5 */
static struct nand_ecclayout picomod6_oob_16 = {
	.eccbytes = 4,
	.eccpos = {8, 9, 10, 11},
	.oobfree = {
		{.offset = 0,
		 . length = 5},           /* Before bad block marker */
                {.offset = 6,
		 . length = 2},           /* Between bad block marker and ECC */
                {.offset = 12,
		 . length = 4},           /* Behind ECC */
	}
};
#else
/* 2048+64 pages: ECC is in bytes 1..4 in OOB, bad block marker is in byte 0 */
static struct nand_ecclayout picomod6_oob_16 = {
	.eccbytes = 4,
	.eccpos = {1, 2, 3, 4},
	.oobfree = {
		{.offset = 5,             /* Behind bad block marker and ECC */
		 .length = 59}}
};
#endif

static inline void delay(unsigned long loops)
{
	__asm__ volatile ("1:\n" "subs %0, %1, #1\n" "bne 1b":"=r" (loops):"0"(loops));
}

/*
 * Miscellaneous platform dependent initialisations
 */

static void ax88796_pre_init(void)
{
    /* Reset AX88796 ethernet chip (Toggle nRST line for min 200us); we can't
       use udelay() here, as timers are not yet initialized */
    GPKDAT_REG &= ~0x00000010;
    delay(10000000);
    GPKDAT_REG |= 0x00000010;

#if 0 //#####HK
	SROM_BW_REG &= ~(0xf << 4);
	SROM_BW_REG |= (1<<7) | (1<<6) | (1<<4);
	SROM_BC1_REG = ((CS8900_Tacs<<28)+(CS8900_Tcos<<24)+(CS8900_Tacc<<16)+(CS8900_Tcoh<<12)+(CS8900_Tah<<8)+(CS8900_Tacp<<4)+(CS8900_PMC));
#endif //####HK
}

int board_init(void)
{
	DECLARE_GLOBAL_DATA_PTR;

	ax88796_pre_init();

	gd->bd->bi_arch_number = MACH_TYPE;
	gd->bd->bi_boot_params = (PHYS_SDRAM_1+0x100);

#if 1
	icache_enable();
	/* We already set up the MMU pagetable and could switch on the data
	   cache, but then the USB driver will not work. The idea is to add
	   another mapping for the RAM without caching and the USB driver
	   should use this. Maybe other drivers don't work too? */
//####	dcache_enable();
#endif
	return 0;
}

int dram_init(void)
{
	DECLARE_GLOBAL_DATA_PTR;

	gd->bd->bi_dram[0].start = PHYS_SDRAM_1;
	gd->bd->bi_dram[0].size = PHYS_SDRAM_1_SIZE;

	return 0;
}

#ifdef BOARD_LATE_INIT
int board_late_init (void)
{
	return 0;
}
#endif

#ifdef CONFIG_DISPLAY_BOARDINFO
int checkboard(void)
{
	printf("Board:   PicoMOD6\n");
	return (0);
}
#endif

#if 0//#### MMU-Test mit 1:1-Mapping
#ifdef CONFIG_ENABLE_MMU
ulong virt_to_phy_picomod6(ulong addr)
{
	if ((0xc0000000 <= addr) && (addr < 0xc8000000))
		return (addr - 0xc0000000 + 0x50000000);
	else
		printf("do not support this address : %08lx\n", addr);

	return addr;
}
#endif
#endif //0####

/* Initialize some board specific nand chip settings */
extern int s3c64xx_nand_init(struct nand_chip *nand);
int board_nand_init(struct nand_chip *nand)
{
#ifdef __NAND_64MB__
	nand->ecc.layout = &picomod6_oob_16;
#else
	nand->ecc.layout = &picomod6_oob_64;
#endif

	/* Call CPU specific init */
	return s3c64xx_nand_init(nand);
}



#ifdef CONFIG_MMC
void mmc_s3c64xx_board_power(unsigned int channel)
{
	switch (channel) {
	case 0:
		/* Set GPN6 (GPIO6) as output low for SD card power, disable
		   any pull-up/down */
		GPNDAT_REG &= ~0x40;
		GPNCON_REG = (GPNCON_REG & ~0x3000) | 0x1000;
		GPNPUD_REG &= ~0x3000;
		break;

	case 1:
		/* There is no power switch on this slot, power is always on. */
		break;

	case 2:
		/* Not used */
		break;
	}
}
#endif

#ifdef CONFIG_LCD
void s3c64xx_lcd_board_init(void)
{
	/* Setup GPF15 to output 0 (backlight intensity 0) */
	__REG(GPFDAT) &= ~(0x1<<15);
	__REG(GPFCON) = (__REG(GPFCON) & ~(0x3<<30)) | (0x1<<30);
	__REG(GPFPUD) &= ~(0x3<<30);

	/* Setup GPK[3] to output 1 (Buffer enable off), GPK[2] to output 1
	   (Display enable off), GPK[1] to output 0 (VCFL off), GPK[0] to
	   output 0 (VLCD off), no pull-up/down */
	__REG(GPKDAT) = (__REG(GPKDAT) & ~(0xf<<0)) | (0xc<<0);
	__REG(GPKCON0) = (__REG(GPKCON0) & ~(0xFFFF<<0)) | (0x1111<<0);
	__REG(GPKPUD) &= ~(0xFF<<0);
}

void s3c64xx_lcd_board_enable(int index)
{
	switch (index) {
	case PON_LOGIC:			  /* Activate VLCD */
		__REG(GPKDAT) |= (1<<0);
		break;

	case PON_DISP:			  /* Activate Display Enable signal */
		__REG(GPKDAT) &= ~(1<<2);
		break;

	case PON_CONTR:			  /* Activate signal buffers */
		__REG(GPKDAT) &= ~(1<<3);
		break;

	case PON_PWM:			  /* Activate VEEK*/
		__REG(GPFDAT) |= (0x1<<15); /* full intensity
					       #### TODO: actual PWM value */
		break;

	case PON_BL:			  /* Activate VCFL */
		__REG(GPKDAT) |= (1<<1);
		break;

	default:
		break;
	}
}

void s3c64xx_lcd_board_disable(int index)
{
	switch (index) {
	case POFF_BL:			  /* Deactivate VCFL */
		__REG(GPKDAT) &= ~(1<<1);
		break;

	case POFF_PWM:			  /* Deactivate VEEK*/
		__REG(GPFDAT) &= ~(0x1<<15);
		break;

	case POFF_CONTR:		  /* Deactivate signal buffers */
		__REG(GPKDAT) |= (1<<3);
		break;

	case POFF_DISP:			  /* Deactivate Display Enable signal */
		__REG(GPKDAT) |= (1<<2);
		break;

	case POFF_LOGIC:		  /* Deactivate VLCD */
		__REG(GPKDAT) &= ~(1<<0);
		break;

	default:
		break;
	}
}
#endif
