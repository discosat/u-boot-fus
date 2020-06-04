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
#include <asm/arch/imx8mn_pins.h>
#include <asm/arch/sys_proto.h>
#include <asm/arch/clock.h>
#include <asm/mach-imx/gpio.h>
#include <asm/mach-imx/mxc_i2c.h>
#include <fsl_esdhc.h>
#include <mmc.h>
#include <asm/arch/imx8m_ddr.h>

#include "../common/fs_board_common.h"	/* fs_board_*() */

DECLARE_GLOBAL_DATA_PTR;

static struct fs_nboot_args nbootargs;

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
	IMX8MN_PAD_NAND_DATA00__GPIO3_IO6  | MUX_PAD_CTRL(FEATURE_JUMPER_PAD_CTRL),
	IMX8MN_PAD_NAND_DATA01__GPIO3_IO7  | MUX_PAD_CTRL(FEATURE_JUMPER_PAD_CTRL),
	IMX8MN_PAD_NAND_DATA02__GPIO3_IO8  | MUX_PAD_CTRL(FEATURE_JUMPER_PAD_CTRL),
	IMX8MN_PAD_NAND_DATA03__GPIO3_IO9  | MUX_PAD_CTRL(FEATURE_JUMPER_PAD_CTRL),
	IMX8MN_PAD_NAND_DATA04__GPIO3_IO10 | MUX_PAD_CTRL(FEATURE_JUMPER_PAD_CTRL),
	IMX8MN_PAD_NAND_DATA05__GPIO3_IO11 | MUX_PAD_CTRL(FEATURE_JUMPER_PAD_CTRL),
	IMX8MN_PAD_NAND_DATA06__GPIO3_IO12 | MUX_PAD_CTRL(FEATURE_JUMPER_PAD_CTRL),
	IMX8MN_PAD_NAND_DATA07__GPIO3_IO13 | MUX_PAD_CTRL(FEATURE_JUMPER_PAD_CTRL),
	IMX8MN_PAD_NAND_ALE__GPIO3_IO0     | MUX_PAD_CTRL(FEATURE_JUMPER_PAD_CTRL),
	IMX8MN_PAD_NAND_CLE__GPIO3_IO5     | MUX_PAD_CTRL(FEATURE_JUMPER_PAD_CTRL),
	IMX8MN_PAD_NAND_RE_B__GPIO3_IO15   | MUX_PAD_CTRL(FEATURE_JUMPER_PAD_CTRL),
	IMX8MN_PAD_NAND_WE_B__GPIO3_IO17   | MUX_PAD_CTRL(FEATURE_JUMPER_PAD_CTRL),
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
}


static int spl_dram_init(void)
{
	ddr_init(&dram_timing);

	board_early_init_f();

	return 0;
}

#define USDHC1_CD_GPIO	IMX_GPIO_NR(1, 6)
#define USDHC1_PWR_GPIO IMX_GPIO_NR(2, 10)

#define USDHC2_CD_GPIO	IMX_GPIO_NR(2, 12)
#define USDHC2_PWR_GPIO IMX_GPIO_NR(2, 19)

#define USDHC_PAD_CTRL	(PAD_CTL_DSE6 | PAD_CTL_HYS | PAD_CTL_PUE |PAD_CTL_PE | \
			 PAD_CTL_FSEL2)
#define USDHC_GPIO_PAD_CTRL (PAD_CTL_HYS | PAD_CTL_DSE1)


static iomux_v3_cfg_t const usdhc1_pads[] = {
	IMX8MN_PAD_SD1_CLK__USDHC1_CLK | MUX_PAD_CTRL(USDHC_PAD_CTRL),
	IMX8MN_PAD_SD1_CMD__USDHC1_CMD | MUX_PAD_CTRL(USDHC_PAD_CTRL),
	IMX8MN_PAD_SD1_DATA0__USDHC1_DATA0 | MUX_PAD_CTRL(USDHC_PAD_CTRL),
	IMX8MN_PAD_SD1_DATA1__USDHC1_DATA1 | MUX_PAD_CTRL(USDHC_PAD_CTRL),
	IMX8MN_PAD_SD1_DATA2__USDHC1_DATA2 | MUX_PAD_CTRL(USDHC_PAD_CTRL),
	IMX8MN_PAD_SD1_DATA3__USDHC1_DATA3 | MUX_PAD_CTRL(USDHC_PAD_CTRL),
	IMX8MN_PAD_SD1_RESET_B__GPIO2_IO10 | MUX_PAD_CTRL(USDHC_GPIO_PAD_CTRL),
};

static iomux_v3_cfg_t const usdhc2_pads[] = {
	IMX8MN_PAD_SD2_CLK__USDHC2_CLK | MUX_PAD_CTRL(USDHC_PAD_CTRL),
	IMX8MN_PAD_SD2_CMD__USDHC2_CMD | MUX_PAD_CTRL(USDHC_PAD_CTRL),
	IMX8MN_PAD_SD2_DATA0__USDHC2_DATA0 | MUX_PAD_CTRL(USDHC_PAD_CTRL),
	IMX8MN_PAD_SD2_DATA1__USDHC2_DATA1 | MUX_PAD_CTRL(USDHC_PAD_CTRL),
	IMX8MN_PAD_SD2_DATA2__USDHC2_DATA2 | MUX_PAD_CTRL(USDHC_PAD_CTRL),
	IMX8MN_PAD_SD2_DATA3__USDHC2_DATA3 | MUX_PAD_CTRL(USDHC_PAD_CTRL),
	IMX8MN_PAD_SD2_RESET_B__GPIO2_IO19 | MUX_PAD_CTRL(USDHC_GPIO_PAD_CTRL),
};

/*
 * The evk board uses DAT3 to detect CD card plugin,
 * in u-boot we mux the pin to GPIO when doing board_mmc_getcd.
 */

static iomux_v3_cfg_t const usdhc1_cd_gpio_pad =
	IMX8MN_PAD_GPIO1_IO06__USDHC1_CD_B | MUX_PAD_CTRL(USDHC_GPIO_PAD_CTRL);

static iomux_v3_cfg_t const usdhc1_cd_b_pad =
	IMX8MN_PAD_GPIO1_IO06__USDHC1_CD_B |
	MUX_PAD_CTRL(USDHC_PAD_CTRL);

static iomux_v3_cfg_t const usdhc2_cd_gpio_pad =
	IMX8MN_PAD_SD2_CD_B__GPIO2_IO12 | MUX_PAD_CTRL(USDHC_GPIO_PAD_CTRL);

static iomux_v3_cfg_t const usdhc2_cd_b_pad =
	IMX8MN_PAD_SD2_CD_B__USDHC2_CD_B |
	MUX_PAD_CTRL(USDHC_PAD_CTRL);


static struct fsl_esdhc_cfg usdhc_cfg[2] = {
	{USDHC1_BASE_ADDR, 0, 1},
	{USDHC2_BASE_ADDR, 0, 1},
};

int board_mmc_init(bd_t *bis)
{
	int i, ret;
	/*
	 * According to the board_mmc_init() the following map is done:
	 * (U-Boot device node)    (Physical Port)
	 * mmc0                    USDHC1
	 * mmc1                    USDHC2
	 */
	for (i = 0; i < CONFIG_SYS_FSL_USDHC_NUM; i++) {
		switch (i) {
		case 0:
			usdhc_cfg[0].sdhc_clk = mxc_get_clock(MXC_ESDHC_CLK);
			imx_iomux_v3_setup_multiple_pads(
				usdhc1_pads, ARRAY_SIZE(usdhc1_pads));
			gpio_request(USDHC1_PWR_GPIO, "usdhc1_reset");
			gpio_direction_output(USDHC1_PWR_GPIO, 0);
			udelay(500);
			gpio_direction_output(USDHC1_PWR_GPIO, 1);
			break;
		case 1:
			usdhc_cfg[1].sdhc_clk = mxc_get_clock(MXC_ESDHC2_CLK);
			imx_iomux_v3_setup_multiple_pads(
				usdhc2_pads, ARRAY_SIZE(usdhc2_pads));
			gpio_request(USDHC2_PWR_GPIO, "usdhc2_reset");
			gpio_direction_output(USDHC2_PWR_GPIO, 0);
			udelay(500);
			gpio_direction_output(USDHC2_PWR_GPIO, 1);
			break;
		default:
			printf("Warning: you configured more USDHC controllers"
				"(%d) than supported by the board\n", i + 1);
			return -EINVAL;
		}

		ret = fsl_esdhc_initialize(bis, &usdhc_cfg[i]);
		if (ret)
			return ret;
	}

	return 0;
}

int board_mmc_getcd(struct mmc *mmc)
{
	struct fsl_esdhc_cfg *cfg = (struct fsl_esdhc_cfg *)mmc->priv;
	int ret = 0;

	switch (cfg->esdhc_base) {
	case USDHC1_BASE_ADDR:
		imx_iomux_v3_setup_pad(usdhc1_cd_gpio_pad);
		gpio_request(USDHC1_CD_GPIO, "usdhc1 cd");
		gpio_direction_input(USDHC1_CD_GPIO);

		/*
		 * Since it is the DAT3 pin, this pin is pulled to
		 * low voltage if no card
		 */
		ret = gpio_get_value(USDHC1_CD_GPIO);
        
	        imx_iomux_v3_setup_pad(usdhc1_cd_b_pad);

		return !ret;
		break;
	case USDHC2_BASE_ADDR:
		imx_iomux_v3_setup_pad(usdhc2_cd_gpio_pad);
		gpio_request(USDHC2_CD_GPIO, "usdhc2 cd");
		gpio_direction_input(USDHC2_CD_GPIO);

		/*
		 * Since it is the DAT3 pin, this pin is pulled to
		 * low voltage if no card
		 */
		ret = gpio_get_value(USDHC2_CD_GPIO);

		imx_iomux_v3_setup_pad(usdhc2_cd_b_pad);
		return ret;
	}

	return 1;
}

/* BL_ON */
#define BL_ON_PAD IMX_GPIO_NR(5, 3)
static iomux_v3_cfg_t const bl_on_pad =
	IMX8MN_PAD_SPDIF_TX__GPIO5_IO3 | MUX_PAD_CTRL(NO_PAD_CTRL);

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
