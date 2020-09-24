/*
 * Copyright 2018 NXP
 * (C) Copyright 2018-2019
 *
 * Hartmut Keller, F&S Elektronik Systeme GmbH, keller@fs-net.de
 * Anatol Derksen, F&S Elektronik Systeme GmbH, derksen@fs-net.de
 *
 * Board specific functions for F&S boards based on Freescale i.MX8MM CPU
 *
 * SPDX-License-Identifier:	GPL-2.0+
 */

#include <common.h>
#include <spl.h>
#include <asm/io.h>
#include <errno.h>
#include <asm/io.h>
#include <asm/mach-imx/iomux-v3.h>
#include <asm/arch/imx8mm_pins.h>
#include <asm/arch/sys_proto.h>
#include <asm/arch/clock.h>
#include <asm/mach-imx/gpio.h>
#include <asm/mach-imx/mxc_i2c.h>
#include <fsl_esdhc.h>
#include <mmc.h>
#include <asm/arch/imx8m_ddr.h>

#include "../common/fs_board_common.h"	/* fs_board_*() */
#include "../common/fs_mmc_common.h"	/* struct fs_mmc_cd, fs_mmc_*(), ... */

DECLARE_GLOBAL_DATA_PTR;

extern struct dram_timing_info dram_timing_k4f8e304hb_mgcj;

static struct fs_nboot_args nbootargs;

#define FEAT2_EMMC    (1<<2)		/* 0: no eMMC, 1: has eMMC */

#define ALE_GPIO IMX_GPIO_NR(3, 0)
#define CLE_GPIO IMX_GPIO_NR(3, 5)
#define RE_B_GPIO IMX_GPIO_NR(3, 15)
#define WE_B_GPIO IMX_GPIO_NR(3, 17)

#define D0_GPIO	IMX_GPIO_NR(3, 6)
#define D1_GPIO	IMX_GPIO_NR(3, 7)
#define D2_GPIO	IMX_GPIO_NR(3, 8)
#define D3_GPIO	IMX_GPIO_NR(3, 9)
#define D4_GPIO	IMX_GPIO_NR(3, 10)
#define D5_GPIO	IMX_GPIO_NR(3, 11)
#define D6_GPIO	IMX_GPIO_NR(3, 12)
#define D7_GPIO	IMX_GPIO_NR(3, 13)

#define FEATURE_JUMPER_PAD_CTRL	(PAD_CTL_DSE1 | PAD_CTL_PE | PAD_CTL_PUE | PAD_CTL_HYS)

static iomux_v3_cfg_t const feature_jumper_pads[] = {
	IMX8MM_PAD_NAND_DATA00_GPIO3_IO6  | MUX_PAD_CTRL(FEATURE_JUMPER_PAD_CTRL),
	IMX8MM_PAD_NAND_DATA01_GPIO3_IO7  | MUX_PAD_CTRL(FEATURE_JUMPER_PAD_CTRL),
	IMX8MM_PAD_NAND_DATA02_GPIO3_IO8  | MUX_PAD_CTRL(FEATURE_JUMPER_PAD_CTRL),
	IMX8MM_PAD_NAND_DATA03_GPIO3_IO9  | MUX_PAD_CTRL(FEATURE_JUMPER_PAD_CTRL),
	IMX8MM_PAD_NAND_DATA04_GPIO3_IO10 | MUX_PAD_CTRL(FEATURE_JUMPER_PAD_CTRL),
	IMX8MM_PAD_NAND_DATA05_GPIO3_IO11 | MUX_PAD_CTRL(FEATURE_JUMPER_PAD_CTRL),
	IMX8MM_PAD_NAND_DATA06_GPIO3_IO12 | MUX_PAD_CTRL(FEATURE_JUMPER_PAD_CTRL),
	IMX8MM_PAD_NAND_DATA07_GPIO3_IO13 | MUX_PAD_CTRL(FEATURE_JUMPER_PAD_CTRL),
	IMX8MM_PAD_NAND_ALE_GPIO3_IO0     | MUX_PAD_CTRL(FEATURE_JUMPER_PAD_CTRL),
	IMX8MM_PAD_NAND_CLE_GPIO3_IO5     | MUX_PAD_CTRL(FEATURE_JUMPER_PAD_CTRL),
	IMX8MM_PAD_NAND_RE_B_GPIO3_IO15   | MUX_PAD_CTRL(FEATURE_JUMPER_PAD_CTRL),
	IMX8MM_PAD_NAND_WE_B_GPIO3_IO17   | MUX_PAD_CTRL(FEATURE_JUMPER_PAD_CTRL),
};

static void fs_board_init_nboot_args(void)
{
	nbootargs.dwID = FSHWCONFIG_ARGS_ID;
	nbootargs.dwSize = 16*4;
	nbootargs.dwNBOOT_VER = 1;

	if(rom_pointer[1])
		nbootargs.dwMemSize = (PHYS_SDRAM_SIZE - rom_pointer[1]) >> 20;
	else
		nbootargs.dwMemSize = PHYS_SDRAM_SIZE >> 20;

	nbootargs.dwNumDram = CONFIG_NR_DRAM_BANKS;
	nbootargs.dwFlashSize = 512;		/* size of NAND flash in MB */
	nbootargs.dwDbgSerPortPA = CONFIG_MXC_UART_BASE;


	imx_iomux_v3_setup_multiple_pads(
		feature_jumper_pads, ARRAY_SIZE(feature_jumper_pads));

#if defined(FUS_CONFIG_BOARDTYPE) && defined(FUS_CONFIG_BOARDREV) && defined(FUS_CONFIG_FEAT2)
	nbootargs.chBoardType = FUS_CONFIG_BOARDTYPE;
	nbootargs.chBoardRev  = FUS_CONFIG_BOARDREV;
	nbootargs.chFeatures2  = FUS_CONFIG_FEAT2;
#warning "Using fixed config values! This Uboot is not portable!"
#else
	/* get board type */
	gpio_direction_input(D0_GPIO);
	nbootargs.chBoardType |= gpio_get_value(D0_GPIO);
	gpio_direction_input(D6_GPIO);
	nbootargs.chBoardType |= (gpio_get_value(D6_GPIO) << 1);
	gpio_direction_input(D7_GPIO);
	nbootargs.chBoardType |= (gpio_get_value(D7_GPIO) << 2);
	/* get board revision */
	gpio_direction_input(D4_GPIO);
	nbootargs.chBoardRev |= gpio_get_value(D4_GPIO);
	gpio_direction_input(D5_GPIO);
	nbootargs.chBoardRev |= (gpio_get_value(D5_GPIO) << 1);

	switch (nbootargs.chBoardRev)
	{
	case 0:
		nbootargs.chBoardRev = 100;
		break;
	case 1:
		nbootargs.chBoardRev = 110;
		break;
	case 2:
		nbootargs.chBoardRev = 120;
		break;
	case 3:
		nbootargs.chBoardRev = 130;
		break;
	default:
		nbootargs.chBoardRev = 255;
		break;
	}

	/* check for features*/
	/* eMMC */
	gpio_direction_input(D1_GPIO);
	nbootargs.chFeatures2 |= (gpio_get_value(D1_GPIO) << 2);
	/* WLAN */
	gpio_direction_input(D2_GPIO);
	nbootargs.chFeatures2 |= (gpio_get_value(D2_GPIO) << 3);
	/* Sound */
	gpio_direction_input(D3_GPIO);
	nbootargs.chFeatures2 |= (gpio_get_value(D3_GPIO) << 5);
	/* LAN1  */
	gpio_direction_input(ALE_GPIO);
	nbootargs.chFeatures2 |= (gpio_get_value(ALE_GPIO));
	/* LAN2  */
	gpio_direction_input(CLE_GPIO);
	nbootargs.chFeatures2 |= (gpio_get_value(CLE_GPIO) << 1);

	/* LVDS  */
	gpio_direction_input(WE_B_GPIO);
	nbootargs.chFeatures2 |= (gpio_get_value(WE_B_GPIO) << 7);

	/* CPU SPEED  */
	gpio_direction_input(RE_B_GPIO);
	nbootargs.chFeatures2 |= (gpio_get_value(RE_B_GPIO) << 6);
#endif
}


static int spl_dram_init(void)
{
	/* TODO: change RAM detection
	 *  Simple detection: Rev. 1.00 k4f8e304hb_mgcj
	 *                    Rev. 1.10 k4f8e3s4hd_mgcl
	 */
	if(ddr_init(&dram_timing))
	{
		if(ddr_init(&dram_timing_k4f8e304hb_mgcj))
			return 1;
	}

	board_early_init_f();

	return 0;
}

#define USDHC1_CD_GPIO	IMX_GPIO_NR(1, 6)
#define USDHC1_PWR_GPIO IMX_GPIO_NR(2, 10)

#define USDHC3_PWR_GPIO IMX_GPIO_NR(3, 16)

#define USDHC_PAD_CTRL	(PAD_CTL_DSE6 | PAD_CTL_HYS | PAD_CTL_PUE |PAD_CTL_PE | \
                         PAD_CTL_FSEL2)
#define USDHC_GPIO_PAD_CTRL (PAD_CTL_HYS | PAD_CTL_DSE1)


static iomux_v3_cfg_t const usdhc1_pads[] = {
	IMX8MM_PAD_SD1_CLK_USDHC1_CLK | MUX_PAD_CTRL(USDHC_PAD_CTRL),
	IMX8MM_PAD_SD1_CMD_USDHC1_CMD | MUX_PAD_CTRL(USDHC_PAD_CTRL),
	IMX8MM_PAD_SD1_DATA0_USDHC1_DATA0 | MUX_PAD_CTRL(USDHC_PAD_CTRL),
	IMX8MM_PAD_SD1_DATA1_USDHC1_DATA1 | MUX_PAD_CTRL(USDHC_PAD_CTRL),
	IMX8MM_PAD_SD1_DATA2_USDHC1_DATA2 | MUX_PAD_CTRL(USDHC_PAD_CTRL),
	IMX8MM_PAD_SD1_DATA3_USDHC1_DATA3 | MUX_PAD_CTRL(USDHC_PAD_CTRL),
	IMX8MM_PAD_SD1_RESET_B_GPIO2_IO10 | MUX_PAD_CTRL(USDHC_GPIO_PAD_CTRL),
};

#if defined(CONFIG_SD_BOOT) && !defined(CONFIG_NAND_BOOT)
/*  */
static iomux_v3_cfg_t const usdhc3_pads[] = {
	IMX8MM_PAD_NAND_WE_B_USDHC3_CLK | MUX_PAD_CTRL(USDHC_PAD_CTRL),
	IMX8MM_PAD_NAND_WP_B_USDHC3_CMD | MUX_PAD_CTRL(USDHC_PAD_CTRL),
	IMX8MM_PAD_NAND_DATA04_USDHC3_DATA0 | MUX_PAD_CTRL(USDHC_PAD_CTRL),
	IMX8MM_PAD_NAND_DATA05_USDHC3_DATA1| MUX_PAD_CTRL(USDHC_PAD_CTRL),
	IMX8MM_PAD_NAND_DATA06_USDHC3_DATA2 | MUX_PAD_CTRL(USDHC_PAD_CTRL),
	IMX8MM_PAD_NAND_DATA07_USDHC3_DATA3 | MUX_PAD_CTRL(USDHC_PAD_CTRL),
	IMX8MM_PAD_NAND_RE_B_USDHC3_DATA4   | MUX_PAD_CTRL(USDHC_PAD_CTRL),
	IMX8MM_PAD_NAND_CE2_B_USDHC3_DATA5  | MUX_PAD_CTRL(USDHC_PAD_CTRL),
	IMX8MM_PAD_NAND_CE3_B_USDHC3_DATA6  | MUX_PAD_CTRL(USDHC_PAD_CTRL),
	IMX8MM_PAD_NAND_CLE_USDHC3_DATA7    | MUX_PAD_CTRL(USDHC_PAD_CTRL),
	IMX8MM_PAD_NAND_CE1_B_GPIO3_IO2     | MUX_PAD_CTRL(USDHC_GPIO_PAD_CTRL),
	/* IMX8MM_PAD_NAND_CE1_B_USDHC3_STROBE | MUX_PAD_CTRL(USDHC_PAD_CTRL), */
	/* IMX8MM_PAD_NAND_READY_B_USDHC3_RESET_B */
	IMX8MM_PAD_NAND_READY_B_GPIO3_IO16 | MUX_PAD_CTRL(USDHC_GPIO_PAD_CTRL),
};
#endif

static iomux_v3_cfg_t const cd_gpio_1[] = {
	IMX8MM_PAD_GPIO1_IO06_USDHC1_CD_B | MUX_PAD_CTRL(USDHC_GPIO_PAD_CTRL),
};

/* Extended SDHC configuration. Pad count is without data signals, the data
   signal count will be added automatically according to bus_width. */
struct fus_sdhc_cfg {
	const iomux_v3_cfg_t *const pads;
	const u8 count;
	const u8 index;
	u16 cd_gpio;
	struct fsl_esdhc_cfg esdhc;
};

enum usdhc_pads {
	usdhc1_ext, usdhc3_int
};

static struct fs_mmc_cfg sdhc_cfg[] = {
	/* pads,                    count, USDHC# */
	[usdhc1_ext] = { usdhc1_pads, 1,     1 },
#if defined(CONFIG_SD_BOOT) && !defined(CONFIG_NAND_BOOT)
	[usdhc3_int] = { usdhc3_pads, 4,     3 },
#endif
};

enum usdhc_cds {
	gpio1_io06
};

static const struct fs_mmc_cd sdhc_cd[] = {
	/* pad,          gpio */
	[gpio1_io06] = { cd_gpio_1,    IMX_GPIO_NR(1, 6) },
};

int board_mmc_init(bd_t *bd)
{
	int ret = 0;

	/* mmc0: USDHC1 (ext. micro SD slot on PicoCoreBBDSI), CD: GPIO1_IO06 */
	ret = fs_mmc_setup(bd, 4, &sdhc_cfg[usdhc1_ext],
			   &sdhc_cd[gpio1_io06]);

#if defined(CONFIG_SD_BOOT) && !defined(CONFIG_NAND_BOOT)	
	/* mmc1: USDHC3 (eMMC, if available), no CD */
	ret = fs_mmc_setup(bd, 8, &sdhc_cfg[usdhc3_int], NULL);

	gpio_request(USDHC3_PWR_GPIO, "usdhc3_reset");
	gpio_direction_output(USDHC3_PWR_GPIO, 0);
	udelay(500);
	gpio_direction_output(USDHC3_PWR_GPIO, 1);

	
#endif
	return ret;
}

/* BL_ON */
#define BL_ON_PAD IMX_GPIO_NR(5, 3)
static iomux_v3_cfg_t const bl_on_pad =
	IMX8MM_PAD_SPDIF_TX_GPIO5_IO3 | MUX_PAD_CTRL(NO_PAD_CTRL);

void spl_board_init(void)
{

	imx_iomux_v3_setup_pad(bl_on_pad);
	/* backlight off*/
	gpio_request(BL_ON_PAD, "BL_ON");
	gpio_direction_output(BL_ON_PAD, 0);


#ifndef CONFIG_SPL_USB_SDP_SUPPORT
	/* Serial download mode */
	if (is_usb_boot()) {
		puts("Back to ROM, SDP\n");
		restore_boot_params();
	}
#endif
	puts("Normal Boot\n");
}

#ifdef CONFIG_SPL_LOAD_FIT
int board_fit_config_name_match(const char *name)
{
	/* Just empty function now - can't decide what to choose */
	debug("%s: %s\n", __func__, name);

	return 0;
}
#endif

void board_init_f(ulong dummy)
{
	int ret;
	struct fs_nboot_args *pargs = (struct fs_nboot_args*)(CONFIG_SYS_SDRAM_BASE + 0x00001000);

	/* Clear the BSS. */
	memset(__bss_start, 0, __bss_end - __bss_start);

	arch_cpu_init();

	board_early_init_f();

	timer_init();

	preloader_console_init();

	ret = spl_init();
	if (ret) {
		debug("spl_init() failed: %d\n", ret);
		hang();
	}

	enable_tzc380();

	fs_board_init_nboot_args();

	/* DDR initialization */
	if(spl_dram_init())
	{
		printf("This UBoot can't be started. RAM initialization fails.\n");
		while(1);
	}

	printf("DDRInfo: RAM initialization success.\n");

	/* fill nboot args first after ram initialisation
	 */

	pargs = fs_board_get_nboot_args();

	pargs->dwID = nbootargs.dwID;
	pargs->dwSize = nbootargs.dwSize;
	pargs->dwNBOOT_VER = nbootargs.dwNBOOT_VER;
	pargs->dwMemSize = nbootargs.dwMemSize;
	pargs->dwNumDram = nbootargs.dwNumDram;
	pargs->dwFlashSize = nbootargs.dwFlashSize;
	pargs->dwDbgSerPortPA = nbootargs.dwDbgSerPortPA;
	pargs->chBoardRev = nbootargs.chBoardRev;
	pargs->chBoardType = nbootargs.chBoardType;
	pargs->chFeatures2 = nbootargs.chFeatures2;

	board_init_r(NULL, 0);
}
