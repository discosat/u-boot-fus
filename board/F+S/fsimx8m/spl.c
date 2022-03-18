/*
 * Copyright 2017 NXP
 * (C) Copyright 2018-2019
 *
 * Hartmut Keller, F&S Elektronik Systeme GmbH, keller@fs-net.de
 * Anatol Derksen, F&S Elektronik Systeme GmbH, derksen@fs-net.de
 *
 * Board specific functions for F&S boards based on Freescale i.MX8M CPU
 *
 * SPDX-License-Identifier:	GPL-2.0+
 */

#include <common.h>
#include <spl.h>
#include <asm/io.h>
#include <errno.h>
#include <asm/io.h>
#include <asm/mach-imx/iomux-v3.h>
#include <asm/arch/ddr.h>
#include <asm/arch/imx8mq_pins.h>
#include <asm/arch/sys_proto.h>
#include "../../freescale/common/pfuze.h"
#include <asm/arch/clock.h>
#include <asm/mach-imx/gpio.h>
#include <asm/mach-imx/mxc_i2c.h>
#include <fsl_esdhc.h>
#include <mmc.h>
#ifdef CONFIG_IMX8M_LPDDR4
#include <asm/arch/ddr.h>
#else
#include "ddr/ddr.h"
#endif
#include "../common/fs_board_common.h"	/* fs_board_*() */

DECLARE_GLOBAL_DATA_PTR;

extern struct dram_timing_info dram_timing_k4f8e3s4hd_mgcl;

static struct fs_nboot_args nbootargs;

#define ALE_GPIO IMX_GPIO_NR(3, 0)
#define CLE_GPIO IMX_GPIO_NR(3, 5)

#define D0_GPIO	IMX_GPIO_NR(3, 6)
#define D1_GPIO	IMX_GPIO_NR(3, 7)
#define D2_GPIO	IMX_GPIO_NR(3, 8)

#define FEATURE_JUMPER_PAD_CTRL	(PAD_CTL_DSE1 | PAD_CTL_PUE | PAD_CTL_HYS)

static iomux_v3_cfg_t const feature_jumper_pads[] = {
	IMX8MQ_PAD_NAND_DATA00__GPIO3_IO6  | MUX_PAD_CTRL(FEATURE_JUMPER_PAD_CTRL),
	IMX8MQ_PAD_NAND_DATA01__GPIO3_IO7  | MUX_PAD_CTRL(FEATURE_JUMPER_PAD_CTRL),
	IMX8MQ_PAD_NAND_DATA02__GPIO3_IO8  | MUX_PAD_CTRL(FEATURE_JUMPER_PAD_CTRL),
	IMX8MQ_PAD_NAND_ALE__GPIO3_IO0     | MUX_PAD_CTRL(FEATURE_JUMPER_PAD_CTRL),
	IMX8MQ_PAD_NAND_CLE__GPIO3_IO5     | MUX_PAD_CTRL(FEATURE_JUMPER_PAD_CTRL),
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
	nbootargs.dwFlashSize = 256;		/* size of NAND flash in MB */
	nbootargs.dwDbgSerPortPA = CONFIG_MXC_UART_BASE;


	imx_iomux_v3_setup_multiple_pads(
				feature_jumper_pads, ARRAY_SIZE(feature_jumper_pads));

	/* get board type */
	gpio_direction_input(D0_GPIO);
        nbootargs.chBoardType |= gpio_get_value(D0_GPIO);
	gpio_direction_input(D1_GPIO);
        nbootargs.chBoardType |= (gpio_get_value(D1_GPIO) << 1);
	gpio_direction_input(D2_GPIO);
        nbootargs.chBoardType |= (gpio_get_value(D1_GPIO) << 2);
	/* get board revision */
	gpio_direction_input(ALE_GPIO);
        nbootargs.chBoardRev |= gpio_get_value(ALE_GPIO);
	gpio_direction_input(CLE_GPIO);
        nbootargs.chBoardRev |= (gpio_get_value(CLE_GPIO) << 1);

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
}

static int spl_dram_init(void)
{
	int ret = 0;
	/* ddr init */
#ifdef CONFIG_IMX8M_LPDDR4
	ret = ddr_init(&dram_timing_k4f8e3s4hd_mgcl);
	if(ret)
	{
		/* try new timing array */
		ret = ddr_init(&dram_timing);
		if(ret)
			return ret;
	}
#else
	ddr_init();
#endif

	board_early_init_f();

	return 0;
}

#define USDHC2_CD_GPIO	IMX_GPIO_NR(2, 12)
#define USDHC1_PWR_GPIO IMX_GPIO_NR(2, 10)
//#define USDHC2_PWR_GPIO IMX_GPIO_NR(2, 19)

int board_mmc_getcd(struct mmc *mmc)
{
	struct fsl_esdhc_cfg *cfg = (struct fsl_esdhc_cfg *)mmc->priv;
	int ret = 0;

	switch (cfg->esdhc_base) {
	case USDHC1_BASE_ADDR: /* eMMC always available */
		ret = 1;
		break;
	case USDHC2_BASE_ADDR: /* Micro SD -> state of CD pin */
		ret = !gpio_get_value(USDHC2_CD_GPIO);
		return ret;
	}

	return 1;
}

#define USDHC_PAD_CTRL	(PAD_CTL_DSE6 | PAD_CTL_HYS | PAD_CTL_PUE | \
			 PAD_CTL_FSEL2)
#define USDHC_GPIO_PAD_CTRL (PAD_CTL_PUE | PAD_CTL_DSE1)

static iomux_v3_cfg_t const usdhc1_pads[] = {
	IMX8MQ_PAD_SD1_CLK__USDHC1_CLK | MUX_PAD_CTRL(USDHC_PAD_CTRL),
	IMX8MQ_PAD_SD1_CMD__USDHC1_CMD | MUX_PAD_CTRL(USDHC_PAD_CTRL),
	IMX8MQ_PAD_SD1_DATA0__USDHC1_DATA0 | MUX_PAD_CTRL(USDHC_PAD_CTRL),
	IMX8MQ_PAD_SD1_DATA1__USDHC1_DATA1 | MUX_PAD_CTRL(USDHC_PAD_CTRL),
	IMX8MQ_PAD_SD1_DATA2__USDHC1_DATA2 | MUX_PAD_CTRL(USDHC_PAD_CTRL),
	IMX8MQ_PAD_SD1_DATA3__USDHC1_DATA3 | MUX_PAD_CTRL(USDHC_PAD_CTRL),
	IMX8MQ_PAD_SD1_DATA4__USDHC1_DATA4 | MUX_PAD_CTRL(USDHC_PAD_CTRL),
	IMX8MQ_PAD_SD1_DATA5__USDHC1_DATA5 | MUX_PAD_CTRL(USDHC_PAD_CTRL),
	IMX8MQ_PAD_SD1_DATA6__USDHC1_DATA6 | MUX_PAD_CTRL(USDHC_PAD_CTRL),
	IMX8MQ_PAD_SD1_DATA7__USDHC1_DATA7 | MUX_PAD_CTRL(USDHC_PAD_CTRL),
	IMX8MQ_PAD_SD1_RESET_B__GPIO2_IO10 | MUX_PAD_CTRL(NO_PAD_CTRL),
	IMX8MQ_PAD_SD1_STROBE__GPIO2_IO11 | MUX_PAD_CTRL(NO_PAD_CTRL),
};

static iomux_v3_cfg_t const usdhc2_pads[] = {
	IMX8MQ_PAD_SD2_CLK__USDHC2_CLK | MUX_PAD_CTRL(USDHC_PAD_CTRL), /* 0xd6 */
	IMX8MQ_PAD_SD2_CMD__USDHC2_CMD | MUX_PAD_CTRL(USDHC_PAD_CTRL), /* 0xd6 */
	IMX8MQ_PAD_SD2_DATA0__USDHC2_DATA0 | MUX_PAD_CTRL(USDHC_PAD_CTRL), /* 0xd6 */
	IMX8MQ_PAD_SD2_DATA1__USDHC2_DATA1 | MUX_PAD_CTRL(USDHC_PAD_CTRL), /* 0xd6 */
	IMX8MQ_PAD_SD2_DATA2__USDHC2_DATA2 | MUX_PAD_CTRL(USDHC_PAD_CTRL), /* 0x16 */
	IMX8MQ_PAD_SD2_DATA3__USDHC2_DATA3 | MUX_PAD_CTRL(USDHC_PAD_CTRL), /* 0xd6 */
	IMX8MQ_PAD_SD2_CD_B__GPIO2_IO12 | MUX_PAD_CTRL(USDHC_GPIO_PAD_CTRL),
};

static struct fsl_esdhc_cfg usdhc_cfg[2] = {
	{USDHC1_BASE_ADDR, 0, 8},
	{USDHC2_BASE_ADDR, 0, 4},
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
			usdhc_cfg[0].sdhc_clk = mxc_get_clock(USDHC1_CLK_ROOT);
			imx_iomux_v3_setup_multiple_pads(
				usdhc1_pads, ARRAY_SIZE(usdhc1_pads));
			gpio_request(USDHC1_PWR_GPIO, "usdhc1_reset");
			gpio_direction_output(USDHC1_PWR_GPIO, 0);
			udelay(500);
			gpio_direction_output(USDHC1_PWR_GPIO, 1);
			break;
		case 1:
			usdhc_cfg[1].sdhc_clk = mxc_get_clock(USDHC2_CLK_ROOT);
			imx_iomux_v3_setup_multiple_pads(
				usdhc2_pads, ARRAY_SIZE(usdhc2_pads));
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

#ifdef CONFIG_POWER
int power_init_board(void)
{
	return 0;
}
#endif

void spl_board_init(void)
{
#ifndef CONFIG_SPL_USB_SDP_SUPPORT
	/* Serial download mode */
	if (is_usb_boot()) {
		puts("Back to ROM, SDP\n");
		restore_boot_params();
	}
#endif

	init_usb_clk();

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

	init_uart_clk(1); /* init UART1 clock */

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

	/* initalize ram area with zero before set */
	memset(pargs, 0x0, sizeof(struct fs_nboot_args));
	/* fill nboot args first after ram initialization */
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
	pargs->chFeatures2 = 0;

	board_init_r(NULL, 0);
}
