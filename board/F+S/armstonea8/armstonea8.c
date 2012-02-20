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
//#include <asm/arch/s3c64xx-regs.h>
//#include <asm/arch/s3c64x0.h>
#include <linux/mtd/nand.h>		  /* struct nand_ecclayout, ... */
#ifdef CONFIG_CMD_NET
#include <net.h>			  /* eth_init(), eth_halt() */
#include <netdev.h>			  /* ne2000_initialize() */
#endif
#ifdef CONFIG_CMD_LCD
#include <cmd_lcd.h>			  /* PON_*, POFF_* */
#endif
#include <asm/arch/cpu.h>		  /* samsung_get_base_gpio() */
#include <asm/gpio.h>			  /* gpio_set_value(), ... */
#ifdef CONFIG_GENERIC_MMC
#include <asm/arch/mmc.h>		  /* s5p_mmc_init() */
#include <asm/arch/clock.h>		  /* struct s5pc110_clock */
#include <asm/arch/clk.h>		  /* get_pll_clk(), MPLL */
#endif

/* ------------------------------------------------------------------------- */

/* 2048+64 pages: We compute 4 bytes ECC for each 512 bytes of the page; ECC
   is in bytes 16..31 in OOB, bad block marker in byte 0, but two bytes are
   checked; so our first free btye is at offset 2. */
static struct nand_ecclayout armstonea8_oob_64 = {
	.eccbytes = 16,
	.eccpos = {16, 17, 18, 19, 20, 21, 22, 23,
		   24, 25, 26, 27, 28, 29, 30, 31},
	.oobfree = {
		{.offset = 2,		  /* Between bad block marker and ECC */
		 .length = 14},
		{.offset = 32,		  /* Behind ECC */
		 .length = 32}}
};

/*
 * Miscellaneous platform dependent initialisations
 */

#ifdef CONFIG_CMD_NET
int board_eth_init(bd_t *bis)
{
	int res;
	unsigned int gph0_3 = s5pc110_gpio_get_nr(h0, 3);

	/* Reset AX88796 ethernet chip (Toggle nRST line for min 200us) */
	gpio_set_value(gph0_3, 0);
	udelay(200);
	gpio_set_value(gph0_3, 1);

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

#if 0 //#### Only rev 1.1
		/* Check for second ethernet chip and register it */
		res = ne2000_initialize(1, CONFIG_DRIVER_NE2000_BASE2);
#endif
	}

	return res;
}
#endif /* CONFIG_CMD_NET */

#if 0
int board_serial_init(void)
{
	s5p_serial_register(0, "fs_ser0");
	s5p_serial_register(1, "fs_ser1");
	s5p_serial_register(2, "fs_ser2");
	s5p_serial_register(3, "fs_ser3");
}
#endif

int board_init(void)
{
	DECLARE_GLOBAL_DATA_PTR;

	gd->bd->bi_arch_number = MACH_TYPE;
	gd->bd->bi_boot_params = (PHYS_SDRAM_0+0x100);

#if 0 //###
	icache_enable();
	/* We already set up the MMU pagetable and could switch on the data
	   cache, but then the USB driver will not work. The idea is to add
	   another mapping for the RAM without caching and the USB driver
	   should use this. Maybe other drivers don't work too? */
//####	dcache_enable();
#endif
	return 0;
}

void dram_init_banksize(void)
{
	DECLARE_GLOBAL_DATA_PTR;

#ifdef CONFIG_ENABLE_MMU
	/* If we use the MMU, RAM is remapped to occupy one long region and we
	   can use both banks */
	gd->bd->bi_dram[0].start = PHYS_SDRAM_0;
	gd->bd->bi_dram[0].size = PHYS_SDRAM_0_SIZE;
	gd->bd->bi_dram[1].start = PHYS_SDRAM_1;
	gd->bd->bi_dram[1].size = PHYS_SDRAM_1_SIZE;
#else
	/* If we don't use the MMU, RAM has a gap and we only use bank 1! */
	gd->bd->bi_dram[0].start = PHYS_SDRAM_1;
	gd->bd->bi_dram[0].size = PHYS_SDRAM_1_SIZE;
#endif
}

int dram_init(void)
{
	DECLARE_GLOBAL_DATA_PTR;

	//#### TODO: read from bd->bi_boot_params
#ifdef CONFIG_ENABLE_MMU
	/* If we use the MMU, RAM is remapped to occupy one long region */
	gd->ram_size = PHYS_SDRAM_0_SIZE + PHYS_SDRAM_1_SIZE;
#else
	/* If we don't use the MMU, RAM has a gap and we can only use bank 1 */
	gd->ram_size = PHYS_SDRAM_1_SIZE;
#endif

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
	printf("Board: armStoneA8\n");
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
extern int s5p_nand_init(struct nand_chip *nand);
int board_nand_init(struct nand_chip *nand)
{
	nand->ecc.layout = &armstonea8_oob_64;

	/* Call CPU specific init */
	return s5p_nand_init(nand);
}


#ifdef CONFIG_GENERIC_MMC
int board_mmc_init(bd_t *bis)
{
	struct s5pc110_gpio *gpio = 
		(struct s5pc110_gpio *)samsung_get_base_gpio();
	struct s5pc110_clock *clock = 
		(struct s5pc110_clock *)samsung_get_base_clock();
	unsigned int div;

	/* We want to use MCLK (667 MHz) as MMC0 clock source */
	writel((readl(&clock->src4) & ~(15 << 0)) | (6 << 0), &clock->src4);

	/* MMC clock must not exceed 52 MHz, the MMC driver expects it to be
	   between 40 an 50 MHz. So we'll compute a divider that generates a
	   value below 50 MHz; the result would have to be rounded up, i.e. we
	   would have to add 1. However we also have to subtract 1 again when
	   we store div in the CLK_DIV4 register, so we can skip this +1-1. */
	div = get_pll_clk(MPLL)/49999999;
	if (div > 15)
		div = 15;
	writel((readl(&clock->div4) & ~(15 << 0)) | (div << 0), &clock->div4);

	/* All clocks are enabled in CLK_SRC_MASK0 and CLK_GATE_IP2 after reset
	   and NBoot does not change this; so MMC0 clock should be enabled and
	   there is no need to set anything there */

	/* Configure GPG0 pins for MMC/SD */
	s5p_gpio_cfg_pin(&gpio->g0, 0, 2);
	s5p_gpio_cfg_pin(&gpio->g0, 1, 2);
	s5p_gpio_cfg_pin(&gpio->g0, 2, 2);
	s5p_gpio_cfg_pin(&gpio->g0, 3, 2);
	s5p_gpio_cfg_pin(&gpio->g0, 4, 2);
	s5p_gpio_cfg_pin(&gpio->g0, 5, 2);
	s5p_gpio_cfg_pin(&gpio->g0, 6, 2);

	/* We have mmc0 with 4 bit bus width */
	return s5p_mmc_init(0, 4);
}
#endif

#ifdef CONFIG_CMD_LCD
// ####TODO
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
