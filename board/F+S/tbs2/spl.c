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
#include <power/pmic.h>
#include <power/bd71837.h>
#include <asm/mach-imx/gpio.h>
#include <asm/mach-imx/mxc_i2c.h>
#include <fsl_esdhc.h>
#include <mmc.h>
#include <asm/arch/imx8m_ddr.h>

#include "../common/fs_board_common.h"	/* fs_board_*() */
#include "../common/fs_mmc_common.h"	/* struct fs_mmc_cd, fs_mmc_*(), ... */

DECLARE_GLOBAL_DATA_PTR;

extern struct dram_timing_info ddr3l_mt41k256m16tw_dram_timing;

static struct fs_nboot_args nbootargs;

#define BT_TBS2				0

#define I2C_PAD_CTRL	(PAD_CTL_DSE6 | PAD_CTL_HYS | PAD_CTL_PUE | PAD_CTL_PE)
#define PC MUX_PAD_CTRL(I2C_PAD_CTRL)
struct i2c_pads_info i2c_pad_tbs2 = {
	.scl = {
		.i2c_mode = IMX8MM_PAD_I2C1_SCL_I2C1_SCL | PC,
		.gpio_mode = IMX8MM_PAD_I2C1_SCL_GPIO5_IO14 | PC,
		.gp = IMX_GPIO_NR(5, 14),
	},
	.sda = {
		.i2c_mode = IMX8MM_PAD_I2C1_SDA_I2C1_SDA | PC,
		.gpio_mode = IMX8MM_PAD_I2C1_SDA_GPIO5_IO15 | PC,
		.gp = IMX_GPIO_NR(5, 15),
	},
};

static void fs_board_init_nboot_args(void)
{
	nbootargs.dwID = FSHWCONFIG_ARGS_ID;
	nbootargs.dwSize = 16*4;
	nbootargs.dwNBOOT_VER = 1;
	/* 1 GB RAM */
	long MemSize = 0x40000000;
	/* Number of DRAM chips */
	long NumDram = 0x2;
	/* eMMC size */
	long FlashSize = 0x100000000;
	/* Serial debug port */
	long dwDbgSerPortPA = UART1_BASE_ADDR;

	/* Setup TBS2 settings */
	nbootargs.chBoardType = BT_TBS2;
	nbootargs.chBoardRev = 100;
	nbootargs.chFeatures2 = 1<<0 | 1<<1 | 1<<2;

	/* Setup RAM settings */
	if(rom_pointer[1])
		nbootargs.dwMemSize = (MemSize - rom_pointer[1]) >> 20;
	else
		nbootargs.dwMemSize = MemSize >> 20;

	nbootargs.dwNumDram = NumDram;
	nbootargs.dwFlashSize = FlashSize;
	nbootargs.dwDbgSerPortPA = dwDbgSerPortPA;
}

#ifdef CONFIG_POWER
#define I2C_PMIC_0	0
int power_init_board(void)
{
	struct pmic *p;
	int ret;

	ret = power_bd71837_init(I2C_PMIC_0);
	if (ret)
		printf("power init failed");

	p = pmic_get("BD71837");

	pmic_probe(p);

	/* decrease RESET key long push time from the default 10s to 10ms */
	pmic_reg_write(p, BD71837_PWRONCONFIG1, 0x0);

	/* unlock the PMIC regs */
	pmic_reg_write(p, BD71837_REGLOCK, 0x1);

	/* increase VDD_SOC to typical value 0.85v before first DRAM access */
	pmic_reg_write(p, BD71837_BUCK1_VOLT_RUN, 0x0f);

	/* increase VDD_DRAM to 0.9v for 3Ghz DDR */
	pmic_reg_write(p, BD71837_BUCK5_VOLT, 0x2);

	/* increase NVCC_DRAM_1V35 to 1.35v for DDR3L */
	pmic_reg_write(p, BD71837_BUCK8_VOLT, 0x37);

	/* setup ldo5 to 1V1 for intel switch */
	pmic_reg_write(p, BD71837_LDO5_VOLT, 0xe3);

	/* lock the PMIC regs */
	pmic_reg_write(p, BD71837_REGLOCK, 0x11);

	return 0;
}
#endif

static int spl_dram_init(void)
{
	if(ddr_init(&ddr3l_mt41k256m16tw_dram_timing))
	{
		return 1;
	}

	return 0;
}

#define USDHC1_CD_GPIO	IMX_GPIO_NR(1, 6)

/* RESET Pin */
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
//	IMX8MM_PAD_SD1_RESET_B_GPIO2_IO10 | MUX_PAD_CTRL(USDHC_GPIO_PAD_CTRL),
};

#if defined(CONFIG_SD_BOOT)
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
 * signal count will be added automatically according to bus_width.
 */
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
#if defined(CONFIG_SD_BOOT)
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

#if defined(CONFIG_SD_BOOT)
	/* mmc1: USDHC3 (eMMC, if available), no CD */
	ret = fs_mmc_setup(bd, 8, &sdhc_cfg[usdhc3_int], NULL);

	gpio_request(USDHC3_PWR_GPIO, "usdhc3_reset");
	gpio_direction_output(USDHC3_PWR_GPIO, 0);
	udelay(500);
	gpio_direction_output(USDHC3_PWR_GPIO, 1);
#endif
	return ret;
}


void spl_board_init(void)
{

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

#define UART_PAD_CTRL	(PAD_CTL_DSE6 | PAD_CTL_FSEL1)
#define WDOG_PAD_CTRL	(PAD_CTL_DSE6 | PAD_CTL_ODE | PAD_CTL_PUE | PAD_CTL_PE)

static iomux_v3_cfg_t const wdog_pads[] = {
	IMX8MM_PAD_GPIO1_IO02_WDOG1_WDOG_B  | MUX_PAD_CTRL(WDOG_PAD_CTRL),
};

static iomux_v3_cfg_t const uart_pads[] = {
	IMX8MM_PAD_SAI2_RXC_UART1_RX | MUX_PAD_CTRL(UART_PAD_CTRL),
	IMX8MM_PAD_SAI2_RXFS_UART1_TX | MUX_PAD_CTRL(UART_PAD_CTRL),
};

void setup_board_f(void)
{
	struct wdog_regs *wdog = (struct wdog_regs*) WDOG1_BASE_ADDR;
	imx_iomux_v3_setup_multiple_pads(wdog_pads, ARRAY_SIZE(wdog_pads));
	set_wdog_reset(wdog);

	imx_iomux_v3_setup_multiple_pads(uart_pads, ARRAY_SIZE(uart_pads));
}

void board_init_f(ulong dummy)
{
	int ret;
	struct fs_nboot_args *pargs = (struct fs_nboot_args*)(CONFIG_SYS_SDRAM_BASE + 0x00001000);

	/* Clear the BSS. */
	memset(__bss_start, 0, __bss_end - __bss_start);

	arch_cpu_init();

	fs_board_init_nboot_args();
	setup_board_f();

	timer_init();

	preloader_console_init();

	ret = spl_init();
	if (ret) {
		debug("spl_init() failed: %d\n", ret);
		hang();
	}

	enable_tzc380();

	setup_i2c(0, CONFIG_SYS_I2C_SPEED, 0x7f, &i2c_pad_tbs2);

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

	board_init_r(NULL, 0);
}
