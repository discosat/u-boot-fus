/*
 * Copyright 2018 NXP
 * (C) Copyright 2018-2021
 *
 * Hartmut Keller, F&S Elektronik Systeme GmbH, keller@fs-net.de
 * Patrick Jakob, F&S Elektronik Systeme GmbH, jakob@fs-net.de
 * Anatol Derksen, F&S Elektronik Systeme GmbH, derksen@fs-net.de
 * Philipp Gerbach, F&S Elektronik Systeme GmbH, gerbach@fs-net.de
 *
 * Board specific functions for F&S boards based on Freescale i.MX8MP CPU
 *
 * SPDX-License-Identifier:	GPL-2.0+
 */

#include <common.h>
#include <cpu_func.h>
#include <hang.h>
#include <spl.h>
#include <asm/io.h>
#include <errno.h>
#include <asm/io.h>
#include <asm/mach-imx/iomux-v3.h>
#include <asm/arch/imx8mp_pins.h>
#include <asm/arch/sys_proto.h>
#include <asm/mach-imx/boot_mode.h>
#include <power/pmic.h>

#include <power/pca9450.h>
#include <asm/arch/clock.h>
#include <asm/mach-imx/gpio.h>
#include <asm/mach-imx/mxc_i2c.h>
#include <fsl_esdhc_imx.h>
#include <mmc.h>
#include <asm/arch/ddr.h>

#include "../common/fs_board_common.h"	/* fs_board_*() */
//#include "../common/fs_mmc_common.h"	/* struct fs_mmc_cd, fs_mmc_*(), ... */

DECLARE_GLOBAL_DATA_PTR;

extern struct dram_timing_info dram_timing_k4f6e3s4hm_mgcj;

static struct fs_nboot_args nbootargs;

#define BT_PICOCOREMX8MP 0x0

#define UART_PAD_CTRL	(PAD_CTL_DSE6 | PAD_CTL_FSEL1)

#define I2C_PAD_CTRL	(PAD_CTL_DSE6 | PAD_CTL_HYS | PAD_CTL_PUE | PAD_CTL_PE)
#define PC MUX_PAD_CTRL(I2C_PAD_CTRL)
struct i2c_pads_info i2c_pad_info_8mp = {
	.scl = {
		.i2c_mode = MX8MP_PAD_SAI5_RXD0__I2C5_SCL | PC,
		.gpio_mode = MX8MP_PAD_SAI5_RXD0__GPIO3_IO21 | PC,
		.gp = IMX_GPIO_NR(3, 21),
	},
	.sda = {
		.i2c_mode = MX8MP_PAD_SAI5_MCLK__I2C5_SDA | PC,
		.gpio_mode = MX8MP_PAD_SAI5_MCLK__GPIO3_IO25 | PC,
		.gp = IMX_GPIO_NR(3, 25),
	},
};

int spl_board_boot_device(enum boot_device boot_dev_spl)
{
#ifdef CONFIG_SPL_BOOTROM_SUPPORT
	return BOOT_DEVICE_BOOTROM;
#else
	switch (boot_dev_spl) {
	case SD1_BOOT:
	case MMC1_BOOT:
	case SD2_BOOT:
	case MMC2_BOOT:
		return BOOT_DEVICE_MMC1;
	case SD3_BOOT:
	case MMC3_BOOT:
		return BOOT_DEVICE_MMC2;
	case QSPI_BOOT:
		return BOOT_DEVICE_NOR;
	case NAND_BOOT:
		return BOOT_DEVICE_NAND;
	case USB_BOOT:
		return BOOT_DEVICE_BOARD;
	default:
		return BOOT_DEVICE_NONE;
	}
#endif
}

static iomux_v3_cfg_t const uart_pads_mp[] = {
	MX8MP_PAD_SAI3_TXFS__UART2_DCE_RX | MUX_PAD_CTRL(UART_PAD_CTRL),
	MX8MP_PAD_SAI3_TXC__UART2_DCE_TX | MUX_PAD_CTRL(UART_PAD_CTRL),
};

ulong board_serial_base(void)
{
	switch (nbootargs.chBoardType)
	{
	case BT_PICOCOREMX8MP:
		return UART2_BASE_ADDR;
	default:
		break;
	}

	return UART2_BASE_ADDR;
}


static void config_uart_pads(void)
{
	switch (nbootargs.chBoardType)
	{
	default:
	case BT_PICOCOREMX8MP:
		/* Setup UART pads */
		imx_iomux_v3_setup_multiple_pads(uart_pads_mp, ARRAY_SIZE(uart_pads_mp));
		/* enable uart clock */
		init_uart_clk(1);
		break;
	}
}

#define USDHC1_CD_GPIO	IMX_GPIO_NR(1, 6)
#define USDHC1_PWR_GPIO IMX_GPIO_NR(2, 10)

#define USDHC_PAD_CTRL	(PAD_CTL_DSE6 | PAD_CTL_HYS | PAD_CTL_PUE |PAD_CTL_PE | \
			 PAD_CTL_FSEL2)
#define USDHC_GPIO_PAD_CTRL (PAD_CTL_HYS | PAD_CTL_DSE1)
#define USDHC_CD_PAD_CTRL (PAD_CTL_PE |PAD_CTL_PUE |PAD_CTL_HYS | PAD_CTL_DSE4)


static iomux_v3_cfg_t const usdhc3_pads[] = {
	MX8MP_PAD_NAND_WE_B__USDHC3_CLK | MUX_PAD_CTRL(USDHC_PAD_CTRL),
	MX8MP_PAD_NAND_WP_B__USDHC3_CMD | MUX_PAD_CTRL(USDHC_PAD_CTRL),
	MX8MP_PAD_NAND_DATA04__USDHC3_DATA0 | MUX_PAD_CTRL(USDHC_PAD_CTRL),
	MX8MP_PAD_NAND_DATA05__USDHC3_DATA1 | MUX_PAD_CTRL(USDHC_PAD_CTRL),
	MX8MP_PAD_NAND_DATA06__USDHC3_DATA2 | MUX_PAD_CTRL(USDHC_PAD_CTRL),
	MX8MP_PAD_NAND_DATA07__USDHC3_DATA3 | MUX_PAD_CTRL(USDHC_PAD_CTRL),
	MX8MP_PAD_NAND_RE_B__USDHC3_DATA4 | MUX_PAD_CTRL(USDHC_PAD_CTRL),
	MX8MP_PAD_NAND_CE2_B__USDHC3_DATA5 | MUX_PAD_CTRL(USDHC_PAD_CTRL),
	MX8MP_PAD_NAND_CE3_B__USDHC3_DATA6 | MUX_PAD_CTRL(USDHC_PAD_CTRL),
	MX8MP_PAD_NAND_CLE__USDHC3_DATA7 | MUX_PAD_CTRL(USDHC_PAD_CTRL),
};

static iomux_v3_cfg_t const usdhc1_pads[] = {
	MX8MP_PAD_SD1_CLK__USDHC1_CLK | MUX_PAD_CTRL(USDHC_PAD_CTRL),
	MX8MP_PAD_SD1_CMD__USDHC1_CMD | MUX_PAD_CTRL(USDHC_PAD_CTRL),
	MX8MP_PAD_SD1_DATA0__USDHC1_DATA0 | MUX_PAD_CTRL(USDHC_PAD_CTRL),
	MX8MP_PAD_SD1_DATA1__USDHC1_DATA1 | MUX_PAD_CTRL(USDHC_PAD_CTRL),
	MX8MP_PAD_SD1_DATA2__USDHC1_DATA2 | MUX_PAD_CTRL(USDHC_PAD_CTRL),
	MX8MP_PAD_SD1_DATA3__USDHC1_DATA3 | MUX_PAD_CTRL(USDHC_PAD_CTRL),
	MX8MP_PAD_SD1_RESET_B__GPIO2_IO10 | MUX_PAD_CTRL(USDHC_GPIO_PAD_CTRL),
	MX8MP_PAD_GPIO1_IO06__USDHC1_CD_B    | MUX_PAD_CTRL(USDHC_CD_PAD_CTRL),
};

static struct fsl_esdhc_cfg usdhc_cfg[2] = {
	{USDHC1_BASE_ADDR, 0, 4},
	{USDHC3_BASE_ADDR, 0, 8},
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
			init_clk_usdhc(1);
			usdhc_cfg[0].sdhc_clk = mxc_get_clock(MXC_ESDHC_CLK);
			imx_iomux_v3_setup_multiple_pads(
				usdhc1_pads, ARRAY_SIZE(usdhc1_pads));
			gpio_request(USDHC1_PWR_GPIO, "usdhc1_reset");
			gpio_direction_output(USDHC1_PWR_GPIO, 0);
			udelay(500);
			gpio_direction_output(USDHC1_PWR_GPIO, 1);
			gpio_request(USDHC1_CD_GPIO, "usdhc1 cd");
			gpio_direction_input(USDHC1_CD_GPIO);
			break;
		case 1:
			init_clk_usdhc(2);
			usdhc_cfg[1].sdhc_clk = mxc_get_clock(MXC_ESDHC3_CLK);
			imx_iomux_v3_setup_multiple_pads(
				usdhc3_pads, ARRAY_SIZE(usdhc3_pads));
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
	case USDHC3_BASE_ADDR:
		ret = 1;
		break;
	case USDHC1_BASE_ADDR:
		ret = !gpio_get_value(USDHC1_CD_GPIO);
		return ret;
	}

	return 1;
}

static void fs_board_init_nboot_args(void)
{
	int dram_size = 0;

	nbootargs.dwID = FSHWCONFIG_ARGS_ID;
	nbootargs.dwSize = 16*4;
	nbootargs.dwNBOOT_VER = 1;

#if defined(FUS_CONFIG_BOARDTYPE) && defined(FUS_CONFIG_BOARDREV) && defined(FUS_CONFIG_FEAT2)
	nbootargs.chBoardType = FUS_CONFIG_BOARDTYPE;
	nbootargs.chBoardRev  = FUS_CONFIG_BOARDREV;
	nbootargs.chFeatures2  = FUS_CONFIG_FEAT2;
#define STRING2(x) #x
#define STRING(x) STRING2(x)
#pragma message "FUS_CONFIG_BOARDTYPE = " STRING(FUS_CONFIG_BOARDTYPE)
#pragma message "FUS_CONFIG_BOARDREV = " STRING(FUS_CONFIG_BOARDREV)
#pragma message "FUS_CONFIG_FEAT2 = " STRING(FUS_CONFIG_FEAT2)
#warning "Using fixed config values! This Uboot is not portable!"
#else
#error "Board Config not set! \
Please set CONFIG_FUS_BOARDTYPE, CONFIG_FUS_BOARDREV and CONFIG_FUS_FEATURES2 according to the documentation"
#endif

	switch (nbootargs.chBoardType)
	{
	case BT_PICOCOREMX8MP:
		/* size of NAND flash in MB */
		nbootargs.dwFlashSize = 0;
		/* HOTFIX: Set ram size hard to 2GB defined in board header */
		dram_size = PHYS_SDRAM_SIZE;
		/* HOTFIX: Set number of DRAMs hard to 1 */
		nbootargs.dwNumDram = 1;
		break;
	}

	nbootargs.dwDbgSerPortPA = board_serial_base();

	if(rom_pointer[1])
		nbootargs.dwMemSize = (dram_size - rom_pointer[1]) >> 20;
	else
		nbootargs.dwMemSize = dram_size >> 20;

}

#ifdef CONFIG_POWER
#define I2C_PMIC_8MP	4
int power_init_board(void)
{
	struct pmic *p;
	int ret;

	switch (nbootargs.chBoardType)
	{
	default:
	case BT_PICOCOREMX8MP:
		setup_i2c(I2C_PMIC_8MP, CONFIG_SYS_I2C_SPEED, 0x30, &i2c_pad_info_8mp);
		ret = power_pca9450b_init(I2C_PMIC_8MP);
		break;
	}

	if (ret)
		printf("power init failed");
	p = pmic_get("PCA9450");
	if(!p)
		printf("NULL...\n");
	pmic_probe(p);

	/* BUCKxOUT_DVS0/1 control BUCK123 output */
	pmic_reg_write(p, PCA9450_BUCK123_DVS, 0x29);

	/*
	 * increase VDD_SOC to typical value 0.95V before first
	 * DRAM access, set DVS1 to 0.85v for suspend.
	 * Enable DVS control through PMIC_STBY_REQ and
	 * set B1_ENMODE=1 (ON by PMIC_ON_REQ=H)
	 */
	pmic_reg_write(p, PCA9450_BUCK1OUT_DVS0, 0x1C);
	pmic_reg_write(p, PCA9450_BUCK1OUT_DVS1, 0x14);
	pmic_reg_write(p, PCA9450_BUCK1CTRL, 0x59);

	/* Kernel uses OD/OD freq for SOC */
	/* To avoid timing risk from SOC to ARM,increase VDD_ARM to OD voltage 0.95v */
	pmic_reg_write(p, PCA9450_BUCK2OUT_DVS0, 0x1C);

	/* set WDOG_B_CFG to cold reset */
	pmic_reg_write(p, PCA9450_RESET_CTRL, 0xA1);

	return 0;
}
#endif

static int spl_dram_init(void)
{
	switch (nbootargs.chBoardType)
	{
	default:
	case BT_PICOCOREMX8MP:
		/* TODO: change RAM detection
		 *  Simple detection: Rev. 1.00 k4f6e3s4hm_mgcj
		 */
		if(ddr_init(&dram_timing_k4f6e3s4hm_mgcj))
		{
			return 1;
		}
		break;
	}

	return 0;
}

void spl_board_init(void)
{
	/* Set GIC clock to 500Mhz for OD VDD_SOC. Kernel driver does not allow to change it.
	 * Should set the clock after PMIC setting done.
	 * Default is 400Mhz (system_pll1_800m with div = 2) set by ROM for ND VDD_SOC
	 */
#ifdef CONFIG_IMX8M_LPDDR4
	clock_enable(CCGR_GIC, 0);
	clock_set_target_val(GIC_CLK_ROOT, CLK_ROOT_ON | CLK_ROOT_SOURCE_SEL(5));
	clock_enable(CCGR_GIC, 1);
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

	fs_board_init_nboot_args();

	config_uart_pads();

	preloader_console_init();

	ret = spl_init();
	if (ret) {
		debug("spl_init() failed: %d\n", ret);
		hang();
	}

	enable_tzc380();

	power_init_board();

	/* DDR initialization */
	if(spl_dram_init())
	{
		printf("This UBoot can't be started. RAM initialization fails.\n");
		while(1);
	}

	printf("DDRInfo: RAM initialization success.\n");

	/* initalize ram area with zero before set */
	memset(pargs, 0x0, sizeof(struct fs_nboot_args));
	/* fill nboot args first after ram initialisation */
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

	printf("Using fixed config: 0x%x\n",nbootargs.chFeatures2);

	board_init_r(NULL, 0);
}
