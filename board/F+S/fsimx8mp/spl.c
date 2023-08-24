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
#include <fdt_support.h>		/* fdt_getprop_u32_default_node() */
#include <power/pmic.h>
#include <power/pca9450.h>
#include <asm/arch/clock.h>
#include <asm/mach-imx/gpio.h>
#include <asm/mach-imx/mxc_i2c.h>
#include <fsl_esdhc_imx.h>
#include <init.h>			/* arch_cpu_init() */
#include <mmc.h>
#include <asm/arch/ddr.h>
#include <dwc3-uboot.h>

#include <asm/mach-imx/boot_mode.h>	/* BOOT_TYPE_* */
#include "../common/fs_image_common.h"	/* fs_image_*() */
#include "../common/fs_board_common.h"	/* fs_board_*() */
#include "../common/fs_mmc_common.h"	/* struct fs_mmc_cd, fs_mmc_*(), ... */
#ifdef CONFIG_FS_SPL_MEMTEST_COMMON
#include "../common/fs_memtest_common.h"
#endif
#include <usb.h>

DECLARE_GLOBAL_DATA_PTR;

#define BT_PICOCOREMX8MP 0x0
#define BT_PICOCOREMX8MPr2 0x1
#define BT_ARMSTONEMX8MP 0x2
#define BT_EFUSMX8MP 0x3

static const char *board_names[] = {
	"PicoCoreMX8MP",
	"PicoCoreMX8MPr2",
	"armStoneMX8MP",
	"efusMX8MP",
	"(unknown)"
};

static unsigned int board_type;
static unsigned int board_rev;
static const char *board_name;
static const char *board_fdt;
static enum boot_device used_boot_dev;	/* Boot device used for NAND/MMC */
static bool boot_dev_init_done;
static unsigned int uboot_offs;
static bool secondary;			/* 0: primary, 1: secondary SPL */
static bool usb_initialized = false;

#ifdef CONFIG_POWER

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

struct i2c_pads_info i2c_pad_info_efusmx8mp = {
	.scl = {
		.i2c_mode = MX8MP_PAD_SAI5_RXFS__I2C6_SCL | PC,
		.gpio_mode = MX8MP_PAD_SAI5_RXFS__GPIO3_IO19 | PC,
		.gp = IMX_GPIO_NR(3, 19),
	},
	.sda = {
		.i2c_mode = MX8MP_PAD_SAI5_RXC__I2C6_SDA | PC,
		.gpio_mode = MX8MP_PAD_SAI5_RXC__GPIO3_IO20 | PC,
		.gp = IMX_GPIO_NR(3, 20),
	},
};

int power_init_board(void)
{
	struct pmic *p;
	int ret;
	struct i2c_pads_info *pi2c_pad_info;
	unsigned int bus;

	switch (board_type)
	{
	default:
	case BT_PICOCOREMX8MP:
	case BT_PICOCOREMX8MPr2:
	case BT_ARMSTONEMX8MP:
		bus = 4;
		pi2c_pad_info = &i2c_pad_info_8mp;
		break;
	case BT_EFUSMX8MP:
		bus = 5;
		pi2c_pad_info = &i2c_pad_info_efusmx8mp;
		break;
	}

	setup_i2c(bus, CONFIG_SYS_I2C_SPEED, 0x30, pi2c_pad_info);
	ret = power_pca9450_init(bus);

	if (ret)
		printf("power init failed");
	p = pmic_get("PCA9450");
	if(!p)
		printf("PMIC structure is NULL.\n");
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

#define WDOG_PAD_CTRL	(PAD_CTL_DSE6 | PAD_CTL_ODE | PAD_CTL_PUE | PAD_CTL_PE)

static iomux_v3_cfg_t const wdog_pads[] = {
	MX8MP_PAD_GPIO1_IO02__WDOG1_WDOG_B  | MUX_PAD_CTRL(WDOG_PAD_CTRL),
};

static void wdog_init(void)
{
	struct wdog_regs *wdog = (struct wdog_regs *)WDOG1_BASE_ADDR;

	imx_iomux_v3_setup_multiple_pads(wdog_pads, ARRAY_SIZE(wdog_pads));

	set_wdog_reset(wdog);
}

#define UART_PAD_CTRL	(PAD_CTL_DSE6 | PAD_CTL_FSEL1)

static iomux_v3_cfg_t const uart_pads_mp[] = {
	MX8MP_PAD_SAI3_TXFS__UART2_DCE_RX | MUX_PAD_CTRL(UART_PAD_CTRL),
	MX8MP_PAD_SAI3_TXC__UART2_DCE_TX | MUX_PAD_CTRL(UART_PAD_CTRL),
};

static iomux_v3_cfg_t const uart_pads_efusmx8mp[] = {
	MX8MP_PAD_SAI2_RXC__UART1_DCE_RX | MUX_PAD_CTRL(UART_PAD_CTRL),
	MX8MP_PAD_SAI2_RXFS__UART1_DCE_TX | MUX_PAD_CTRL(UART_PAD_CTRL),
};

#define UART_AUTOMOD_CTRL (PAD_CTL_DSE2 | PAD_CTL_PUE | PAD_CTL_PE)
#define SHDN_R232_PAD IMX_GPIO_NR(1, 3)
#define AUTONLINE_R232_PAD IMX_GPIO_NR(1, 4)

static iomux_v3_cfg_t const uart_auto_mode[] = {
	MX8MP_PAD_GPIO1_IO03__GPIO1_IO03 | MUX_PAD_CTRL(UART_AUTOMOD_CTRL),
	MX8MP_PAD_GPIO1_IO04__GPIO1_IO04 | MUX_PAD_CTRL(UART_AUTOMOD_CTRL),
};

static void config_uart(int bt)
{
	iomux_v3_cfg_t const *pad_list;
	unsigned pad_list_count;
	u32 clk_index;

	/* Set board type early for board_serial_base function.
	 * This is needed to use different UART ports dependent on board type,
	 * because default_serial_console uses board_serial_base to select correct
	 * serial_device structure.
	 * */
	board_type = bt;

	switch (board_type)
	{
	default:
	case BT_ARMSTONEMX8MP:
		/* Initialize SHDN_RS232 and AUTONLINE_RS232
		 * to auto online mode.
		 */
		imx_iomux_v3_setup_multiple_pads(uart_auto_mode, ARRAY_SIZE(uart_auto_mode));
		gpio_request (SHDN_R232_PAD, "SHDN");
		gpio_direction_output (SHDN_R232_PAD, 1);
		gpio_request (AUTONLINE_R232_PAD, "ONLINE");
		gpio_direction_output (AUTONLINE_R232_PAD, 0);
	case BT_PICOCOREMX8MP:
	case BT_PICOCOREMX8MPr2:
		pad_list = uart_pads_mp;
		clk_index = 1;
		pad_list_count = ARRAY_SIZE(uart_pads_mp);
		break;
	case BT_EFUSMX8MP:
		pad_list = uart_pads_efusmx8mp;
		clk_index = 0;
		pad_list_count = ARRAY_SIZE(uart_pads_efusmx8mp);
		break;
	}

	/* Setup UART pads */
	imx_iomux_v3_setup_multiple_pads(pad_list, pad_list_count);
	/* enable uart clock */
	init_uart_clk(clk_index);

	preloader_console_init();
}
/* Set base address depends on board type.
 * Override function from serial_mxc.c
 * */
ulong board_serial_base(void)
{
	switch (board_type)
	{
	case BT_EFUSMX8MP:
		return UART1_BASE;
	case BT_PICOCOREMX8MP:
	case BT_PICOCOREMX8MPr2:
	case BT_ARMSTONEMX8MP:
	default:
		break;
	}
	return UART2_BASE;
}

#ifdef CONFIG_MMC
/*
 * SD/MMC support.
 *
 *   Board             USDHC   CD-Pin                 Slot
 *   ----------------------------------------------------------------------
 *   PicoCoreMX8MP:    USDHC3  NA                     On-board eMMC
 *   ----------------------------------------------------------------------
 *   PicoCoreMX8MPr2:  USDHC3  NA                     On-board eMMC
 *   ----------------------------------------------------------------------
 *   armStoneMX8MP:    USDHC1  NA                     On-board eMMC
 *   ----------------------------------------------------------------------
 *   efusMX8MP:        USDHC1  NA                     On-board eMMC
 */
/* Pad settings if using eMMC */
#define USDHC_PAD_CTRL	(PAD_CTL_DSE6 | PAD_CTL_HYS | PAD_CTL_PUE |PAD_CTL_PE | \
			 PAD_CTL_FSEL2)
#define USDHC_GPIO_PAD_CTRL (PAD_CTL_HYS | PAD_CTL_DSE1)
#define USDHC_CD_PAD_CTRL (PAD_CTL_PE |PAD_CTL_PUE |PAD_CTL_HYS | PAD_CTL_DSE4)

#define USDHC3_RST_GPIO IMX_GPIO_NR(3, 16)

static iomux_v3_cfg_t const usdhc3_pads_int[] = {
	MUX_PAD_CTRL(USDHC_PAD_CTRL) | MX8MP_PAD_NAND_WE_B__USDHC3_CLK,
	MUX_PAD_CTRL(USDHC_PAD_CTRL) | MX8MP_PAD_NAND_WP_B__USDHC3_CMD,
	MUX_PAD_CTRL(USDHC_PAD_CTRL) | MX8MP_PAD_NAND_DATA04__USDHC3_DATA0,
	MUX_PAD_CTRL(USDHC_PAD_CTRL) | MX8MP_PAD_NAND_DATA05__USDHC3_DATA1,
	MUX_PAD_CTRL(USDHC_PAD_CTRL) | MX8MP_PAD_NAND_DATA06__USDHC3_DATA2,
	MUX_PAD_CTRL(USDHC_PAD_CTRL) | MX8MP_PAD_NAND_DATA07__USDHC3_DATA3,
	MUX_PAD_CTRL(USDHC_PAD_CTRL) | MX8MP_PAD_NAND_RE_B__USDHC3_DATA4,
	MUX_PAD_CTRL(USDHC_PAD_CTRL) | MX8MP_PAD_NAND_CE2_B__USDHC3_DATA5,
	MUX_PAD_CTRL(USDHC_PAD_CTRL) | MX8MP_PAD_NAND_CE3_B__USDHC3_DATA6,
	MUX_PAD_CTRL(USDHC_PAD_CTRL) | MX8MP_PAD_NAND_CLE__USDHC3_DATA7,
	MUX_PAD_CTRL(USDHC_PAD_CTRL) | MX8MP_PAD_NAND_CE1_B__USDHC3_STROBE,
	MUX_PAD_CTRL(USDHC_PAD_CTRL) | MX8MP_PAD_NAND_READY_B__USDHC3_RESET_B,
};

static iomux_v3_cfg_t const usdhc1_pads_int[] = {
	MUX_PAD_CTRL(USDHC_PAD_CTRL) | MX8MP_PAD_SD1_CLK__USDHC1_CLK,
	MUX_PAD_CTRL(USDHC_PAD_CTRL) | MX8MP_PAD_SD1_CMD__USDHC1_CMD,
	MUX_PAD_CTRL(USDHC_PAD_CTRL) | MX8MP_PAD_SD1_DATA0__USDHC1_DATA0,
	MUX_PAD_CTRL(USDHC_PAD_CTRL) | MX8MP_PAD_SD1_DATA1__USDHC1_DATA1,
	MUX_PAD_CTRL(USDHC_PAD_CTRL) | MX8MP_PAD_SD1_DATA2__USDHC1_DATA2,
	MUX_PAD_CTRL(USDHC_PAD_CTRL) | MX8MP_PAD_SD1_DATA3__USDHC1_DATA3,
	MUX_PAD_CTRL(USDHC_PAD_CTRL) | MX8MP_PAD_SD1_DATA4__USDHC1_DATA4,
	MUX_PAD_CTRL(USDHC_PAD_CTRL) | MX8MP_PAD_SD1_DATA5__USDHC1_DATA5,
	MUX_PAD_CTRL(USDHC_PAD_CTRL) | MX8MP_PAD_SD1_DATA6__USDHC1_DATA6,
	MUX_PAD_CTRL(USDHC_PAD_CTRL) | MX8MP_PAD_SD1_DATA7__USDHC1_DATA7,
	MUX_PAD_CTRL(USDHC_PAD_CTRL) | MX8MP_PAD_SD1_STROBE__USDHC1_STROBE,
	MUX_PAD_CTRL(USDHC_PAD_CTRL) | MX8MP_PAD_SD1_RESET_B__USDHC1_RESET_B,
};

static void fs_spl_init_emmc_pads(iomux_v3_cfg_t const *emmc_pads, unsigned size)
{
	imx_iomux_v3_setup_multiple_pads(emmc_pads, size);
	boot_dev_init_done = true;
}

int board_mmc_getcd(struct mmc *mmc)
{
	return 1;			/* eMMC always present */
}

int board_mmc_init(struct bd_info *bd)
{
	struct fsl_esdhc_cfg esdhc;
	int ret = 0;

	switch (used_boot_dev) {
	case MMC1_BOOT:
		init_clk_usdhc(0);
		esdhc.esdhc_base = USDHC1_BASE_ADDR;
		esdhc.sdhc_clk = mxc_get_clock(MXC_ESDHC_CLK);
		esdhc.max_bus_width = 8;
		break;

	case MMC2_BOOT:
		init_clk_usdhc(1);
		esdhc.esdhc_base = USDHC2_BASE_ADDR;
		esdhc.sdhc_clk = mxc_get_clock(MXC_ESDHC2_CLK);
		break;

	case MMC3_BOOT:
		init_clk_usdhc(2);
		esdhc.esdhc_base = USDHC3_BASE_ADDR;
		esdhc.sdhc_clk = mxc_get_clock(MXC_ESDHC3_CLK);
		esdhc.max_bus_width = 8;
		gpio_direction_output(USDHC3_RST_GPIO, 0);
		break;
	default:
		return -ENODEV;
	}

	ret = fsl_esdhc_initialize(bd, &esdhc);

	return ret;
}
#endif /* CONFIG_MMC */

/* Configure (and optionally start) the given boot device */
static int fs_spl_init_boot_dev(enum boot_device boot_dev, bool start,
				const char *type)
{
	if (boot_dev_init_done)
		return 0;

	used_boot_dev = boot_dev;
	switch (boot_dev) {
#ifdef CONFIG_MMC
	case MMC1_BOOT: /* armStoneMX8MP, efusMX8MP */
		fs_spl_init_emmc_pads(usdhc1_pads_int, ARRAY_SIZE(usdhc1_pads_int));
		if (start)
			mmc_initialize(NULL);
		break;
	case MMC3_BOOT: /* PicoCoreMX8MP(r2) */
		fs_spl_init_emmc_pads(usdhc3_pads_int, ARRAY_SIZE(usdhc3_pads_int));
		if (start)
			mmc_initialize(NULL);
		break;
		//### TODO: Also have setups for MMC1_BOOT and MMC2_BOOT
#endif
	case USB_BOOT:
		/* Nothing to do */
		break;

	default:
		printf("Can not handle %s boot device %s\n", type,
		       fs_board_get_name_from_boot_dev(boot_dev));
		return -ENODEV;
	}

	return 0;
}

/* Do the basic board setup when we have our final BOARD-CFG */
static void basic_init(void)
{
	void *fdt = fs_image_get_cfg_fdt();
	int offs = fs_image_get_board_cfg_offs(fdt);
	int i;
	char c;
	int index;
	const char *boot_dev_name;
	enum boot_device boot_dev;

	board_name = fdt_getprop(fdt, offs, "board-name", NULL);
	for (i = 0; i < ARRAY_SIZE(board_names); i++) {
		if (!strcmp(board_name, board_names[i]))
			break;
	}
	board_type = i;

	/*
	 * If an fdt-name is not given, use board name in lower case. Please
	 * note that this name is only used for the U-Boot device tree. The
	 * Linux device tree name is defined by executing U-Boot's environment
	 * variable set_bootfdt.
	 */
	board_fdt = fdt_getprop(fdt, offs, "board-fdt", NULL);
	if (!board_fdt) {
		static char board_name_lc[32];

		i = 0;
		do {
			c = board_name[i];
			if ((c >= 'A') && (c <= 'Z'))
				c += 'a' - 'A';
			board_name_lc[i++] = c;
		} while (c);

		board_fdt = (const char *)&board_name_lc[0];
	}

	board_rev = fdt_getprop_u32_default_node(fdt, offs, 0,
						 "board-rev", 100);
	config_uart(board_type);
	if (secondary)
		puts("Warning! Running secondary SPL, please check if"
		     " primary SPL is damaged.\n");

	boot_dev_name = fdt_getprop(fdt, offs, "boot-dev", NULL);
	boot_dev = fs_board_get_boot_dev_from_name(boot_dev_name);

	printf("BOARD-ID: %s\n", fs_image_get_board_id());

	/* Get U-Boot offset */
#ifdef CONFIG_FS_UPDATE_SUPPORT
	index = 0;			/* ### TODO: Select slot A or B */
#else
	index = 0;
#endif
	offs = fs_image_get_nboot_info_offs(fdt);
	uboot_offs = fdt_getprop_u32_default_node(fdt, offs, index,
						  "uboot-start", 0);

	/* We need to have the boot device pads active when starting U-Boot */
	fs_spl_init_boot_dev(boot_dev, false, "BOARD-CFG");

	power_init_board();
}

void board_init_f(ulong dummy)
{
	int ret;
	enum boot_device boot_dev;

	/* Clear the BSS. */
	memset(__bss_start, 0, __bss_end - __bss_start);

	arch_cpu_init();

	wdog_init();
	timer_init();

#if 0
	/*
	 * Enable this to have early debug output before BOARD-CFG is loaded
	 * You have to provide the board type, we do not know it yet
	 */
	//config_uart(BT_PICOCOREMX8MP);
	//config_uart(BT_PICOCOREMX8MPr2);
	//config_uart(BT_ARMSTONEMX8MP);
	config_uart(BT_EFUSMX8MP);
#endif
	/* Init malloc_f pool and boot stages */
	ret = spl_init();
	if (ret) {
		debug("spl_init() failed: %d\n", ret);
		hang();
	}
	enable_tzc380();

#if 0
	// ### TODO: How do we determine this on i.MX8MP?
	{
		struct src *src;
		/* Determine if we are running on primary or secondary SPL */
		src = (struct src *)SRC_BASE_ADDR;
		if (readl(&src->gpr10) & (1 << 30))
			secondary = true;
	}
#endif

	/* Try loading from the current boot dev. If this fails, try USB. */
	boot_dev = get_boot_device();
	if (boot_dev != USB_BOOT) {
		if (fs_spl_init_boot_dev(boot_dev, true, "current")
		    || fs_image_load_system(boot_dev, secondary, basic_init))
			boot_dev = USB_BOOT;
	}
	if (boot_dev == USB_BOOT) {
		bool need_cfg = true;

		/* Try loading a BOARD-CFG from the fused boot device first */
		boot_dev = fs_board_get_boot_dev_from_fuses();
		if (!fs_spl_init_boot_dev(boot_dev, true, "fused")
		    && !fs_image_load_system(boot_dev, secondary, NULL))
			need_cfg = false;

		/* Load the system from USB with Serial Download Protocol */
		fs_image_all_sdp(need_cfg, basic_init);
	}

	/* If running on secondary SPL, mark BOARD-CFG to pass info to U-Boot */
	if (secondary)
		fs_image_mark_secondary();

	/* At this point we have a valid system configuration */
	board_init_r(NULL, 0);
}
#ifdef CONFIG_FS_SPL_MEMTEST_COMMON
void dram_test(void)
{
	void *fdt = fs_image_get_cfg_fdt();
	int offs = fs_image_get_board_cfg_offs(fdt);
	unsigned long dram_size = fdt_getprop_u32_default_node(fdt, offs, 0,
			      "dram-size", 0x400);
	dram_size = dram_size << 20;
	gd->ram_size = dram_size;

	/* Enable caches */
	gd->arch.tlb_size = PGTABLE_SIZE;
	gd->arch.tlb_addr = 0x970000;
	gd->bd->bi_dram[0].start = PHYS_SDRAM;
	gd->bd->bi_dram[0].size  = dram_size;
	enable_caches();

	memtester(PHYS_SDRAM, dram_size);
	panic(" ");
}
#endif

void spl_board_init(void)
{
	iomux_v3_cfg_t bl_on_pad;
	unsigned bl_on_gpio;

	switch (board_type)
	{
	default:
	case BT_PICOCOREMX8MP:
	case BT_PICOCOREMX8MPr2:
		bl_on_pad = MX8MP_PAD_SPDIF_TX__GPIO5_IO03 | MUX_PAD_CTRL(NO_PAD_CTRL);
		bl_on_gpio = IMX_GPIO_NR(5, 3);
		break;
	case BT_ARMSTONEMX8MP:
		bl_on_pad = MX8MP_PAD_SAI3_RXFS__GPIO4_IO28 | MUX_PAD_CTRL(NO_PAD_CTRL);
		bl_on_gpio = IMX_GPIO_NR(4, 28);
		break;
	case BT_EFUSMX8MP:
		bl_on_pad = MX8MP_PAD_GPIO1_IO01__GPIO1_IO01 | MUX_PAD_CTRL(NO_PAD_CTRL);
		bl_on_gpio = IMX_GPIO_NR(1, 1);
		break;
	}

	imx_iomux_v3_setup_pad(bl_on_pad);
	/* backlight off*/
	gpio_request(bl_on_gpio, "BL_ON");
	gpio_direction_output(bl_on_gpio, 0);

	/* Set GIC clock to 500Mhz for OD VDD_SOC. Kernel driver does not allow to change it.
	 * Should set the clock after PMIC setting done.
	 * Default is 400Mhz (system_pll1_800m with div = 2) set by ROM for ND VDD_SOC
	 */
#ifdef CONFIG_IMX8M_LPDDR4
	clock_enable(CCGR_GIC, 0);
	clock_set_target_val(GIC_CLK_ROOT, CLK_ROOT_ON | CLK_ROOT_SOURCE_SEL(5));
	clock_enable(CCGR_GIC, 1);
#endif

#ifndef CONFIG_SPL_USB_SDP_SUPPORT
	/* Serial download mode */
	if (is_usb_boot()) {
		puts("Back to ROM, SDP\n");
		restore_boot_params();
	}
#endif

#ifdef CONFIG_FS_SPL_MEMTEST_COMMON
	    dram_test();
#endif
	debug("Normal Boot\n");
}

/* Return the sector number where U-Boot starts in eMMC (User HW partition) */
unsigned long spl_mmc_get_uboot_raw_sector(struct mmc *mmc)
{
	return uboot_offs / 512;
}

/* U-Boot is always loaded from the User HW partition */
int spl_boot_partition(const u32 boot_device)
{
	return 0;
}

/* U-Boot is always loaded from the User HW partition */
int spl_mmc_emmc_boot_partition(struct mmc *mmc)
{
	return 0;
}

#if defined(CONFIG_USB_DWC3) || defined(CONFIG_USB_XHCI_IMX8M)

#define USB_PHY_CTRL0			0xF0040
#define USB_PHY_CTRL0_REF_SSP_EN	BIT(2)

#define USB_PHY_CTRL1			0xF0044
#define USB_PHY_CTRL1_RESET		BIT(0)
#define USB_PHY_CTRL1_COMMONONN		BIT(1)
#define USB_PHY_CTRL1_ATERESET		BIT(3)
#define USB_PHY_CTRL1_VDATSRCENB0	BIT(19)
#define USB_PHY_CTRL1_VDATDETENB0	BIT(20)

#define USB_PHY_CTRL2			0xF0048
#define USB_PHY_CTRL2_TXENABLEN0	BIT(8)

#define USB_PHY_CTRL6			0xF0058

#define HSIO_GPR_BASE                               (0x32F10000U)
#define HSIO_GPR_REG_0                              (HSIO_GPR_BASE)
#define HSIO_GPR_REG_0_USB_CLOCK_MODULE_EN_SHIFT    (1)
#define HSIO_GPR_REG_0_USB_CLOCK_MODULE_EN          (0x1U << HSIO_GPR_REG_0_USB_CLOCK_MODULE_EN_SHIFT)


static struct dwc3_device dwc3_device_data = {
	.maximum_speed = USB_SPEED_HIGH,
	.base = USB2_BASE_ADDR,
	.dr_mode = USB_DR_MODE_PERIPHERAL,
	.index = 1,
	.power_down_scale = 2,
};

int usb_gadget_handle_interrupts(int index)
{
	dwc3_uboot_handle_interrupt(index);
	return 0;
}

static void dwc3_nxp_usb_phy_init(struct dwc3_device *dwc3)
{
	u32 RegData;

	/* enable usb clock via hsio gpr */
	RegData = readl(HSIO_GPR_REG_0);
	RegData |= HSIO_GPR_REG_0_USB_CLOCK_MODULE_EN;
	writel(RegData, HSIO_GPR_REG_0);

	/* USB3.0 PHY signal fsel for 100M ref */
	RegData = readl(dwc3->base + USB_PHY_CTRL0);
	RegData = (RegData & 0xfffff81f) | (0x2a<<5);
	writel(RegData, dwc3->base + USB_PHY_CTRL0);

	RegData = readl(dwc3->base + USB_PHY_CTRL6);
	RegData &=~0x1;
	writel(RegData, dwc3->base + USB_PHY_CTRL6);

	RegData = readl(dwc3->base + USB_PHY_CTRL1);
	RegData &= ~(USB_PHY_CTRL1_VDATSRCENB0 | USB_PHY_CTRL1_VDATDETENB0 |
			USB_PHY_CTRL1_COMMONONN);
	RegData |= USB_PHY_CTRL1_RESET | USB_PHY_CTRL1_ATERESET;
	writel(RegData, dwc3->base + USB_PHY_CTRL1);

	RegData = readl(dwc3->base + USB_PHY_CTRL0);
	RegData |= USB_PHY_CTRL0_REF_SSP_EN;
	writel(RegData, dwc3->base + USB_PHY_CTRL0);

	RegData = readl(dwc3->base + USB_PHY_CTRL2);
	RegData |= USB_PHY_CTRL2_TXENABLEN0;
	writel(RegData, dwc3->base + USB_PHY_CTRL2);

	RegData = readl(dwc3->base + USB_PHY_CTRL1);
	RegData &= ~(USB_PHY_CTRL1_RESET | USB_PHY_CTRL1_ATERESET);
	writel(RegData, dwc3->base + USB_PHY_CTRL1);
}

/* efusmx8mp */
//iomux_v3_cfg_t usb2_pwr_pad = (MX8MP_PAD_GPIO1_IO14__GPIO1_IO14 | MUX_PAD_CTRL(NO_PAD_CTRL));
//#define USB2_PWR_EN IMX_GPIO_NR(1, 14)
iomux_v3_cfg_t usb2_pwr_pad = (MX8MP_PAD_GPIO1_IO14__USB2_OTG_PWR | MUX_PAD_CTRL(NO_PAD_CTRL));

//iomux_v3_cfg_t usb2_oc_pad = (MX8MP_PAD_GPIO1_IO15__GPIO1_IO15 | MUX_PAD_CTRL(NO_PAD_CTRL));
//#define USB2_OC IMX_GPIO_NR(1, 15)
iomux_v3_cfg_t usb2_oc_pad = (MX8MP_PAD_GPIO1_IO15__USB2_OTG_OC | MUX_PAD_CTRL(NO_PAD_CTRL));

#define USB1_PWR_EN IMX_GPIO_NR(1, 12)

int board_usb_init(int index, enum usb_init_type init)
{
	if(usb_initialized)
		return 0;

	if (index == 0 && init == USB_INIT_DEVICE)
		/* usb host only */
		return 0;

	debug("USB%d: %s init.\n", index, (init)?"otg":"host");

	if (!usb_initialized)
		imx8m_usb_power(index, true);

	if (index == 1 && init == USB_INIT_DEVICE) {
		if (!usb_initialized) {
			usb_initialized = true;
			switch (board_type)
			{
			case BT_EFUSMX8MP:
				imx_iomux_v3_setup_pad(usb2_oc_pad);
				//gpio_request(USB2_OC, "usb2_oc");
				//gpio_direction_output(USB2_OC, 1);
				/* Enable otg power */
				imx_iomux_v3_setup_pad(usb2_pwr_pad);
				//gpio_request(USB2_PWR_EN, "usb2_pwr");
				//gpio_direction_output(USB2_PWR_EN, 0);
				break;
			default:
				break;
			}
			dwc3_nxp_usb_phy_init(&dwc3_device_data);
			return dwc3_uboot_init(&dwc3_device_data);
		}
	}

	return 0;
}

int board_usb_cleanup(int index, enum usb_init_type init)
{
	int ret = 0;

	if (index == 0 && init == USB_INIT_DEVICE)
		/* usb host only */
		return 0;

	debug("USB%d: %s cleanup.\n", index, (init)?"otg":"host");
	if (index == 1 && init == USB_INIT_DEVICE) {
		if (usb_initialized) {
			usb_initialized = false;
			dwc3_uboot_exit(index);
			/* Disable otg power */
			//gpio_request(USB2_PWR_EN, "usb2_pwr");
			//gpio_direction_output(USB2_PWR_EN, 1);
		}
	} else if (index == 0 && init == USB_INIT_HOST) {
		/* Disable host power */
		gpio_direction_output(USB1_PWR_EN, 0);
	}

	imx8m_usb_power(index, false);

	return ret;
}
#endif


/*
 * Provide our own boot order, which in fact has just one entry: the current
 * boot device. Unfortunately the regular SPL infrastructure of U-Boot uses an
 * own set of boot devices (defined in arch/arm/include/asm/spl.h) that
 * differs from NXP's definitions in arch/arm/include/asm/mach-imx/boot_mode.h.
 * Here only BOOT_DEVICE_MMC1, BOOT_DEVICE_MMC2 and BOOT_DEVICE_MMC2_2 are
 * known as MMC devices. These values do not actually refer to a hardware
 * device of this number, they simply mean the MMC index. So BOOT_DEVICE_MMC1
 * is the first MMC device with index 0 and both BOOT_DEVICE_MMC2 as well as
 * BOOT_DEVICE_MMC2_2 point to the second MMC device with index 1. See
 * spl_mmc_get_device_index() in common/spl/spl_mmc.c. So at some point, the
 * NXP boot device has to be converted to U-Boot's device. This happens in
 * spl_boot_device() in arch/arm/mach-imx/spl.c.
 *
 * Unfortunately this mapping there does not fit our needs. We only have one
 * MMC device active in SPL, and this is the boot device. It will always be on
 * index 0, no matter what USDHC port we actually boot from. So we always have
 * to map to BOOT_DEVICE_MMC1. By overwriting the weak board_boot_order() in
 * common/spl/spl.c, we can change this to our needs.
 */
void board_boot_order(u32 *spl_boot_list)
{
	switch (get_boot_device()) {
	case MMC1_BOOT:
	case MMC2_BOOT:
	case MMC3_BOOT:
		spl_boot_list[0] = BOOT_DEVICE_MMC1;
		break;
	case NAND_BOOT:
		spl_boot_list[0] = BOOT_DEVICE_NAND;
		break;
	case USB_BOOT:
		spl_boot_list[0] = BOOT_DEVICE_BOARD;
		break;
	default:
		spl_boot_list[0] = BOOT_DEVICE_NONE;
		break;
	}
}

#ifdef CONFIG_SPL_LOAD_FIT
/*
 * This function is called for each appended device tree. If we signal a match
 * (return value 0), the referenced device tree (and only this) is loaded
 * behind U-Boot. So from the view of U-Boot, it always has the right device
 * tree when starting. See doc/README.multi-dtb-fit for details.
 */
int board_fit_config_name_match(const char *name)
{
	return strcmp(name, board_fdt);
}
#endif
