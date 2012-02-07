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
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston,
 * MA 02111-1307 USA
 */

#include <common.h>
#include <asm/arch/s3c64xx-regs.h>
#include <asm/arch/s3c64x0.h>
#include <linux/mtd/nand.h>		  /* struct nand_ecclayout, ... */
#ifdef CONFIG_CMD_NET
#include <net.h>			  /* eth_init(), eth_halt() */
#include <netdev.h>			  /* ne2000_initialize() */
#endif
#ifdef CONFIG_CMD_LCD
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
		 . length = 5},		  /* Before bad block marker */
		{.offset = 6,
		 . length = 2},		  /* Between bad block marker and ECC */
		{.offset = 12,
		 . length = 4},		  /* Behind ECC */
	}
};
#else
/* 2048+64 pages: ECC is in bytes 1..4 in OOB, bad block marker is in byte 0 */
static struct nand_ecclayout picomod6_oob_16 = {
	.eccbytes = 4,
	.eccpos = {1, 2, 3, 4},
	.oobfree = {
		{.offset = 5,		  /* Behind bad block marker and ECC */
		 .length = 59}}
};
#endif

#if 0 //#####
static inline void delay(unsigned long loops)
{
	__asm__ volatile ("1:\n" "subs %0, %1, #1\n" "bne 1b":"=r" (loops):"0"(loops));
}
#endif //####

/*
 * Miscellaneous platform dependent initialisations
 */

#ifdef CONFIG_CMD_NET
int board_eth_init(bd_t *bis)
{
	int res;

	/* Reset AX88796 ethernet chip (Toggle nRST line for min 200us) */
	GPKDAT_REG &= ~0x00000010;
	udelay(200);
	GPKDAT_REG |= 0x00000010;

#if 0 /* Already done in NBoot */
	/* Set correct CS timing for ethernet access */
	SROM_BW_REG &= ~(0xf << 4);
	SROM_BW_REG |= (1<<7) | (1<<6) | (1<<4);
	SROM_BC1_REG = ((CS8900_Tacs<<28) + (CS8900_Tcos<<24) +
			(CS8900_Tacc<<16) + (CS8900_Tcoh<<12) +
			(CS8900_Tah<<8) + (CS8900_Tacp<<4) + (CS8900_PMC));
#endif

	/* Check for ethernet chip and register it */
	res = ne2000_initialize(0, CONFIG_DRIVER_NE2000_BASE);
	if (res >= 0) {
		eth_init(bis);		 /* Set MAC-Address in any case */
		eth_halt();
	}

	return res;
}
#endif /* CONFIG_CMD_NET */

int board_init(void)
{
	DECLARE_GLOBAL_DATA_PTR;

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

	/* Set RAM banks */
	gd->bd->bi_dram[0].start = PHYS_SDRAM_1;
	gd->bd->bi_dram[0].size = PHYS_SDRAM_1_SIZE;

	/* Set total RAM size */
	gd->ram_size = PHYS_SDRAM_1_SIZE;

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
	printf("Board: PicoCOM3\n");
	return (0);
}
#endif

#if 0//#### MMU-Test mit 1:1-Mapping
#ifdef CONFIG_ENABLE_MMU
ulong virt_to_phy_picocom3(ulong addr)
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

#ifdef CONFIG_CMD_LCD
void s3c64xx_lcd_board_init(void)
{
	/* Setup GPF15 to output 0 (backlight intensity 0) */
	__REG(GPFDAT) &= ~(0x1<<15);
	__REG(GPFCON) = (__REG(GPFCON) & ~(0x3<<30)) | (0x1<<30);
	__REG(GPFPUD) &= ~(0x3<<30);

	/* Setup GPK[2] to output 1 (Display enable off), GPK[1] to output 1
	   (VCFL off), GPK[0] to output 1 (VLCD off), no pull-up/down */
	__REG(GPKDAT) = (__REG(GPKDAT) & ~(0x7<<0)) | (0x7<<0);
	__REG(GPKCON0) = (__REG(GPKCON0) & ~(0xFFF<<0)) | (0x111<<0);
	__REG(GPKPUD) &= ~(0x3F<<0);
}

void s3c64xx_lcd_board_enable(int index)
{
	switch (index) {
	case PON_LOGIC:			  /* Activate VLCD */
		__REG(GPKDAT) &= ~(1<<0);
		break;

	case PON_DISP:			  /* Activate Display Enable signal */
		__REG(GPKDAT) &= ~(1<<2);
		break;

	case PON_CONTR:			  /* Activate signal buffers */
#if 0  /* There is no buffer enable signal on PicoCOM3 */
		__REG(GPKDAT) &= ~(1<<3);
#endif
		break;

	case PON_PWM:			  /* Activate VEEK*/
		__REG(GPFDAT) |= (0x1<<15); /* full intensity
					       #### TODO: actual PWM value */
		break;

	case PON_BL:			  /* Activate VCFL */
		__REG(GPKDAT) &= ~(1<<1);
		break;

	default:
		break;
	}
}

void s3c64xx_lcd_board_disable(int index)
{
	switch (index) {
	case POFF_BL:			  /* Deactivate VCFL */
		__REG(GPKDAT) |= (1<<1);
		break;

	case POFF_PWM:			  /* Deactivate VEEK*/
		__REG(GPFDAT) &= ~(0x1<<15);
		break;

	case POFF_CONTR:		  /* Deactivate signal buffers */
#if 0  /* There is no buffer enable signal on PicoCOM3 */
		__REG(GPKDAT) |= (1<<3);
#endif
		break;

	case POFF_DISP:			  /* Deactivate Display Enable signal */
		__REG(GPKDAT) |= (1<<2);
		break;

	case POFF_LOGIC:		  /* Deactivate VLCD */
		__REG(GPKDAT) |= (1<<0);
		break;

	default:
		break;
	}
}
#endif

#ifdef CONFIG_USB_OHCI_NEW
int usb_board_init(void)
{
	/* Clock source 48MHz for USB */
	CLK_SRC_REG &= ~(3<<5);

	/* Activate USB Host clock */
	HCLK_GATE_REG |=  (1<<29);

	/* Activate USB OTG clock */
	HCLK_GATE_REG |= (1<<20);

	/* Set OTG special flag */
	OTHERS_REG |= 1<<16;

	/* Power up OTG PHY */
	__REG(S3C_OTG_PHYPWR) = 0;

	/* Use UTMI interface of OTG or serial 1 interface of USB1.1 host */
	__REG(S3C_OTG_PHYCTRL) = 0x10; /* 0x50 for serial 1, or 0x10 for OTG */

	/* Reset PHY */
	__REG(S3C_OTG_RSTCON) = 1;
	udelay(50);
	__REG(S3C_OTG_RSTCON) = 0;
	udelay(50);

	/* Switch PWR1 on */
	GPKDAT_REG |= (1<<7);		  /* Out 1 */
	GPKCON0_REG = (GPKCON0_REG & ~0xf0000000) | 0x10000000; /* Output */
	GPKPUD_REG &= 0xffff3fff;	  /* No Pullup/down */

	/* Let the power voltage settle */
	udelay(10000);
	//udelay(50000);

	return 0;
}

int usb_board_init_fail(void)
{
	return 0;
}

int usb_board_stop(void)
{
	GPKDAT_REG &= ~(1<<7);		  /* Out 0 */

	return 0;
}
#endif
